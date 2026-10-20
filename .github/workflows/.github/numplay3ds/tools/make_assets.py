#!/usr/bin/env python3
"""make_assets.py <icon.png> <out dir> [title] [subtitle]: writes icon.png (48x48), banner.png (256x128) and audio.wav
(short silence) into <out dir>, from a game's icon."""
import sys, wave
from PIL import Image, ImageDraw, ImageFont
src, out = sys.argv[1], sys.argv[2]
title = sys.argv[3] if len(sys.argv) > 3 else "NumPlay"
sub = sys.argv[4] if len(sys.argv) > 4 else "for 3DS"
import os
os.makedirs(out, exist_ok=True)
ic = Image.open(src).convert("RGBA")
ic.resize((48, 48), Image.LANCZOS).save(out + "/icon.png")
b = Image.new("RGBA", (256, 128), (24, 28, 36, 255))
b.alpha_composite(ic.resize((96, 96), Image.LANCZOS), (16, 16))
d = ImageDraw.Draw(b)
size = 26
while size > 12 and d.textlength(title, font=ImageFont.load_default(size)) > 124:
    size -= 2  # long titles shrink to fit the banner
d.text((124, 40), title, fill=(255, 255, 255, 255), font=ImageFont.load_default(size))
d.text((124, 76), sub, fill=(190, 200, 215, 255), font=ImageFont.load_default(16))
b.save(out + "/banner.png")
w = wave.open(out + "/audio.wav", "wb")
w.setnchannels(2); w.setsampwidth(2); w.setframerate(32728)
w.writeframes(b"\0" * (4 * 32728 // 2))  # half a second of silence
w.close()
