#!/usr/bin/env python3
"""Small, exact patches applied to NumPlay's source before a native build.

Every replacement must match exactly once (or already be applied), otherwise the build stops with a clear message,
so a change upstream can never silently turn a feature off.

  0. Final Earth 2 and NumPowder: the stylus (see FE2_PEN / NUMPOWDER_PEN below).
  1. games/common/epsilon_app.h : outside a calculator, epsilon_storage() returned nothing, so no game could save.
     It now asks the port layer (np_port_storage in port/eadk_port.c) for a buffer that lives on the SD card.
  2. games/blockblast/src/main.c : stylus support. Touch a shape to pick it, drag onto the board (the shape follows
     the pen), lift to place it. Taps also press the menu and help buttons.
"""
import os
import sys


class PatchError(Exception):
    pass


def edit(text, old, new, marker):
    """text with old replaced by new, exactly once; unchanged if marker (the patch's own text) is already there."""
    if marker in text:
        return text
    if old is None:  # append
        return text + new
    if text.count(old) != 1:
        raise PatchError("expected this text exactly once, found %d:\n%s" % (text.count(old), old))
    return text.replace(old, new)


def patch_file(path, edits):
    """All edits of one file apply, or none do. Returns True if the file changed."""
    text = original = open(path).read()
    for old, new, marker in edits:
        text = edit(text, old, new, marker)
    if text != original:
        open(path, "w").write(text)
        return True
    return False


