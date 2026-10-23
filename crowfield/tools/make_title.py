"""Crowfield's title screen art (v0.9.2, Caleb's picks: logo option 1 "blood moon", background 3 "red dusk").

Writes three indexed PNGs into art/ (only if missing; --force redraws them):
  title_bg.png     240x160, the back layer (BG2): a dusk sky falling from purple to red and orange, the blood
                   moon, and the field's edge in silhouette (corn, a fence, a scarecrow). No transparent pixels.
  title_logo.png   240x160, the front layer (BG0, over the sprites): the CROWFIELD letters, the crow perched on
                   the D, and the tagline. Color 0 is transparent.
  title_words.png  240x80, five 240x16 strips for the text layer: PRESS START, > PLAY, PLAY, > STATS, STATS.
                   Each strip covers two tile rows; its text sits 4 px down. Color 0 is transparent.
The circling crows are the game's crow sprites (main.c), drawn between the two layers in a silhouette palette.
"""
import math
import os
import random
import sys

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ART = os.path.join(ROOT, "art")
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from build_data import FONT  # noqa: E402  (the game's 5x7 font, for the small text)

# ---- the letters: hand-drawn, 11 rows tall, 2-px strokes, drawn at 2x ----
LETTERS = {
    "C": ["..#####.", ".##...##", "##.....#", "##......", "##......", "##......", "##......", "##......", "##.....#", ".##...##", "..#####."],
    "R": ["######..", "##...##.", "##....##", "##....##", "##...##.", "######..", "##.##...", "##..##..", "##...##.", "##....##", "##....##"],
    "O": ["..####..", ".##..##.", "##....##", "##....##", "##....##", "##....##", "##....##", "##....##", "##....##", ".##..##.", "..####.."],
    "W": ["##......##", "##......##", "##......##", "##......##", "##..##..##", "##..##..##", "##.####.##", "##.####.##", "####..####", "###....###", "##......##"],
    "F": ["########", "##......", "##......", "##......", "######..", "######..", "##......", "##......", "##......", "##......", "##......"],
    "I": ["####", ".##.", ".##.", ".##.", ".##.", ".##.", ".##.", ".##.", ".##.", ".##.", "####"],
    "E": ["########", "##......", "##......", "##......", "######..", "######..", "##......", "##......", "##......", "##......", "########"],
    "L": ["##......"] * 10 + ["########"],
    "D": ["######..", "##...##.", "##....##", "##....##", "##....##", "##....##", "##....##", "##....##", "##....##", "##...##.", "######.."],
}
PERCHED = [".......###....", "......#####...", ".....######>>.", "...#########..", ".###########..",
           "#############.", "..##########..", "....######....", ".....#..#.....", ".....#..#....."]

# ---- colors ----
KEY = (255, 0, 255)                     # transparent in the logo and word layers
BLACK, SHEEN, EDGE = (22, 18, 26), (70, 66, 84), (110, 22, 20)
BEAK, EYE = (236, 170, 40), (250, 60, 40)
TAG_FILL, TAG_EDGE = (250, 210, 150), (30, 8, 10)
SKY = [(28, 12, 34), (72, 18, 40), (138, 34, 38), (214, 84, 46)]
MOON = [(236, 98, 56), (206, 64, 40), (156, 38, 30), (182, 52, 36)]   # lit, mid, rim, craters
HALO = (120, 34, 28)
GROUND, SILHOUETTE, FAR = (16, 8, 10), (10, 6, 8), (60, 18, 26)

MOON_C, MOON_R = (120, 50), 34          # the stack: moon 12..88, tagline 94, PRESS START 132
HORIZON = 110
TAG_Y = 94
WORD_Y = 4                              # text offset inside a 16-px word strip
BAYER = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]


