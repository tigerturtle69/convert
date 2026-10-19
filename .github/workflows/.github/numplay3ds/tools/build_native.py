#!/usr/bin/env python3
"""Builds the NumPlay launcher and its games as ONE native program (no calculator, no emulation).

This repeats what NumPlay's own `make sim` does for the Epsilon simulator: every game is compiled with its
`main` renamed to np_<id>_main, partially linked, and all its other global symbols hidden, then linked with the
launcher (compiled with -DNP_SIMULATOR=1). The only platform-specific part is the EADK implementation
(port/eadk_port.c + one backend), so the same objects run on a PC (headless, for tests) and on the 3DS.

  build_native.py --numplay NumPlay-main --target host --out build/host [--games "snake g2048 ..."]
  build_native.py --numplay NumPlay-main --target 3ds  --out build/3ds  (needs devkitARM: CC etc. from env)

It leaves build/<target>/numplay_objs.txt (everything to link) and, for host, the executable `numplay`.
"""
import argparse
import json
import os
import re
import shlex
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
PORT = os.path.join(os.path.dirname(HERE), "port")

# Per game: sources (relative to games/<id>), C standard, extra flags. Mirrors each game's `sim-module` recipe.
NUMDASH_CORE = "game trig level inflate leveldata objdefs assets assets_lv gfx scene fx save".split()
GAMES = {
    "numdash": dict(src=["%s.c" % f for f in NUMDASH_CORE + "ui app menu play editor platform_eadk main".split()],
                    std="c11", flags=["-Isrc"]),
    "crossyroad": dict(src=["crossy.c"], std="c11", flags=["-fno-math-errno", "-DPLATFORM_SIM=1"]),
    "numdrive": dict(src=["%s.c" % f for f in "main world vm physics render inflate gfx ui save font platform_eadk".split()],
                     std="gnu11", flags=["-fsingle-precision-constant", "-Wno-unused-function", "-DNO_STORAGE",
                                         "-Dpack_data=pack_bin"], pack="src/pack.bin"),
    "balatro": dict(src="*", std="c99", flags=[], skip=["libc.c"]),
    "buckshot": dict(src=["%s.c" % f for f in "main game scene anim story gfx pixcode assets font".split()],
                     std="c99", flags=[]),
    "portal": dict(src=["main.c", "game.c", "gfx.c", "inflate.c"], std="c99", flags=[]),
    "chess": dict(src=["main.c", "chess.c"], std="c99", flags=[]),
    "blockblast": dict(src=["main.c"], std="c11", flags=["-DNP_MODULE=1"]),
}
for g in "flappybird pacman snake connectfour solitaire g2048 minesweeper breakout numvisuals".split():
    GAMES[g] = dict(src=["main.c"], std="c11", flags=[])
# (numblocks is left out: it needs too many keys for a 3DS; tetris is Rust: it needs Rust's 3DS target, so it is left out for now)


