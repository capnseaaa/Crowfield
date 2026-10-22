# Crowfield — Design

*Grow. Defend. Survive.*

The crows are hungry. Grow crops, sell the harvest, and upgrade your gear before they take it all.

## Core loop
Days keep going with no set end (Caleb dropped the Day 20 test-build stop, 2026-10-22). Each day has two phases.

1. **Day (grow + fight).** Crows arrive in waves. Crops grow on a timer. The farmer harvests
   ripe crops and fights crows at the same time. Crows land on crops and eat them.
   The day does not end until every crow is dead. If the crows eat everything, the farmer
   still has to beat them.
2. **Dusk (shop).** The waves stop. The farmer sells crops and buys upgrades, seeds, and turrets.
   The next day starts when the player leaves the shop.

**Game over:** the farmer has no crops in the field, no seeds, and not enough coins to buy a seed.
Nothing can grow, so the farm is dead. There is no win state; the score is the last day reached.

## The field
- One screen, no scrolling: 15 x 10 metatiles of 16x16 px. This keeps every crow on screen,
  which matters for a horde game, and keeps the camera code out.
- Top row (or bottom row) is the HUD: `Day 3`, coins, crops in stock, hearts.
- A fenced plot in the middle holds the crop tiles. Bushes and trees ring the edge.
  Crows enter from the screen edges.
- **The field grows by day.** The run starts with one small plot (first guess 3x3 soil).
  New plots open on set days (first guess: day 4, 8, 12, 16). The screen keeps open grass
  for them from the start, so nothing scrolls.

## Crops
Each soil tile moves through: empty soil -> sprout -> young -> grown -> ripe.
- Ripe crops stay ripe until harvested or eaten.
- A crow on a crop tile eats one stage at a time. Eaten to empty soil = lost.
- Planting: stand on empty soil, press A with seeds in stock.
- First guess: 1 stage per 8 seconds (480 frames), so sprout to ripe takes 24 s. Tune by play.
- The farmer works the tile in front of his feet. A white marker shows it.
- Start of a run (first guess): 3 seeds, 0 crops, 0 coins, 3 hearts.

## Farmer
- D-pad move, R run. A: farm the tile in front (sickle on a ripe crop, plant on empty soil).
  B: swing the sword. The sword has its own button because almost every field tile is soil,
  so a shared A button would plant seeds when you meant to swing.
- The sword is visible. Before the first B press the farmer holds nothing. B draws the sword
  and swings it through 3 poses (4 frames each); then it rests in his hand. A puts it away.
  Later weapons (e.g. a scythe with a longer swing) can be shop upgrades.
- The Lv.1 sword is a short wooden sword (Caleb, 2026-10-22: the old steel one was far too long; he wants a
  wooden sword like the one in A Link to the Past). About 13 px long; the farmer is 16 px tall.
- Sword Lv.1 (first guess): 1 damage, hit box 28x28 px centered 10 px in front (14 before the short sword), 12-frame swing,
  6-frame cooldown. One hit per crow per swing. A hit crow flashes white and is knocked back.
- A crow peck takes 1 heart and knocks the farmer back. He blinks for 1 s and cannot be hit.
- Hearts: the concept art shows 3 to 5 hearts. Crows peck the farmer. This is PvE, not only
  crop defense. At 0 hearts the farmer is knocked out
  for 3 seconds while crows eat freely, then gets up with full hearts.

## Tools and upgrades (3 levels each)
| Item | Lv.1 | Lv.2 | Lv.3 | Notes |
|---|---|---|---|---|
| Sickle | 1 tile | 2 tiles straight ahead | 3 tiles straight ahead | Each A harvests ripe tiles and plants empty soil (Caleb, v0.4.0). |
| Sword | short swing | more damage | longer range | "More damage, longer range." |
| Scarecrow turret | shoots nearest crow | faster shots | wider range / 2 shots | Placed on a soil tile. It uses up that crop slot. |

