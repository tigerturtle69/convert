/* The NumWorks EADK API, implemented on top of port.h.
 *
 * Built against the official eadk.h in its "simulator" flavour (PLATFORM_DEVICE not defined), where
 * eadk_keyboard_scan() and eadk_timing_millis() are inline wrappers over the _eadk_* functions below.
 */
#include <eadk.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "port.h"
#include "font_data.h"

/* see np_native.h */
const uint8_t numplay_launcher[1] = {1};

static uint16_t shadow[NP_W * NP_H]; /* what the calculator's display controller would hold */

/* ------------------------------------------------------------------ display */
static bool clip(eadk_rect_t *r) {
  if (r->x >= NP_W || r->y >= NP_H) return false;
  if (r->x + r->width > NP_W) r->width = (uint16_t)(NP_W - r->x);
  if (r->y + r->height > NP_H) r->height = (uint16_t)(NP_H - r->y);
  return r->width && r->height;
}

void eadk_display_push_rect(eadk_rect_t rect, const eadk_color_t *pixels) {
  eadk_rect_t r = rect;
  if (!clip(&r)) return;
  for (int y = 0; y < r.height; y++) /* pixels is laid out with the caller's (unclipped) width */
    memcpy(&shadow[(r.y + y) * NP_W + r.x], pixels + (size_t)y * rect.width, (size_t)r.width * 2);
  port_present(shadow, r.x, r.y, r.width, r.height);
}

void eadk_display_push_rect_uniform(eadk_rect_t rect, eadk_color_t color) {
  if (!clip(&rect)) return;
  for (int y = 0; y < rect.height; y++) {
    uint16_t *row = &shadow[(rect.y + y) * NP_W + rect.x];
    for (int x = 0; x < rect.width; x++) row[x] = color;
  }
  port_present(shadow, rect.x, rect.y, rect.width, rect.height);
}

void eadk_display_pull_rect(eadk_rect_t rect, eadk_color_t *pixels) {
  eadk_rect_t r = rect;
  if (!clip(&r)) return;
  for (int y = 0; y < r.height; y++)
    memcpy(pixels + (size_t)y * rect.width, &shadow[(r.y + y) * NP_W + r.x], (size_t)r.width * 2);
}

bool eadk_display_wait_for_vblank(void) {
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

void _eadk_keyboard_scan_do_scan(void) { last_scan = port_keys(); }
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
    port_sleep_ms(8);
    left -= 8;
  }
}

/* ------------------------------------------------------------------ timing */
uint32_t _eadk_timing_millis_low(void) { return port_millis(); }
uint32_t _eadk_timing_millis_high(void) { return 0; }
void eadk_timing_msleep(uint32_t ms) { port_sleep_ms(ms); }
void eadk_timing_usleep(uint32_t us) { port_sleep_ms((us + 999) / 1000); }

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
