#!/usr/bin/env python3
"""make_rsf.py <id> <Title(8)> <ProductCode suffix(4)> <UniqueId hex>  ->  3ds/<id>/app.rsf, from 3ds/app.rsf.
Every game needs its own title ID so they install side by side."""
import os, re, sys
gid, title, pc, uid = sys.argv[1:5]
root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "3ds")
s = open(os.path.join(root, "app.rsf")).read()
s = re.sub(r"(Title\s*:\s*)\S+", r"\g<1>" + title, s, count=1)
s = re.sub(r"CTR-P-\w+", "CTR-P-" + pc, s, count=1)
s = re.sub(r"(UniqueId\s*:\s*)0x[0-9A-Fa-f]+", r"\g<1>" + uid, s, count=1)
os.makedirs(os.path.join(root, gid), exist_ok=True)
open(os.path.join(root, gid, "app.rsf"), "w").write(s)
print(gid, title, "CTR-P-" + pc, uid)
