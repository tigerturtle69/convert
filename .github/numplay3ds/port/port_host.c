/* Headless PC backend: runs the real games with scripted keys on a virtual clock and saves screenshots.
 *
 *   NP_SCRIPT="500-600:right,900-950:ok"   keys held over virtual time ranges (ms)
 *   NP_SHOTS="800,2000"                    save a screenshot at these times
 *   NP_END_MS=3000                         exit when the virtual clock gets there
 *   NP_OUT=dir                             where shot_<ms>.ppm files go (default ".")
 *
 * The clock only moves when the game sleeps or waits for vblank (16 ms), so runs are fast and
 * repeatable. */
#include <execinfo.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "port.h"

typedef struct {
  uint32_t a, b;
  int key;
} press_t;

typedef struct {
  uint32_t a, b;
  int x1, y1, x2, y2;
} tap_t;
static tap_t taps[128];
static int ntaps;
static const char *savepath;
static unsigned long n_vblank, n_present, n_pixels;

static press_t presses[512];
static int npresses;
static uint32_t shots[64];
static int nshots, next_shot;
static uint32_t vt, end_ms;
static const char *outdir = ".";
static const uint16_t *last_fb;
static uint32_t spin;

static const struct {
  const char *name;
  int key;
} names[] = {{"left", 0},  {"up", 1},     {"down", 2},       {"right", 3}, {"ok", 4},    {"back", 5},   {"home", 6},
             {"shift", 12}, {"alpha", 13}, {"xnt", 14},       {"var", 15},  {"toolbox", 16}, {"backspace", 17},
             {"ln", 19},    {"exe", 52},   {"0", 48},         {"1", 42},    {"2", 43},    {"3", 44},     {"4", 36},
             {"5", 37},     {"6", 38},     {"7", 30},         {"8", 31},    {"9", 32},    {"plus", 45},  {"minus", 46}};

static void dump(uint32_t ms) {
  if (!last_fb) return;
  char path[512];
  snprintf(path, sizeof path, "%s/shot_%u.ppm", outdir, ms);
  FILE *f = fopen(path, "wb");
  if (!f) return;
  fprintf(f, "P6\n%d %d\n255\n", NP_W, NP_H);
  for (int i = 0; i < NP_W * NP_H; i++) {
    uint16_t c = last_fb[i];
    uint8_t px[3] = {(uint8_t)((c >> 11) * 255 / 31), (uint8_t)(((c >> 5) & 63) * 255 / 63), (uint8_t)((c & 31) * 255 / 31)};
    fwrite(px, 1, 3, f);
  }
  fclose(f);
}

static void advance(uint32_t ms) {
  static uint32_t traced;
  vt += ms;
  if (getenv("NP_TRACE") && vt / 1000 != traced) fprintf(stderr, "vt=%u s\n", traced = vt / 1000);
  spin = 0;
  while (next_shot < nshots && shots[next_shot] <= vt) dump(shots[next_shot++]);
  if (end_ms && vt >= end_ms) exit(0);
}

static unsigned long ncalls[4];
static void watchdog(int sig) {
  fprintf(stderr, "vt=%u keys=%lu millis=%lu sleep=%lu present=%lu\n", vt, ncalls[0], ncalls[1], ncalls[2], ncalls[3]);
  void *bt[40];
  int n = backtrace(bt, 40);
  for (int i = 0; i < n; i++) fprintf(stderr, "%p\n", bt[i]);
  _exit(3);
}

