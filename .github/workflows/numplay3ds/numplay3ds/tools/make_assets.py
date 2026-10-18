#!/usr/bin/env python3
"""Makes 3ds/icon.png (48x48), 3ds/banner.png (256x128) and 3ds/audio.wav (short silence) from NumPlay's icon."""
import sys, wave
from PIL import Image, ImageDraw, ImageFont
src, out = sys.argv[1], sys.argv[2]
ic = Image.open(src).convert("RGBA")
ic.resize((48, 48), Image.LANCZOS).save(out + "/icon.png")
b = Image.new("RGBA", (256, 128), (24, 28, 36, 255))
b.alpha_composite(ic.resize((96, 96), Image.LANCZOS), (16, 16))
d = ImageDraw.Draw(b)
d.text((124, 40), "NumPlay", fill=(255, 255, 255, 255), font=ImageFont.load_default(26))
d.text((124, 76), "for 3DS", fill=(190, 200, 215, 255), font=ImageFont.load_default(16))
b.save(out + "/banner.png")
w = wave.open(out + "/audio.wav", "wb")
w.setnchannels(2); w.setsampwidth(2); w.setframerate(32728)
w.writeframes(b"\0" * (4 * 32728 // 2))  # half a second of silence
w.close()
