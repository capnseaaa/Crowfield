# Crowfield — ChatGPT sprite prompts

Two prompts for the farmer. Paste each one into ChatGPT as written. Attach
`crowfield/art/concept/concept_sheet.png` as a reference for his look.

Rules that made Rootflow's player art work (keep them):
- Exact frame sizes, each game pixel drawn as an exact 10x10 block.
- Only the listed hex colors, on a flat magenta background.
- A fixed grid with fixed gaps. No text, labels, grid lines, or shadows.
- Ask for PNG. WebP comes back blurry.

When the images come back: save them as `art/source/farmer_walk.png` and
`art/source/farmer_sword.png` and tell Claude. Claude converts them exactly
(like Rootflow's `tools/import_sheet.py --grid`) and changes the engine to the
16x24 farmer (today's farmer is 16x16).

---

## Prompt 1 — farmer walking

```
Make a pixel-art sprite sheet for a Game Boy Advance farming game. Save it as a PNG (not WebP).

Character: a young farmer. Wide straw hat with a darker brim shade, short brown hair, red shirt,
blue overalls with darker blue shading, brown boots. Friendly, cozy, 16-bit style like Pokémon
Emerald and Stardew Valley. Clean 1-pixel dark outline around the whole character. Match the
farmer in the attached concept sheet.

Size rules (exact):
- Each frame is 16 pixels wide and 24 pixels tall.
- Draw every pixel as a solid 10x10 block, so each frame is 160 x 240 image pixels.
- The character fills the frame. His boots touch the bottom row in every frame.
  His feet stay in the same place in every frame of a row.

Layout (exact):
- 4 columns x 3 rows of frames.
- Columns, left to right: standing still, left foot forward, passing (feet together, body 1 pixel
  higher), right foot forward.
- Rows, top to bottom: facing the viewer (front), facing away (back, we see the hat and the back of
  the overall straps), facing right (side view).
- 40 image pixels of empty space between frames and around the edge.
- The whole image is exactly 840 x 880 pixels.

Colors (exact). Use only these hex colors, with no blending, anti-aliasing, or gradients:
- #FF00FF background (everything that is not the character)
- #1C1C1E outline
- #F0C8A0 skin
- #C89870 skin shade
- #D6A846 straw hat
- #967032 hat shade
- #6E4A2E hair and boots
- #D64A30 shirt
- #A8342A shirt shade
- #3C64AA overalls
- #284278 overalls shade
- #F0F0F0 eye highlight (1 pixel per eye at most)

Do not add text, labels, numbers, grid lines, shadows on the ground, or a frame border.
```

---

## Prompt 2 — farmer swinging his wooden sword

```
Make a pixel-art sprite sheet for a Game Boy Advance farming game. Save it as a PNG (not WebP).

Character: the same young farmer as before (wide straw hat, short brown hair, red shirt, blue
overalls, brown boots, 1-pixel dark outline, 16-bit style like Pokémon Emerald). He swings a short
wooden training sword, in the spirit of the sword in Zelda: A Link to the Past, but made of wood:
- a light tan wooden blade 2 pixels wide with a darker wood shade on one side and a rounded tip,
- a wide crossguard of darker wood,
- a short blue-wrapped grip,
- a small round pommel,
- a 1-pixel dark outline around the whole sword.
The sword is about 13 pixels long from tip to pommel. The farmer holds it in one hand.

Size rules (exact):
- Each frame is 32 x 32 pixels. The farmer's body is 16 pixels wide and 24 pixels tall, centered
  left to right (pixel columns 8 to 23) with his boots on pixel row 27 in every frame.
- The sword stays fully inside the 32 x 32 frame in every pose.
- Draw every pixel as a solid 10x10 block, so each frame is 320 x 320 image pixels.

Layout (exact):
- 4 columns x 3 rows of frames.
- Columns, left to right, one sword swing:
  1. ready: standing, sword held low in front of him, pointing forward and down
  2. wind-up: sword raised high
  3. mid-swing: sword at a 45 degree angle, coming down
  4. follow-through: sword pointing straight out in the direction he faces
- Rows, top to bottom: facing the viewer (front; the swing ends pointing down toward the viewer),
  facing away (back; the swing ends pointing up, away from the viewer), facing right (side view;
  the swing ends pointing right).
- 40 image pixels of empty space between frames and around the edge.
- The whole image is exactly 1480 x 1120 pixels.

Colors (exact). Use only these hex colors, with no blending, anti-aliasing, or gradients:
- #FF00FF background (everything that is not the farmer or the sword)
- #1C1C1E outline
- #F0C8A0 skin
- #C89870 skin shade
- #D6A846 straw hat and the sword's light wood
- #967032 hat shade and the sword's dark wood (blade shade, crossguard, pommel)
- #6E4A2E hair and boots
- #D64A30 shirt
- #A8342A shirt shade
- #3C64AA overalls and the sword's grip
- #284278 overalls shade
- #F0F0F0 eye highlight (1 pixel per eye at most)

Do not add text, labels, numbers, grid lines, motion lines, swoosh trails, shadows on the ground,
or a frame border.
```
