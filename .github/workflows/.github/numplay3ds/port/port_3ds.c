/* Nintendo 3DS backend (libctru).
 *
 * Screen: the game runs on the bottom screen, which is exactly 320x240 and is also the touch screen. The top screen
 * shows the key map. (Define NP_GAME_ON_TOP to centre the game on the top screen instead, 40 px borders either side,
 * and leave the touch screen free.)
 *
 * Keys: the 3DS has fewer buttons than a NumWorks, so each kind of game gets its own layout, chosen when the game is
 * built (NP_KEYMAP_*, see tools/build_big.py). D-pad and Circle Pad are always the arrows, START is always Home
 * (leave / save and quit).
 *
 *   NumPlay (default)  A OK  B Back  X EXE  Y Backspace  SELECT Toolbox  ZL/ZR Shift/Alpha
 *                      hold L: A B X Y = 1 2 3 4        hold R: A B X Y = 5 6 7 8
 *   ACTION  (Hollow Knight)  A jump  B attack  Y dash  X focus/cast  R map  L inventory  SELECT pause
 *   CELESTE            A jump  B dash/talk  R or Y grab  X EXE  SELECT pause
 *                      (its letterboxed 320x180 picture is scaled to fill the top screen: NP_TOP_SCALE)
 *   CHAMPION           A, Y OK (action)  B Back (map, pause)  X EXE
 *   BUILDER (Final Earth 2, NumPowder)  A OK  B Back  X toolbox  Y var  L shift  R alpha  SELECT backspace
 *                      ZL minus  ZR plus (New 3DS)
 */
#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "port.h"
#ifdef NP_TOP_SCALE
#include "scale_top.h"
#endif

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

/* NumWorks key numbers (eadk_key_t) used below */
enum { K_LEFT = 0, K_UP = 1, K_DOWN = 2, K_RIGHT = 3, K_OK = 4, K_BACK = 5, K_HOME = 6, K_SHIFT = 12, K_ALPHA = 13,
       K_VAR = 15, K_TOOLBOX = 16, K_BACKSPACE = 17, K_PLUS = 45, K_MINUS = 46, K_EXE = 52 };
/* ... and for the digit layers of the default layout: 1 2 3 4 / 5 6 7 8 */
enum { K_1 = 42, K_2 = 43, K_3 = 44, K_4 = 36, K_5 = 37, K_6 = 38, K_7 = 30, K_8 = 31 };

typedef struct {
  u32 button;
  int key;
} bind_t;

#if defined(NP_KEYMAP_ACTION)
#define HELP_TITLE "Hollow Knight for 3DS"
#define HELP_TEXT \
  "D-pad / Circle Pad  move, look, rest (up)\n" \
  "A jump   B attack   Y dash   X focus/cast\n" \
  "R quick map   L inventory\n" \
  "SELECT pause   START save and quit\n"
static const bind_t binds[] = {{KEY_A, K_OK}, {KEY_B, K_BACK}, {KEY_Y, K_SHIFT}, {KEY_X, K_ALPHA}, {KEY_R, K_VAR},
                               {KEY_L, K_TOOLBOX}, {KEY_SELECT, K_BACKSPACE}};
#elif defined(NP_KEYMAP_CELESTE)
#define HELP_TITLE "Celeste for 3DS (game: top screen)"
#define HELP_TEXT \
  "D-pad / Circle Pad  move, aim the dash\n" \
  "A jump  B dash/talk  R/Y grab, climb\n" \
  "X EXE (menus)   SELECT pause\n" \
  "START save and quit\n"
static const bind_t binds[] = {{KEY_A, K_OK}, {KEY_B, K_BACK}, {KEY_R, K_TOOLBOX}, {KEY_Y, K_TOOLBOX},
                               {KEY_X, K_EXE}, {KEY_SELECT, K_BACKSPACE}};
