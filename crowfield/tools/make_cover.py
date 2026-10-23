"""Crowfield's box cover (v0.9.2): art/box_cover.png, 326x360, the size and layout of Caleb's GBA box art.

The left side of a GBA box has a silver stripe with the "GAME BOY ADVANCE" name. Caleb asked for that area to be
plain white, cut to exactly where the stripe was (its right edge curves: x = A*y^2 + B*y + C, measured on his
reference image to within a pixel). The rest is a portrait version of the title screen: the red dusk, the blood
moon, the CROWFIELD letters with a crow perched on the D, crow silhouettes, the field's edge, and the tagline.
The scene is drawn at half size in the game's palette and doubled; the letters and the perched crow are drawn at
third size and tripled, so the title spans the art. Every pixel stays sharp.

Run: python3 tools/make_cover.py   (writes art/box_cover.png every time; not part of the ROM build)
"""
import math
import os
import random
import sys

from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_title as mt  # noqa: E402  (the title's colors, letters, and small font)

W, H = 326, 360
EDGE_A, EDGE_B, EDGE_C = -0.00026278, 0.100911, 63.630   # the stripe's right edge, x at row y
ART_X = 64                                             # the art starts here; the white covers its left edge
NW, NH = (W - ART_X) // 2, H // 2                      # 131 x 180, doubled
CX = 68                                                # the middle of the visible art, in half-size pixels
HORIZON = 132
MOON_C, MOON_R = (CX, 60), 30
LOGO_SCALE = 3                                         # the letters and the perched crow, at 3x on the cover


def letters_1x(text):
    """The title letters at their drawn size (11 px tall, 2-px strokes)."""
    ink, x = set(), 0
    for ch in text:
        g = mt.LETTERS[ch]
        for r, row in enumerate(g):
            for c, v in enumerate(row):
                if v == "#":
                    ink.add((x + c, r))
        x += len(g[0]) + 1
    return ink, x - 1


