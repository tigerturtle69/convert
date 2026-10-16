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

- **Saves don't persist.** NumPlay's storage hook returns nothing on non-calculator builds. It needs a small
  SD-card-backed version of `epsilon_storage()`.
- **Left out:** Tetris (Rust), NumBlocks (needs too many keys), and the big standalone games (Celeste, Hollow
  Knight, etc.), which are separate apps with their own build and are the likeliest to be too slow on an Old 3DS.
- **No touch keypad** yet. Games that need many keys use the L/R layers above.
- **Performance is unmeasured.** The whole program is ~3 MB, but real frame rates on Old vs New 3DS are unknown.
- `eadk_display_draw_string` uses a generated stand-in font (`tools/genfont.py`); NumPlay's own text engine is
  used almost everywhere, so this is rarely hit.

## License

NumPlay is GPL-3.0, so this port (a derivative) should be GPL-3.0 too. The official `eadk.h` is fetched at build
time from the `nwlink` npm package and is not included.
