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
    (232, 128, 40),   # 10 orange (carrot, pumpkin; was an unused water blue)
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
        "FENCE_V", "FENCE_TL", "FENCE_TR", "FENCE_BL", "FENCE_BR", "CROP_YOUNG", "SCARECROW", "SCARECROW_OFF",
        "CARROT_GROWN", "CARROT_RIPE", "TOMATO_GROWN", "TOMATO_RIPE", "PUMPKIN_GROWN", "PUMPKIN_RIPE"]
# Scarecrow turret on tilled soil. F outline, G straw (hat, arm ends), C sack head, R red shirt,
# W light wood, w dark wood; "." keeps the soil.
SCARECROW = [
    "................",
    "......FFFF......",
    ".....FGGGGF.....",
    "....FGGGGGGF....",
    "...FFFFFFFFFF...",
    ".....FCCCCF.....",
    ".....FFCCFF.....",
    ".....FCCCCF.....",
    ".FFFFFFRRFFFFFF.",
    "GGWWWWRRRRWWWWGG",
    ".FFFFFRRRRFFFFF.",
    ".....FRRRRF.....",
    ".....FFRRFF.....",
    ".......Ww.......",
    ".......Ww.......",
    "......FwwF......",
]
SCARECROW_COLORS = {"F": 15, "G": 12, "C": 14, "R": 11, "W": 8, "w": 7}
# v0.5 crops: grown and ripe looks, drawn on tilled soil ("."). 1/2/3 greens, D deep leaf, 7 wood, A orange,
# R red, C gold, E cream, F outline.
CROP_COLORS = {"1": 1, "2": 2, "3": 3, "7": 7, "A": 10, "R": 11, "C": 12, "D": 13, "E": 14, "F": 15}
CROP_ART = {
    "CARROT_GROWN": [
        "................", "................", "................", "................",
        "......2...2.....", ".....2.2.2.2....", "......2.2.2.....", ".......222......",
        "......21212.....", ".......212......", "........1.......", "........1.......",
        "................", "................", "................", "................"],
    "CARROT_RIPE": [
        "................", "................", "................", "......2...2.....",
        ".....2.2.2.2....", "......2.2.2.....", ".......222......", "......21212.....",
        ".......212......", "......FAAAF.....", "......FAAAF.....", ".......FAF......",
        ".......FAF......", "........F.......", "................", "................"],
    "TOMATO_GROWN": [
        "................", "................", "..........7.....", "......2...7.....",
        ".....222..7.....", "......2D2.7.....", ".....2223.7.....", "......2..27.....",
        ".....2D22.7.....", "......23..7.....", ".....222..7.....", "......D...7.....",
        ".......D..7.....", "..........7.....", "................", "................"],
    "TOMATO_RIPE": [
        "................", "................", "..........7.....", "......2...7.....",
        "....RR22..7.....", "....RE.D2.7.....", ".....222RR7.....", "......2.RE7.....",
        ".....2D22.7.....", "...RR.2...7.....", "...RE222..7.....", "......D...7.....",
        ".......D..7.....", "..........7.....", "................", "................"],
    "PUMPKIN_GROWN": [
        "................", "................", "................", "................",
        "................", "................", "................", ".......2........",
        "....22.2.22.....", "...2D22.22D2....", "....2.22.2......", "......CC........",
        "......CC........", "................", "................", "................"],
    "PUMPKIN_RIPE": [
        "................", "................", "................", "................",
        ".......77.......", "........7.2.....", "....FFFFFFFF....", "...FAARAARAAF...",
        "..FAAARAARAAAF..", "..FAAARAARAAAF..", "..FAAARAARAAAF..", "..FAAARAARAAAF..",
        "...FAARAARAAF...", "....FFFFFFFF....", "................", "................"],
}
for _name, _rows in CROP_ART.items():
    assert len(_rows) == 16 and all(len(r) == 16 for r in _rows), _name
SCARECROW_OFF_COLORS = {"F": 15, "G": 9, "C": 14, "R": 9, "W": 8, "w": 7}   # switched off: gray hat and shirt
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
        if name in CROP_ART:
            for y, row in enumerate(CROP_ART[name]):
                for x, c in enumerate(row):
                    if c != ".":
                        px[y][o + x] = CROP_COLORS[c]
        if name in ("SCARECROW", "SCARECROW_OFF"):
            colors = SCARECROW_COLORS if name == "SCARECROW" else SCARECROW_OFF_COLORS
            for y, row in enumerate(SCARECROW):
                for x, c in enumerate(row):
                    if c != ".":
                        px[y][o + x] = colors[c]
        if name == "BUSH":
            for y in range(2, 15):
                for x in range(2, 14):
                    if (x - 7.5) ** 2 + (y - 8.5) ** 2 < 36:
                        px[y][o + x] = 13 if (x + y) % 5 else 2
    return px


