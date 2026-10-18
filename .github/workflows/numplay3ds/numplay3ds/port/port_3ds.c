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
#include <string.h>
#include <sys/stat.h>
#include "port.h"

/* Saves go to sdmc:/3ds/<NP_SAVE_DIR>/save.bin */
#ifndef NP_SAVE_DIR
#define NP_SAVE_DIR "NumPlay"
#endif
#define SAVE_PATH "sdmc:/3ds/" NP_SAVE_DIR "/save.bin"

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
  /* New 3DS: 804 MHz and the L2 cache. A CIA asks for this in its RSF; a .3dsx started from the Homebrew Launcher
   * runs at the old 268 MHz unless it asks here. (Old 3DS: no effect.) */
  osSetSpeedupEnable(true);
#ifdef NP_GAME_ON_TOP
  gfxInit(GSP_RGB565_OES, GSP_BGR8_OES, false);
#else
  gfxInit(GSP_BGR8_OES, GSP_RGB565_OES, false);
#endif
  /* single buffer: games draw partial rectangles over what is already there */
  gfxSetDoubleBuffering(GAME_SCREEN, false);
  atexit(gfxExit);
  { /* start black: pushes of unchanged pixels are skipped, so nothing may be left on the screen from before */
    u16 w, h;
    memset(gfxGetFramebuffer(GAME_SCREEN, GFX_LEFT, &w, &h), 0, (size_t)w * h * 2);
  }
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
         "hold R:  A=5  B=6  X=7  Y=8\n\n"
         "Block Blast: touch a shape, drag it to the\n"
         "board with the stylus, lift to place it.\n");
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
  /* gfxGetFramebuffer reports the frame buffer as it is stored: 240 wide, 320 tall (bottom) or 400 tall (top).
   * It is rotated: stored column by column, each column 240 pixels long, bottom pixel first. So the pixel (x, y)
   * of our upright 320x240 image is at fbuf[(x + X_OFFSET) * 240 + (239 - y)]. (240 is fw, not fh.) */
  u16 *fbuf = (u16 *)gfxGetFramebuffer(GAME_SCREEN, GFX_LEFT, &fw, &fh);
  /* In blocks of 16 columns: each row of a block is read in one go (32 bytes, one cache line), and the writes of
   * the next 15 rows land in the same 16 cache lines of the rotated buffer, instead of every store touching a
   * different line. */
  for (int cx0 = x; cx0 < x + w; cx0 += 16) {
    int cw = x + w - cx0 < 16 ? x + w - cx0 : 16;
    u16 *dst = fbuf + (size_t)(cx0 + X_OFFSET) * fw + (fw - 1 - y);
    const uint16_t *src = fb + y * NP_W + cx0;
    for (int cy = 0; cy < h; cy++, src += NP_W, dst--)
      for (int i = 0; i < cw; i++) dst[(size_t)i * fw] = src[i];
  }
  dirty = true;
}

uint32_t port_storage_load(uint8_t *buf, uint32_t cap) {
  FILE *f = fopen(SAVE_PATH, "rb");
  if (!f) return 0;
  size_t n = fread(buf, 1, cap, f);
  fclose(f);
  return (uint32_t)n;
}

bool port_storage_store(const uint8_t *buf, uint32_t len) {
  mkdir("sdmc:/3ds", 0777);
  mkdir("sdmc:/3ds/" NP_SAVE_DIR, 0777);
  FILE *f = fopen(SAVE_PATH, "wb");
  if (!f) return false;
  size_t n = fwrite(buf, 1, len, f);
  return fclose(f) == 0 && n == len;
}

/* The game is on the bottom screen, which is the touch screen: 320x240, the same pixels as the game. */
bool port_touch(int *x, int *y) {
#ifdef NP_GAME_ON_TOP
  (void)x, (void)y;
  return false;
#else
  ensure_init();
  if (!(hidKeysHeld() & KEY_TOUCH)) return false;
  touchPosition t;
  hidTouchRead(&t);
  *x = t.px, *y = t.py;
  return true;
#endif
}

void port_vblank(void) {
  ensure_init();
  service_system();
  flush();
  gspWaitForVBlank();
}