def word_mask(text):
    ink, x = set(), 0
    for ch in text:
        g = LETTERS[ch]
        for r, row in enumerate(g):
            for c, v in enumerate(row):
                if v == "#":
                    for yy in range(2):
                        for xx in range(2):
                            ink.add((x + c * 2 + xx, r * 2 + yy))
        x += len(g[0]) * 2 + 4
    return ink, x - 4


def small_mask(text):   # the game's 5x7 font, 6 px a letter
    ink, x = set(), 0
    for ch in text:
        if ch != " ":
            for r, row in enumerate(FONT[ch].split()):
                for c, v in enumerate(row):
                    if v == "1":
                        ink.add((x + c, r))
        x += 6
    return ink, x - 1


def outline(ink):
    return {(x + dx, y + dy) for (x, y) in ink for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))} - ink


def paint(img, pts, ox, oy, col):
    w, h = img.size
    for (x, y) in pts:
        if 0 <= ox + x < w and 0 <= oy + y < h:
            img.putpixel((ox + x, oy + y), col)


def small_text(img, text, x, y):
    t, _ = small_mask(text)
    paint(img, outline(t) | {(a + 1, b + 1) for a, b in t}, x, y, TAG_EDGE)
    paint(img, t, x, y, TAG_FILL)


def draw_bg():
    img = Image.new("RGB", (240, 160))
    n = len(SKY) - 1
    for y in range(HORIZON):   # the sky: dithered bands
        t = y / (HORIZON - 1) * n
        i = min(n - 1, int(t))
        f = t - i
        for x in range(240):
            img.putpixel((x, y), SKY[i + 1] if f * 16 > BAYER[y % 4][x % 4] else SKY[i])
    rnd = random.Random(7)   # the moon, lit from the top left, a few craters, a dithered halo
    cx, cy, R = MOON_C[0], MOON_C[1], MOON_R
    spots = [(cx + rnd.randint(-R // 2, R // 2), cy + rnd.randint(-R // 2, R // 2), rnd.randint(2, 4)) for _ in range(6)]
    for y in range(cy - R - 4, cy + R + 5):
        for x in range(cx - R - 4, cx + R + 5):
            d = math.hypot(x - cx, y - cy)
            if d <= R:
                t = math.hypot(x - (cx - R * 0.35), y - (cy - R * 0.35)) / (R * 1.35)
                c = MOON[0] if t < 0.45 else MOON[1] if t < 0.8 else MOON[2]
                if any(math.hypot(x - sx, y - sy) <= sr for sx, sy, sr in spots):
                    c = MOON[3]
                img.putpixel((x, y), c)
            elif d <= R + 4 and (x + y) % 2 == 0:
                img.putpixel((x, y), HALO)
    y = HORIZON   # the field's edge
    for yy in range(y, 160):
        for x in range(240):
            img.putpixel((x, yy), GROUND)
    r = random.Random(2)
    for x in range(240):   # a low tree line far away
        h = 4 + int(3 * math.sin(x / 9) + 2 * math.sin(x / 4.3)) + r.randint(0, 1)
        for yy in range(y - h, y):
            img.putpixel((x, yy), FAR)
    for x in range(0, 240, 6):   # corn
        h = r.randint(4, 7)
        for yy in range(y - h, y + 2):
            img.putpixel((x + 2, yy), SILHOUETTE)
        for dx, dy in ((1, 3), (3, 5), (1, 7)):
            if y - h + dy < y:
                img.putpixel((x + dx, y - h + dy), SILHOUETTE)
    for x in range(240):   # the fence
        for yy in (y + 3, y + 6):
            img.putpixel((x, yy), SILHOUETTE)
        if x % 16 < 2:
            for yy in range(y + 1, y + 10):
                img.putpixel((x, yy), SILHOUETTE)
    sx = 30   # a scarecrow
    for yy in range(y - 26, y + 10):
        img.putpixel((sx, yy), SILHOUETTE)
        img.putpixel((sx + 1, yy), SILHOUETTE)
    for xx in range(sx - 10, sx + 12):
        img.putpixel((xx, y - 18), SILHOUETTE)
        img.putpixel((xx, y - 17), SILHOUETTE)
    for yy in range(y - 32, y - 25):
        for xx in range(sx - 3, sx + 5):
            if (xx - sx - 0.5) ** 2 / 16 + (yy - y + 28.5) ** 2 / 12 <= 1:
                img.putpixel((xx, yy), SILHOUETTE)
    for xx in range(sx - 6, sx + 8):
        img.putpixel((xx, y - 32), SILHOUETTE)
    for yy in range(y - 36, y - 32):
        for xx in range(sx - 2, sx + 4):
            img.putpixel((xx, yy), SILHOUETTE)
    for yy in range(y - 18, y - 8):
        for xx in range(sx - 4 + (yy % 3 == 0), sx + 6 - (yy % 2)):
            img.putpixel((xx, yy), SILHOUETTE)
    return img


def draw_logo():
    img = Image.new("RGB", (240, 160), KEY)
    ink, w = word_mask("CROWFIELD")
    ox, oy = (240 - w) // 2, MOON_C[1] - 10
    paint(img, outline(ink) | {(x + 1, y + 2) for x, y in ink}, ox, oy, EDGE)
    paint(img, ink, ox, oy, BLACK)
    paint(img, {(x, y) for x, y in ink if (x, y - 1) not in ink}, ox, oy, SHEEN)   # a feather sheen on top
    cx0, cy0 = ox + w - 15, oy - 10   # the crow perched on the D
    for r, row in enumerate(PERCHED):
        for c, v in enumerate(row):
            if v != ".":
                img.putpixel((cx0 + c, cy0 + r), BLACK if v == "#" else BEAK)
    img.putpixel((cx0 + 9, cy0 + 1), EYE)
    _, tw = small_mask("GROW. DEFEND. SURVIVE.")
    small_text(img, "GROW. DEFEND. SURVIVE.", (240 - tw) // 2, TAG_Y)
    return img


WORDS = ["PRESS START", "> PLAY", "  PLAY", "> STATS", "  STATS"]


def draw_words():
    img = Image.new("RGB", (240, 16 * len(WORDS)), KEY)
    _, pw = small_mask("PRESS START")
    small_text(img, "PRESS START", (240 - pw) // 2, WORD_Y)
    _, mw = small_mask("> STATS")   # the menu lines share a left edge, so the cursor lines up
    for k, word in enumerate(WORDS[1:], 1):
        small_text(img, word, (240 - mw) // 2, 16 * k + WORD_Y)
    return img


def pixels(img):
    return list(img.get_flattened_data() if hasattr(img, "get_flattened_data") else img.getdata())


def save_indexed(images, transparent):
    """Save images with one exact palette (they share a GBA palette bank): color 0 is transparent (or unused),
    then each color they use."""
    cols = sorted(set().union(*(set(pixels(im)) for im, _ in images)) - ({KEY} if transparent else set()))
    assert len(cols) <= 15, "%d colors; a GBA palette bank holds 15" % len(cols)
    pal = [KEY] + cols
    index = {c: i for i, c in enumerate(pal)}
    for im, name in images:
        out = Image.new("P", im.size)
        out.putpalette([v for c in pal for v in c] + [0] * (768 - 3 * len(pal)))
        out.putdata([index.get(c, 0) for c in pixels(im)])
        out.save(os.path.join(ART, name))
        print("drew", name, "(%d colors in its palette)" % len(cols))


def main():
    force = "--force" in sys.argv
    if force or not os.path.exists(os.path.join(ART, "title_bg.png")):
        save_indexed([(draw_bg(), "title_bg.png")], False)
    if force or not all(os.path.exists(os.path.join(ART, n)) for n in ("title_logo.png", "title_words.png")):
        save_indexed([(draw_logo(), "title_logo.png"), (draw_words(), "title_words.png")], True)   # one bank


if __name__ == "__main__":
    main()
