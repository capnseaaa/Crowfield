# Crowfield — Design

*Grow. Defend. Survive.*

The crows are hungry. Grow crops, sell the harvest, and upgrade your gear before they take it all.

## Core loop
Days keep going with no set end. The test build stops at Day 20. Each day has two phases.

1. **Day (grow + fight).** Crows arrive in waves. Crops grow on a timer. The farmer harvests
   ripe crops and fights crows at the same time. Crows land on crops and eat them.
   The day does not end until every crow is dead. If the crows eat everything, the farmer
   still has to beat them.
2. **Dusk (shop).** The waves stop. The farmer sells crops and buys upgrades, seeds, and turrets.
   The next day starts when the player leaves the shop.

**Game over:** the farmer has no crops in the field, no seeds, and not enough coins to buy a seed.
Nothing can grow, so the farm is dead. There is no win state; the score is the last day reached. (Test build: Day 20 ends it.)

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
(3 HP, +30%, bites every 1.5 s), gold (4 HP, +45%, bites every 1 s, drops 5 coins when killed). A crow that
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
  L cycles which unlocked seed A plants; the HUD seed count is for that seed. Scarecrows fire corn seeds
  only (Caleb). The SEEDS page lists every seed (locked ones show their unlock day) and tells the sell
  price and grow time of the seed under the cursor.
- Selling: (v0.4) each crop sold for 3 coins (Caleb, 2026-10-22; the first guess of 8 gave too much cash).
  Sold at dusk: SELL CROPS sells the whole bag.
- Seeds cost 1 coin each at dusk (Caleb; was 5). BUY SEED buys one per press.
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
  arc at Lv.2 and bonus crop at Lv.3. Sword Lv.2: 2 damage (a fed crow dies in one hit). Lv.3: 2 damage, hit box 16 px in
  front instead of 10 (the sword sprite keeps its size).
- Game over counts crops in the bag as the coins they sell for, so a full bag never ends the run.
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
- Days are endless; the test build ends at Day 20.
- Crows hurt the farmer and eat crops.
- Turrets go on soil tiles and cost a crop slot.