def draw_art():
    img = Image.new("RGB", (NW, NH))
    n = len(mt.SKY) - 1
    for y in range(HORIZON):   # the dusk, dithered like the title's
        t = y / (HORIZON - 1) * n
        i = min(n - 1, int(t))
        f = t - i
        for x in range(NW):
            img.putpixel((x, y), mt.SKY[i + 1] if f * 16 > mt.BAYER[y % 4][x % 4] else mt.SKY[i])
    rnd = random.Random(7)   # the moon
    cx, cy, R = MOON_C[0], MOON_C[1], MOON_R
    spots = [(cx + rnd.randint(-R // 2, R // 2), cy + rnd.randint(-R // 2, R // 2), rnd.randint(1, 3)) for _ in range(5)]
    for y in range(cy - R - 3, cy + R + 4):
        for x in range(cx - R - 3, cx + R + 4):
            d = math.hypot(x - cx, y - cy)
            if d <= R:
                t = math.hypot(x - (cx - R * 0.35), y - (cy - R * 0.35)) / (R * 1.35)
                c = mt.MOON[0] if t < 0.45 else mt.MOON[1] if t < 0.8 else mt.MOON[2]
                if any(math.hypot(x - sx, y - sy) <= sr for sx, sy, sr in spots):
                    c = mt.MOON[3]
                img.putpixel((x, y), c)
            elif d <= R + 3 and (x + y) % 2 == 0:
                img.putpixel((x, y), mt.HALO)
    y = HORIZON   # the field's edge, the same shapes as the title
    for yy in range(y, NH):
        for x in range(NW):
            img.putpixel((x, yy), mt.GROUND)
    r = random.Random(2)
    for x in range(NW):
        h = 3 + int(2 * math.sin(x / 7) + 1.5 * math.sin(x / 3.3)) + r.randint(0, 1)
        for yy in range(y - h, y):
            img.putpixel((x, yy), mt.FAR)
    for x in range(0, NW - 3, 5):
        h = r.randint(3, 6)
        for yy in range(y - h, y + 2):
            img.putpixel((x + 2, yy), mt.SILHOUETTE)
        img.putpixel((x + 1, y - h + 2), mt.SILHOUETTE)
        img.putpixel((x + 3, y - h + 3), mt.SILHOUETTE)
    for x in range(NW):
        for yy in (y + 3, y + 6):
            img.putpixel((x, yy), mt.SILHOUETTE)
        if x % 14 < 2:
            for yy in range(y + 1, y + 9):
                img.putpixel((x, yy), mt.SILHOUETTE)
    sx = 22   # the scarecrow, left of the middle
    for yy in range(y - 30, y + 8):
        img.putpixel((sx, yy), mt.SILHOUETTE)
        img.putpixel((sx + 1, yy), mt.SILHOUETTE)
    for xx in range(sx - 11, sx + 13):
        img.putpixel((xx, y - 21), mt.SILHOUETTE)
        img.putpixel((xx, y - 20), mt.SILHOUETTE)
    for yy in range(y - 37, y - 29):
        for xx in range(sx - 3, sx + 5):
            if (xx - sx - 0.5) ** 2 / 16 + (yy - y + 33) ** 2 / 14 <= 1:
                img.putpixel((xx, yy), mt.SILHOUETTE)
    for xx in range(sx - 6, sx + 8):
        img.putpixel((xx, y - 37), mt.SILHOUETTE)
    for yy in range(y - 41, y - 37):
        for xx in range(sx - 2, sx + 4):
            img.putpixel((xx, yy), mt.SILHOUETTE)
    for yy in range(y - 21, y - 10):
        for xx in range(sx - 4 + (yy % 3 == 0), sx + 6 - (yy % 2)):
            img.putpixel((xx, yy), mt.SILHOUETTE)
    # crow silhouettes: the game's crow sprite (sprites.png cell 3 or 4), all near black but the beak
    sheet = Image.open(os.path.join(mt.ART, "sprites.png"))
    for (x0, y0, cell, flip) in ((6, 12, 3, False), (98, 8, 4, False), (104, 92, 3, False), (36, 100, 4, False)):
        for yy in range(16):
            for xx in range(16):
                i = sheet.getpixel((cell * 16 + xx, yy))
                if i:
                    px, py = x0 + (15 - xx if flip else xx), y0 + yy
                    if 0 <= px < NW and 0 <= py < NH:
                        img.putpixel((px, py), mt.BEAK if i == 11 else (16, 8, 12))
    for k, line in enumerate(("GROW. DEFEND.", "SURVIVE.")):   # the tagline on the dark ground
        _, tw = mt.small_mask(line)
        mt.small_text(img, line, CX - tw // 2, 150 + 11 * k)
    return img


def draw_logo():
    """The letters with the title's outline, drop, and sheen, and the crow perched on the D, on a clear background."""
    ink, w = letters_1x("CROWFIELD")
    img = Image.new("RGBA", (w + 4, 11 + 14), (0, 0, 0, 0))
    ox, oy = 1, 12
    mt.paint(img, mt.outline(ink) | {(x + 1, y + 1) for x, y in ink}, ox, oy, mt.EDGE + (255,))
    mt.paint(img, ink, ox, oy, mt.BLACK + (255,))
    mt.paint(img, {(x, y) for x, y in ink if (x, y - 1) not in ink}, ox, oy, mt.SHEEN + (255,))
    px0, py0 = ox + w - 14, oy - 10
    for r_, row in enumerate(mt.PERCHED):
        for c, v in enumerate(row):
            if v != ".":
                img.putpixel((px0 + c, py0 + r_), (mt.BLACK if v == "#" else mt.BEAK) + (255,))
    img.putpixel((px0 + 9, py0 + 1), mt.EYE + (255,))
    return img, oy


def main():
    art = draw_art().resize((NW * 2, NH * 2), Image.NEAREST)
    cover = Image.new("RGB", (W, H), (255, 255, 255))
    cover.paste(art, (ART_X, 0))
    logo, base = draw_logo()
    big = logo.resize((logo.width * LOGO_SCALE, logo.height * LOGO_SCALE), Image.NEAREST)
    mx, my = ART_X + MOON_C[0] * 2, MOON_C[1] * 2           # the moon's middle on the cover
    cover.paste(big, (mx - big.width // 2, my - (base + 5) * LOGO_SCALE - 1), big)   # letters centered on it
    for y in range(H):   # white over the stripe, cut to its curved edge, with a one-pixel soft step
        e = EDGE_A * y * y + EDGE_B * y + EDGE_C
        for x in range(0, int(e) + 2):
            if x <= int(e):
                cover.putpixel((x, y), (255, 255, 255))
            else:   # the edge pixel: part white, part art
                k = e - int(e)
                r, g, b = cover.getpixel((x, y))
                cover.putpixel((x, y), (round(255 * k + r * (1 - k)), round(255 * k + g * (1 - k)), round(255 * k + b * (1 - k))))
    path = os.path.join(mt.ART, "box_cover.png")
    cover.save(path)
    print("wrote", os.path.relpath(path, os.path.dirname(mt.ART)))


if __name__ == "__main__":
    main()
