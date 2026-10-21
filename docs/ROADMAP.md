# Crowfield — Roadmap

Each version is one playable step. Check boxes in the same commit as the code.
Every step adds or extends a script in `crowfield/test/`.

## v0.1 — skeleton (done)
- [x] Build pipeline copied from Rootflow (crt0, linker script, header fix, harness)
- [x] Procedural art: terrain metatiles and farmer/crow sprites
- [x] One-screen farm map with fence, soil, sprouts, bushes
- [x] Farmer walks and runs; fences and bushes block him
- [x] One test crow circles the field
- [x] `test/boot.txt`

## v0.2 — crops
- [ ] Crop growth timer per soil tile (soil -> sprout -> young -> grown -> ripe)
- [ ] A on ripe crop: harvest, add to stock
- [ ] A on empty soil: plant a seed
- [ ] 5x7 font from Rootflow, HUD row: day, coins, crops, hearts
- [ ] `test/crops.txt`

## v0.3 — crows
- [ ] Crow pool (fixed array, no malloc). States: fly in, land, eat, flee, dead
- [ ] Crows pick a target crop and eat one stage per bite
- [ ] Wave spawner from the screen edges
- [ ] `test/crows.txt`

## v0.4 — combat
- [ ] Sword swing: hitbox in front of the farmer, swing sprite, knockback
- [ ] Crow HP, hit flash, death puff
- [ ] Farmer hearts, crow peck, game over screen
- [ ] `test/combat.txt`

## v0.5 — days and the shop
- [ ] Day timer, dusk phase, `Day x/N` banner
- [ ] Sell crops for coins
- [ ] Shop panel: Sickle Lv.2/3, Sword Lv.2/3, seeds
- [ ] `test/shop.txt`

## v0.6 — scarecrow turrets
- [ ] Buy and place a turret
- [ ] Turret aims at the nearest crow and fires a projectile
- [ ] Turret levels
- [ ] `test/turret.txt`

## v0.7 — crow types and scaling
- [ ] Fast, Armored, Big, Swarm crows with their own palettes and sprites
- [ ] Wave table per day
- [ ] Sprite budget check with a full swarm (see TECH.md)

## v0.8 — polish
- [ ] Title screen ("Grow. Defend. Survive.")
- [ ] Sound effects
- [ ] Win screen ("A peaceful farm... for now.")
- [ ] Save best run to SRAM

## v1.0 — release
- [ ] Cover art (`tools/make_cover.py`) and `tools/package.sh` for the R36 SD card
- [ ] Balance pass from playtests
- [ ] README for players