def sh(cmd, **kw):
    r = subprocess.run(cmd, shell=isinstance(cmd, str), capture_output=True, text=True, **kw)
    if r.returncode:
        sys.exit("FAILED: %s\n%s%s" % (cmd if isinstance(cmd, str) else " ".join(cmd), r.stdout, r.stderr))
    return r.stdout


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--numplay", required=True)
    ap.add_argument("--target", choices=["host", "3ds"], required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--games", help="space separated ids (default: all supported)")
    ap.add_argument("--eadk-inc", help="dir with the official eadk.h (default: from nwlink via npx)")
    ap.add_argument("--opt", default="-O2")
    a = ap.parse_args()

    np_dir = os.path.abspath(a.numplay)
    sys.path.insert(0, HERE)
    import np_patches
    print("patches:", np_patches.apply(np_dir) or "already applied")
    out = os.path.abspath(a.out)
    os.makedirs(out, exist_ok=True)
    ids = a.games.split() if a.games else list(GAMES)
    # keep NumPlay's order (the launcher's carousel order)
    order = [g["id"] for g in json.load(open(os.path.join(np_dir, "games/games.json")))]
    ids = [g for g in order if g in ids]

    if a.target == "host":
        cc, ld, objcopy = os.environ.get("CC", "gcc"), "ld", "objcopy"
        arch = []
    else:
        pre = os.environ.get("CROSS", "arm-none-eabi-")
        cc, ld, objcopy = pre + "gcc", pre + "ld", pre + "objcopy"
        arch = ["-march=armv6k", "-mtune=mpcore", "-mfpu=vfp", "-mfloat-abi=hard", "-mtp=soft", "-mword-relocations",
                "-ffunction-sections", "-fdata-sections", "-D__3DS__"]

    eadk = a.eadk_inc
    if not eadk:
        flags = shlex.split(sh("npx --yes -- nwlink@1.0.0 eadk-cflags-simulator"))
        eadk = [f[2:] for f in flags if f.startswith("-I")][0]
    base = [cc] + arch + shlex.split(a.opt) + ["-w", "-I" + eadk, "-I" + PORT, "-include", "np_native.h"] + shlex.split(os.environ.get("NP_CPPFLAGS", ""))

    jobs = []  # (cmd, obj)

    def comp(cmd, obj):
        jobs.append((cmd, obj))
        return obj

    game_objs = {}
    for gid in ids:
        spec = GAMES[gid]
        gdir = os.path.join(np_dir, "games", gid, "src")
        odir = os.path.join(out, "games", gid)
        os.makedirs(odir, exist_ok=True)
        if spec["src"] == "*":
            files = sorted(f for f in os.listdir(gdir) if f.endswith(".c") and f not in spec.get("skip", []))
        else:
            files = spec["src"]
        # (numdash/numblocks list their files with a src/ prefix in the Makefile: ours are bare names in src/)
        files = [os.path.basename(f) for f in files]
        extra = []
        if spec.get("pack"):  # numdrive embeds pack.bin; the Makefile uses xxd -i
            data = open(os.path.join(np_dir, "games", gid, spec["pack"]), "rb").read()
            pc = os.path.join(odir, "pack_bin.c")
            with open(pc, "w") as f:
                f.write("unsigned char pack_bin[] = {%s};\nunsigned int pack_bin_len = %d;\n"
                        % (",".join(str(b) for b in data), len(data)))
            extra.append(comp(base + ["-c", pc, "-o", os.path.join(odir, "pack_bin.o")], os.path.join(odir, "pack_bin.o")))
        objs = []
        for f in files:
            o = os.path.join(odir, f[:-2] + ".o")
            cmd = base + ["-std=" + spec["std"], "-Dmain=np_%s_main" % gid] + spec["flags"] + \
                  ["-I" + gdir, "-c", os.path.join(gdir, f), "-o", o]
            objs.append(comp(cmd, o))
        game_objs[gid] = objs + extra

    # the launcher
    lsrc = os.path.join(np_dir, "launcher/src")
    lobjs = []
    lflags = ["-std=gnu11", "-DNP_SIMULATOR=1", "-DNP_VERSION=\"%s\"" % open(os.path.join(np_dir, "VERSION")).read().strip(),
              "-I" + lsrc]
    os.makedirs(os.path.join(out, "launcher"), exist_ok=True)
    for f in sorted(os.listdir(lsrc)):
        if f.endswith(".c"):
            o = os.path.join(out, "launcher", f[:-2] + ".o")
            lobjs.append(comp(base + lflags + ["-c", os.path.join(lsrc, f), "-o", o], o))

    # generated glue: game table and the screenshots' data
    gen = os.path.join(out, "gen")
    os.makedirs(gen, exist_ok=True)
    mk = open(os.path.join(np_dir, "build/games.mk")).read() if os.path.exists(os.path.join(np_dir, "build/games.mk")) \
        else sh([sys.executable, os.path.join(np_dir, "tools/games_mk.py"), os.path.join(np_dir, "games/games.json")])
    sh([sys.executable, os.path.join(np_dir, "tools/gen_games.py"), os.path.join(np_dir, "games/games.json"),
        os.path.join(out, "nomodules"), gen, "--simulator", "--only", " ".join(ids)])
    # room in the launcher's arena for every decoded screenshot (see np_patches.py): +40 slots of 192x144
    gt = os.path.join(gen, "gametable.c")
    src = open(gt).read()
    m = re.search(r"uint8_t np_arena\[(\d+)\]", src)
    if m and "np-port-arena" not in src:
        n = int(m.group(1)) + 40 * 192 * 144 + 4096
        src = src.replace("np_arena[%s]" % m.group(1), "np_arena[%d]" % n)
        src = re.sub(r"(const uint32_t np_arena_size = )\d+;", r"\g<1>%d; /* np-port-arena */" % n, src)
        open(gt, "w").write(src)
    gobjs = [comp(base + lflags + ["-c", os.path.join(gen, "gametable.c"), "-o", os.path.join(gen, "gametable.o")],
                  os.path.join(gen, "gametable.o"))]
    for gid in ids:
        idx = re.search(r"^INDEX_%s := (\d+)" % gid, mk, re.M).group(1)
        col = re.search(r"^COLORS_%s := (.*)$" % gid, mk, re.M)
        rel = re.search(r"^SHOTS_REL_%s := (.*)$" % gid, mk, re.M).group(1).split()
        c = os.path.join(gen, "shots_%s.c" % gid)
        sh([sys.executable, os.path.join(np_dir, "tools/shots.py"), "--id", gid, "--index", idx] +
           (col.group(1).split() if col else []) + [c] + rel, cwd=os.path.join(np_dir, "games", gid))
        gobjs.append(comp(base + lflags + ["-c", c, "-o", c[:-2] + ".o"], c[:-2] + ".o"))

    # the EADK implementation and the backend
    pobjs = []
    backend = "port_host.c" if a.target == "host" else "port_3ds.c"
    for f in ["eadk_port.c", backend]:
        o = os.path.join(out, "port_" + f[:-2] + ".o")
        pobjs.append(comp(base + ["-std=gnu11", "-Wall", "-c", os.path.join(PORT, f), "-o", o], o))

    def run(j):
        cmd, obj = j
        r = subprocess.run(cmd, capture_output=True, text=True)
        return obj, r

    with ThreadPoolExecutor(max_workers=os.cpu_count() or 4) as ex:
        failed = False
        for obj, r in ex.map(run, jobs):
            if r.returncode:
                failed = True
                print("FAILED", os.path.relpath(obj, out), "\n", r.stderr[:1500])
    if failed:
        sys.exit(1)

    # one object per game, only np_<id>_main visible
    final = []
    for gid in ids:
        raw, hid = os.path.join(out, "games", gid + ".r.o"), os.path.join(out, "games", gid + ".o")
        sh([ld, "-r"] + game_objs[gid] + ["-o", raw])
        sh([objcopy, "--keep-global-symbol=np_%s_main" % gid, raw, hid])
        final.append(hid)
    # a game that has only eadk_* undefined: keep-global-symbol leaves undefined symbols alone, as the sim does

    objs = lobjs + gobjs + pobjs + final
    open(os.path.join(out, "numplay_objs.txt"), "w").write("\n".join(objs))
    print("compiled %d objects, %d games: %s" % (len(jobs), len(ids), " ".join(ids)))
    if a.target == "host":
        sh([cc, "-o", os.path.join(out, "numplay")] + objs + ["-lm"])
        print("linked", os.path.join(out, "numplay"))


if __name__ == "__main__":
    main()
