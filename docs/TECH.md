# Crowfield — Tech notes

## Hardware facts we rely on
- CPU: ARM7TDMI at about 16.78 MHz. No FPU, no hardware divide.
- IWRAM 32 KB at 0x03000000 (fast; code data and globals live here via gba.ld).
- EWRAM 256 KB at 0x02000000 (slower; not used yet).
- Screen 240x160, mode 0 (tiled backgrounds).
- OAM holds 128 sprites. The per-scanline sprite limit is in render cycles, not count
  (GBATEK). A full crow swarm must be tested in the emulator; see the budget below.

## Video layout
| Item | Where |
|---|---|
| BG tiles (terrain metatiles) | charblock 0 |
| Farm map | BG1, screenblock 28, 32x32 tiles, priority 2 |
| HUD text | BG0, font in charblock 1, screenblock 26, priority 0. Row 0 = HUD, row 19 = messages |
| BG palette bank 1 | text: 1 cream, 2 dark slate background, 3 heart red, 4 coin gold, 5 crop green |
| Sprite tiles | 0x06010000, 1D mapping, 4 tiles per 16x16 cell |
| BG palette bank 0 | field palette (`BG_PAL` in make_art.py) |
| OBJ palette bank 0 | farmer + normal crow (`OBJ_PAL`) |
| OBJ palette bank 1 | hit flash: every color white |

Planned OBJ palette banks: 2 Armored crow, 3 Big crow, 4 Fast crow, 5 turret, 6 fed crow.
Crow types should share sprite cells and differ by palette where they can.

## Metatiles
16x16 cells in `art/terrain.png`, in this order (make_art.py `META`, build_data.py, main.c enum):
`GRASS, GRASS_TUFT, DIRT, SOIL, SPROUT, CROP, CROP_RIPE, FENCE, BUSH,
FENCE_V, FENCE_TL, FENCE_TR, FENCE_BL, FENCE_BR, CROP_YOUNG`.
The map stores every fence as `FENCE`; `look_at()` in main.c picks the joined piece to draw.
Add new tiles at the end, and update all three lists together.

## Sprite cells
`art/sprites.png`, 16x16 cells: farmer down, farmer up, farmer side (left = hflip), crow wings up,
crow wings down, target marker, sword right, sword up, puff big, puff small,
farmer knocked out, sword up-right. Flips give the other sword directions; `swing_pose[]`
in main.c lists each facing's 3 swing poses and its resting pose. `put_sprite(x, y, cell, flags)` takes HFLIP, VFLIP, FLASH.

## Sprite budget
- Planned max on screen: 1 farmer + 1 sword swing + 8 turrets + 8 shots + 48 crows + HUD icons.
- That is under 128 OAM entries. Swarm crows can be 8x8 to cut scanline cost.
- If crows still flicker when bunched up, rotate which crows draw first each frame.

## Fixed point
- Positions: 1/16 pixel (`px >> 4` is the screen pixel).
- Angles: 64 steps per turn, `sin64[]` scaled by 256.

## Save
None. Caleb decided Crowfield has no saves; every run starts fresh.

## Sound (v0.1.0)
- Clips: `sound/*.wav` (mono 16-bit 22050 Hz, committed). `tools/import_sound.sh` cut them from the
  downloaded packs in `sound/source/` (not committed; needs ffmpeg). `tools/synth_sound.py` makes
  whoosh.wav and poof.wav from code (no pack had a good whoosh or poof; stdlib only). Sources and licenses: `sound/CREDITS.md`.
- `tools/make_sound.py` (run by build.sh, stdlib only) -> `src/sound.h`: signed 8-bit at 13379 Hz, silence
  trimmed, normalized to a per-clip peak, 10 ms fade-out. `--preview DIR` writes the 8-bit clips as WAV.
  Its SOUNDS list order = the `SND_*` enum in main.c. 9 clips = 30 KB of ROM.
- Events: whoosh (sword swing), hit (crow hit), poof (crow killed), caw (crow spawns), hurt (peck), knockdown (0 hearts,
  layered on hurt), plant, harvest, noseeds.
- Playback: Direct Sound A. Timer 0 at 1254 cycles per sample = 224 samples per frame. DMA 1 feeds FIFO A
  from a 2x224-byte double buffer (DMA 1 registers start at 0x040000BC). Right after vsync,
  `sound_vblank()` restarts the DMA every other frame; `sound_mix()` sums up to 4 channels into the half
  that plays next, with clamping. PSG channels are off.
- `play(SND_x)`: the same clip restarts on its own channel; else a free channel; else the oldest.
- A game loop longer than one frame replays stale samples. Measured 2026-10-21: 601 loops in 602 frames
  with crows and sound, so no dropped frames now.
- Test: `HARNESS_WAV=/abs/path.wav sh test/run.sh test/x.txt` records the game's audio (stereo 32768 Hz).
  Use an absolute path: run.sh changes folder first, and a relative path crashes the harness.

## Test harness
`test/harness.c` runs the ROM in libmgba with no window. It reads a script, presses keys,
saves `.ppm` screenshots, prints memory (`@`), writes memory (`!`), and power-cycles (`^`).
`test/resolve.py` swaps `$symbol` for its address from `build/crowfield.elf`.
