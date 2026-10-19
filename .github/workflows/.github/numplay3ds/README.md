# NumPlay for 3DS

Runs NumPlay (the NumWorks game collection) natively on a Nintendo 3DS. It does **not**
emulate a calculator or run `.nwa` files: it compiles NumPlay's own source for the 3DS and supplies the
~15 NumWorks EADK functions the games use (screen, keys, timing, random). NumPlay's source is untouched.

## Status

| Piece | State |
|---|---|
| NumPlay launcher + 17 games, as one native program | works; installed and run on a New 3DS |
| 5 big games: Hollow Knight, Celeste, Champion Island Games, The Final Earth 2, NumPowder | build natively; run headless on PC through real play (screenshots); compile for the 3DS CPU. The four newest are not yet run on a 3DS |
| Saves, Block Blast stylus, launcher speed-up | built and tested on PC; see the sections below |
| `.3dsx` / `.cia` packaging (`Makefile`, `3ds/*/app.rsf`) | all six CIAs build with the real makerom/bannertool and have distinct title IDs |

Not in this version: **Undertale**, which NumPlay lists as planned ("Undertale's fights") but has not released, and
**NumBlocks** (needs too many keys).

## Build

```
# devkitPro with 3ds-dev installed (devkitARM + libctru), plus makerom and bannertool on PATH,
# python3 + Pillow, and node/npx (fetches the official eadk.h, which is not redistributed here).
make NUMPLAY=../NumPlay-main all                 # NumPlay + the five big games, .3dsx and .cia each
make NUMPLAY=../NumPlay-main 3dsx                # just NumPlay's .3dsx (quickest to iterate on)
make NUMPLAY=../NumPlay-main celeste             # one big game: hollowknight celeste championisland finalearth2 numpowder
make dist BASE_URL=https://github.com/<user>/<repo>/releases/download/latest   # everything in dist/, plus QR codes
make NUMPLAY=../NumPlay-main host                # headless PC build of the launcher, no 3DS tools needed
```

Output goes to `build/3ds/` (NumPlay) and `build/<game>/`. The big games are built by `tools/build_big.py` (one table
entry per game); the launcher and its games by `tools/build_native.py`.

## The big games

Each of these is an app of its own on a calculator, with its own program, platform file and assets. Here each is its
own `.cia` and `.3dsx` with its own icon, banner, title ID and save folder (`sdmc:/3ds/<Name>/save.bin`), so they
install side by side. Verified on PC by playing into each one: Hollow Knight into King's Pass, Celeste into the
Prologue, Champion Island onto the boat and into dialogue, Final Earth 2 into its build menu, NumPowder pouring powder.

| Game | A | B | X | Y | L | R | SELECT |
|---|---|---|---|---|---|---|---|
| Hollow Knight | jump | attack | focus / cast | dash | inventory | quick map | pause |
| Celeste | jump | dash, talk | EXE (menus) | grab | | grab | pause |
| Champion Island Games | OK (action) | back: map, pause | EXE | OK (action) | | | |
| Final Earth 2, NumPowder | OK | back | toolbox | var (menu) | shift (hold) | alpha (pause) | backspace |

D-pad and Circle Pad are the arrows and START is Home (save and quit) in all of them. In Final Earth 2 and NumPowder,
ZL is minus and ZR is plus (zoom, brush size): New 3DS only. The top screen shows the layout of the running game.
No touch support in these yet (Final Earth 2 and NumPowder are the ones that would gain most).

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

- **Left out:** Tetris (Rust), NumBlocks (needs too many keys), Undertale (not released upstream yet).
- **Touch only in Block Blast** (below). There is no general on-screen keypad; other games use the L/R layers.
- **Performance is unmeasured.** The whole program is ~3 MB, but real frame rates on Old vs New 3DS are unknown.
- `eadk_display_draw_string` uses a generated stand-in font (`tools/genfont.py`); NumPlay's own text engine is
  used almost everywhere, so this is rarely hit.

## License

NumPlay is GPL-3.0, so this port (a derivative) should be GPL-3.0 too. The official `eadk.h` is fetched at build
time from the `nwlink` npm package and is not included.
