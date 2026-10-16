#!/usr/bin/env python3
"""Builds Hollow Knight (a standalone NumPlay game) natively, with the same EADK layer as the launcher build.

  build_hk.py --numplay NumPlay-main --target host --out build/hk      (also links ./build/hk/hk, runs headless)
  build_hk.py --numplay NumPlay-main --target 3ds  --out build/hk3ds   (objects + numplay_objs.txt for the Makefile)
"""
import argparse, os, shlex, subprocess, sys
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
PORT = os.path.join(os.path.dirname(HERE), "port")
GAME = ("res tex room gfx phys anim hero camera game world obj enemy hud spell text npc title vm msg save death menu "
        "inv shop collect stag map").split()

def sh(cmd, **kw):
    r = subprocess.run(cmd, capture_output=True, text=True, **kw)
    if r.returncode:
        sys.exit("FAILED: %s\n%s%s" % (" ".join(cmd), r.stdout, r.stderr))
    return r.stdout

ap = argparse.ArgumentParser()
ap.add_argument("--numplay", required=True); ap.add_argument("--target", choices=["host", "3ds"], required=True)
ap.add_argument("--out", required=True); ap.add_argument("--opt", default="-O2")
a = ap.parse_args()
gd = os.path.join(os.path.abspath(a.numplay), "games/hollowknight"); src = os.path.join(gd, "src")
out = os.path.abspath(a.out); os.makedirs(out, exist_ok=True)
if a.target == "host":
    cc, arch = os.environ.get("CC", "gcc"), []
else:
    pre = os.environ.get("CROSS", "arm-none-eabi-"); cc = pre + "gcc"
    arch = ["-march=armv6k", "-mtune=mpcore", "-mfpu=vfp", "-mfloat-abi=hard", "-mtp=soft", "-mword-relocations",
            "-ffunction-sections", "-fdata-sections", "-D__3DS__", "-DNP_KEYMAP_ACTION"]
eadk = [f[2:] for f in shlex.split(sh(["npx", "--yes", "--", "nwlink@1.0.0", "eadk-cflags-simulator"])) if f.startswith("-I")][0]
base = [cc] + arch + shlex.split(a.opt) + ["-w", "-I" + eadk, "-I" + PORT, "-I" + src, "-include", "np_native.h",
        "-std=gnu11", "-fno-math-errno", "-fsingle-precision-constant"] + shlex.split(os.environ.get("NP_CPPFLAGS", ""))

jobs = []
def comp(srcfile, name, extra=()):
    o = os.path.join(out, name + ".o"); jobs.append((base + list(extra) + ["-c", srcfile, "-o", o])); return o
objs = [comp(os.path.join(src, g + ".c"), "hk_" + g) for g in GAME]
objs.append(comp(os.path.join(src, "lzma/LzmaDec.c"), "hk_lzma"))
objs += [comp(os.path.join(src, f + ".c"), "hk_" + f) for f in ("plat_eadk", "main")]
objs += [comp(os.path.join(PORT, f + ".c"), "port_" + f) for f in ("hk_heap", "eadk_port", "port_3ds" if a.target == "3ds" else "port_host")]
# the assets: data.bin assembled into the program (no force-included header here)
data_o = os.path.join(out, "hk_data.o")
sh([cc] + arch + ["-c", '-DDATA_BIN="%s"' % os.path.join(src, "data.bin"), os.path.join(src, "data.S"), "-o", data_o])
objs.append(data_o)

def run(cmd):
    r = subprocess.run(cmd, capture_output=True, text=True); return cmd, r
bad = False
with ThreadPoolExecutor(os.cpu_count() or 4) as ex:
    for cmd, r in ex.map(run, jobs):
        if r.returncode: bad = True; print("FAILED", cmd[-1], "\n", r.stderr[:1200])
if bad: sys.exit(1)
open(os.path.join(out, "numplay_objs.txt"), "w").write("\n".join(objs))
print("compiled %d objects" % len(objs))
if a.target == "host":
    sh([cc, "-o", os.path.join(out, "hk")] + objs + ["-lm"]); print("linked", os.path.join(out, "hk"))
