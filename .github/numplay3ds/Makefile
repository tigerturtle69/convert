# NumPlay for 3DS: builds the NumPlay launcher (with its games) and NumPlay's big standalone games, each as a
# .3dsx (Homebrew Launcher) and an installable .cia.
#
# Needs: devkitPro with devkitARM + libctru (pacman -S 3ds-dev), plus python3, Pillow and node (npx, for the
# official eadk.h), and for the CIAs: makerom and bannertool on your PATH.
#
#   make NUMPLAY=../NumPlay-main all           everything
#   make NUMPLAY=../NumPlay-main 3dsx          just NumPlay's .3dsx (quickest to iterate on)
#   make NUMPLAY=../NumPlay-main celeste       one big game (hollowknight celeste championisland finalearth2 numpowder)
#   make dist BASE_URL=https://host/path       gather every .cia/.3dsx into dist/ and make a QR code per .cia
#   make host                                  headless PC build of the launcher (no 3DS tools needed)

ifeq ($(strip $(DEVKITPRO)),)
DEVKITPRO := /opt/devkitpro
endif
DEVKITARM ?= $(DEVKITPRO)/devkitARM
export PATH := $(DEVKITARM)/bin:$(DEVKITPRO)/tools/bin:$(PATH)

NUMPLAY ?= ../NumPlay-main
B       := build
OUT     := $(B)/3ds
NAME    := numplay
TITLE   := NumPlay
DESC    := NumWorks games on 3DS
AUTHOR  := NumPlay (port)
DIST    ?= dist

ARCH    := -march=armv6k -mtune=mpcore -mfpu=vfp -mfloat-abi=hard -mtp=soft
LIBCTRU := $(DEVKITPRO)/libctru

# NumPlay's big standalone games: each is an app of its own on a calculator. Add one: a line here, an entry in
# tools/build_big.py, and its folder in 3ds/ (tools/make_assets.py, tools/make_rsf.py).
BIG := hollowknight celeste championisland finalearth2 numpowder \
  undertale subway fnaf doom isaac hillclimb pvz sonic cookieclicker leveldevil jetpackjoyride stickmanhook \
  donkeykong dune monkeymart tombofthemask uno fnf
TITLE_hollowknight   := Hollow Knight
DESC_hollowknight    := Hollow Knight for NumWorks, on 3DS
TITLE_celeste        := Celeste
DESC_celeste         := Celeste for NumWorks, on 3DS
TITLE_championisland := Champion Island Games
DESC_championisland  := Champion Island Games for NumWorks, on 3DS
TITLE_finalearth2    := The Final Earth 2
DESC_finalearth2     := The Final Earth 2 for NumWorks, on 3DS
TITLE_numpowder      := NumPowder
DESC_numpowder       := NumPowder for NumWorks, on 3DS
TITLE_undertale       := Undertale
DESC_undertale        := Undertale for NumWorks, on 3DS
TITLE_subway          := Subway Surfers
DESC_subway           := Subway Surfers for NumWorks, on 3DS
TITLE_fnaf            := Five Nights at Freddy's
DESC_fnaf             := Five Nights at Freddy's for NumWorks, on 3DS
TITLE_doom            := Doom
DESC_doom             := Doom for NumWorks, on 3DS
TITLE_isaac           := The Binding of Isaac
DESC_isaac            := The Binding of Isaac for NumWorks, on 3DS
TITLE_hillclimb       := Hill Climb Racing
DESC_hillclimb        := Hill Climb Racing for NumWorks, on 3DS
TITLE_pvz             := Plants vs. Zombies
DESC_pvz              := Plants vs. Zombies for NumWorks, on 3DS
TITLE_sonic           := Sonic the Hedgehog
DESC_sonic            := Sonic the Hedgehog for NumWorks, on 3DS
TITLE_cookieclicker   := Cookie Clicker
DESC_cookieclicker    := Cookie Clicker for NumWorks, on 3DS
TITLE_leveldevil      := Level Devil
DESC_leveldevil       := Level Devil for NumWorks, on 3DS
TITLE_jetpackjoyride  := Jetpack Joyride
DESC_jetpackjoyride   := Jetpack Joyride for NumWorks, on 3DS
TITLE_stickmanhook    := Stickman Hook
DESC_stickmanhook     := Stickman Hook for NumWorks, on 3DS
TITLE_donkeykong      := Donkey Kong
DESC_donkeykong       := Donkey Kong for NumWorks, on 3DS
TITLE_dune            := Dune
DESC_dune             := Dune for NumWorks, on 3DS
TITLE_monkeymart      := Monkey Mart
DESC_monkeymart       := Monkey Mart for NumWorks, on 3DS
TITLE_tombofthemask   := Tomb of the Mask
DESC_tombofthemask    := Tomb of the Mask for NumWorks, on 3DS
TITLE_uno             := Uno
DESC_uno              := Uno for NumWorks, on 3DS
TITLE_fnf             := Friday Night Funkin'
DESC_fnf              := Friday Night Funkin' for NumWorks, on 3DS

