/* The NumWorks EADK API, implemented on top of port.h.
 *
 * Built against the official eadk.h in its "simulator" flavour (PLATFORM_DEVICE not defined), where
 * eadk_keyboard_scan() and eadk_timing_millis() are inline wrappers over the _eadk_* functions below.
 */
#include <eadk.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "port.h"
#include "font_data.h"

/* see np_native.h */
const uint8_t numplay_launcher[1] = {1};

static uint16_t shadow[NP_W * NP_H]; /* what the calculator's display controller would hold */


/* ------------------------------------------------------------------ saved games
 * The games save through epsilon_storage(), which on a calculator is a region of RAM holding a list of records
 * (16-bit size, name, content; a zero size ends the list). A native build has no such region, so np_port_storage()
 * (called from games/common/epsilon_app.h, see tools/np_patches.py) provides a buffer of its own, filled from the
 * backend's persistent copy at first use and written back when its contents change. */
#define NP_STORAGE_SIZE (256 * 1024)
static uint8_t store[NP_STORAGE_SIZE + 8] __attribute__((aligned(8)));
static bool store_open;
static uint32_t store_hash, store_polled;

/* bytes up to and including the terminating zero size, or -1 if the list is corrupt */
static int store_used(void) {
  uint32_t p = 0;
  while (p + 2 <= NP_STORAGE_SIZE) {
    uint32_t n = (uint32_t)(store[p] | store[p + 1] << 8);
    if (!n) return (int)(p + 2);
    if (n < 4 || p + n > NP_STORAGE_SIZE) return -1;
    p += n;
  }
  return -1;
}
static uint32_t fnv(const uint8_t *d, uint32_t n) {
  uint32_t h = 2166136261u;
  while (n--) h = (h ^ *d++) * 16777619u;
  return h;
}
static void store_flush(void) {
  int used = store_used();
  if (used < 0) return; /* never overwrite a good copy with a half-written list */
  uint32_t h = fnv(store, (uint32_t)used);
  if (h == store_hash) return;
  if (port_storage_store(store, (uint32_t)used)) store_hash = h;
}
static void store_poll(void) {
  if (!store_open) return;
  uint32_t t = port_millis();
  if (t - store_polled < 2000) return;
  store_polled = t;
  store_flush();
}

uint8_t *np_port_storage(uint32_t *size) {
  if (!store_open) {
    store_open = true;
    uint32_t got = port_storage_load(store, NP_STORAGE_SIZE);
    if (got && store_used() < 0) memset(store, 0, sizeof store); /* a damaged copy: start empty */
    int u = store_used();
    store_hash = u > 0 ? fnv(store, (uint32_t)u) : 0; /* what is already saved is not written again */
    atexit(store_flush);
  }
  if (size) *size = NP_STORAGE_SIZE;
  return store;
}

bool np_touch(int *x, int *y) { return port_touch(x, y); }

/* ------------------------------------------------------------------ display */
static bool clip(eadk_rect_t *r) {
  if (r->x >= NP_W || r->y >= NP_H) return false;
  if (r->x + r->width > NP_W) r->width = (uint16_t)(NP_W - r->x);
  if (r->y + r->height > NP_H) r->height = (uint16_t)(NP_H - r->y);
  return r->width && r->height;
}

/* Games push whole screens every frame even when little changed. Comparing against the shadow copy is cheap next
 * to showing it (on the 3DS every pixel shown is a store into a rotated buffer), so only the rectangle that really
 * changed is passed on. NP_NO_DIFF turns this off (to check that it changes nothing). */
#ifdef NP_NO_DIFF
#define DIFF 0
#else
#define DIFF 1
#endif

void eadk_display_push_rect(eadk_rect_t rect, const eadk_color_t *pixels) {
  eadk_rect_t r = rect;
  if (!clip(&r)) return;
  int minx = r.width, maxx = -1, miny = r.height, maxy = -1;
  for (int y = 0; y < r.height; y++) { /* pixels is laid out with the caller's (unclipped) width */
    uint16_t *a = &shadow[(r.y + y) * NP_W + r.x];
    const uint16_t *b = pixels + (size_t)y * rect.width;
    if (DIFF) {
      if (!memcmp(a, b, (size_t)r.width * 2)) continue;
      int x0 = 0, x1 = r.width - 1;
      while (a[x0] == b[x0]) x0++;
      while (a[x1] == b[x1]) x1--;
      if (x0 < minx) minx = x0;
      if (x1 > maxx) maxx = x1;
    } else {
      minx = 0, maxx = r.width - 1;
    }
    if (y < miny) miny = y;
    maxy = y;
    memcpy(a, b, (size_t)r.width * 2);
  }
  if (maxy < 0) return; /* nothing changed */
  port_present(shadow, r.x + minx, r.y + miny, maxx - minx + 1, maxy - miny + 1);
}