def apply(np_dir):
    """Applies every patch it can. A patch whose anchor text has changed upstream is skipped with a warning
    (that feature is then missing, but the build still works). Returns the names of the patches applied."""
    done = []

    def attempt(name, path, edits):
        if not os.path.exists(path):
            return
        try:
            if patch_file(path, edits):
                done.append(name)
        except PatchError as e:
            print("np_patches: WARNING: skipped '%s' (NumPlay's %s changed upstream): %s"
                  % (name, os.path.relpath(path, np_dir), e), file=sys.stderr)

    # 1. saves
    attempt("saves", os.path.join(np_dir, "games/common/epsilon_app.h"), [(
        "__attribute__((unused)) static uint8_t *epsilon_storage(uint32_t *size) {\n  (void)size;\n  return NULL;\n}\n",
        "/* np-port: a buffer saved on the SD card (port/eadk_port.c) */\n"
        "extern uint8_t *np_port_storage(uint32_t *size);\n"
        "__attribute__((unused)) static uint8_t *epsilon_storage(uint32_t *size) { return np_port_storage(size); }\n",
        "np-port: a buffer saved")])

    # 2. launcher speed: the screenshot cache.
    # The launcher keeps its screenshots compressed and decodes them into a few slots, because a calculator has
    # almost no RAM. It draws in strips and every strip asks for every visible card again, so with fewer slots than
    # cards each request evicts the image the next one needs: ~36 decompressions per frame, 90% of the frame.
    # A 3DS has the RAM: enough slots to keep every game's screenshot decoded. (build_native.py makes the arena
    # big enough for them in the generated game table.)
    attempt("launcher-cache", os.path.join(np_dir, "launcher/src/ui.c"), [(
        "#define MAX_SLOTS 6\n", "#define MAX_SLOTS 40 /* np-port: keep every screenshot decoded (was 6) */\n",
        "np-port: keep every screenshot")])

    # 2a. the launcher tells the port when a game starts and ends, so the picture can move to the right screen
    attempt("launcher-screens", os.path.join(np_dir, "launcher/src/games.c"), [(
        "  heap_top = heap_of(g);\n  g->main();\n}\n",
        "  heap_top = heap_of(g);\n  extern void np_game_starting(const char *), np_game_returned(void); /* np-port */\n"
        "  np_game_starting(g->id);\n  g->main();\n  np_game_returned();\n}\n",
        "np_game_starting(g->id)")])

    # 2b. Doom: 16-bit pointers into the calculator's RAM are for the calculator. A native build (NP_NATIVE) uses
    # ordinary pointers and a bigger zone (a 3DS has the RAM). Everything else in Doom's HOST mode stays off.
    attempt("doom-native", os.path.join(np_dir, "games/doom/src/nw/doomconf.h"), [(
        "#if !defined(HOST)\n#define NW_SHORTPTR 1",
        "#if !defined(HOST) && !defined(NP_NATIVE) /* np-port */\n#define NW_SHORTPTR 1",
        "np-port")])
    attempt("doom-zone", os.path.join(np_dir, "games/doom/src/nw/nw.h"), [(
        "#define NW_HOST_ZONE (96 * 1024)", "#define NW_HOST_ZONE (512 * 1024) /* np-port: was 96 KB */", "np-port: was 96 KB")])

    # 3. Final Earth 2 and NumPowder: the stylus (the port turns swipes and taps into keys for menus and windows)
    hook = lambda fn: [("  uint32_t now = plat_keys();\n",
                        "  uint32_t now = plat_keys();\n  extern uint32_t " + fn + "(uint32_t); /* np-pen */\n  now = " + fn + "(now);\n",
                        "extern uint32_t " + fn)]
    attempt("finalearth2-pen-hook", os.path.join(np_dir, "games/finalearth2/src/input.c"), hook("fe_pen_keys"))
    attempt("finalearth2-pen", os.path.join(np_dir, "games/finalearth2/src/ui_city.c"), [(None, FE2_PEN, "np-pen: the stylus")])
    attempt("numpowder-pen-hook", os.path.join(np_dir, "games/numpowder/src/input.c"), hook("np_pen_keys"))
    attempt("numpowder-pen", os.path.join(np_dir, "games/numpowder/src/ui.c"), [(None, NUMPOWDER_PEN, "np-pen: the stylus")])

    # 3b. Balatro, Plants vs. Zombies and Cookie Clicker: the stylus
    attempt("balatro-pen-hook", os.path.join(np_dir, "games/balatro/src/main.c"), [(
        "  keys = eadk_keyboard_scan();\n",
        "  keys = eadk_keyboard_scan();\n  extern uint64_t bal_pen(uint64_t); /* np-pen */\n  keys = bal_pen(keys);\n",
        "extern uint64_t bal_pen"), (None, BAL_PEN, "np-pen: the stylus")])
    attempt("pvz-pen", os.path.join(np_dir, "games/pvz/src/screens.c"), [(
        "void game_frame(uint32_t keys) {\n",
        "void game_frame(uint32_t keys) {\n  extern uint32_t pvz_pen(uint32_t); /* np-pen */\n  keys = pvz_pen(keys);\n",
        "extern uint32_t pvz_pen"), (None, PVZ_PEN, "np-pen: the stylus")])
    attempt("cookieclicker-pen-hook", os.path.join(np_dir, "games/cookieclicker/src/ui.c"), [(
        "void ui_keys(uint32_t pressed, uint32_t held) {\n",
        "void ui_keys(uint32_t pressed, uint32_t held) {\n  extern void cc_pen(uint32_t *, uint32_t *); /* np-pen */\n  cc_pen(&pressed, &held);\n",
        "extern void cc_pen")])
    attempt("cookieclicker-pen", os.path.join(np_dir, "games/cookieclicker/src/ui_store.c"), [(None, COOKIE_PEN, "np-pen: the stylus")])

    # 4. Block Blast stylus
    attempt("blockblast-touch", os.path.join(np_dir, "games/blockblast/src/main.c"), BLOCKBLAST)
    return done