__attribute__((constructor)) static void host_init(void) {
  if (getenv("NP_WATCHDOG")) {
    signal(SIGALRM, watchdog);
    alarm((unsigned)atoi(getenv("NP_WATCHDOG")));
  }
  if (getenv("NP_OUT")) outdir = getenv("NP_OUT");
  if (getenv("NP_END_MS")) end_ms = (uint32_t)atoi(getenv("NP_END_MS"));
  const char *s = getenv("NP_SHOTS");
  while (s && *s && nshots < 64) {
    shots[nshots++] = (uint32_t)strtoul(s, (char **)&s, 10);
    if (*s == ',') s++;
  }
  savepath = getenv("NP_SAVE");
  s = getenv("NP_TOUCH"); /* "500-900:230,70;1000-1400:260,131>100,100" : pen down over a time range, optionally dragging */
  while (s && *s && ntaps < 128) {
    char *e;
    tap_t t;
    t.a = (uint32_t)strtoul(s, &e, 10);
    if (*e != '-') break;
    t.b = (uint32_t)strtoul(e + 1, &e, 10);
    if (*e != ':') break;
    t.x1 = (int)strtol(e + 1, &e, 10), t.y1 = (int)strtol(e + 1, &e, 10);
    t.x2 = t.x1, t.y2 = t.y1;
    if (*e == '>') t.x2 = (int)strtol(e + 1, &e, 10), t.y2 = (int)strtol(e + 1, &e, 10);
    taps[ntaps++] = t;
    s = e;
    if (*s == ';') s++;
  }
  s = getenv("NP_SCRIPT");
  while (s && *s && npresses < 512) {
    char *e;
    uint32_t a = (uint32_t)strtoul(s, &e, 10), b = a + 60;
    if (*e == '-') b = (uint32_t)strtoul(e + 1, &e, 10);
    if (*e != ':') break;
    const char *n = e + 1;
    size_t len = strcspn(n, ",");
    for (unsigned i = 0; i < sizeof names / sizeof names[0]; i++)
      if (strlen(names[i].name) == len && !strncmp(names[i].name, n, len)) presses[npresses++] = (press_t){a, b, names[i].key};
    s = n + len;
    if (*s == ',') s++;
  }
}

uint64_t port_keys(void) {
  uint64_t m = 0;
  ncalls[0]++;
  if (++spin > 64) advance(1); /* a loop polling keys must not freeze the clock either */
  for (int i = 0; i < npresses; i++)
    if (vt >= presses[i].a && vt < presses[i].b) m |= 1ull << presses[i].key;
  return m;
}

uint32_t port_millis(void) {
  ncalls[1]++;
  if (++spin > 64) advance(1); /* a game spinning on the clock must not hang the run */
  return vt;
}

void port_set_screen(bool top) { (void)top; }

void port_sleep_ms(uint32_t ms) {
  ncalls[2]++; advance(ms ? ms : 1); }
bool port_touch(int *x, int *y) {
  if (++spin > 20000) advance(1);
  for (int i = 0; i < ntaps; i++)
    if (vt >= taps[i].a && vt < taps[i].b) {
      int span = (int)(taps[i].b - taps[i].a), at = (int)(vt - taps[i].a);
      *x = taps[i].x1 + (taps[i].x2 - taps[i].x1) * at / span;
      *y = taps[i].y1 + (taps[i].y2 - taps[i].y1) * at / span;
      return true;
    }
  return false;
}

uint32_t port_storage_load(uint8_t *buf, uint32_t cap) {
  if (!savepath) return 0;
  FILE *f = fopen(savepath, "rb");
  if (!f) return 0;
  size_t n = fread(buf, 1, cap, f);
  fclose(f);
  return (uint32_t)n;
}
bool port_storage_store(const uint8_t *buf, uint32_t len) {
  if (!savepath) return true;
  FILE *f = fopen(savepath, "wb");
  if (!f) return false;
  size_t n = fwrite(buf, 1, len, f);
  return fclose(f) == 0 && n == len;
}

void port_present(const uint16_t *fb, int x, int y, int w, int h) {
  (void)x, (void)y;
  n_present++, n_pixels += (unsigned long)w * (unsigned long)h;
  ncalls[3]++;
  last_fb = fb;
}
void port_vblank(void) {
  n_vblank++;
  advance(16);
}

static void print_stats(void) {
  if (getenv("NP_STATS"))
    fprintf(stderr, "vblanks=%lu presents=%lu pixels_pushed=%lu (%.0f per vblank)\n", n_vblank, n_present, n_pixels,
            n_vblank ? (double)n_pixels / (double)n_vblank : 0.0);
}
__attribute__((constructor)) static void stats_init(void) { atexit(print_stats); }
