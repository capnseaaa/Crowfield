# Crowfield — Design

*Grow. Defend. Survive.*

The crows are hungry. Grow crops, sell the harvest, and upgrade your gear before they take it all.

## Core loop
One run is a set number of days (rounds). Each day has two phases.

1. **Day (grow + fight).** Crows arrive in waves. Crops grow on a timer. The farmer harvests
   ripe crops and fights crows at the same time. Crows land on crops and eat them.
2. **Dusk (shop).** The waves stop. The farmer sells crops and buys upgrades, seeds, and turrets.
   The next day starts when the player leaves the shop.

A run is won when the farmer survives the last day. A run is lost when the farmer runs out of
hearts, or when every crop tile is dead (to be decided, see Open questions).

## The field
- One screen, no scrolling: 15 x 10 metatiles of 16x16 px. This keeps every crow on screen,
  which matters for a horde game, and keeps the camera code out.
- Top row (or bottom row) is the HUD: `Day 3/10`, coins, crops in stock, hearts.
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
- Hearts: the concept art shows 3 to 5 hearts. Some crows (Big, Armored) peck the farmer.

## Tools and upgrades (3 levels each)
| Item | Lv.1 | Lv.2 | Lv.3 | Notes |
|---|---|---|---|---|
| Sickle | harvest 1 tile | faster swing | harvests a 3-tile arc, +1 crop per tile | "Faster harvest, more crops per tile." |
| Sword | short swing | more damage | longer range | "More damage, longer range." |
| Scarecrow turret | shoots nearest crow | faster shots | wider range / 2 shots | Placed on grass. Limited count. |

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
1. Run length: the concept shows both `Day x/10` and `Day 20/20`. Which one?
2. Lose condition: hearts only, all crops lost, or both?
3. Does a crow peck the farmer at all, or only the crops?
4. Are turrets placed anywhere on grass, or only on fixed posts?
5. Is a save needed mid-run, or only best-run stats?
6. Should the field grow (more plots) as the run goes on, like the "Finished Run" concept?
