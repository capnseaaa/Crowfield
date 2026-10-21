"""Draw Crowfield's pixel art and save it as indexed PNGs in art/.

The PNGs are the source of truth after the first run: edit them in any
pixel editor (keep the 16-color palette order), then rebuild.
Run with --force to regenerate them from this script.
"""
import os
import random
import sys
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ART = os.path.join(ROOT, "art")

# ---- field palette (BG bank 0), taken from the concept sheet ----
BG_PAL = [
    (16, 20, 24),     # 0 backdrop
    (63, 122, 58),    # 1 grass dark
    (106, 168, 79),   # 2 grass mid
    (156, 204, 101),  # 3 grass light
    (110, 74, 46),    # 4 soil dark
    (139, 94, 60),    # 5 dirt
    (176, 128, 80),   # 6 dirt light
    (90, 58, 34),     # 7 wood dark
    (150, 104, 62),   # 8 wood light
    (122, 120, 116),  # 9 rock
    (58, 150, 214),   # 10 water
    (214, 74, 48),    # 11 red
    (232, 200, 84),   # 12 wheat gold
    (40, 92, 52),     # 13 leaf deep
    (240, 226, 182),  # 14 cream
    (28, 28, 30),     # 15 outline
]

# ---- sprite palette (OBJ bank 0) ----
OBJ_PAL = [
    (255, 0, 255),    # 0 transparent
    (28, 28, 30),     # 1 outline / crow body
    (240, 200, 160),  # 2 skin
    (214, 168, 70),   # 3 straw hat
    (150, 112, 50),   # 4 hat shade
    (60, 100, 170),   # 5 overalls
    (40, 66, 120),    # 6 overalls shade
    (214, 74, 48),    # 7 shirt red
    (110, 74, 46),    # 8 boots
    (70, 70, 84),     # 9 crow wing
    (110, 110, 128),  # 10 crow sheen
    (240, 160, 40),   # 11 beak / eye
    (200, 200, 210),  # 12 blade
    (240, 240, 240),  # 13 white
    (106, 168, 79),   # 14 green
    (120, 80, 40),    # 15 handle
]

# Metatile order. Must match build_data.py and the enum in main.c.
META = ["GRASS", "GRASS_TUFT", "DIRT", "SOIL", "SPROUT", "CROP", "CROP_RIPE", "FENCE", "BUSH",
        "FENCE_V", "FENCE_TL", "FENCE_TR", "FENCE_BL", "FENCE_BR", "CROP_YOUNG"]
# Fence pieces by which neighbors they join: up, down, left, right. FENCE is the horizontal piece.
FENCE_JOIN = {"FENCE": "lr", "FENCE_V": "ud", "FENCE_TL": "rd", "FENCE_TR": "ld", "FENCE_BL": "ru", "FENCE_BR": "lu"}


def blank(n_tiles_w, n_tiles_h=1):
    return [[0] * (16 * n_tiles_w) for _ in range(16 * n_tiles_h)]


def speckle(px, ox, colors, base, rnd, n=18):
    for y in range(16):
        for x in range(16):
            px[y][ox + x] = base
    for _ in range(n):
        px[rnd.randrange(16)][ox + rnd.randrange(16)] = rnd.choice(colors)


def draw_fence(px, o, join):
    """A post in the middle, with rails out to each joined side so neighbors connect."""
    if "l" in join or "r" in join:
        x0 = 0 if "l" in join else 8
        x1 = 16 if "r" in join else 8
        for y in (6, 7, 11, 12):
            for x in range(x0, x1):
                px[y][o + x] = 8 if y in (6, 11) else 7
    if "u" in join or "d" in join:
        y0 = 0 if "u" in join else 8
        y1 = 16 if "d" in join else 8
        for y in range(y0, y1):
            for x in (4, 5, 10, 11):
                px[y][o + x] = 8 if x in (4, 10) else 7
    for y in range(3, 15):   # post
        for x in range(6, 10):
            px[y][o + x] = 8 if x < 8 else 7
    for x in range(6, 10):   # post top
        px[3][o + x] = 6
        px[14][o + x] = 15


