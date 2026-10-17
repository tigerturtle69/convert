# NumPlay for 3DS

Runs NumPlay (the NumWorks game collection) natively on a Nintendo 3DS. It does **not**
emulate a calculator or run `.nwa` files: it compiles NumPlay's own source for the 3DS and supplies the
~15 NumWorks EADK functions the games use (screen, keys, timing, random). NumPlay's source is untouched.

## Status

| Piece | State |
|---|---|
| Launcher + 17 games build into one native program | works (PC), compiles for ARMv6K/VFP (3DS CPU) |
| Games run through the real launcher | verified on PC, headless, scripted keys, screenshots (`docs/host-test-sheet.png`) |
| 3DS backend (`port/port_3ds.c`) | written against libctru headers, compiles. **Not run on hardware** |
| `.3dsx` / `.cia` packaging (`Makefile`, `3ds/app.rsf`) | written, **never run** (needs devkitPro, makerom, bannertool) |

Because nothing has run on a real 3DS yet, expect to fix a few things on first launch. Send the error and the
fix should be small. The RSF's `SystemCallAccess` and `ServiceAccessControl` lists are the usual homebrew set but
are the most likely place for a CIA install/launch failure.

## Build

```
# devkitPro with 3ds-dev installed (devkitARM + libctru), plus makerom and bannertool on PATH,
# python3 + Pillow, and node/npx (fetches the official eadk.h, which is not redistributed here).
make NUMPLAY=../NumPlay-main 3dsx      # numplay.3dsx for the Homebrew Launcher (iterate with this)
make NUMPLAY=../NumPlay-main cia       # installable .cia (CFW with signature patches needed)
make NUMPLAY=../NumPlay-main host      # headless PC build for testing, no 3DS tools needed
```

Output goes to `build/3ds/`. Games are chosen in `tools/build_native.py` (`GAMES`).

## Hollow Knight

Hollow Knight is a separate app (`make hk`, producing `hollowknight.3dsx` / `.cia`), built by `tools/build_hk.py`
with the same EADK layer. Verified on PC: title, menus and King's Pass render and the Knight moves
(`docs/hk-play.png`); compiles for the 3DS CPU; **not run on hardware**. Its keys: A jump, B attack, Y dash,
X focus/cast, R quick map, L inventory, SELECT pause, START save and quit, D-pad move/look/rest.
On a calculator the game adapts its drawing when frames run slow (`SLOW_MS` in its `main.c`); how fast it runs on an
Old 3DS is unknown.

## Get the CIAs and QR codes without setting up devkitPro

`.github/workflows/build.yml` builds both CIAs in GitHub Actions (official devkitPro image), publishes them as
release assets, and creates a QR code for each. Put this folder in a GitHub repository, run the **build-cia**
workflow (Actions tab > Run workflow), then on the 3DS open FBI > Remote Install > Scan QR Code and scan
`numplay-qr.png` / `hollowknight-qr.png` from the release page. This workflow has not been run yet.

## Saves

Games save through NumPlay's own storage layer, which on a calculator is a RAM region. A native build had none, so
nothing saved. `port/eadk_port.c` now provides that buffer, loads it from the SD card at start and writes it back
within 2 seconds of any change and on exit (Home menu included):

- NumPlay (launcher, every game, launcher settings): `sdmc:/3ds/NumPlay/save.bin`
- Hollow Knight: `sdmc:/3ds/HollowKnight/save.bin`

Delete the file to reset. A power-off or crash can lose the last 2 seconds. Verified on PC: Block Blast resumes with its
score and placed blocks, and Hollow Knight's profile screen shows the saved game.

## Block Blast with the stylus

The game is on the bottom screen, which is the touch screen, so a touch lands on the same pixel. Touch a shape in the
tray to pick it, drag onto the board (the shape follows the pen, centred under it), lift to place it; lifting off the
board cancels. Taps also press menu and help buttons. The buttons still work. Applied by `tools/np_patches.py`;
if NumPlay changes upstream so a patch no longer fits, that patch is skipped with a warning and the build still works.

## Launcher speed

The home screen was slow because the launcher keeps its screenshots compressed and re-decompresses them: it draws in
strips, each strip asks for every card again, and its cache held 6 images for more cards than that, so about 36
decompressions ran every frame (90% of the frame, measured with gprof on PC). The cache now holds every screenshot
(`tools/np_patches.py`). On PC the same scrolling workload went from ~8.3 s to ~0.44 s of CPU. Also: a New 3DS is
switched to 804 MHz (a `.3dsx` otherwise runs at 268 MHz), the screen copy works in cache-friendly blocks, and unchanged
pixels are no longer sent to the screen (45-68% fewer pixels in the launcher, Snake and Block Blast, with identical
pictures). Real frame rates on a 3DS are unmeasured.

## Install by QR code

A `.cia` can't contain its own download QR. The usual 3DS flow is FBI > Remote Install > Scan QR Code, where the QR
holds a direct link to the `.cia`. Host the file (a GitHub release asset works), then
`make qr URL=https://.../numplay.cia OUT=numplay-qr.png`.

## Controls

The game runs on the bottom screen; the top screen shows this key map.

| 3DS | NumWorks |
|---|---|
| D-pad / Circle Pad | arrows |
| A / B / X / Y | OK / Back / EXE / Backspace |
| hold L + A B X Y | 1 2 3 4 |
| hold R + A B X Y | 5 6 7 8 |
| START | Home (leave a game) |
| SELECT | Toolbox |
| ZL / ZR (New 3DS) | Shift / Alpha |

Define `NP_GAME_ON_TOP` (in `port_3ds.c`) to put the game on the top screen instead.

## Testing without a 3DS

`build/host/numplay` runs on a virtual clock with scripted keys:

```
NP_SCRIPT="2000-2150:right,4500-4650:ok" NP_SHOTS="6000" NP_END_MS=6500 NP_OUT=out ./build/host/numplay
python3 tools/sweep.py build/host/numplay ../NumPlay-main out/sweep "snake g2048 ..."   # launch every game
```

## Not done / known limits

- **Left out:** Tetris (Rust), NumBlocks (needs too many keys), and the big standalone games (Celeste, Hollow
  Knight, etc.), which are separate apps with their own build and are the likeliest to be too slow on an Old 3DS.
- **Touch only in Block Blast** (below). There is no general on-screen keypad; other games use the L/R layers.
- **Performance is unmeasured.** The whole program is ~3 MB, but real frame rates on Old vs New 3DS are unknown.
- `eadk_display_draw_string` uses a generated stand-in font (`tools/genfont.py`); NumPlay's own text engine is
  used almost everywhere, so this is rarely hit.

## License

NumPlay is GPL-3.0, so this port (a derivative) should be GPL-3.0 too. The official `eadk.h` is fetched at build
time from the `nwlink` npm package and is not included.
