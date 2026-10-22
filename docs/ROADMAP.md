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

## v0.4 — scarecrow turrets (done)
- [x] Buy a turret and place it on a soil tile (uses a crop slot); SELECT places and picks up
- [x] Turret aims at the nearest crow and fires a projectile (a corn kernel)
- [x] Turret levels (one level for all scarecrows)
- [x] Sickle Lv.2/3 works 2/3 tiles straight ahead, harvesting and planting (Caleb)
- [x] 15 s to harvest and plant after the last crow, before dusk (Caleb)
- [x] Scarecrow shots use the farmer's seeds, 1 per shot (Caleb)
- [x] Hold B facing a scarecrow to switch it off or on (Caleb)
- [x] v0.4.1: hold A to place or pick up a scarecrow (SELECT still works)
- [x] v0.4.1: shop split into a main page (SELL CROPS, SEEDS, WEAPONS, NEXT DAY) and SEEDS / WEAPONS pages (Caleb: the list was too long)
- [x] `test/turret.txt`

## v0.5 — harder crows, more crops, dusk (done; plan: docs/V0.5_PLAN.md, approved by Caleb)
- [x] Crow levels: black, red, purple, gold (drops 5 coins); a crow turns purple after its 3rd crop
- [x] Day table: a random crow count and mix every day; red from Day 10, purple 20, gold 30, all gold from 40
- [x] Carrot, tomato, pumpkin (unlock Days 5, 12, 20); corn sells for 2; L cycles seeds; corn-only ammo
- [x] SEEDS page lists every seed; details for the one under the cursor
- [x] Dusk: sunset fade during the countdown; night with fireflies in the shop
- [x] `test/v05.txt`, `test/days_check.py`
- [x] v0.5.1: colored seed icons in the HUD and shop; 2 more crows on Days 35-39; gold crows 5 HP (Caleb)

## v0.6 — the blood moon (done; plan: docs/V0.6_PLAN.md, approved by Caleb)
- [x] Blood moon the night after Day 34, then every 4-6 nights; warning and red countdown the day before
- [x] 90 s night of fast gold crows (10 coins each); 6 s knockouts, the 3rd ends the run; scarecrows Lv.1
- [x] Dawn: crows fly away, +25 coins, the shop in pink morning light
- [x] `test/blood.txt`

## v0.7 — the end (done)
- [x] After 5 blood moons: the last shop (+150), then 3 final blood moons in a row, no shop
- [x] White crows (8 HP; 4 hits with a Lv.3 sword); 5 knockouts for the 3 nights
- [x] Win screen ("YOU WON! THANK YOU FOR PLAYING!") and "YOU DID YOUR BEST" screens with stats
- [x] Sprites behind messages and windows
- [x] `test/finale.txt`

## Plan to v1.0 (Caleb, 2026-10-22)

## v0.8 — weapons and the farm expansion
- [ ] Shop: more weapons on the WEAPONS page; a new weapon replaces the current one (which weapons: ask Caleb)
- [ ] Shop: EXPANSION page that makes the crop field bigger (sizes and prices: ask Caleb)
- [ ] Tests for both

## v0.9 — music and a title screen
- [ ] Music: a looping tune (how many songs, and when each plays: ask Caleb)
- [ ] Title screen ("Grow. Defend. Survive.")
- [ ] Stats page on the title screen once a run has been played: each lost (or won) run's results go there
      (highest day, crops grown, crows defeated), so the player sees how earlier runs went. This needs the
      results saved on the cart (SRAM); the runs themselves still start fresh (the "no saves" rule is about
      runs, not this history; confirm with Caleb)

## v1.0 — full release
- [ ] Last touch-ups from Caleb's play
- [ ] Cover art (`tools/make_cover.py`) and `tools/package.sh` for the R36 SD card
- [ ] Balance pass from playtests
- [ ] README for players

## Old plans (replaced)
- v0.4b field growth by day: replaced by the EXPANSION shop page (Caleb).
- Old crow types (Fast, Armored, Big, Swarm): replaced by the v0.5 levels and the white crows.
