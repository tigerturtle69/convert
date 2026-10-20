#!/usr/bin/env python3
"""Builds one of NumPlay's big standalone games natively (the ones that are an app of their own on a calculator).

  build_big.py --game celeste --numplay NumPlay-main --target host --out build/celeste   (links ./build/celeste/<game>)
  build_big.py --game celeste --numplay NumPlay-main --target 3ds  --out build/celeste   (objects + numplay_objs.txt)

Each game is its own program with its own main(), an EADK platform file (plat_eadk.c) and its assets embedded from
src/data.bin by src/data.S. Their sources are every top-level src/*.c except plat_host.c (the PC test platform),
plus src/lzma/LzmaDec.c: the same lists their own Makefiles build. The EADK itself is port/eadk_port.c.
"""
import argparse, os, shlex, subprocess, sys
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
PORT = os.path.join(os.path.dirname(HERE), "port")
sys.path.insert(0, HERE)
import np_patches

# id: (3DS save folder, bytes of the spare-RAM pool the game gets, its 3DS key layout, extra defines)
GAMES = {
    "hollowknight":   dict(save="HollowKnight", heap=262144, keymap="ACTION"),
    "celeste":        dict(save="Celeste", heap=262144, keymap="CELESTE", defs=["-DNP_GAME_ON_TOP", "-DNP_TOP_SCALE"]),
    "championisland": dict(save="ChampionIsland", heap=262144, keymap="CHAMPION"),
    "finalearth2":    dict(save="FinalEarth2", heap=262144, keymap="BUILDER"),
    "numpowder":      dict(save="NumPowder", heap=524288, keymap="BUILDER"),
}


def sh(cmd, **kw):
    r = subprocess.run(cmd, capture_output=True, text=True, **kw)
    if r.returncode:
        sys.exit("FAILED: %s\n%s%s" % (" ".join(cmd), r.stdout, r.stderr))
    return r.stdout


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--game", required=True, choices=sorted(GAMES))
    ap.add_argument("--numplay", required=True)
    ap.add_argument("--target", choices=["host", "3ds"], required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--opt", default="-O2")
    a = ap.parse_args()
    cfg = GAMES[a.game]
    np_dir = os.path.abspath(a.numplay)
    print("patches:", np_patches.apply(np_dir) or "already applied")
    src = os.path.join(np_dir, "games", a.game, "src")
    out = os.path.abspath(a.out)
    os.makedirs(out, exist_ok=True)

    if a.target == "host":
        cc, arch = os.environ.get("CC", "gcc"), []
    else:
        cc = os.environ.get("CROSS", "arm-none-eabi-") + "gcc"
        arch = ["-march=armv6k", "-mtune=mpcore", "-mfpu=vfp", "-mfloat-abi=hard", "-mtp=soft", "-mword-relocations",
                "-ffunction-sections", "-fdata-sections", "-D__3DS__", "-DNP_KEYMAP_" + cfg["keymap"],
                '-DNP_SAVE_DIR="%s"' % cfg["save"]] + cfg.get("defs", [])
    flags = shlex.split(sh(["npx", "--yes", "--", "nwlink@1.0.0", "eadk-cflags-simulator"]))
    eadk = [f[2:] for f in flags if f.startswith("-I")][0]
    base = [cc] + arch + shlex.split(a.opt) + ["-w", "-I" + eadk, "-I" + PORT, "-I" + src, "-include", "np_native.h",
            "-std=gnu11", "-fno-math-errno", "-fsingle-precision-constant", "-DNP_HEAP_BYTES=%d" % cfg["heap"]] + \
           shlex.split(os.environ.get("NP_CPPFLAGS", ""))

    files = sorted(f for f in os.listdir(src) if f.endswith(".c") and f != "plat_host.c")
    if os.path.exists(os.path.join(src, "lzma/LzmaDec.c")):
        files.append("lzma/LzmaDec.c")
    jobs, objs = [], []

    def comp(srcfile, name):
        o = os.path.join(out, name + ".o")
        jobs.append(base + ["-c", srcfile, "-o", o])
        objs.append(o)

    for f in files:
        comp(os.path.join(src, f), "g_" + f.replace("/", "_")[:-2])
    for f in ("heap", "eadk_port", "port_3ds" if a.target == "3ds" else "port_host"):
        comp(os.path.join(PORT, f + ".c"), "port_" + f)
    # assets: data.bin assembled into the program (no force-included header here)
    if os.path.exists(os.path.join(src, "data.S")):
        data_o = os.path.join(out, "g_data.o")
        sh([cc] + arch + ["-c", '-DDATA_BIN="%s"' % os.path.join(src, "data.bin"), os.path.join(src, "data.S"), "-o", data_o])
        objs.append(data_o)

    bad = False
    with ThreadPoolExecutor(os.cpu_count() or 4) as ex:
        for cmd, r in ex.map(lambda c: (c, subprocess.run(c, capture_output=True, text=True)), jobs):
            if r.returncode:
                bad = True
                print("FAILED", cmd[-3], "\n", r.stderr[:1500])
    if bad:
        sys.exit(1)
    open(os.path.join(out, "numplay_objs.txt"), "w").write("\n".join(objs))
    print("compiled %d objects (%d game files)" % (len(objs), len(files)))
    if a.target == "host":
        sh([cc, "-o", os.path.join(out, a.game)] + objs + ["-lm"])
        print("linked", os.path.join(out, a.game))


if __name__ == "__main__":
    main()
