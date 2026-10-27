/* Nintendo 3DS backend (libctru).
 *
 * Screens. The game is 320x240. Games that use the stylus run on the bottom screen, which is exactly 320x240 and is
 * the touch screen; every other game runs on the top screen (400x240), centred with a 40 px border either side, and
 * the key map is shown on the screen that is free. (Celeste's letterboxed picture is scaled up to fill the top screen
 * instead: NP_TOP_SCALE.) Which screen is chosen when the program starts (NP_GAME_ON_TOP) and, in the NumPlay launcher,
 * again each time it starts a game (port_set_screen, called from eadk_port.c).
 *
 * Keys. The 3DS has fewer buttons than a NumWorks, so each kind of game gets its own layout, chosen when the game is
 * built (NP_KEYMAP_*, see tools/build_big.py). START is always Home (save and quit); the D-pad and, unless the layout
 * says otherwise, the Circle Pad are the arrows. The layouts are the tables below, each with its help text.
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
#define START_ON_TOP true
#else
#define START_ON_TOP false
#endif
#define BORDER ((400 - NP_W) / 2) /* the 320 px picture centred on the 400 px top screen */

/* NumWorks key numbers (eadk_key_t) used below */
enum {
  K_LEFT = 0, K_UP = 1, K_DOWN = 2, K_RIGHT = 3, K_OK = 4, K_BACK = 5, K_HOME = 6, K_SHIFT = 12, K_ALPHA = 13,
  K_XNT = 14, K_VAR = 15, K_TOOLBOX = 16, K_BACKSPACE = 17, K_LPAR = 33, K_RPAR = 34, K_MUL = 39, K_DIV = 40, K_0 = 48,
  K_1 = 42, K_2 = 43, K_3 = 44, K_4 = 36, K_5 = 37, K_6 = 38, K_7 = 30, K_8 = 31, K_9 = 32, K_PLUS = 45, K_MINUS = 46,
  K_EXE = 52
};

typedef struct {
  u32 button;
  int key;
} bind_t;

/* what the Circle Pad presses (left, up, down, right): the arrows unless a layout says otherwise */
#define STICK_DEFAULT {K_LEFT, K_UP, K_DOWN, K_RIGHT}

#ifndef NP_GAME_TITLE
#define NP_GAME_TITLE "NumPlay"
#endif
#define HELP_TITLE NP_GAME_TITLE " for 3DS"

#if defined(NP_KEYMAP_ACTION) /* Hollow Knight */
#define HELP_TEXT \
  "D-pad / Circle Pad  move, look, rest\n" \
  "A jump  B attack  Y dash  X focus/cast\n" \
  "R quick map   L inventory\n" \
  "SELECT pause   START save and quit\n"
static const bind_t binds[] = {{KEY_A, K_OK}, {KEY_B, K_BACK}, {KEY_Y, K_SHIFT}, {KEY_X, K_ALPHA}, {KEY_R, K_VAR},
                               {KEY_L, K_TOOLBOX}, {KEY_SELECT, K_BACKSPACE}};
#elif defined(NP_KEYMAP_CELESTE)
#define HELP_TEXT \
  "D-pad / Circle Pad  move, aim the dash\n" \
  "A jump  B dash/talk  R/Y grab, climb\n" \
  "X EXE (menus)   SELECT pause\n" \
  "START save and quit\n"
static const bind_t binds[] = {{KEY_A, K_OK}, {KEY_B, K_BACK}, {KEY_R, K_TOOLBOX}, {KEY_Y, K_TOOLBOX},
                               {KEY_X, K_EXE}, {KEY_SELECT, K_BACKSPACE}};
#elif defined(NP_KEYMAP_CHAMPION)
#define HELP_TEXT \
  "D-pad / Circle Pad  walk, play\n" \
  "A or Y  OK: talk, doors, sport actions\n" \
  "B  island: map   in a sport: pause\n" \
  "START save and quit\n"
static const bind_t binds[] = {{KEY_A, K_OK}, {KEY_Y, K_OK}, {KEY_B, K_BACK}, {KEY_X, K_EXE}};
#elif defined(NP_KEYMAP_BUILDER) /* Final Earth 2, NumPowder: with the stylus */
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
#elif defined(NP_KEYMAP_ARCADE) /* the many games that use the arrows, OK and Back (and Shift) */
#define HELP_TEXT \
  "D-pad / Circle Pad  arrows\n" \
  "A OK   X EXE   Y Shift\n" \
  "B Back (pause, back)\n" \
  "SELECT backspace   START save and quit\n"
static const bind_t binds[] = {{KEY_A, K_OK}, {KEY_B, K_BACK}, {KEY_X, K_EXE}, {KEY_Y, K_SHIFT},
                               {KEY_SELECT, K_BACKSPACE}};
