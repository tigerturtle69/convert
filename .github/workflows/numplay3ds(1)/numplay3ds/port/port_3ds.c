/* Nintendo 3DS backend (libctru). NOT yet tested on hardware: written against the libctru headers.
 *
 * Screen: the game runs on the bottom screen, which is exactly 320x240. The top screen shows the key map.
 * (Define NP_GAME_ON_TOP to centre the game on the top screen instead, 40 px borders either side, and leave
 * the touch screen free.)
 *
 * Keys: the 3DS has fewer buttons than a NumWorks. Base layer, then L and R each turn A/B/X/Y into digits:
 *   D-pad / Circle Pad  arrows      A  OK      B  Back      X  EXE      Y  Backspace
 *   START  Home (leave a game)      SELECT  Toolbox         ZL / ZR (New 3DS)  Shift / Alpha
 *   hold L:  A=1 B=2 X=3 Y=4        hold R:  A=5 B=6 X=7 Y=8
 */
#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include "port.h"

#ifdef NP_GAME_ON_TOP
#define GAME_SCREEN GFX_TOP
#define X_OFFSET ((400 - NP_W) / 2)
#else
#define GAME_SCREEN GFX_BOTTOM
#define X_OFFSET 0
#endif

static bool ready;
static bool dirty;
static uint64_t t0;

static void ensure_init(void) {
  if (ready) return;
  ready = true;
#ifdef NP_GAME_ON_TOP
  gfxInit(GSP_RGB565_OES, GSP_BGR8_OES, false);
#else
  gfxInit(GSP_BGR8_OES, GSP_RGB565_OES, false);
#endif
  /* single buffer: games draw partial rectangles over what is already there */
  gfxSetDoubleBuffering(GAME_SCREEN, false);
  atexit(gfxExit);
#ifdef NP_GAME_ON_TOP
  consoleInit(GFX_BOTTOM, NULL);
#else
  consoleInit(GFX_TOP, NULL);
#endif
#ifdef NP_KEYMAP_ACTION
  printf("\x1b[1;1HHollow Knight for 3DS\n\n"
         "D-pad / Circle Pad  move, look, rest (up)\n"
         "A jump   B attack   Y dash   X focus/cast\n"
         "R quick map   L inventory\n"
         "SELECT pause   START save and quit\n");
#else
  printf("\x1b[1;1HNumPlay for 3DS\n\n"
         "D-pad / Circle Pad  arrows\n"
         "A OK   B Back   X EXE   Y Backspace\n"
         "START  Home (leave game)\n"
         "SELECT Toolbox\n"
         "ZL / ZR  Shift / Alpha (New 3DS)\n\n"
         "hold L:  A=1  B=2  X=3  Y=4\n"
         "hold R:  A=5  B=6  X=7  Y=8\n");
#endif
  t0 = svcGetSystemTick();
}

/* the HOME menu / power button: returns false when the app must close */
static void service_system(void) {
  if (!aptMainLoop()) exit(0);
}

static void flush(void) {
  if (!dirty) return;
  dirty = false;
  gfxFlushBuffers();
  gfxSwapBuffers();
}

uint64_t port_keys(void) {
  ensure_init();
  service_system();
  hidScanInput();
  u32 h = hidKeysHeld();
  uint64_t m = 0;
#define K(bit, key) \
  if (h & (bit)) m |= 1ull << (key)
  K(KEY_DLEFT, 0);
  K(KEY_DUP, 1);
  K(KEY_DDOWN, 2);
  K(KEY_DRIGHT, 3);
  K(KEY_START, 6);
#ifndef NP_KEYMAP_ACTION
  K(KEY_SELECT, 16); /* toolbox */
  K(KEY_ZL, 12);     /* shift */
  K(KEY_ZR, 13);     /* alpha */
#endif

  circlePosition cp;
  hidCircleRead(&cp);
  if (cp.dx < -40) m |= 1ull << 0;
  if (cp.dy > 40) m |= 1ull << 1;
  if (cp.dy < -40) m |= 1ull << 2;
  if (cp.dx > 40) m |= 1ull << 3;

#ifdef NP_KEYMAP_ACTION
  /* action games (Hollow Knight): every button it needs fits on a plain 3DS.
   *   A jump (OK)   B attack (Back)   Y dash (Shift)   X focus / cast (Alpha)
   *   R quick map (Var)   L inventory (Toolbox)   SELECT pause menu (Backspace)   START save and quit (Home) */
  if (h & KEY_A) m |= 1ull << 4;
  if (h & KEY_B) m |= 1ull << 5;
  if (h & KEY_Y) m |= 1ull << 12;
  if (h & KEY_X) m |= 1ull << 13;
  if (h & KEY_R) m |= 1ull << 15;
  if (h & KEY_L) m |= 1ull << 16;
  if (h & KEY_SELECT) m |= 1ull << 17;
#else
  /* A/B/X/Y: OK/Back/EXE/Backspace, or digits while L or R is held */
  static const int base[4] = {4, 5, 52, 17};
  static const int layerL[4] = {42, 43, 44, 36}; /* 1 2 3 4 */
  static const int layerR[4] = {37, 38, 30, 31}; /* 5 6 7 8 */
  const int *map = (h & KEY_L) ? layerL : (h & KEY_R) ? layerR : base;
  if (h & KEY_A) m |= 1ull << map[0];
  if (h & KEY_B) m |= 1ull << map[1];
  if (h & KEY_X) m |= 1ull << map[2];
  if (h & KEY_Y) m |= 1ull << map[3];
#endif
  return m;
}

uint32_t port_millis(void) {
  ensure_init();
  return (uint32_t)(((svcGetSystemTick() - t0) * 1000ull) / SYSCLOCK_ARM11);
}

void port_sleep_ms(uint32_t ms) {
  ensure_init();
  service_system();
  flush(); /* a game that only sleeps between frames still gets its drawing shown */
  svcSleepThread((s64)ms * 1000000ll);
}

void port_present(const uint16_t *fb, int x, int y, int w, int h) {
  ensure_init();
  u16 fw, fh;
  /* The 3DS frame buffer is rotated: it is stored column by column, bottom pixel first. For a 16 bit format
   * the pixel (x, y) of the upright 320x240 image is at fbuf[(x + X_OFFSET) * fh + (fh - 1 - y)] where fh = 240. */
  u16 *fbuf = (u16 *)gfxGetFramebuffer(GAME_SCREEN, GFX_LEFT, &fw, &fh);
  for (int cx = x; cx < x + w; cx++) {
    u16 *col = fbuf + (size_t)(cx + X_OFFSET) * fh + (fh - 1);
    const uint16_t *src = fb + y * NP_W + cx;
    for (int cy = y; cy < y + h; cy++, src += NP_W) col[-cy] = *src;
  }
  dirty = true;
}

void port_vblank(void) {
  ensure_init();
  service_system();
  flush();
  gspWaitForVBlank();
}