.PHONY: all 3dsx cia big host clean assets qr dist hk
all: 3dsx cia big
3dsx: $(OUT)/$(NAME).3dsx
cia: $(OUT)/$(NAME).cia
big: $(BIG)
hk: hollowknight

# ---- NumPlay: the launcher and its games, as one program (see tools/build_native.py)
$(OUT)/numplay_objs.txt: $(wildcard port/*.c port/*.h tools/build_native.py tools/np_patches.py)
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

# ---- the big games: the same rules for each one in $(BIG) (tools/build_big.py)
define BIGGAME
.PHONY: $(1)
$(1): $(B)/$(1)/$(1).3dsx $(B)/$(1)/$(1).cia
$(B)/$(1)/numplay_objs.txt: $$(wildcard port/*.c port/*.h tools/build_big.py tools/np_patches.py)
	NP_CPPFLAGS="-I$(LIBCTRU)/include" CROSS=arm-none-eabi- \
	  python3 tools/build_big.py --game $(1) --numplay $(NUMPLAY) --target 3ds --out $(B)/$(1) --opt=-O2
$(B)/$(1)/$(1).elf: $(B)/$(1)/numplay_objs.txt
	$$(shell if [ -s $(B)/$(1)/link_cxx ]; then echo arm-none-eabi-g++; else echo arm-none-eabi-gcc; fi) $(ARCH) -specs=3dsx.specs -Wl,--gc-sections -g \
	  -o $$@ @$(B)/$(1)/numplay_objs.txt -L$(LIBCTRU)/lib -lctru -lm
$(B)/$(1)/$(1).smdh: 3ds/$(1)/icon.png
	smdhtool --create "$(TITLE_$(1))" "$(DESC_$(1))" "$(AUTHOR)" 3ds/$(1)/icon.png $$@
$(B)/$(1)/$(1).3dsx: $(B)/$(1)/$(1).elf $(B)/$(1)/$(1).smdh
	3dsxtool $$< $$@ --smdh=$(B)/$(1)/$(1).smdh
$(B)/$(1)/banner.bnr: 3ds/$(1)/banner.png 3ds/$(1)/audio.wav
	bannertool makebanner -i 3ds/$(1)/banner.png -a 3ds/$(1)/audio.wav -o $$@
$(B)/$(1)/icon.icn: 3ds/$(1)/icon.png
	bannertool makesmdh -i 3ds/$(1)/icon.png -s "$(TITLE_$(1))" -l "$(DESC_$(1))" -p "$(AUTHOR)" -o $$@
$(B)/$(1)/$(1).cia: $(B)/$(1)/$(1).elf $(B)/$(1)/banner.bnr $(B)/$(1)/icon.icn 3ds/$(1)/app.rsf
	makerom -f cia -o $$@ -elf $(B)/$(1)/$(1).elf -rsf 3ds/$(1)/app.rsf \
	  -icon $(B)/$(1)/icon.icn -banner $(B)/$(1)/banner.bnr -exefslogo -target t
endef
$(foreach g,$(BIG),$(eval $(call BIGGAME,$(g))))

# ---- whatever was built, in one folder, with a QR code that installs each .cia through FBI (skips what failed)
#   make dist BASE_URL=https://github.com/<user>/<repo>/releases/download/latest [DIST=dir]
dist:
	mkdir -p "$(DIST)"
	for g in $(NAME) $(BIG); do \
	  d=$(B)/$$g; if [ "$$g" = "$(NAME)" ]; then d=$(OUT); fi; \
	  for e in cia 3dsx; do if [ -f "$$d/$$g.$$e" ]; then cp "$$d/$$g.$$e" "$(DIST)/"; fi; done; \
	  if [ -f "$$d/$$g.cia" ] && [ -n "$(BASE_URL)" ]; then \
	    python3 tools/make_qr.py "$(BASE_URL)/$$g.cia" "$(DIST)/$$g-qr.png"; fi; \
	  if [ ! -f "$$d/$$g.cia" ]; then echo "dist: $$g was not built, skipped"; fi; \
	done

#   make qr URL=https://host/numplay.cia OUT=numplay-qr.png     (one QR code)
qr:
	python3 tools/make_qr.py "$(URL)" "$(OUT)"

assets:
	python3 tools/make_assets.py $(NUMPLAY)/launcher/assets/icon.png 3ds

host:
	python3 tools/build_native.py --numplay $(NUMPLAY) --target host --out $(B)/host
	@echo "run:  NP_SCRIPT=... NP_SHOTS=... ./build/host/numplay   (see port/port_host.c)"

clean:
	rm -rf $(B)
