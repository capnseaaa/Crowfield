"""Convert art/*.png and the starting farm into src/data.h."""
import os
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ART = os.path.join(ROOT, "art")
OUT = os.path.join(ROOT, "src", "data.h")

# Metatile indices (must match make_art.py META and main.c enum)
(GRASS, GRASS_TUFT, DIRT, SOIL, SPROUT, CROP, CROP_RIPE, FENCE, BUSH,
 FENCE_V, FENCE_TL, FENCE_TR, FENCE_BL, FENCE_BR, CROP_YOUNG, SCARECROW, SCARECROW_OFF,
 CARROT_GROWN, CARROT_RIPE, TOMATO_GROWN, TOMATO_RIPE, PUMPKIN_GROWN, PUMPKIN_RIPE) = range(23)

# The whole farm is one screen: 15 x 10 metatiles of 16x16 = 240 x 160.
MAP_W, MAP_H = 15, 10
PLAYER_START = (7, 8)

FARM = [
    "###############",
    "#.,...........#",
    "#..FFFFFFFFF..#",
    "#..FsscccsrF..#",
    "#..FcccssccF.,#",
    "#..FsscccscF..#",
    "#,.FFFF_FFFF..#",
    "#.....___.....#",
    "#......_......#",
    "###############",
]
LEGEND = {".": GRASS, ",": GRASS_TUFT, "_": DIRT, "s": SOIL, "c": SPROUT, "r": CROP_RIPE, "F": FENCE, "#": BUSH}


# ---- 5x7 font ----
FONT = {
    "+": "00000 00100 00100 11111 00100 00100 00000",
    "/": "00001 00010 00010 00100 01000 01000 10000",
    "-": "00000 00000 00000 11111 00000 00000 00000",
    ",": "00000 00000 00000 00000 00110 00100 01000",
    "A": "01110 10001 10001 11111 10001 10001 10001",
    "B": "11110 10001 10001 11110 10001 10001 11110",
    "C": "01110 10001 10000 10000 10000 10001 01110",
    "D": "11110 10001 10001 10001 10001 10001 11110",
    "E": "11111 10000 10000 11110 10000 10000 11111",
    "F": "11111 10000 10000 11110 10000 10000 10000",
    "G": "01110 10001 10000 10111 10001 10001 01111",
    "H": "10001 10001 10001 11111 10001 10001 10001",
    "I": "01110 00100 00100 00100 00100 00100 01110",
    "J": "00111 00010 00010 00010 00010 10010 01100",
    "K": "10001 10010 10100 11000 10100 10010 10001",
    "L": "10000 10000 10000 10000 10000 10000 11111",
    "M": "10001 11011 10101 10101 10001 10001 10001",
    "N": "10001 10001 11001 10101 10011 10001 10001",
    "O": "01110 10001 10001 10001 10001 10001 01110",
    "P": "11110 10001 10001 11110 10000 10000 10000",
    "Q": "01110 10001 10001 10001 10101 10010 01101",
    "R": "11110 10001 10001 11110 10100 10010 10001",
    "S": "01111 10000 10000 01110 00001 00001 11110",
    "T": "11111 00100 00100 00100 00100 00100 00100",
    "U": "10001 10001 10001 10001 10001 10001 01110",
    "V": "10001 10001 10001 10001 10001 01010 00100",
    "W": "10001 10001 10001 10101 10101 10101 01010",
    "X": "10001 10001 01010 00100 01010 10001 10001",
    "Y": "10001 10001 01010 00100 00100 00100 00100",
    "Z": "11111 00001 00010 00100 01000 10000 11111",
    "0": "01110 10001 10011 10101 11001 10001 01110",
    "1": "00100 01100 00100 00100 00100 00100 01110",
    "2": "01110 10001 00001 00010 00100 01000 11111",
    "3": "11111 00010 00100 00010 00001 10001 01110",
    "4": "00010 00110 01010 10010 11111 00010 00010",
    "5": "11111 10000 11110 00001 00001 10001 01110",
    "6": "00110 01000 10000 11110 10001 10001 01110",
    "7": "11111 00001 00010 00100 01000 01000 01000",
    "8": "01110 10001 10001 01110 10001 10001 01110",
    "9": "01110 10001 10001 01111 00001 00010 01100",
    "%": "11001 11010 00010 00100 01000 01011 10011",
    ":": "00000 01100 01100 00000 01100 01100 00000",
    "!": "00100 00100 00100 00100 00100 00000 00100",
    ".": "00000 00000 00000 00000 00000 01100 01100",
    "?": "01110 10001 00001 00010 00100 00000 00100",
    "'": "00100 00100 01000 00000 00000 00000 00000",
    " ": "00000 00000 00000 00000 00000 00000 00000",
}
# Icons: "*" heart, "$" coin, "&" crop, "^" seed, ">" menu cursor, "#" scarecrow.
FONT["*"] = "00000 01010 11111 11111 01110 00100 00000"
FONT["$"] = "00000 01110 11111 11011 11111 01110 00000"
FONT["&"] = "00100 01110 00100 10101 01110 00100 00100"
FONT["^"] = "00000 00100 01110 11111 11111 01110 00000"
FONT[">"] = "01000 00100 00010 00001 00010 00100 01000"   # menu cursor
FONT["<"] = "00010 00100 01000 10000 01000 00100 00010"   # dev mode: the value being changed
FONT["="] = "00000 00000 11111 00000 11111 00000 00000"
FONT["#"] = "01110 00100 11111 00100 00100 00100 01110"   # scarecrow icon
# Seed icons, one per crop (v0.5.1; Caleb could not tell which seed was chosen). "2" pixels are leaf green.
# "^" corn stays the gold kernel. "{" carrot, "}" tomato, "|" pumpkin.
FONT["{"] = "20202 02220 11111 01110 01110 00100 00100"   # carrot: green top, orange point
FONT["}"] = "00200 02220 11111 11111 11111 01110 00000"   # tomato: red, green stem
FONT["|"] = "00020 00200 01110 11111 11011 11111 01110"   # pumpkin: wide orange, stem
GLYPH_COLOR = {"*": 3, "$": 4, "&": 5, "^": 4, ">": 4, "#": 4, "{": 6, "}": 3, "|": 6, "<": 4}   # 6 orange   # text palette: 1 cream, 3 red, 4 gold, 5 green
FONT_CHARS = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789%:!.?'+/-,*$&^>=#{}|<"


