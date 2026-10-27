# NumPlay for 3DS

Runs NumPlay (the NumWorks game collection) natively on a Nintendo 3DS. It does **not**
emulate a calculator or run `.nwa` files: it compiles NumPlay's own source for the 3DS and supplies the
~15 NumWorks EADK functions the games use (screen, keys, timing, random). NumPlay's source is untouched.

## Status

Built for NumPlay 1.16.0.

| Piece | State |
|---|---|
| NumPlay launcher with 27 small games, as one native program | works; installed and run on a New 3DS (the games added in 1.16 not yet) |
| 23 big apps (each its own `.cia` / `.3dsx`, listed below) | build natively; run headless on PC through scripted play (screenshots of each); compile for the 3DS CPU. The 18 newest (and Celeste, Champion Island, Final Earth 2, NumPowder) have not been run on a 3DS yet |
| Stylus: Block Blast, Balatro, Plants vs. Zombies, Cookie Clicker, Final Earth 2, NumPowder | tested on PC with simulated touches (see the stylus sections) |
| `.3dsx` / `.cia` packaging (`Makefile`, `3ds/*/app.rsf`) | all 24 CIAs build with the real makerom / bannertool and have distinct title IDs |

Not in this version: **NumCraft** (NumBlocks) and **Tetris** (Rust). RetroGo (the emulator) is left out too.

## Which screen

Games with a stylus run on the bottom screen (the touch screen, exactly 320x240); every other game runs on the top
screen (400x240, the picture centred) and the key map shows on the free screen. Celeste's letterboxed picture is scaled
up to fill the top screen. In the NumPlay launcher the picture moves by itself: the home screen is on the touch screen
(swipe = arrows, tap = OK), Block Blast and Balatro stay there with their stylus, and any other game moves to the top
screen and back when it ends.

## Build

```
# devkitPro with 3ds-dev installed (devkitARM + libctru), plus makerom and bannertool on PATH,
# python3 + Pillow + numpy, git and node/npx (the official eadk.h; Friday Night Funkin' fetches its assets).
make NUMPLAY=../NumPlay-main all                 # NumPlay + all 23 big apps, .3dsx and .cia each
make NUMPLAY=../NumPlay-main 3dsx                # just NumPlay's .3dsx (quickest to iterate on)
make NUMPLAY=../NumPlay-main celeste             # one big app (the names are BIG in the Makefile)
make dist BASE_URL=https://github.com/<user>/<repo>/releases/download/latest   # everything in dist/, plus QR codes
make NUMPLAY=../NumPlay-main host                # headless PC build of the launcher, no 3DS tools needed
```

Output goes to `build/3ds/` (NumPlay) and `build/<game>/`. The big apps are built by `tools/build_big.py` (one table entry
per game; C++ for Hill Climb and Doom, scripts for Level Devil and FNF's data); the launcher and its games by
`tools/build_native.py`.

## The big apps

Each is an app of its own on a calculator, with its own program and assets. Here each is its own `.cia` and `.3dsx` with
its own icon, banner, title ID and save folder (`sdmc:/3ds/<Name>/save.bin`), so they install side by side. Every one
was played on PC through the same EADK layer and looked at (title screens, menus, gameplay). D-pad and Circle Pad are the
arrows and START is Home (save and quit) in all of them, except where noted. The top screen / free screen shows the
layout of the running game.

| App | Screen | A | B | X | Y | Others |
|---|---|---|---|---|---|---|
| Hollow Knight | top | jump | attack | focus / cast | dash | L inventory, R quick map, SELECT pause |
| Celeste | top, scaled | jump | dash, talk | EXE (menus) | grab | R grab, SELECT pause |
| Champion Island Games | top | OK (action) | back: map, pause | EXE | OK (action) | |
| Undertale | top | OK (Z) | back (X) | EXE | menu (C) | SELECT backspace |
| Subway Surfers | top | OK (hoverboard) | pause, back | headstart (1) | score booster (3) | |
| Five Nights at Freddy's | top | OK (monitor) | pause | left light | right light | L left door, R right door, SELECT mute call |
| Doom | top | fire | use | next weapon | previous weapon | L / R strafe, ZL run, ZR automap, SELECT menu |
| The Binding of Isaac | top | use item | pause | bomb | pill / card | D-pad walk, **Circle Pad tears**, L bomb, R item, SELECT map |
| Hill Climb Racing, Sonic, Level Devil, Jetpack Joyride, Stickman Hook, Donkey Kong, Dune, Monkey Mart, Tomb of the Mask, Uno | top | OK | back | EXE | shift | SELECT backspace |
| Friday Night Funkin' | top | note right | note down | note up | note left | D-pad also plays notes, L OK (choose), R shift, SELECT back |
| The Final Earth 2, NumPowder | **bottom** (stylus) | OK | back | toolbox | var (menu) | L shift, R alpha, SELECT backspace, ZL minus, ZR plus |
| Plants vs. Zombies | **bottom** (stylus) | OK | back | EXE | backspace (shovel) | SELECT toolbox (almanac), hold L / R + A B X Y = packets 1-4 / 5-8 |
| Cookie Clicker | **bottom** (stylus) | OK (click) | back (menu) | EXE (buy) | var (wrinkler) | L divide, R multiply, ZL shift, ZR alpha, SELECT toolbox |

ZL and ZR are New 3DS only (Doom's run and automap, the zoom and brush size of Final Earth 2 and NumPowder).

**Stylus in Final Earth 2 and NumPowder** (the buttons still work too):

| | Final Earth 2 | NumPowder |
|---|---|---|
| On the play field | tap to click; drag the empty view to scroll | the brush follows the pen, which draws; hold B as well to erase |
| Building | pick a building, then drag it onto the island and lift to place it (drag again to build along) | |
| Buttons | tap the toolbox button, the build menu's buttons and the panel's buttons | tap below the canvas: left half opens the materials, right half the options menu |
| Menus, windows, title | swipe = arrows, tap = OK | swipe = arrows, tap = OK |

**Stylus in Plants vs. Zombies:** touch a seed packet or the shovel to pick it; on the lawn the cursor follows the pen
(sun and coins under it are collected) and lifting plants, digs or whacks there. **Cookie Clicker:** touch the big cookie to
click it (hold to keep clicking), a golden cookie to pop it, a store item to choose it and the same item again to buy
it, the Buy / Sell / 1 / 10 / 100 words above the store, and drag up or down in the store to move the choice.
**Balatro:** during a round touch a hand card to select it, a joker or consumable to open its menu (SELL / USE are touched
too), and the Play Hand, Discard, Rank and Suit buttons. Everywhere else in these three the port turns swipes into
arrows and a tap into OK (`np_gestures` in `port/eadk_port.c`; a game switches it on only where the pen is not a pointer).
The patches are in `tools/np_patches.py`; each skips itself with a warning if NumPlay changes the code it hooks into.

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

- **Left out:** NumCraft (NumBlocks), Tetris (Rust) and RetroGo.
- **Touch only in Block Blast** (below). There is no general on-screen keypad; other games use the L/R layers.
- **Performance is unmeasured.** The whole program is ~3 MB, but real frame rates on Old vs New 3DS are unknown.
- `eadk_display_draw_string` uses a generated stand-in font (`tools/genfont.py`); NumPlay's own text engine is
  used almost everywhere, so this is rarely hit.

## License

NumPlay is GPL-3.0, so this port (a derivative) should be GPL-3.0 too. The official `eadk.h` is fetched at build
time from the `nwlink` npm package and is not included.