FE2_PEN = r'''
/* ---------------------------------------------------------------- np-pen: the stylus (3DS port)
 * Added by tools/np_patches.py. In the city a touch is a pointer: where nothing is being built, dragging moves the
 * view and a tap clicks; while a building (or the destroy mode) is picked, the cursor follows the pen with OK held,
 * so lifting places it, and dragging builds along (the drag modes). The toolbox button, the build menu's buttons and
 * the panel's buttons are tapped. In windows and on the title the port turns swipes into arrows and a tap into OK. */
#ifdef NP_PEN_DEBUG
#include <stdio.h>
#endif
extern bool np_touch(int *x, int *y) __attribute__((weak));
extern void np_gestures(bool on) __attribute__((weak));

static bool pen_in(int px, int py, int x, int y, int w, int h) { return px >= x && px < x + w && py >= y && py < y + h; }
static bool pen_placing(void) {
  return cu.build_kind != BK_NONE || building_mode == BM_DESTROY || building_mode == BM_DESTROY_LEAVING_HOLE || follow_mode;
}

uint32_t fe_pen_keys(uint32_t now) {
  enum { T_CITY, T_TOOLBOX, T_MENU, T_PANEL };
  static bool was, panning, click_pending;
  static int target, which, sx, sy, lx, ly, click_x, click_y, pulse_n;
  static uint32_t pulse_key;
  int px = 0, py = 0;
  bool pen = np_touch && np_touch(&px, &py);
  bool city_ctx = !ui.title && !ui.setup && nwins == 0;
  if (np_gestures) np_gestures(!city_ctx);
  uint32_t out = now;
  if (!city_ctx) {
    was = panning = click_pending = false, pulse_n = 0;
    return out;
  }
  bool down = pen && !was, up = !pen && was;
  was = pen;
  float z = (float)camera.zoom;
#ifdef NP_PEN_DEBUG
  if (pen || up) {
    fprintf(stderr, "pen %3d,%3d zone %d nwins %d target %d placing %d panning %d cam %.1f,%.1f zoom %d\n", px, py, (int)cu.zone,
            (int)nwins, target, (int)pen_placing(), (int)panning, camera.x, camera.y, camera.zoom);
  }
#endif
  if (down) {
    sx = lx = px, sy = ly = py, panning = false, target = T_CITY;
    bool menu_visible = cu.zone == Z_BUILD || cu.category >= 0 || cu.modes_open || cu.mgmt_open;
    if (menu_visible) {
      BMenu m;
      bmenu_layout(&m);
      for (int i = 0; i < m.n; i++)
        if (pen_in(px, py, m.b[i].x, m.b[i].y, 22, 22)) target = T_MENU, which = i;
    } else if (pen_in(px, py, 0, SCREEN_H - 22, 22, 22)) {
      target = T_TOOLBOX;
    }
    Panel pl;
    panel_layout(&pl);
    for (int k = 0; k < PR_MAT0 + M_COUNT; k++)
      if (pl.shown[k] && pen_in(px, py, pl.r[k].x, pl.r[k].y, pl.r[k].w, pl.r[k].h)) target = T_PANEL, which = k;
    if (target == T_CITY && cu.zone == Z_PANEL) cu.zone = Z_CITY;
  }
  if (pen) {
    if (target == T_CITY) {
      if (cu.zone != Z_BUILD && pen_placing()) {
        cursor_to((int)(camera.x + px / z), (int)(camera.y + py / z));
        out |= K_OK;
      } else if (panning || abs(px - sx) > 6 || abs(py - sy) > 6) {
        panning = true;
        camera.x -= (px - lx) / z;
        camera.y -= (py - ly) / z;
        camera_clamp_view();
        cursor_to_camera();
      }
    }
    lx = px, ly = py;
  }
  if (up) {
    bool still = abs(lx - sx) <= 8 && abs(ly - sy) <= 8;
    if (target == T_CITY) {
      if (cu.zone == Z_BUILD) {
        if (still) bmenu_close(); /* a tap beside the build menu closes it */
      } else if (!panning && !pen_placing() && still) {
        click_x = (int)(camera.x + sx / z), click_y = (int)(camera.y + sy / z);
        cursor_to(click_x, click_y);
        click_pending = true, pulse_key = K_OK, pulse_n = 2;
      }
    } else if (still) {
      if (target == T_TOOLBOX) {
        bmenu_open();
      } else if (target == T_MENU) {
        cu.zone = Z_BUILD, cu.bfocus = (int8_t)which;
        pulse_key = K_OK, pulse_n = 2;
      } else if (target == T_PANEL) {
        panel_press(which);
      }
    }
  }
  if (pulse_n > 0) {
    out |= pulse_key;
    if (click_pending) cursor_to(click_x, click_y);
    if (--pulse_n == 0) click_pending = false;
  }
  return out;
}
'''