def font_words(underline=False, low=False):
    """Tile 0 is transparent. Glyph tiles: color 2 background, text in color 1 (or GLYPH_COLOR); "2" pixels green.
    underline (v1.0): the same glyphs (without the empty tile 0) with a thin gold line (color 4) on the bottom row,
    for the equipped weapon's name. low (v1.0): the glyphs one pixel lower, so a line drawn under an underlined
    one keeps a gap."""
    words = [] if underline or low else [0] * 8
    for ch in FONT_CHARS:
        rows = FONT[ch].split()
        ink = GLYPH_COLOR.get(ch, 1)
        for y in range(8):
            w = 0
            gy = y - 1 if low else y   # the glyph row drawn on this pixel row
            for x in range(8):
                c = 2
                if 1 <= x <= 5 and 0 <= gy < 7 and rows[gy][x - 1] == "1":
                    c = ink
                elif 1 <= x <= 5 and 0 <= gy < 7 and rows[gy][x - 1] == "2":
                    c = 5   # leaf green (text palette color 5)
                if underline and y == 7:
                    c = 4   # coin gold
                w |= c << (4 * x)
            words.append(w)
    return words


# ---- title screen (v0.9.2): art/title_bg.png (BG2), art/title_logo.png and art/title_words.png (BG0) ----
def tile_set(images):
    """Deduplicated 8x8 tiles over the images (tile 0 = all clear), and each image's map of tile numbers."""
    tiles, index, maps = [], {}, []
    blank = tuple([0] * 64)
    if images[0][1]:   # transparent layers: tile 0 is the empty tile
        tiles.append(blank); index[blank] = 0
    for img, _ in images:
        px = img.load()
        m = []
        for ty in range(img.height // 8):
            for tx in range(img.width // 8):
                t = tuple(px[tx * 8 + x, ty * 8 + y] for y in range(8) for x in range(8))
                if t not in index:
                    index[t] = len(tiles); tiles.append(t)
                m.append(index[t])
        maps.append(m)
    words = []
    for t in tiles:
        for y in range(8):
            v = 0
            for x in range(8):
                v |= t[y * 8 + x] << (4 * x)
            words.append(v)
    return words, maps


def title_data(fg_base):
    bg = Image.open(os.path.join(ART, "title_bg.png"))
    logo = Image.open(os.path.join(ART, "title_logo.png"))
    words = Image.open(os.path.join(ART, "title_words.png"))
    bg_tiles, (bg_map,) = tile_set([(bg, False)])
    fg_tiles, (logo_map, word_map) = tile_set([(logo, True), (words, True)])
    assert len(bg_tiles) // 8 <= 512 and fg_base + len(fg_tiles) // 8 <= 512
    return [
        c_array("title_bg_pal", "unsigned short", palette(bg)),
        c_array("title_fg_pal", "unsigned short", palette(logo)),
        c_array("title_bg_tiles", "unsigned int", bg_tiles),
        c_array("title_fg_tiles", "unsigned int", fg_tiles),
        "#define TITLE_FG_BASE %d   /* first title tile in charblock 1, after the font */" % fg_base,
        c_array("title_bg_map", "unsigned short", bg_map, 30),
        c_array("title_logo_map", "unsigned short", logo_map, 30),
        c_array("title_word_map", "unsigned short", word_map, 30),   # 5 words, 2 rows of 30 each
    ]


def bgr555(rgb):
    r, g, b = (v >> 3 for v in rgb[:3])
    return r | (g << 5) | (b << 10)


def palette(img):
    raw = img.getpalette()[:48]
    return [bgr555(raw[i:i + 3]) for i in range(0, 48, 3)]


def tile_words(img, x0, y0):
    """One 8x8 4bpp tile as 8 u32 words."""
    words = []
    for y in range(8):
        w = 0
        for x in range(8):
            w |= (img.getpixel((x0 + x, y0 + y)) & 15) << (4 * x)
        words.append(w)
    return words


def metatiles(img):
    """Each 16x16 cell becomes 4 tiles in order TL, TR, BL, BR."""
    words = []
    for cx in range(img.width // 16):
        for k in range(4):
            words += tile_words(img, cx * 16 + (k & 1) * 8, (k >> 1) * 8)
    return words


def c_array(name, ctype, values, per_line=8):
    lines = [f"static const {ctype} {name}[{len(values)}] = {{"]
    for i in range(0, len(values), per_line):
        lines.append("    " + ", ".join(f"0x{v:X}" for v in values[i:i + per_line]) + ",")
    lines.append("};")
    return "\n".join(lines)


def main():
    terrain = Image.open(os.path.join(ART, "terrain.png"))
    sprites = Image.open(os.path.join(ART, "sprites.png"))
    farm = [LEGEND[c] for row in FARM for c in row]
    assert len(farm) == MAP_W * MAP_H
    out = [
        "// Generated by tools/build_data.py. Do not edit.",
        "#pragma once",
        f"#define MAP_W {MAP_W}",
        f"#define MAP_H {MAP_H}",
        f"#define PLAYER_START_X {PLAYER_START[0]}",
        f"#define PLAYER_START_Y {PLAYER_START[1]}",
        c_array("bg_pal", "unsigned short", palette(terrain)),
        c_array("obj_pal", "unsigned short", palette(sprites)),
        c_array("terrain_tiles", "unsigned int", metatiles(terrain)),
        c_array("sprite_tiles", "unsigned int", metatiles(sprites)),
        c_array("farm_start", "unsigned char", farm, 15),
        c_array("font_tiles", "unsigned int", font_words()),
        "static const char FONT_CHARS[] = \"%s\";" % FONT_CHARS.replace("\\", "\\\\"),
    ]
    out += title_data(len(font_words()) // 8)
    out.append(c_array("font_ul_tiles", "unsigned int", font_words(underline=True)))
    out.append(c_array("font_low_tiles", "unsigned int", font_words(low=True)))
    with open(OUT, "w") as f:
        f.write("\n".join(out) + "\n")
    print("wrote", os.path.relpath(OUT, ROOT))


if __name__ == "__main__":
    main()