#elif defined(NP_KEYMAP_UNDERTALE)
#define HELP_TEXT \
  "D-pad / Circle Pad  walk, move the SOUL\n" \
  "A OK (Z: talk, confirm)\n" \
  "B Back (X: cancel, skip text)\n" \
  "Y menu (C: items, stats, phone)\n" \
  "X EXE   SELECT backspace\n" \
  "START keep the moment and quit\n"
static const bind_t binds[] = {{KEY_A, K_OK}, {KEY_B, K_BACK}, {KEY_X, K_EXE}, {KEY_Y, K_TOOLBOX},
                               {KEY_SELECT, K_BACKSPACE}};
#elif defined(NP_KEYMAP_SUBWAY)
#define HELP_TEXT \
  "D-pad / Circle Pad  change track, jump,\n" \
  "                    roll\n" \
  "A hoverboard / choose   B pause, back\n" \
  "X headstart (1)   Y score booster (3)\n" \
  "START save and quit\n"
static const bind_t binds[] = {{KEY_A, K_OK}, {KEY_B, K_BACK}, {KEY_X, K_1}, {KEY_Y, K_3}, {KEY_SELECT, K_BACKSPACE}};
#elif defined(NP_KEYMAP_FNAF)
#define HELP_TEXT \
  "D-pad Left/Right  look left, right\n" \
  "A monitor up/down (OK)   B pause\n" \
  "L left door (7)    R right door (9)\n" \
  "X left light (1)   Y right light (3)\n" \
  "Up/Down/arrows: cameras on the monitor\n" \
  "SELECT mute the call (0)\n" \
  "START keep the night and quit\n"
static const bind_t binds[] = {{KEY_A, K_OK}, {KEY_B, K_BACK}, {KEY_L, K_7}, {KEY_R, K_9}, {KEY_X, K_1}, {KEY_Y, K_3},
                               {KEY_SELECT, K_0}};
#elif defined(NP_KEYMAP_DOOM)
#define HELP_TEXT \
  "D-pad / Circle Pad  move, turn\n" \
  "A fire    B use (doors, switches)\n" \
  "X next weapon   Y previous weapon\n" \
  "L strafe left   R strafe right\n" \
  "ZL run (hold)   ZR automap (New 3DS)\n" \
  "SELECT menu / pause\n" \
  "START save and quit\n"
static const bind_t binds[] = {{KEY_A, K_OK}, {KEY_B, K_BACK}, {KEY_X, K_RPAR}, {KEY_Y, K_LPAR}, {KEY_L, K_XNT},
                               {KEY_R, K_VAR}, {KEY_ZL, K_ALPHA}, {KEY_ZR, K_TOOLBOX}, {KEY_SELECT, K_EXE}};
#elif defined(NP_KEYMAP_ISAAC)
#undef STICK_DEFAULT
#define STICK_DEFAULT {K_4, K_8, K_2, K_6} /* the Circle Pad is the tears: left, up, down, right */
#define HELP_TEXT \
  "D-pad  walk (and menus)\n" \
  "Circle Pad  cry tears\n" \
  "A use item   B pause   X bomb\n" \
  "Y pill / card   L bomb   R use item\n" \
  "SELECT hold: see the map\n" \
  "START keep the run and quit\n"
static const bind_t binds[] = {{KEY_A, K_OK}, {KEY_B, K_BACK}, {KEY_X, K_9}, {KEY_Y, K_1}, {KEY_L, K_SHIFT},
                               {KEY_R, K_ALPHA}, {KEY_SELECT, K_TOOLBOX}};
#elif defined(NP_KEYMAP_FNF)
#define HELP_TEXT \
  "Notes: D-pad, or the face buttons:\n" \
  "   X up   Y left   B down   A right\n" \
  "L OK (choose)   R shift (freeplay)\n" \
  "SELECT Back (pause)\n" \
  "START save and quit\n"
static const bind_t binds[] = {{KEY_X, K_LPAR}, {KEY_Y, K_7}, {KEY_B, K_8}, {KEY_A, K_RPAR}, {KEY_L, K_OK},
                               {KEY_R, K_SHIFT}, {KEY_SELECT, K_BACK}};
#elif defined(NP_KEYMAP_COOKIE) /* Cookie Clicker: with the stylus */
#define HELP_TEXT \
  "Stylus: tap the cookie, the store, the\n" \
  "buttons; swipe + tap in other menus.\n\n" \
  "A OK (click)   B Back (menu)   X EXE\n" \
  "Y var (pop wrinkler)   L divide\n" \
  "R multiply (1/10/100)   ZL shift\n" \
  "ZR alpha   SELECT toolbox (stats)\n" \
  "START save and quit\n"
static const bind_t binds[] = {{KEY_A, K_OK}, {KEY_B, K_BACK}, {KEY_X, K_EXE}, {KEY_Y, K_VAR}, {KEY_L, K_DIV},
                               {KEY_R, K_MUL}, {KEY_ZL, K_SHIFT}, {KEY_ZR, K_ALPHA}, {KEY_SELECT, K_TOOLBOX}};