NUMPOWDER_PEN = r'''
/* ---------------------------------------------------------------- np-pen: the stylus (3DS port)
 * Added by tools/np_patches.py. On the canvas the pen is the brush: the cursor follows it and OK is held while it is
 * down (hold B too to draw with the other tool, the eraser). A tap below the canvas opens the material menu
 * (left half) or the options menu (right half). In every menu, and on the intro card, the port turns swipes into
 * arrows and a tap into OK. */
extern bool np_touch(int *x, int *y) __attribute__((weak));
extern void np_gestures(bool on) __attribute__((weak));

uint32_t np_pen_keys(uint32_t now) {
  static bool was, from_canvas;
  static int pulse_n;
  static uint32_t pulse_key;
  int px = 0, py = 0;
  bool pen = np_touch && np_touch(&px, &py);
  bool draw_ctx = ui.mode == MODE_SIM && ui.intro <= 0;
  if (np_gestures) np_gestures(!draw_ctx);
  uint32_t out = now;
  if (!draw_ctx) {
    was = from_canvas = false, pulse_n = 0;
    return out;
  }
  bool down = pen && !was;
  was = pen;
  if (down) from_canvas = py < PANEL_Y;
  if (pen && from_canvas) {
    int cx = (px - SIM_X) / PX_SCALE, cy = py / PX_SCALE;
    ui.cx = cx < 0 ? 0 : cx >= XRES ? XRES - 1 : cx;
    ui.cy = cy < 0 ? 0 : cy >= YRES ? YRES - 1 : cy;
    if (!(now & K_BACK)) out |= K_OK;
  } else if (down) {
    pulse_key = px < 160 ? K_TOOLBOX : K_VAR, pulse_n = 2;
  }
  if (!pen) from_canvas = false;
  if (pulse_n > 0) {
    out |= pulse_key;
    pulse_n--;
  }
  return out;
}
'''

BAL_PEN = r'''
/* ---------------------------------------------------------------- np-pen: the stylus (3DS port)
 * Added by tools/np_patches.py. During a round, a touch on a hand card toggles it, on a joker or consumable opens its
 * menu (SELL / USE are then touched too; a touch elsewhere closes it), and the Play Hand, Discard and Rank / Suit
 * buttons are touched. In the other phases (blind select, shop, packs, cash out) the port turns swipes into arrows
 * and a tap into OK. */
extern bool np_touch(int *x, int *y) __attribute__((weak));
extern void np_gestures(bool on) __attribute__((weak));

static bool bal_in(int px, int py, int x, int y, int w, int h) { return px >= x && px < x + w && py >= y && py < y + h; }

uint64_t bal_pen(uint64_t keys) {
  static bool was;
  static int pulse_n;
  static uint64_t pulse_key;
  int px = 0, py = 0;
  bool pen = np_touch && np_touch(&px, &py);
  bool round = R.phase == PH_ROUND;
  if (np_gestures) np_gestures(!round);
  uint64_t out = keys;
  if (!round) {
    was = false, pulse_n = 0;
    return out;
  }
  bool down = pen && !was;
  was = pen;
  if (down && pulse_n == 0) {
    uint64_t key = 0;
    bool card_menu = menu_open >= 0 && (zone == Z_JOKERS || zone == Z_CONS);
    if (card_menu) { /* SELL / USE (as draw_menu places them), or a touch elsewhere: close */
      int kinds[4], n = menu_items(kinds);
      vitem_t *v = zone == Z_JOKERS ? &V.jokers[menu_open] : &V.cons[menu_open];
      const int bw = 32;
      int x = (int)v->x + CW - 3, y = ROW_Y + (n > 1 ? 3 : 10), hit = -1;
      if (x + bw > 318) x = (int)v->x - bw + 3;
      for (int i = 0; i < n; i++) {
        if (bal_in(px, py, x, y + i * 19, bw, kinds[i] == 1 ? 22 : 15)) hit = i;
        if (kinds[i] == 2) y -= 1;
      }
      if (hit >= 0) menu_sel = hit, key = 1ull << eadk_key_ok;
      else key = 1ull << eadk_key_back;
    } else {
      int found = -1;
      for (int i = 0; i < V.nhand; i++) /* later cards are on top */
        if (bal_in(px, py, ui_hand_x(i, V.nhand), HAND_Y - 12, CW, CH + 12)) found = i;
      if (found >= 0) {
        zone = Z_HAND, zi = found, key = 1ull << eadk_key_ok;
      } else {
        for (int i = 0; i < V.njokers && !key; i++)
          if (bal_in(px, py, (int)V.jokers[i].x, ROW_Y, CW, CH)) zone = Z_JOKERS, zi = i, key = 1ull << eadk_key_ok;
        for (int i = 0; i < V.ncons && !key; i++)
          if (bal_in(px, py, (int)V.cons[i].x, ROW_Y, CW, CH)) zone = Z_CONS, zi = i, key = 1ull << eadk_key_ok;
      }
      if (!key) { /* the buttons under the hand (as round_buttons places them) */
        const int px0 = HAND_X, pw = 52, sx = px0 + pw + 4, sw = 59, dx = sx + sw + 4, dw = 52;
        if (bal_in(px, py, px0, BTN_Y, pw, 22)) key = 1ull << eadk_key_exe;
        else if (bal_in(px, py, dx, BTN_Y, dw, 22)) key = 1ull << eadk_key_backspace;
        else if (bal_in(px, py, sx + 3, BTN_Y + 10, 25, 11)) key = R.sort_suit ? 1ull << eadk_key_shift : 0; /* Rank */
        else if (bal_in(px, py, sx + sw - 28, BTN_Y + 10, 25, 11)) key = R.sort_suit ? 0 : 1ull << eadk_key_shift; /* Suit */
      }
    }
    if (key) pulse_key = key, pulse_n = 2;
  }
  if (pulse_n > 0) {
    out |= pulse_key;
    pulse_n--;
  }
  return out;
}
'''

