# NumPlay for 3DS: builds numplay.3dsx (Homebrew Launcher) and numplay.cia (installable).
#
# Needs: devkitPro with devkitARM + libctru (pacman -S 3ds-dev), plus python3, Pillow and node (npx, for the
# official eadk.h), and for the CIA: makerom and bannertool on your PATH.
#
#   make NUMPLAY=../NumPlay-main          3dsx + cia
#   make 3dsx                             just the .3dsx (quickest to iterate on)
#   make host                             headless PC build + tests (no 3DS tools needed)

ifeq ($(strip $(DEVKITPRO)),)
DEVKITPRO := /opt/devkitpro
endif
DEVKITARM ?= $(DEVKITPRO)/devkitARM
export PATH := $(DEVKITARM)/bin:$(DEVKITPRO)/tools/bin:$(PATH)

NUMPLAY ?= ../NumPlay-main
OUT     := build/3ds
NAME    := numplay
TITLE   := NumPlay
DESC    := NumWorks games on 3DS
AUTHOR  := NumPlay (port)

ARCH    := -march=armv6k -mtune=mpcore -mfpu=vfp -mfloat-abi=hard -mtp=soft
LIBCTRU := $(DEVKITPRO)/libctru

.PHONY: all 3dsx cia host clean assets hk hk-3dsx hk-cia qr
all: 3dsx cia
3dsx: $(OUT)/$(NAME).3dsx
cia: $(OUT)/$(NAME).cia

# every game + the launcher + the EADK layer, as objects (see tools/build_native.py)
$(OUT)/numplay_objs.txt: $(wildcard port/*.c port/*.h tools/build_native.py)
	NP_CPPFLAGS="-I$(LIBCTRU)/include" CROSS=arm-none-eabi- \
	  python3 tools/build_native.py --numplay $(NUMPLAY) --target 3ds --out $(OUT) --opt "-O2 -g"

$(OUT)/$(NAME).elf: $(OUT)/numplay_objs.txt
	arm-none-eabi-gcc $(ARCH) -specs=3dsx.specs -Wl,--gc-sections -g \
	  -o $@ @$(OUT)/numplay_objs.txt -L$(LIBCTRU)/lib -lctru -lm

$(OUT)/$(NAME).smdh: 3ds/icon.png
	smdhtool --create "$(TITLE)" "$(DESC)" "$(AUTHOR)" 3ds/icon.png $@

$(OUT)/$(NAME).3dsx: $(OUT)/$(NAME).elf $(OUT)/$(NAME).smdh
	3dsxtool $< $@ --smdh=$(OUT)/$(NAME).smdh

$(OUT)/banner.bnr: 3ds/banner.png 3ds/audio.wav
	bannertool makebanner -i 3ds/banner.png -a 3ds/audio.wav -o $@
$(OUT)/icon.icn: 3ds/icon.png
	bannertool makesmdh -i 3ds/icon.png -s "$(TITLE)" -l "$(DESC)" -p "$(AUTHOR)" -o $@

$(OUT)/$(NAME).cia: $(OUT)/$(NAME).elf $(OUT)/banner.bnr $(OUT)/icon.icn 3ds/app.rsf
	makerom -f cia -o $@ -elf $(OUT)/$(NAME).elf -rsf 3ds/app.rsf \
	  -icon $(OUT)/icon.icn -banner $(OUT)/banner.bnr -exefslogo -target t

assets:
	python3 tools/make_assets.py $(NUMPLAY)/launcher/assets/icon.png 3ds

host:
	python3 tools/build_native.py --numplay $(NUMPLAY) --target host --out build/host
	@echo "run:  NP_SCRIPT=... NP_SHOTS=... ./build/host/numplay   (see port/port_host.c)"

clean:
	rm -rf build

# ---- Hollow Knight: its own app (it needs the whole app space on a calculator; here it is a separate .3dsx/.cia)
HK := build/hk3ds
hk: hk-3dsx hk-cia
hk-3dsx: $(HK)/hollowknight.3dsx
hk-cia: $(HK)/hollowknight.cia

$(HK)/numplay_objs.txt: $(wildcard port/*.c port/*.h tools/build_hk.py)
	NP_CPPFLAGS="-I$(LIBCTRU)/include" CROSS=arm-none-eabi- \
	  python3 tools/build_hk.py --numplay $(NUMPLAY) --target 3ds --out $(HK) --opt=-O2
$(HK)/hollowknight.elf: $(HK)/numplay_objs.txt
	arm-none-eabi-gcc $(ARCH) -specs=3dsx.specs -Wl,--gc-sections -g \
	  -o $@ @$(HK)/numplay_objs.txt -L$(LIBCTRU)/lib -lctru -lm
$(HK)/hollowknight.smdh: 3ds/hk/icon.png
	smdhtool --create "Hollow Knight" "Hollow Knight for NumWorks, on 3DS" "$(AUTHOR)" 3ds/hk/icon.png $@
$(HK)/hollowknight.3dsx: $(HK)/hollowknight.elf $(HK)/hollowknight.smdh
	3dsxtool $< $@ --smdh=$(HK)/hollowknight.smdh
$(HK)/banner.bnr: 3ds/hk/banner.png 3ds/hk/audio.wav
	bannertool makebanner -i 3ds/hk/banner.png -a 3ds/hk/audio.wav -o $@
$(HK)/icon.icn: 3ds/hk/icon.png
	bannertool makesmdh -i 3ds/hk/icon.png -s "Hollow Knight" -l "Hollow Knight for NumWorks, on 3DS" -p "$(AUTHOR)" -o $@
$(HK)/hollowknight.cia: $(HK)/hollowknight.elf $(HK)/banner.bnr $(HK)/icon.icn 3ds/hk/app.rsf
	makerom -f cia -o $@ -elf $(HK)/hollowknight.elf -rsf 3ds/hk/app.rsf \
	  -icon $(HK)/icon.icn -banner $(HK)/banner.bnr -exefslogo -target t

# ---- QR code that installs a hosted .cia through FBI (Remote Install > Scan QR Code)
#   make qr URL=https://host/numplay.cia OUT=numplay-qr.png
qr:
	python3 tools/make_qr.py "$(URL)" "$(OUT)"
