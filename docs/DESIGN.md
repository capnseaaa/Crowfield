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

## Crops
Each soil tile moves through: empty soil -> sprout -> young -> grown -> ripe.
- Ripe crops stay ripe until harvested or eaten.
- A crow on a crop tile eats one stage at a time. Eaten to empty soil = lost.
- Planting: stand on empty soil, press A with seeds in stock.
- First guess: 1 stage per 8 seconds. Tune by play.

## Farmer
- D-pad move, B run. A: use the tool that fits the tile (sickle on a ripe crop, sword on a crow,
  plant on empty soil). One button keeps it simple on the R36.
- Hearts: the concept art shows 3 to 5 hearts. Crows peck the farmer. This is PvE, not only
  crop defense. At 0 hearts the farmer is knocked out for a few seconds
  while crows eat freely, then gets up with full hearts (3 seconds).

## Tools and upgrades (3 levels each)
| Item | Lv.1 | Lv.2 | Lv.3 | Notes |
|---|---|---|---|---|
| Sickle | harvest 1 tile | faster swing | harvests a 3-tile arc, +1 crop per tile | "Faster harvest, more crops per tile." |
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

**Eating buffs crows.** Each crop stage a crow eats gives it more HP (first guess +1 HP per bite,
capped at double its base HP). A crow that feeds for long is harder to kill, so the farmer
must stop crows fast. A fed crow should look different (bigger, or a palette change).

Crows get stronger and more numerous each day. First guess for wave size: `4 + 2 * day`,
and new types unlock on later days (Fast day 3, Armored day 5, Big day 7, Swarm day 9).

## Economy
- Selling: each crop sells for a fixed price (first guess 8 coins). Sold at dusk.
- Coins buy upgrades, seeds, and turrets. Crops in stock carry over.

## Look and feel
- Palette from the concept: greens, browns, gray, sky blue, red, dark brown, near-black.
- 16-bit inspired, cozy, clean outlines. See `crowfield/art/concept/concept_sheet.png`.
- Sound: short PSG chimes like Rootflow (harvest, hit, coin, crow caw).

## Open questions for the owner
1. How does the field grow? Options: buy new plots in the shop (recommended: it competes with
   upgrades for coins), or a new plot every few days for free.
2. Where do new plots go on a one-screen farm? The current map has room for about two more
   fenced plots; more than that needs a smaller layout or scrolling.

## Decided
- Lose: no crops, no seeds, and not enough coins for a seed.
- 0 hearts: knocked out for 3 seconds, not game over.
- No saves. Every run starts fresh.
- The field grows during a run (more plots), like the "Finished Run" concept.
- Fed crows snowballing is handled by balance (HP cap, numbers), not a new rule.
- Crops eaten buff the crow. A day ends only when all crows die.
- Days are endless; the test build ends at Day 20.
- Crows hurt the farmer and eat crops.
- Turrets go on soil tiles and cost a crop slot.
