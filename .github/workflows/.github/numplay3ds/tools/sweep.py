#!/usr/bin/env python3
"""Launch each game through the launcher (scripted keys), screenshot it, report crashes."""
import json, os, subprocess, sys
from PIL import Image
exe, np_dir, outd = sys.argv[1], sys.argv[2], sys.argv[3]
ids = sys.argv[4].split()
os.makedirs(outd, exist_ok=True)
sheet = []
for i, gid in enumerate(ids):
    t = 1500
    keys = []
    for _ in range(i):
        keys.append(f"{t}-{t+150}:right"); t += 800
    keys.append(f"{t+500}-{t+650}:ok")
    launch = t + 500
    shots = [launch + 2500, launch + 6000]
    env = dict(os.environ, NP_OUT=outd + "/" + gid, NP_SCRIPT=",".join(keys),
               NP_SHOTS=",".join(map(str, shots)), NP_END_MS=str(shots[-1] + 100))
    os.makedirs(env["NP_OUT"], exist_ok=True)
    r = subprocess.run([exe], env=env, capture_output=True, text=True, timeout=180)
    status = "ok" if r.returncode == 0 else f"EXIT {r.returncode}"
    print(f"{gid:12s} {status}", flush=True)
    f = f"{env['NP_OUT']}/shot_{shots[-1]}.ppm"
    if os.path.exists(f):
        im = Image.open(f); im.save(f.replace(".ppm", ".png")); sheet.append((gid, im.copy()))
cols = 4
rows = (len(sheet) + cols - 1) // cols
W = Image.new("RGB", (cols * 330, rows * 250), (30, 30, 30))
for k, (gid, im) in enumerate(sheet):
    W.paste(im, ((k % cols) * 330, (k // cols) * 250))
W.save(outd + "/sheet.png")