#else /* NumPlay's own layout: the launcher and its games, Plants vs. Zombies */
#define NP_KEYMAP_DEFAULT 1
#define HELP_TEXT \
  "D-pad / Circle Pad  arrows\n" \
  "A OK  B Back  X EXE  Y Backspace\n" \
  "SELECT Toolbox  START Home (leave)\n" \
  "ZL Shift  ZR Alpha (New 3DS)\n" \
  "hold L: A B X Y = 1 2 3 4\n" \
  "hold R: A B X Y = 5 6 7 8\n\n" \
  "Stylus: Block Blast, Balatro and Plants\n" \
  "vs. Zombies use the touch screen; in\n" \
  "the menus, swipe = arrows, tap = OK.\n"
static const bind_t binds[] = {{KEY_SELECT, K_TOOLBOX}, {KEY_ZL, K_SHIFT}, {KEY_ZR, K_ALPHA}};
#endif

static const int stick_keys[4] = STICK_DEFAULT;

static bool ready, dirty, game_top = START_ON_TOP;
static uint64_t t0;
static PrintConsole con_top, con_bot;

/* the key map, on the screen the game is not on */
static void show_help(void) {
  consoleSelect(game_top ? &con_bot : &con_top);
  consoleClear();
  printf("\x1b[1;1H%s\n\n%s", HELP_TITLE, HELP_TEXT);
}

static void clear_screen(gfxScreen_t s) {
  u16 w, h;
  memset(gfxGetFramebuffer(s, GFX_LEFT, &w, &h), 0, (size_t)w * h * 2);
}

static void ensure_init(void) {
  if (ready) return;
  ready = true;
  /* New 3DS: 804 MHz and the L2 cache. A CIA asks for this in its RSF; a .3dsx started from the Homebrew Launcher
   * runs at the old 268 MHz unless it asks here. (Old 3DS: no effect.) */
  osSetSpeedupEnable(true);
  gfxInit(GSP_RGB565_OES, GSP_RGB565_OES, false); /* both screens 16 bit, like the game's own pixels */
  /* single buffer: games draw partial rectangles over what is already there */
  gfxSetDoubleBuffering(GFX_TOP, false);
  gfxSetDoubleBuffering(GFX_BOTTOM, false);
  atexit(gfxExit);
  consoleInit(GFX_TOP, &con_top);
  consoleInit(GFX_BOTTOM, &con_bot);
  /* start black: pushes of unchanged pixels are skipped, so nothing may be left on the screen from before */
  clear_screen(game_top ? GFX_TOP : GFX_BOTTOM);
  show_help();
  t0 = svcGetSystemTick();
}

/* The NumPlay launcher moves the picture between screens as it starts and ends games (games with a stylus
 * go on the touch screen). The caller repaints afterwards. */
void port_set_screen(bool top) {
  ensure_init();
  if (top == game_top) return;
#ifdef NP_TOP_SCALE
  return; /* Celeste stays where it is */
#endif
  game_top = top;
  clear_screen(GFX_TOP);
  clear_screen(GFX_BOTTOM);
  show_help();
  dirty = true;
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
  if (cp.dx < -40) m |= 1ull << stick_keys[0];
  if (cp.dy > 40) m |= 1ull << stick_keys[1];
  if (cp.dy < -40) m |= 1ull << stick_keys[2];
  if (cp.dx > 40) m |= 1ull << stick_keys[3];

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
   * of our upright 320x240 image is at fbuf[(x + offset) * 240 + (239 - y)]. (240 is fw, not fh.) */
  u16 *fbuf = (u16 *)gfxGetFramebuffer(game_top ? GFX_TOP : GFX_BOTTOM, GFX_LEFT, &fw, &fh);
#ifdef NP_TOP_SCALE
  np_blit_scaled(fbuf, fw, fb, x, y, w, h); /* Celeste: its 320x180 picture, scaled to fill the top screen */
  dirty = true;
  return;
#endif
  const int off = game_top ? BORDER : 0;
  /* In blocks of 16 columns: each row of a block is read in one go (32 bytes, one cache line), and the writes of
   * the next 15 rows land in the same 16 cache lines of the rotated buffer, instead of every store touching a
   * different line. */
  for (int cx0 = x; cx0 < x + w; cx0 += 16) {
    int cw = x + w - cx0 < 16 ? x + w - cx0 : 16;
    u16 *dst = fbuf + (size_t)(cx0 + off) * fw + (fw - 1 - y);
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

/* The touch screen is 320x240, the same pixels as the game, when the game is on the bottom screen. */
bool port_touch(int *x, int *y) {
  ensure_init();
  if (game_top) return false;
  if (!(hidKeysHeld() & KEY_TOUCH)) return false;
  touchPosition t;
  hidTouchRead(&t);
  *x = t.px, *y = t.py;
  return true;
}

void port_vblank(void) {
  ensure_init();
  service_system();
  flush();
  gspWaitForVBlank();
}