PVZ_PEN = r'''
/* ---------------------------------------------------------------- np-pen: the stylus (3DS port)
 * Added by tools/np_patches.py. On the lawn the cursor follows the pen (sun and coins under it are collected) and
 * lifting is OK: it plants, digs or whacks there. The seed packets and the shovel are touched. In every other screen
 * the port turns swipes into arrows and a tap into OK. */
extern bool np_touch(int *x, int *y) __attribute__((weak));
extern void np_gestures(bool on) __attribute__((weak));
extern float cam_x;

uint32_t pvz_pen(uint32_t keys) {
  static bool was, from_lawn;
  static int pulse_n;
  static uint32_t pulse_key;
  int px = 0, py = 0;
  bool pen = np_touch && np_touch(&px, &py);
  bool playing = g_state == ST_PLAY;
  if (np_gestures) np_gestures(!playing);
  uint32_t out = keys;
  if (!playing) {
    was = from_lawn = false, pulse_n = 0;
    return out;
  }
  bool down = pen && !was, up = !pen && was;
  was = pen;
  if (down) {
    from_lawn = py >= 40;
    if (!from_lawn) { /* the seed bank, the conveyor belt, the shovel */
      int shovel_x = has_conveyor() ? 208 : bank_width() + 2;
      if ((progress.items & ITEM_SHOVEL) && px >= shovel_x && px < shovel_x + 36) {
        pulse_key = K_SHOVEL, pulse_n = 2;
      } else if (has_conveyor()) {
        for (int i = 0; i < B.npackets && i < 10; i++) {
          int x = 6 + (int)(B.packets[i].x * 0.4f);
          if (px >= x && px < x + 21) pulse_key = K_D1 << i, pulse_n = 2;
        }
      } else {
        int i = (px - 34) / 21;
        if (px >= 34 && i < B.nseeds && i < 10) pulse_key = K_D1 << i, pulse_n = 2;
      }
    }
  }
  if (pen && from_lawn) {
    float wx = px / 0.4f + cam_x, wy = py / 0.4f;
    int col = (int)((wx - BOARD_LEFT) / CELL_W), best = 0, bd = 9999;
    col = col < 0 ? 0 : col > COLS - 1 ? COLS - 1 : col;
    float cx = cell_x(col);
    for (int r = 0; r < B.nrows; r++) {
      int d = py - screen_y(row_y(r, cx) - 30);
      d = d < 0 ? -d : d;
      if (d < bd) bd = d, best = r;
    }
    hud.col = col, hud.row = best;
    board_collect(wx, wy, 40);
  }
  if (up && from_lawn) pulse_key = K_OK, pulse_n = 2, from_lawn = false;
  if (pulse_n > 0) {
    out |= pulse_key;
    pulse_n--;
  }
  return out;
}
'''

