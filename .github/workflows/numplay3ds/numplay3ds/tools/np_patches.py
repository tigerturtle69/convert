#!/usr/bin/env python3
"""Small, exact patches applied to NumPlay's source before a native build.

Every replacement must match exactly once (or already be applied), otherwise the build stops with a clear message,
so a change upstream can never silently turn a feature off.

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

    # 3. Block Blast stylus
    attempt("blockblast-touch", os.path.join(np_dir, "games/blockblast/src/main.c"), BLOCKBLAST)
    return done


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