#elif defined(NP_KEYMAP_CHAMPION)
#define HELP_TITLE "Champion Island Games for 3DS"
#define HELP_TEXT \
  "D-pad / Circle Pad  walk, play\n" \
  "A or Y  OK: talk, doors, each sport's action\n" \
  "B  island: map   in a sport: pause\n" \
  "START save and quit\n"
static const bind_t binds[] = {{KEY_A, K_OK}, {KEY_Y, K_OK}, {KEY_B, K_BACK}, {KEY_X, K_EXE}};
#elif defined(NP_KEYMAP_BUILDER)
#define HELP_TITLE "Builder / sandbox layout"
#define HELP_TEXT \
  "D-pad / Circle Pad  move cursor or brush\n" \
  "A OK   B Back   X toolbox   Y var (menu)\n" \
  "L shift (hold)   R alpha (pause)\n" \
  "SELECT backspace   ZL minus   ZR plus\n" \
  "(ZL / ZR: New 3DS)   START save and quit\n\n" \
  "Stylus: touch the play field to point or draw;\n" \
  "drag the empty view to scroll (Final Earth 2).\n" \
  "In menus and windows: swipe = arrows, tap = OK.\n"
static const bind_t binds[] = {{KEY_A, K_OK}, {KEY_B, K_BACK}, {KEY_X, K_TOOLBOX}, {KEY_Y, K_VAR}, {KEY_L, K_SHIFT},
                               {KEY_R, K_ALPHA}, {KEY_SELECT, K_BACKSPACE}, {KEY_ZL, K_MINUS}, {KEY_ZR, K_PLUS}};
#else
#define NP_KEYMAP_DEFAULT 1
#define HELP_TITLE "NumPlay for 3DS"
#define HELP_TEXT \
  "D-pad / Circle Pad  arrows\n" \
  "A OK   B Back   X EXE   Y Backspace\n" \
  "START  Home (leave game)\n" \
  "SELECT Toolbox\n" \
  "ZL / ZR  Shift / Alpha (New 3DS)\n\n" \
  "hold L:  A=1  B=2  X=3  Y=4\n" \
  "hold R:  A=5  B=6  X=7  Y=8\n\n" \
  "Block Blast: touch a shape, drag it to the\n" \
  "board with the stylus, lift to place it.\n"
static const bind_t binds[] = {{KEY_SELECT, K_TOOLBOX}, {KEY_ZL, K_SHIFT}, {KEY_ZR, K_ALPHA}};
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
  printf("\x1b[1;1H%s\n\n%s", HELP_TITLE, HELP_TEXT);
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
  if (h & KEY_DLEFT) m |= 1ull << K_LEFT;
  if (h & KEY_DUP) m |= 1ull << K_UP;
  if (h & KEY_DDOWN) m |= 1ull << K_DOWN;
  if (h & KEY_DRIGHT) m |= 1ull << K_RIGHT;
  if (h & KEY_START) m |= 1ull << K_HOME;

  circlePosition cp;
  hidCircleRead(&cp);
  if (cp.dx < -40) m |= 1ull << K_LEFT;
  if (cp.dy > 40) m |= 1ull << K_UP;
  if (cp.dy < -40) m |= 1ull << K_DOWN;
  if (cp.dx > 40) m |= 1ull << K_RIGHT;

  for (unsigned i = 0; i < sizeof binds / sizeof binds[0]; i++)
    if (h & binds[i].button) m |= 1ull << binds[i].key;

#ifdef NP_KEYMAP_DEFAULT
  /* A/B/X/Y: OK/Back/EXE/Backspace, or digits while L or R is held */
  static const int base[4] = {K_OK, K_BACK, K_EXE, K_BACKSPACE};
  static const int layerL[4] = {K_1, K_2, K_3, K_4};
  static const int layerR[4] = {K_5, K_6, K_7, K_8};
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
#ifdef NP_TOP_SCALE
  np_blit_scaled(fbuf, fw, fb, x, y, w, h); /* Celeste: its 320x180 picture, scaled to fill the top screen */
  dirty = true;
  return;
#endif
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