Prices from the concept: Sickle Lv.2 = 120, Sword Lv.2 = 150, Scarecrow Turret = 100.
All other numbers are open until playtests.

## Crows
| Type | Look | Behavior |
|---|---|---|
| Normal | black | flies to a crop, lands, eats. 1 hit. |
| Fast | thin, dark | faster flight, 1 hit, flees after one bite. |
| Armored | blue sheen | 3 hits. Turret shots do half. |
| Big | large, red eye | 5 hits, eats 2 stages per bite, can peck the farmer. |
| Swarm | flock of tiny crows | many weak crows that come as one group. |

**Crow levels and eating (v0.5, Caleb).** Lv.1 black (1 HP), Lv.2 red (2 HP, +15% speed), Lv.3 purple
(3 HP, +30%, bites every 1.5 s), gold (5 HP since v0.5.1, +45%, bites every 1 s, drops 5 coins when killed).
Sword damage (v0.5.1, Caleb): Lv.1 1, Lv.2 1.5, Lv.3 2 (HP is kept in half-points). Hits to kill:
Lv.1 black 1, red 2, purple 3, gold 5; Lv.2 1, 2, 2, 4; Lv.3 1, 1, 2, 3. Scarecrow shots do 1.
v0.5.1: Days 35-39 bring 2 more crows (Caleb). Up to 30 crows a day; 24 fit on screen, the rest wait. A crow that
has eaten its 3rd whole crop becomes Lv.3 (purple). Bites no longer add HP or tint a crow (v0.2-v0.4 did),
so a crow's color always tells its level. The day table (random count and mix per day): docs/V0.5_PLAN.md.

**v0.2 crow numbers (first guesses, tune by play):** flight 6/16 px per frame (the old sparring crows
flew 10); one bite every 2 s, so a ripe crop is gone after 4 bites; each crow goes for the nearest crop
no other crow has picked; a crow pecks the farmer within 10 px (12 px while eating), then waits 1.5 s;
day n brings `2 + n` crows (3 on day 1; v0.2 used `4 + 2 * day`, and Caleb found 6 too many), one
every `160 - 10 * n` frames (2.5 s on day 1, never under 0.75 s), each from a random screen edge.

**Days (v0.3.0).** A day starts with a `DAY n` banner; its first crow comes after 2 s. When all of the
day's crows have come and died, "DAY n DONE!" shows, then 15 s of free farming with a "DUSK IN n"
countdown (Caleb, v0.4.0; was 2 s), then dusk:
the shop window opens and the farm pauses (no crows, no growth). Leaving the shop starts the next day. A crow that feeds for long is harder to kill, so the farmer
must stop crows fast. A fed crow should look different (bigger, or a palette change).

Caleb's direction: early days have slower, weaker crows. Each day adds more crows and makes
them faster, stronger, and tougher, a little at a time. (The v0.1 sparring crows are only for
testing combat; their speed is not the day 1 speed.)

Crows get stronger and more numerous each day. First guess for wave size: `4 + 2 * day`,
and new types unlock on later days (Fast day 3, Armored day 5, Big day 7, Swarm day 9).

## Economy
- Shop pages (v0.4.1, Caleb): the main page lists SELL CROPS, SEEDS, WEAPONS, NEXT DAY. SEEDS lists each
  seed with its price, what its crop sells for, and its grow time (corn: 1, 3, 24 s; the only crop so far).
  WEAPONS lists Sickle, Sword, Scarecrow, Scarecrow level. A opens a page or buys; B goes back.
  EXPANSION (a bigger field) is planned but not built.
- Crops (v0.5): corn (seed 1, sells 2, grows 24 s), carrot (2, 5, 32 s, from Day 5), tomato (3, 8, 40 s,
  from Day 12), pumpkin (5, 14, 56 s, from Day 20). Corn went from 3 to 2 to balance the new crops (Caleb).
  L cycles which unlocked seed A plants; the HUD shows that seed's own colored icon and count (v0.5.1:
  corn gold kernel, carrot orange with leaves, tomato red, pumpkin round orange), so it reads at a glance. Scarecrows fire corn seeds
  only (Caleb). The SEEDS page lists every seed (locked ones show their unlock day) and tells the sell
  price and grow time of the seed under the cursor.