def draw_sprites():
    """Cells: 0 farmer down, 1 farmer up, 2 farmer side (right uses hflip), 3 crow wings up, 4 crow wings down,
    5 target marker, 6 sword pointing right, 7 sword pointing up, 8 puff big, 9 puff small,
    10 farmer knocked out, 11 sword pointing up-right, 12 corn kernel (scarecrow shot), 13 firefly bright,
    14 firefly dim (fireflies use OBJ palette bank 5, set in main.c). Flips give the other directions.
    The hand grips the sword at (2, 8) right, (8, 13) up, (4, 13) up-right (main.c grip_x/grip_y)."""
    px = blank(15)

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

    # Short wooden sword (a training sword in the spirit of A Link to the Past's): light wood blade with a
    # rounded tip, outlined crossguard, blue-wrapped grip, round pommel. Colors: 1 outline, 3 wood,
    # 4 wood shade, 5 blue grip. The black outline keeps the hilt visible on brown soil.
    SWORD_UP = [
        "................",
        "................",
        "................",
        ".......11.......",
        "......1331......",
        "......1341......",
        "......1341......",
        "......1341......",
        "......1341......",
        "......1341......",
        "....11134111....",
        "...1444444441...",
        "....11155111....",
        "......1551......",
        "......1441......",
        ".......11.......",
    ]

    def draw_grid(o, grid):
        for y, row in enumerate(grid):
            for x, c in enumerate(row):
                if c != ".":
                    px[y][o + x] = int(c, 16)

    def sword_up(o):
        draw_grid(o, SWORD_UP)

    def sword_right(o):   # sword_up turned a quarter clockwise
        sword_up(o)
        cell = [[px[y][o + x] for x in range(16)] for y in range(16)]
        for y in range(16):
            for x in range(16):
                px[y][o + x] = cell[15 - x][y]

    SWORD_DIAG = [        # SWORD_UP leaned 45 degrees: tip at the top right, grip at the bottom left
        "................",
        "................",
        "................",
        "................",
        "................",
        "...........11...",
        "..........1341..",
        ".........1341...",
        "...1....1341....",
        "....41.1341.....",
        "....141341......",
        ".....141........",
        "....15141.......",
        "...151.141......",
        "..151...11......",
        ".141............",
    ]

    def sword_diag(o):
        draw_grid(o, SWORD_DIAG)

    def puff(o, r):
        for y in range(16):
            for x in range(16):
                d = (x - 7.5) ** 2 + (y - 7.5) ** 2
                if (r - 2) ** 2 <= d <= r ** 2 and (x * 3 + y) % 4:
                    px[y][o + x] = 13 if (x + y) % 3 else 10

    farmer(0, "down")
    marker(80)
    sword_right(96)
    sword_up(112)
    sword_diag(176)
    puff(128, 7)
    puff(144, 4)
    for y in range(16):   # knocked out: the down-facing farmer turned on his side
        for x in range(16):
            px[y][160 + x] = px[15 - x][y]
    for x, y in ((3, 1), (7, 0), (11, 2)):   # dizzy stars
        px[y][160 + x] = 3
    farmer(16, "up")
    farmer(32, "side")
    crow(48, True)
    crow(64, False)
    for y, row in enumerate(["......111.......", ".....1D3B1......", ".....13BB1......", ".....1BBB1......",
                             "......111......."]):   # corn kernel, centered: D white, 3 straw, B gold
        for x, c in enumerate(row):
            if c != ".":
                px[6 + y][192 + x] = {"1": 1, "D": 13, "3": 3, "B": 11}[c]
    for y, row in enumerate([".....E.....", "....EDE....", "...EDDDE...", "....EDE....", ".....E....."]):
        for x, c in enumerate(row):   # firefly: D bright core (13), E glow (14), centered
            if c != ".":
                px[5 + y][208 + 2 + x] = {"D": 13, "E": 14}[c]
    for x, y in ((7, 7), (8, 7), (7, 8), (8, 8)):   # dim firefly: a small glow
        px[y][224 + x] = 14
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
