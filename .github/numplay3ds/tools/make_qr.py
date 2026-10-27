#!/usr/bin/env python3
"""QR code for installing a .cia over the network.

FBI (the usual 3DS CIA installer) has  Remote Install > Scan QR Code : it reads a QR that holds a direct URL to
a .cia and downloads + installs it. So: upload the .cia somewhere that serves it as a plain file download (a GitHub
release asset works), then

    python3 tools/make_qr.py https://example.com/numplay.cia numplay-qr.png

The QR is just the URL, so it can only be made once the file is hosted. Needs: pip install "qrcode[pil]"
"""
import sys
import qrcode
if len(sys.argv) != 3 or not sys.argv[1].startswith(("http://", "https://")):
    sys.exit(__doc__)
q = qrcode.QRCode(error_correction=qrcode.constants.ERROR_CORRECT_M, box_size=10, border=4)
q.add_data(sys.argv[1]); q.make(fit=True)
q.make_image(fill_color="black", back_color="white").save(sys.argv[2])
print("wrote", sys.argv[2], "for", sys.argv[1])
