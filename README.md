# Crowfield

A horde survival farming game for the Game Boy Advance, played in emulators (built for the R36 Ultra handheld).

The crows are hungry. Grow your crops, sell your harvest, and upgrade your gear before they take it all.
*Grow. Defend. Survive.*

## How it plays
- Each day, crows come in waves to eat your crops. Fight them off and harvest what is ripe.
- At dusk, sell crops and buy upgrades: a better sickle, a better sword, and scarecrow turrets.
- Every day the crows come back stronger and in bigger numbers. Survive the last day to win.

## Controls
- D-pad: move, B: run
- A: harvest, plant, or swing (planned)

## Build
Needs `arm-none-eabi-gcc` and Python 3 with Pillow.

```sh
sh crowfield/build.sh          # -> crowfield/crowfield.gba
```

Tests need `libmgba`: `sh crowfield/test/run.sh test/boot.txt` plays a scripted run and saves screenshots.

No external GBA libraries. All art is drawn procedurally by `crowfield/tools/make_art.py`.
Design and plans live in `docs/`.