def draw_terrain():
    rnd = random.Random(7)
    px = blank(len(META))
    for i, name in enumerate(META):
        o = i * 16
        if name in ("GRASS", "GRASS_TUFT", "BUSH") or name in FENCE_JOIN:
            speckle(px, o, [1, 3], 2, rnd)
        elif name == "DIRT":
            speckle(px, o, [4, 6], 5, rnd)
        else:   # tilled soil rows for crops
            for y in range(16):
                for x in range(16):
                    px[y][o + x] = 4 if (y % 4) in (0, 3) else 5
        if name == "GRASS_TUFT":
            for x, y in ((4, 9), (5, 8), (6, 9), (10, 4), (11, 3), (12, 4)):
                px[y][o + x] = 1
        if name == "SPROUT":
            for x, y in ((7, 10), (8, 10), (7, 9), (6, 8), (9, 8)):
                px[y][o + x] = 2
        if name == "CROP_YOUNG":
            for y in range(7, 13):
                px[y][o + 7] = 13
                px[y][o + 8] = 1
            for x, y in ((5, 8), (6, 8), (9, 9), (10, 9), (6, 11), (9, 7)):
                px[y][o + x] = 2
        if name in ("CROP", "CROP_RIPE"):
            for y in range(3, 14):
                px[y][o + 7] = 13
                px[y][o + 8] = 1
            for y, dx in ((5, -3), (7, 3), (9, -3), (11, 3)):
                for k in range(3):
                    px[y][o + 8 + (dx // 3) * (k + 1)] = 2
            if name == "CROP_RIPE":
                for y in range(2, 6):
                    px[y][o + 7] = 12
                    px[y][o + 8] = 12
        if name in FENCE_JOIN:
            draw_fence(px, o, FENCE_JOIN[name])
        if name == "BUSH":
            for y in range(2, 15):
                for x in range(2, 14):
                    if (x - 7.5) ** 2 + (y - 8.5) ** 2 < 36:
                        px[y][o + x] = 13 if (x + y) % 5 else 2
    return px


def draw_sprites():
    """Cells: 0 farmer down, 1 farmer up, 2 farmer side (right uses hflip), 3 crow wings up, 4 crow wings down,
    5 target marker."""
    px = blank(6)

    def farmer(o, face):
        rows = [
            "....444444......",
            "...33333333.....",
            "..3333333333....",
            "....122221......",
            "....222222......",
            "....722227......",
            "...77555577.....",
            "...27555572.....",
            "....555555......",
            "....556655......",
            "....55..55......",
            "....88..88......",
        ]
        for y, row in enumerate(rows):
            for x, c in enumerate(row):
                if c != ".":
                    px[y + 3][o + x + 1] = int(c, 16)
        if face == "up":   # back of head: hide the face
            for x in range(5, 11):
                px[6][o + x] = 3
        if face == "side":
            px[6][o + 6] = 2
            px[6][o + 9] = 1

    def crow(o, wings_up):
        body = [(x, y) for y in range(7, 12) for x in range(4, 12) if (x - 7.5) ** 2 / 16 + (y - 9) ** 2 / 4 < 1]
        for x, y in body:
            px[y][o + x] = 1
        for x in range(1, 6):
            px[5 if wings_up else 12][o + x] = 9
            px[6 if wings_up else 11][o + x + 1] = 10
        for x in range(10, 15):
            px[5 if wings_up else 12][o + x] = 9
            px[6 if wings_up else 11][o + x - 1] = 10
        px[8][o + 12] = 11
        px[9][o + 13] = 11
        px[8][o + 10] = 13

    def marker(o):   # four corner brackets around a tile
        for i in range(4):
            for x, y in ((i, 0), (0, i), (15 - i, 0), (15, i), (i, 15), (0, 15 - i), (15 - i, 15), (15, 15 - i)):
                px[y][o + x] = 13

    farmer(0, "down")
    marker(80)
    farmer(16, "up")
    farmer(32, "side")
    crow(48, True)
    crow(64, False)
    return px


def save(name, px, pal):
    h, w = len(px), len(px[0])
    img = Image.new("P", (w, h))
    flat = []
    for c in pal:
        flat.extend(c)
    img.putpalette(flat + [0] * (768 - len(flat)))
    img.putdata([v for row in px for v in row])
    img.save(os.path.join(ART, name))


def main():
    os.makedirs(ART, exist_ok=True)
    force = "--force" in sys.argv
    for name, fn, pal in (("terrain.png", draw_terrain, BG_PAL), ("sprites.png", draw_sprites, OBJ_PAL)):
        if force or not os.path.exists(os.path.join(ART, name)):
            save(name, fn(), pal)
            print("drew", name)


if __name__ == "__main__":
    main()
