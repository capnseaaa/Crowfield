# Crowfield — Tech notes

## Hardware facts we rely on
- CPU: ARM7TDMI at about 16.78 MHz. No FPU, no hardware divide.
- IWRAM 32 KB at 0x03000000 (fast; code data and globals live here via gba.ld).
- EWRAM 256 KB at 0x02000000 (slower; not used yet).
- Screen 240x160, mode 0 (tiled backgrounds).
- OAM holds 128 sprites. The per-scanline sprite limit is in render cycles, not count
  (GBATEK). A full crow swarm must be tested in the emulator; see the budget below.

## Video layout (v0.1)
| Item | Where |
|---|---|
| BG tiles (terrain metatiles) | charblock 0 |
| Farm map | BG1, screenblock 28, 32x32 tiles, priority 2 |
| HUD text | BG0, font in charblock 1, screenblock 26, priority 0. Row 0 = HUD, row 19 = messages |
| BG palette bank 1 | text: 1 cream, 2 dark slate background, 3 heart red, 4 coin gold, 5 crop green |
| Sprite tiles | 0x06010000, 1D mapping, 4 tiles per 16x16 cell |
| BG palette bank 0 | field palette (`BG_PAL` in make_art.py) |
| OBJ palette bank 0 | farmer + normal crow (`OBJ_PAL`) |

Planned OBJ palette banks: 1 Armored crow, 2 Big crow, 3 Fast crow, 4 turret, 5 effects.
Crow types should share sprite cells and differ by palette where they can.

## Metatiles
16x16 cells in `art/terrain.png`, in this order (make_art.py `META`, build_data.py, main.c enum):
`GRASS, GRASS_TUFT, DIRT, SOIL, SPROUT, CROP, CROP_RIPE, FENCE, BUSH,
FENCE_V, FENCE_TL, FENCE_TR, FENCE_BL, FENCE_BR, CROP_YOUNG`.
The map stores every fence as `FENCE`; `look_at()` in main.c picks the joined piece to draw.
Add new tiles at the end, and update all three lists together.

## Sprite cells
`art/sprites.png`, 16x16 cells: farmer down, farmer up, farmer side (left = hflip), crow wings up,
crow wings down, target marker.

## Sprite budget
- Planned max on screen: 1 farmer + 1 sword swing + 8 turrets + 8 shots + 48 crows + HUD icons.
- That is under 128 OAM entries. Swarm crows can be 8x8 to cut scanline cost.
- If crows still flicker when bunched up, rotate which crows draw first each frame.

## Fixed point
- Positions: 1/16 pixel (`px >> 4` is the screen pixel).
- Angles: 64 steps per turn, `sin64[]` scaled by 256.

## Save
None. Caleb decided Crowfield has no saves; every run starts fresh.

## Test harness
`test/harness.c` runs the ROM in libmgba with no window. It reads a script, presses keys,
saves `.ppm` screenshots, prints memory (`@`), writes memory (`!`), and power-cycles (`^`).
`test/resolve.py` swaps `$symbol` for its address from `build/crowfield.elf`.