- Selling: (v0.4) each crop sold for 3 coins (Caleb, 2026-10-22; the first guess of 8 gave too much cash).
  Sold at dusk: SELL CROPS sells the whole bag.
- Seeds cost 1 coin each at dusk (Caleb; was 5). A buys one; holding A keeps buying (after 1/3 s, one every
  4 frames) until A is let go or the coins run out (v0.7.2).
- Upgrade prices (v0.3.1), rescaled from the concept's Sickle 120 / Sword 150 (set against 8-coin crops) to
  about the same number of crops at 3 coins: Sickle Lv.2 45, Lv.3 90; Sword Lv.2 60, Lv.3 120.
  Scarecrow: 40 coins each (the concept's 100 rescaled); Scarecrow Lv.2 60, Lv.3 120.

**Scarecrow turrets (v0.4.0, first guesses).** Bought at dusk into the bag (`#` in the shop). In the
field, holding A for 1/3 s facing an empty soil tile places one (it takes that crop slot), and holding A
facing one picks it up (v0.4.1; Caleb could not place them with SELECT). SELECT does the same in one
press. While a scarecrow is in the bag, a tap of A on empty soil plants on release instead of on press,
so a hold is never mistaken for a tap.
At most 8 on the field. A scarecrow is solid for the farmer; crows ignore it. It shoots a seed
(2.5 px per frame, 1 damage, small knockback) at the nearest crow within range, from its head. Every
shot uses 1 of the farmer's seeds (Caleb, v0.4.0); with no seeds the scarecrows stop and "SCARECROWS
NEED SEEDS" shows once. Lv.3's second shot goes to a second crow, or to the same crow only if one hit
cannot kill it, so no seed is wasted. Facing a scarecrow and holding B for 0.5 s switches it off (gray
hat and shirt, no shots, no seeds used) or back on (Caleb, v0.4.0); a tap of B still just swings.
Seeds spent by scarecrows count toward the game-over rule (no crops, no seeds, no coins).
Lv.1: range 64 px, one shot every 1.5 s. Lv.2: one shot every 0.83 s. Lv.3: range 88 px and 2 shots
(at the two nearest crows). The level applies to every scarecrow. Scarecrows keep shooting while the
farmer is knocked out.
- What the levels do. Sickle (Caleb, v0.4.0): Lv.1 works the tile in front, Lv.2 the 2 tiles straight
  ahead, Lv.3 the 3 tiles straight ahead (the marker shows them). One A press harvests each ripe tile and
  plants a seed in each empty soil tile (one seed per tile while seeds last). This replaced v0.3.1's 3-tile
  arc at Lv.2 and bonus crop at Lv.3. Sword Lv.2: 1.5 damage (v0.5.1; was 2). Lv.3: 2 damage, hit box 16 px in
  front instead of 10 (the sword sprite keeps its size).
- Game over counts crops in the bag as the coins they sell for, so a full bag never ends the run.
- Blood moon (v0.6, Caleb; docs/V0.6_PLAN.md): the night after Day 34, then every 4 to 6 nights at random.
  The day before, the countdown warns "BLOOD MOON TOMORROW!"; on the day it reads "BLOOD MOON IN n" and
  the sky turns red. No shop: a 30 s blood-red night (90 s until v0.7.2; Caleb: too long) of fast gold crows (one a second, 5 HP, 10 coins
  each). Knockouts last 6 s; the 3rd loses the night. Losing one of the first 3 blood moons only takes back that
  night's coins and the bonus; after the 3rd a notice says the training is over, and from then on a lost
  blood moon ends the run ("THE BLOOD MOON WON"). Scarecrows fire at Lv.1
  for the night. Crops do not grow. At dawn the crows fly away, +25 coins, and the shop opens in pink
  morning light (no fireflies). Then the next day.
- The end (v0.7, Caleb): after the 5th blood moon, the next dusk is the last shop ("LAST TIME TO BUY +150":
  150 coins to spend). Then 3 blood moons in a row (45 s each) with no shop between them (a 5 s break, "BLOOD MOON 2 OF
  3"), with white and gold crows. White crows are the hardest: 8 HP, so a Lv.3 sword needs 4 hits (Lv.1 8,
  Lv.2 6); 1 in 4 crows are white on night 1, 1 in 3 on night 2, 1 in 2 on night 3. The 3 nights share 5
  knockouts; the 5th loses. Survive all 3 and the run is won: "YOU WON! THANK YOU FOR PLAYING!" with the
  highest day, crops grown (harvested), and crows defeated. Every other ending shows the same numbers under
  "YOU DID YOUR BEST" ("THE FARM IS LOST", "THE BLOOD MOON WON", or "THE LAST MOON WON").
- Dev mode (v0.7.1, Caleb; for testing): L + R + B together, while playing, in the shop, or on an end
  screen, opens DEV MODE. Set DAY (1-99), SWORD / SICKLE / SCARE LV, SCARECROWS in the bag, SEEDS EACH (every
  unlocked crop), BLOOD MOONS already done (0-5), TONIGHT (NORMAL / BLOOD / FINAL), MONEY (NORMAL / INFINITE:
  coins stay at 9999), INVINCIBLE (pecks do nothing). Up/Down pick, Left/Right change, L/R by 10 on DAY and
  SEEDS. A starts a fresh run on that day with that gear; B closes without changes.
- v0.8.0 (Caleb; numbers in docs/V0.8_PLAN.md): crops sell for 3 / 7 / 12 / 20, upgrades cost about a third
  less. The shop has SELL (all, or one crop), SEEDS, WEAPONS, FARM pages. Every blood moon has an exact
  lineup of crow types, sent in spurts that fill the night (6, or 9 on a final night); every crow is 20%
  faster at night; kills pay by type (2 / 4 / 6 / 10 / 15). Surviving one opens the dawn shop on CHOOSE A
  BLESSING: +0.5 weapon damage, +0.5 scarecrow damage, or +1 heart (max 5). The FARM page sells field
  steps 7x4 (60), 9x4 (120), 9x5 (200).
- All sprites draw behind the text layer, so messages, notices, the HUD, and windows cover them (Caleb).
- Dusk (v0.5, Caleb): the field fades toward sunset orange during the 15 s "DUSK IN n" countdown; the shop
  opens at night (dark blue field) with fireflies drifting and blinking around the window. Fireflies show
  only at night in the shop. The next day starts bright.
- Coins buy upgrades, seeds, and turrets. Crops in stock carry over.

## Look and feel
- Palette from the concept: greens, browns, gray, sky blue, red, dark brown, near-black.
- 16-bit inspired, cozy, clean outlines. See `crowfield/art/concept/concept_sheet.png`.
- Sound: real sound clips since v0.1.0 (Caleb's pick over retuned chimes). Free CC0 clips from Kenney
  and OpenGameArt; see `crowfield/sound/CREDITS.md`.

## Open questions for the owner
1. Exact plot sizes and unlock days (first guesses above). Endless days means unlocks must stop
   when the screen is full; after that, growth could come from upgrades instead.

## Decided
- Lose: no crops, no seeds, and not enough coins for a seed.
- 0 hearts: knocked out for 3 seconds, not game over.
- No saves. Every run starts fresh.
- The field grows during a run: start with a small plot, new plots open on set days.
- Fed crows snowballing is handled by balance (HP cap, numbers), not a new rule.
- Crops eaten buff the crow. A day ends only when all crows die.
- Days are endless (no Day 20 stop; Caleb, 2026-10-22).
- Crows hurt the farmer and eat crops.
- Turrets go on soil tiles and cost a crop slot.
