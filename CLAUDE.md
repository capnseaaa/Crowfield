# Crowfield — Handoff

Caleb is making a game called Crowfield. Read this whole file before you change anything.
Keep it current: after each feature or decision, update the matching section. Write "unknown"
instead of guessing. Remove anything that turns out wrong.

## How Caleb wants responses

- Plain, simple sentences in subject-verb-object order. High readability.
- No flattery and no automatic agreement. Give direct critique.
- Say "I do not know" instead of guessing. Do not fill gaps with assumptions.
- He prefers momentum over polish, and terse, direct updates while executing.
- He makes the design calls. Offer options with a recommendation; do not decide big design questions for him.

## Working rules

- **Ask before any push to GitHub,** unless Caleb says otherwise for that push.
- **Ask before writing to devices or cards** (handheld SD cards, phones), every time. Back up any file you would change on a device first.
- Ask before anything destructive or hard to undo (deleting files, rewriting history, force pushes).
- **Verify by running, not by reading.** A successful build is not proof. Run the game or its tests and look at the result (screenshots, values) before you call something done.
- When a test fails, find the real cause before you change the test. Never weaken a check just to make it pass.
- Report clearly: what you verified, what you changed, what you did not test, and what is still unknown.
- Prefer small, focused changes. Do not rewrite working systems without a reason.
- Do not reopen a recorded decision without a good reason.
- Caleb also plays builds on his phone. When he asks for a build while away, send him the file directly (or link a GitHub Release download).

## Versions, commits, releases

- Version: `v0.MINOR.PATCH`. MINOR = a feature version. PATCH = a tweak or fix.
- Commit subject: `vX.Y.Z <type>: <summary>`; type is feat, fix, art, test, docs, or chore. The body says what and why.
- Tag: an annotated `vX.Y.Z` tag on the last commit of a version.
- Release: one GitHub Release per version Caleb will play, titled `Crowfield vX.Y.Z — <name>`, with the playable build attached. Notes: What's new / Fixes / Known issues.
- Agent and handoff files stay out of the public repo unless Caleb says otherwise. For Crowfield he said otherwise: this CLAUDE.md is committed, so he can clone it onto his laptop.

## Lessons from Rootflow that may apply

- **Automated playthroughs pay off.** A scripted run that presses buttons and saves screenshots catches real bugs. After each feature, run it and look at the screenshots.
- **Make tests robust to timing and memory layout.** Step scripts by game frames, not wall time. Refer to variables by name, not by hard-coded address.
- **Decode data with a script, not by eye.** Misreading hex once caused a false bug report.
- **AI-generated pixel art needs a strict prompt:** an exact frame size, each pixel as an exact block (e.g. 10×10), a fixed hex palette, a flat magenta background, a grid layout, no text or shadows, and saving as PNG (not WebP). Then convert it exactly.
- **Caleb likes visible feedback:** clear on-screen messages, a menu in the style of Pokémon Emerald, and art he can recognize at small size.

## Caleb's taste (from Rootflow)

Favorite games: Pokémon Emerald, Minecraft, Zelda BotW/TotK, Animal Crossing NH, Stardew Valley. He likes team building, free building, exploration, and a wide mix of activities. Which of these matter for Crowfield: unknown.

---

## Crowfield