COOKIE_PEN = r'''
/* ---------------------------------------------------------------- np-pen: the stylus (3DS port)
 * Added by tools/np_patches.py. Touch the big cookie to click it (hold: it keeps clicking), a golden cookie to pop
 * it, a store item to choose it and the same item again to buy it, the Buy / Sell / 1 / 10 / 100 words above the
 * store, and drag up or down in the store to move the choice. In menus, prompts and the ascension the port turns
 * swipes into arrows and a tap into OK. */
extern bool np_touch(int *x, int *y) __attribute__((weak));
extern void np_gestures(bool on) __attribute__((weak));

#ifndef TIP_TIME
#define TIP_TIME (FPS * 6) /* as in ui.c: how long the tooltip of a chosen item stays */
#endif
static bool cc_in(int px, int py, int x, int y, int w, int h) { return px >= x && px < x + w && py >= y && py < y + h; }

void cc_pen(uint32_t *pressed, uint32_t *held) {
  enum { T_NONE, T_COOKIE, T_STORE };
  static bool was, moved;
  static int target, sx, sy, ly, acc, clicks, last_sec = -1, last_idx = -1; /* the item the pen chose last */
  int px = 0, py = 0;
  bool pen = np_touch && np_touch(&px, &py);
  bool main_view = U.overlay == OV_NONE && !G.on_ascend && G.ascend_timer <= 0;
  if (np_gestures) np_gestures(!main_view);
  if (!main_view) {
    was = false, target = T_NONE;
    return;
  }
  bool down = pen && !was, up = !pen && was;
  was = pen;
  if (down) {
    sx = px, sy = py, ly = py, acc = 0, moved = false, clicks = 0, target = T_NONE;
    for (int i = 0; i < MAX_SHIMMERS; i++) { /* a golden cookie (or reindeer) */
      Shimmer *m = &G.shimmers[i];
      int dx = px - (int)m->x, dy = py - (int)m->y;
      if (m->used && dx * dx + dy * dy < 28 * 28) {
        shimmer_pop(m);
        return;
      }
    }
    int dx = px - COOKIE_X, dy = py - COOKIE_Y;
    if (px < LEFT_W && dx * dx + dy * dy < 62 * 62) target = T_COOKIE;
    else if (px >= STORE_X && py >= NEWS_H) target = T_STORE;
  }
  if (pen && target == T_COOKIE) { /* a click when it goes down, then about five a second while it is held */
    *held |= K_OK;
    if (clicks++ % 6 == 0) *pressed |= K_OK;
  }
  if (pen && target == T_STORE && U.view == VIEW_STORE) {
    int dy = py - ly;
    ly = py;
    acc += dy;
    if (abs(px - sx) > 8 || abs(py - sy) > 8) moved = true;
    if (acc <= -22) acc += 22, *pressed |= K_DOWN; /* dragging up moves the choice down the list */
    else if (acc >= 22) acc -= 22, *pressed |= K_UP;
  }
  if (up && target == T_STORE && !moved && U.view == VIEW_STORE) {
    if (sy < NEWS_H + 14) { /* the words above the store (as draw_store lays them out) */
      static const char *const opts[5] = {"Buy", "Sell", "1", "10", "100"};
      int x = SCREEN_W - 3;
      for (int i = 4; i >= 0; i--) {
        const char *t = (i == 4 && G.buy_mode == -1 && G.buy_bulk == -1) ? "all" : opts[i];
        int w = gfx_text_width(F_SMALL, t);
        x -= w + 4;
        if (cc_in(sx, sy, x - 2, NEWS_H + 2, w + 4, 11)) {
          if (i < 2) {
            if (G.buy_mode != (i == 0 ? 1 : -1)) *pressed |= K_DIV;
          } else if (i < 4) {
            G.buy_bulk = i == 2 ? 1 : 10, G.store_dirty = true;
          } else {
            G.buy_bulk = 100, G.store_dirty = true;
          }
        }
        if (i == 2) x -= 5;
      }
    } else if (sy >= STORE_TOP) {
      int base = STORE_TOP - (int)U.scroll;
      for (int i = 0; i < nitems; i++) {
        const Item *it = &items[i];
        if (it->h == 0 || !cc_in(sx, sy, it->x, base + it->y, it->w, it->h)) continue;
        if (!U.on_ticker && U.sec == it->sec && U.idx == it->idx && last_sec == it->sec && last_idx == it->idx)
          *pressed |= K_EXE; /* chosen by the pen already: buy */
        else U.sec = it->sec, U.idx = it->idx, U.on_ticker = false, U.tip_t = TIP_TIME, last_sec = it->sec, last_idx = it->idx;
        break;
      }
    }
  }
  if (!pen) target = T_NONE;
}
'''