void eadk_display_push_rect_uniform(eadk_rect_t rect, eadk_color_t color) {
  if (!clip(&rect)) return;
  int minx = rect.width, maxx = -1, miny = rect.height, maxy = -1;
  for (int y = 0; y < rect.height; y++) {
    uint16_t *row = &shadow[(rect.y + y) * NP_W + rect.x];
    int x0 = 0, x1 = rect.width - 1;
    if (DIFF) {
      while (x0 <= x1 && row[x0] == color) x0++;
      if (x0 > x1) continue; /* the row already has this colour */
      while (row[x1] == color) x1--;
    }
    if (x0 < minx) minx = x0;
    if (x1 > maxx) maxx = x1;
    if (y < miny) miny = y;
    maxy = y;
    for (int x = x0; x <= x1; x++) row[x] = color;
  }
  if (maxy < 0) return;
  port_present(shadow, rect.x + minx, rect.y + miny, maxx - minx + 1, maxy - miny + 1);
}

void eadk_display_pull_rect(eadk_rect_t rect, eadk_color_t *pixels) {
  eadk_rect_t r = rect;
  if (!clip(&r)) return;
  for (int y = 0; y < r.height; y++)
    memcpy(pixels + (size_t)y * rect.width, &shadow[(r.y + y) * NP_W + r.x], (size_t)r.width * 2);
}

bool eadk_display_wait_for_vblank(void) {
  store_poll();
  port_vblank();
  return true;
}

/* Fallback text (the calculator draws it with its firmware font; this is a stand-in with fixed cells). */
void eadk_display_draw_string(const char *text, eadk_point_t p, bool large, eadk_color_t fg, eadk_color_t bg) {
  const int cw = large ? NP_LARGE_W : NP_SMALL_W, ch = large ? NP_LARGE_H : NP_SMALL_H;
  const uint8_t *font = large ? np_font_large : np_font_small;
  const int bpr = (cw + 7) / 8; /* bytes per glyph row */
  int x = p.x;
  for (const unsigned char *s = (const unsigned char *)text; *s; s++, x += cw) {
    if (x >= NP_W) break;
    const uint8_t *g = (*s >= 32 && *s < 127) ? font + (size_t)(*s - 32) * ch * bpr : NULL;
    for (int row = 0; row < ch && p.y + row < NP_H; row++)
      for (int col = 0; col < cw && x + col < NP_W; col++) {
        bool on = g && (g[row * bpr + col / 8] >> (7 - col % 8)) & 1;
        shadow[(p.y + row) * NP_W + x + col] = on ? fg : bg;
      }
  }
  int w = x - p.x;
  if (w > NP_W - p.x) w = NP_W - p.x;
  if (w > 0 && p.y < NP_H) port_present(shadow, p.x, p.y, w, p.y + ch > NP_H ? NP_H - p.y : ch);
}

/* ------------------------------------------------------------------ keyboard */
static uint64_t last_scan;
static uint64_t prev_events;


/* ------------------------------------------------------------------ pen gestures as keys
 * For menus and windows, which are made for the arrows and OK: while a game has asked for it (np_gestures(true)),
 * dragging the pen is arrow presses (one per G_STEP pixels, along the larger movement) and a quick tap is OK. The
 * game switches this off where the pen is a pointer instead (a play field), and each key is held for 2 scans and
 * released for 2, so any game, at any frame rate, sees a clean press and release. */
#define G_STEP 14
#define G_TAP_MS 450
#ifdef NP_LAUNCHER
#define GEST_DEFAULT true /* the launcher's home screen: swipe = arrows, tap = OK */
#else
#define GEST_DEFAULT false
#endif
static bool gest_on = GEST_DEFAULT;
static struct {
  bool down, ignore, moved;
  int ax, ay;
  uint32_t t0;
} g;
static uint8_t gq[16];
static int gq_n, g_key = -1, g_hold, g_gap;

void np_gestures(bool on) { gest_on = on; }

/* ---- the NumPlay launcher, which runs every small game in one program (patched in by tools/np_patches.py):
 * a game that uses the stylus goes on the touch screen, any other on the top screen, and the home screen is back on
 * the touch screen, usable with the stylus (gestures). */
static const char *const pen_games[] = {"blockblast", "balatro", NULL};

static void np_set_screen(bool top) {
  port_set_screen(top);
  port_present(shadow, 0, 0, NP_W, NP_H); /* repaint what is already there on the screen now in use */
}

