# Crowfield — Roadmap

Each version is one playable step. Check boxes in the same commit as the code.
Every step adds or extends a script in `crowfield/test/`.

Numbering: v0.1.0 is the first published release (2026-10-22). Work before it carried pre-release
numbers v0.1.0-v0.3.3 in commit subjects; those numbers are history only and have no tags.

## v0.1.0 — first release (done)

### skeleton
- [x] Build pipeline copied from Rootflow (crt0, linker script, header fix, harness)
- [x] Procedural art: terrain metatiles and farmer/crow sprites
- [x] One-screen farm map with fence, soil, sprouts, bushes
- [x] Farmer walks and runs; fences and bushes block him
- [x] One test crow circles the field
- [x] `test/boot.txt`

### crops
- [x] Crop growth timer per soil tile (soil -> sprout -> young -> grown -> ripe)
- [x] A on ripe crop: harvest, add to stock
- [x] A on empty soil: plant a seed
- [x] 5x7 font from Rootflow, HUD row: day, coins, crops, seeds, hearts
- [x] Target marker on the tile in front of the farmer
- [x] `test/crops.txt`

### combat
- [x] Sword on B (run moved to R): hit box in front of the farmer, slash sprite, knockback
- [x] Crow pool (fixed arrays), crow HP, white hit flash, death puff
- [x] Sparring crows fly at the farmer and peck him
- [x] Farmer hearts, 1 s blink after a hit, knockout for 3 s at 0 hearts, then full hearts
- [x] `test/combat.txt`
- [x] The sword shows in the farmer's hand (B draws and swings, A puts away); `test/sword.txt`

### sound
- [x] Sound effects: CC0 clips for swing, hit, crow death, crow caw on spawn, hurt, knockdown, plant, harvest, no seeds
- [x] v0.1.1: short wooden sword; crow death, spawn caw, seed and knockdown sounds
- [x] v0.1.2: synthesized whoosh for the swing and a sparkly poof for a crow kill (Caleb found the old ones unclear)

## v0.2 — crows (done)
- [x] Crows pick a target crop, land, and eat one stage per bite; they still peck the farmer
- [x] Each bite buffs the crow's HP (fed crows look different: red tint)
- [x] When no crops are left, crows go after the farmer
- [x] Wave spawner from the screen edges (replaces sparring crows)
- [x] Game over when no crops, no seeds, and not enough coins for a seed; START starts a new run
- [x] `test/crows.txt`

## v0.3 — days and the shop
- [x] v0.3.0: day ends when all of its crows are dead ("DAY n DONE!"), then dusk; `DAY n` banner
- [x] v0.3.0: day 1 has 3 crows (Caleb: 6 was too many), +1 per day; 2.5 s apart on day 1, faster later
- [x] v0.3.0: dusk shop window: sell crops for coins, buy seeds, next day (the farm is paused meanwhile)
- [x] v0.3.0: shop and day checks in `test/crows.txt`
- [x] v0.3.1: shop sells Sickle Lv.2/3 and Sword Lv.2/3; `test/shop.txt`
- [ ] Test build caps at Day 20

## v0.4 — scarecrow turrets (done)
- [x] Buy a turret and place it on a soil tile (uses a crop slot); SELECT places and picks up
- [x] Turret aims at the nearest crow and fires a projectile (a corn kernel)
- [x] Turret levels (one level for all scarecrows)
- [x] `test/turret.txt`

## v0.4b — field growth
- [ ] Start map: one small plot, open grass reserved for later plots
- [ ] New plots open on set days (first guess 4, 8, 12, 16)
- [ ] `test/field.txt`

## v0.5 — crow types and scaling
- [ ] Fast, Armored, Big, Swarm crows with their own palettes and sprites
- [ ] Wave table per day
- [ ] Sprite budget check with a full swarm (see TECH.md)

## v0.6 — polish
- [ ] Title screen ("Grow. Defend. Survive.")
- [ ] Day 20 end screen for the test build ("A peaceful farm... for now.")

## v1.0 — release
- [ ] Cover art (`tools/make_cover.py`) and `tools/package.sh` for the R36 SD card
- [ ] Balance pass from playtests
- [ ] README for players