### The game
- Pitch: a farmer grows crops while crows come to eat them. He sells crops to upgrade his sickle and sword and to buy scarecrow turrets. Each day the crows come back stronger and in bigger numbers. *Grow. Defend. Survive.*
- Genre and core loop: horde survival + farming. Each day: grow, harvest, and fight waves of crows. At dusk: sell crops and shop. Repeat. Full rules and numbers: `docs/DESIGN.md`.
- Pillars: unknown — ask Caleb.
- Concept art: `crowfield/art/concept/concept_sheet.png` (Caleb's). Match its mood and palette.

### Platform and devices
- Platform: Game Boy Advance ROM, 240x160, played in emulators. Caleb chose GBA.
- Devices: R36 Ultra handheld (main) and sometimes his phone.
- Performance limits: ARM7TDMI ~16.78 MHz, no FPU, no hardware divide; 32 KB IWRAM; 128 OAM sprites, plus a per-scanline sprite limit. A full crow swarm may flicker; see `docs/TECH.md`.

### Build and run
- Requirements: `arm-none-eabi-gcc`, Python 3 + Pillow; `libmgba` for tests. Verified on Ubuntu (cloud container): `apt-get install -y gcc-arm-none-eabi libmgba-dev`. The SessionStart hook in `.claude/hooks/` installs these in cloud sessions. Other OSes: unverified.
- Build: `sh crowfield/build.sh` -> `crowfield/crowfield.gba`.
- Run: load `crowfield/crowfield.gba` in any GBA emulator (mGBA).
- Test: `sh crowfield/test/run.sh test/boot.txt` (headless mGBA; `.ppm` screenshots land in `crowfield/test/`).
- Package for the R36 SD card: `sh crowfield/tools/package.sh` -> `crowfield/release/`.
- Test script lines: `<frames> <keys> [shot.ppm | @addr,addr | !addr=val]`. Keys: A=1 B=2 SELECT=4 START=8 RIGHT=16 LEFT=32 UP=64 DOWN=128 R=256 L=512. `$name` / `$name+off` resolve to ELF symbols. One script frame = one game loop (steps by the global `frame`). `^` power-cycles.

### Code map
```
crowfield/
  build.sh            build the ROM
  src/main.c          all game code (one file)
  src/data.h          GENERATED by tools/build_data.py; never edit; not committed
  src/crt0.s gba.ld   startup code and linker script (from Rootflow)
  art/*.png           indexed 16-color PNGs; source of truth for art once drawn
  art/concept/        reference art, not used by the build
  tools/make_art.py   draws art/*.png only if missing (--force redraws)
  tools/build_data.py PNGs + farm map -> src/data.h
  tools/fixheader.py  GBA header checksum
  tools/package.sh    R36 SD-card release
  test/harness.c      headless mGBA runner; test/run.sh builds and runs it
  test/*.txt          scripted input runs
docs/DESIGN.md        rules, numbers, crow types, upgrades
docs/ROADMAP.md       versions and task checkboxes; pick the next unchecked task
docs/TECH.md          VRAM layout, palettes, sprite budget
```
Code rules: no external GBA libraries; plain freestanding C, `-O2 -mthumb`. No floats, no division in hot loops; positions are 1/16 pixel; `sin64[]` for angles. Game state in plain static globals so tests can find it by name. Metatile order must match in `make_art.py` META, `build_data.py`, and the `main.c` enum.

### Design direction
- The whole farm is one screen with no scrolling, so every crow stays visible.
- A farms the tile in front (plant or harvest). B draws and swings the sword; it stays in hand until A.
- Early days: slow, weak crows. Each day adds more crows and more speed, strength, and health.
- Sound: square-wave chimes made by code (no files). Caleb dislikes them; a fix is an open question.

### Decisions already made (do not reopen without reason)
- Game over: no crops in the field and no seeds left.
- Crops a crow eats make that crow stronger. After crows eat everything, the farmer still has to beat them. A day ends only when every crow is dead.
- Days are endless. The test build stops at Day 20.
- Crows hurt the farmer (PvE) and eat crops.
- Turrets go on soil tiles and use up a crop slot.
- At 0 hearts the farmer is knocked out for 3 seconds while crows eat freely. Not game over.
- No saves. Every run starts fresh.
- The field grows during a run: start with one small plot; new plots open on set days.
- With no crops and no seeds but enough coins to buy a seed, the run goes on.
- The "fed crows snowball" risk is a balance problem; fix it with numbers in playtests.
- Platform GBA, devices R36 Ultra and phone. Version and commit scheme as in Rootflow (above).

### Open questions
- Sound: retune the square-wave chimes, or switch to real sound clips (.wav converted at build time)?
- Plot sizes and unlock days (first guess: 3x3 start, new plots on days 4, 8, 12, 16). What grows after the screen is full?

### Current state
- Version: v0.3.2. The sword is visible in the farmer's hand: B draws and swings it, it rests in hand, A puts it away. Sword on B, run on R. Crows have HP, flash white, get knocked back, and die in a puff. Two sparring crows fly at the farmer and peck him (1 heart). At 0 hearts he is knocked out for 3 s. From v0.2: crops grow on a timer (sprout -> young -> grown -> ripe). A plants a seed on empty soil and harvests a ripe crop. A marker shows the target tile. HUD: day, coins, crops, seeds, hearts. Fences join up.
- Branches: `main` is the real branch. Push to `main` (Caleb approved). Art is a rough placeholder.
- Tested: `test/boot.txt`, `test/crops.txt`, `test/combat.txt`, `test/sword.txt` in headless mGBA (screenshots and values checked). Caleb played v0.1.5 and liked it (asked for joined fences). Caleb played v0.2.0 (works). Caleb played v0.3.0: crows and knockout fine; the slash arc looked like wind (fixed in v0.3.2); he dislikes the sound. v0.3.2 not yet played.
- Order Caleb set: combat (v0.3), then crows (v0.4), then the shop (v0.5).

### Gotchas
- `@`, `!` lines in test scripts need the `<frames> <keys>` prefix (e.g. `1 0 @$px,$py`); a bare `@...` line is skipped silently.
- Tests that need exact positions must set `$sparring=0` first, or random crows peck the farmer.
- `vsync()` has a compiler memory barrier. Without it, GCC keeps globals in registers and test `!` pokes do nothing. Keep it.
- Cloud sessions cannot push tags or create GitHub Releases (tag push is dropped; no release tool). Send the ROM to Caleb directly; he makes the Release on GitHub.
- Commit v0.1.0 used `setup:`, which is not in the type list. Later commits follow the list.