void np_game_starting(const char *id) {
  bool pen = false;
  for (int i = 0; pen_games[i]; i++) pen |= !strcmp(id, pen_games[i]);
  gest_on = false; /* a game that wants gestures asks for them itself */
  np_set_screen(!pen);
}

void np_game_returned(void) {
  gest_on = true;
  np_set_screen(false);
}
static void gq_push(int key) {
  if (gq_n < 16) gq[gq_n++] = (uint8_t)key;
}

static uint64_t gesture_keys(void) {
  int x = 0, y = 0;
  bool pen = port_touch(&x, &y);
  if (!gest_on) { /* a press that goes on after a switch to gestures is not a tap or a swipe */
    g.down = pen, g.ignore = true, gq_n = 0, g_key = -1, g_hold = g_gap = 0;
    return 0;
  }
  uint32_t now = port_millis();
  if (pen && !g.down) {
    g.down = true, g.ignore = false, g.moved = false, g.ax = x, g.ay = y, g.t0 = now;
  } else if (pen && !g.ignore) {
    int dx = x - g.ax, dy = y - g.ay;
    if (abs(dx) >= G_STEP || abs(dy) >= G_STEP) {
      g.moved = true;
      if (abs(dx) >= abs(dy)) {
        while (abs(dx) >= G_STEP) gq_push(dx < 0 ? 0 : 3), g.ax += dx < 0 ? -G_STEP : G_STEP, dx = x - g.ax;
        g.ay = y;
      } else {
        while (abs(dy) >= G_STEP) gq_push(dy < 0 ? 1 : 2), g.ay += dy < 0 ? -G_STEP : G_STEP, dy = y - g.ay;
        g.ax = x;
      }
    }
  } else if (!pen && g.down) {
    if (!g.ignore && !g.moved && now - g.t0 < G_TAP_MS) gq_push(4); /* a tap: OK */
    g.down = false, g.ignore = false;
  }
  if (g_key >= 0) {
    if (--g_hold <= 0) g_key = -1, g_gap = 2;
  } else if (g_gap > 0) {
    g_gap--;
  } else if (gq_n) {
    g_key = gq[0];
    memmove(gq, gq + 1, (size_t)--gq_n);
    g_hold = 2;
  }
  return g_key >= 0 ? 1ull << g_key : 0;
}

void _eadk_keyboard_scan_do_scan(void) {
  uint64_t k = port_keys();
  k |= gesture_keys();
  last_scan = k;
}
uint32_t _eadk_keyboard_scan_low(void) { return (uint32_t)last_scan; }
uint32_t _eadk_keyboard_scan_high(void) { return (uint32_t)(last_scan >> 32); }

/* Blocks up to *timeout ms for a key press; returns its event number (the key number for the keys that
 * produce events), or 0 on timeout. *timeout is left as the time remaining. */
eadk_event_t eadk_event_get(int32_t *timeout) {
  int32_t left = timeout ? *timeout : 0;
  for (;;) {
    uint64_t now = port_keys();
    uint64_t fresh = now & ~prev_events;
    prev_events = now;
    for (int k = 0; k < 64; k++) {
      if (!((fresh >> k) & 1)) continue;
      if (k == 6 || k == 8) continue; /* Home and On/Off are not events */
      if (timeout) *timeout = left;
      return (eadk_event_t)k;
    }
    if (timeout && left <= 0) {
      *timeout = 0;
      return 0;
    }
    store_poll();
    port_sleep_ms(8);
    left -= 8;
  }
}

/* ------------------------------------------------------------------ timing */
uint32_t _eadk_timing_millis_low(void) { return port_millis(); }
uint32_t _eadk_timing_millis_high(void) { return 0; }
void eadk_timing_msleep(uint32_t ms) {
  store_poll();
  port_sleep_ms(ms);
}
void eadk_timing_usleep(uint32_t us) {
  store_poll();
  port_sleep_ms((us + 999) / 1000);
}

/* ------------------------------------------------------------------ the rest */
static uint8_t brightness = 240;
void eadk_backlight_set_brightness(uint8_t b) { brightness = b; }
uint8_t eadk_backlight_brightness(void) { return brightness; }
bool eadk_battery_is_charging(void) { return false; }
uint8_t eadk_battery_level(void) { return 100; }
float eadk_battery_voltage(void) { return 4.0f; }
bool eadk_usb_is_plugged(void) { return false; }

const char *eadk_external_data = NULL;
size_t eadk_external_data_size = 0;

uint32_t eadk_random(void) {
  static uint32_t s;
  if (!s) s = port_millis() * 2654435761u | 1u;
  s ^= s << 13;
  s ^= s >> 17;
  s ^= s << 5;
  return s;
}