BLOCKBLAST = [
    ("#define KEY(k) (1ull << (k))\n",
     "#define KEY(k) (1ull << (k))\n"
     "/* np-touch: stylus input, provided by the 3DS port (absent on a calculator) */\n"
     "extern bool np_touch(int *x, int *y) __attribute__((weak));\n"
     "static bool np_tpen, np_tboard, np_tignore;\n",
     "np-touch: stylus input"),
    ("    bool ok = hit & (KEY(eadk_key_ok) | KEY(eadk_key_exe));\n",
     "    bool ok = hit & (KEY(eadk_key_ok) | KEY(eadk_key_exe));\n"
     "    int np_tx = 0, np_ty = 0;\n"
     "    bool np_pen = np_touch && np_touch(&np_tx, &np_ty);\n"
     "    bool np_down = np_pen && !np_tpen, np_up = !np_pen && np_tpen;\n"
     "    bool np_skip = np_tignore; /* a press that dismissed a menu must not also place a block */\n"
     "    if (!np_pen) np_tignore = false;\n"
     "    else if (np_down && ov) np_tignore = true;\n"
     "    np_skip = np_skip || np_tignore;\n",
     "np_skip = np_skip || np_tignore"),
    ("      int n = ov == OV_MENU ? 4 : ov == OV_OVER ? 2 : 1;\n",
     "      int n = ov == OV_MENU ? 4 : ov == OV_OVER ? 2 : 1;\n"
     "      if (np_down) { /* a tap: on a button, or anywhere on the help */\n"
     "        int y0 = ov == OV_MENU ? 84 : 166;\n"
     "        if (ov == OV_HELP) ok = true;\n"
     "        else if (np_tx >= 90 && np_tx < 230)\n"
     "          for (int i = 0; i < n; i++)\n"
     "            if (np_ty >= y0 + 34 * i && np_ty < y0 + 34 * i + 26) sel = i, ok = true;\n"
     "      }\n",
     "a tap: on a button, or anywhere on the help"),
    ("      if (ok && fits(sv.tray[sv.cur], sv.cx, sv.cy)) place();\n",
     "      if (ok && fits(sv.tray[sv.cur], sv.cx, sv.cy)) place();\n"
     "      if (np_pen && !np_skip && sv.tray[sv.cur] >= 0) {\n"
     "        if (np_down) /* pen down on a shape in the tray: pick it */\n"
     "          for (int i = 0; i < 3; i++)\n"
     "            if (sv.tray[i] >= 0 && np_tx >= TX && np_tx < TX + 100 && np_ty >= 32 + 69 * i && np_ty < 32 + 69 * i + 67)\n"
     "              sv.cur = (uint8_t)i, clamp();\n"
     "        measure(sv.tray[sv.cur]);\n"
     "        np_tboard = np_tx >= BX - 8 && np_tx < BX + 8 * P + 8 && np_ty >= BY - 8 && np_ty < BY + 8 * P + 8;\n"
     "        if (np_tboard) { /* the shape follows the pen, centred under it */\n"
     "          int gx = np_tx < BX ? 0 : (np_tx - BX) / P, gy = np_ty < BY ? 0 : (np_ty - BY) / P;\n"
     "          int cx = gx - (w - 1) / 2, cy = gy - (h - 1) / 2;\n"
     "          if (cx < 0) cx = 0;\n"
     "          if (cy < 0) cy = 0;\n"
     "          if (cx > N - w) cx = N - w;\n"
     "          if (cy > N - h) cy = N - h;\n"
     "          sv.cx = (uint8_t)cx, sv.cy = (uint8_t)cy;\n"
     "        }\n"
     "      }\n"
     "      if (np_up && !np_skip && np_tboard && sv.tray[sv.cur] >= 0 && fits(sv.tray[sv.cur], sv.cx, sv.cy)) place();\n"
     "      if (np_up) np_tboard = false; /* lifted: placed, or cancelled by lifting off the board */\n",
     "pen down on a shape in the tray: pick it"),
    ("    /* what the frame shows: where the block in hand would go, and what it would clear */\n",
     "    np_tpen = np_pen;\n"
     "    /* what the frame shows: where the block in hand would go, and what it would clear */\n",
     "    np_tpen = np_pen;\n"),
]


if __name__ == "__main__":
    print("applied:", apply(sys.argv[1]) or "nothing (already patched)")
