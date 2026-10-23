"""menu_check.py: every window's text stays inside its box, clear of the edge (Caleb: text must never leave the
box or touch its edge).

It visits every window state (each shop page with the cursor on each line, the dawn, last-shop and blessing
pages, the end screens, dev mode with the cursor on each line, the stats page), with the biggest numbers a
line can show (99999 coins, 999 of each crop and seed, 65535 crops and crows in a run). For each state it
reads the text map (BG0, screenblock 26) straight from video memory and checks the window rows 2-17: the box
is columns 2-27; columns 2 and 27 must hold only blank box cells, and nothing may be drawn outside them.
Run from crowfield/test: python3 menu_check.py
"""
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SB = 0x0600D000          # text screenblock 26
ROWS = range(2, 18)      # the window: SHOP_T 2, SHOP_ROWS 16
BOX_L, BOX_R = 2, 27   # the window since v0.9.1 (it was 3-26)
A, B, UP, DOWN, LEFT, RIGHT, L, R = 1, 2, 64, 128, 32, 16, 512, 256
SPACE = 1                # font tile 1 is the space glyph (box background)

checks = []              # labels, in the order their peeks appear


def press(L_, k, n=1):
    for _ in range(n):
        L_ += ["1 %d" % k, "1 0"]


def check(L_, label):
    checks.append(label)
    for r in ROWS:
        L_.append("1 0 @" + ",".join("%08X" % (SB + r * 64 + 4 * w) for w in range(16)))


def script():
    L_ = ["0 0 !0203FFF0=1413829460", "1 0 !$spawning=0", "5 0"]
    big = "$coins=99999,$crops=999,$crops+4=999,$crops+8=999,$crops+12=999,$seeds=999,$seeds+4=999,$seeds+8=999,$seeds+12=999,$scarecrow_bag=8"
    L_ += ["1 0 !$day=99," + big, "1 0 !$spawning=1,$day_left=0,$crow_state=0", "1 0 !$day_done=2", "6 0"]
    # main page: each cursor line
    L_ += ["1 0 !$shop_cursor=0"]
    press(L_, DOWN); press(L_, UP)
    for c in range(5):
        check(L_, "main, cursor %d, day 99" % c)
        press(L_, DOWN)
    # each page, each line
    for page, items, name in ((0, 5, "SELL"), (1, 4, "SEEDS"), (2, 5, "WEAPONS"), (3, 5, "FARM")):
        L_ += ["1 0 !$shop_page=0,$shop_cursor=%d" % page]
        press(L_, A)
        for c in range(items):
            check(L_, "%s, cursor %d" % (name, c))
            press(L_, DOWN)
        press(L_, B)
    # weapons with every weapon in hand at Lv.3, and the others owned before (their "LV" info)
    for w in range(5):
        L_ += ["1 0 !$weapon=%d,$sword_lv=3,$sword_damage=99,$weapon_lv=3,$weapon_lv+4=3,$weapon_lv+8=3,$weapon_lv+12=3,$weapon_lv+16=3" % w,
               "1 0 !$shop_page=0,$shop_cursor=2"]
        press(L_, A)
        for c in range(5):
            check(L_, "WEAPONS, %d in hand, cursor %d" % (w, c))
            press(L_, DOWN)
        press(L_, B)
    # farm, everything at max
    L_ += ["1 0 !$sickle_lv=3,$scarecrow_lv=3,$cannons=2,$farm_step=3,$shop_page=0,$shop_cursor=3"]
    press(L_, A)
    for c in range(5):
        check(L_, "FARM maxed, cursor %d" % c)
        press(L_, DOWN)
    press(L_, B)
    # locked seeds (day 1)
    L_ += ["1 0 !$day=1,$shop_page=0,$shop_cursor=1"]
    press(L_, A)
    for c in range(4):
        check(L_, "SEEDS locked, cursor %d" % c)
        press(L_, DOWN)
    press(L_, B)
    # the dawn and last-shop titles, day 999
    for flag in ("$shop_dawn=1", "$shop_dawn=0,$shop_last=1", "$shop_last=0,$day=999"):
        L_ += ["1 0 !" + flag]
        press(L_, DOWN); press(L_, UP)
        check(L_, "main title (%s)" % flag)
    # the blessing page, with 5 hearts too
    for mh in (3, 5):
        L_ += ["1 0 !$max_hearts=%d,$shop_page=5,$shop_cursor=0" % mh]
        press(L_, DOWN); press(L_, UP)
        for c in range(3):
            check(L_, "BLESSING, max hearts %d, cursor %d" % (mh, c))
            press(L_, DOWN)
    # dev mode: cursor on each line, every value at its widest
    L_ += ["1 0 !$shop_page=0"]
    press(L_, B)
    L_ += ["1 %d" % (L | R | B), "1 0"]
    L_ += ["1 0 !$dev_val=99,$dev_val+4=3,$dev_val+8=3,$dev_val+12=3,$dev_val+16=3,$dev_val+20=8,$dev_val+24=2,$dev_val+28=3,$dev_val+32=99,$dev_val+36=5,$dev_val+40=2,$dev_val+44=1,$dev_val+48=1"]
    press(L_, DOWN); press(L_, UP)
    for c in range(13):
        check(L_, "DEV, cursor %d" % c)
        press(L_, DOWN)
    for w in range(5):   # each weapon name
        L_ += ["1 0 !$dev_val+4=%d" % w]
        press(L_, UP); press(L_, DOWN)
        check(L_, "DEV, weapon %d" % w)
    press(L_, B)
    # end screens with the biggest numbers
    L_ += ["1 0 !$shop_open=0,$day=999,$stat_crops=65535,$stat_kills=65535,$dev_run=1"]
    L_ += ["1 0 !$spawning=1,$finale=3,$blood_on=1,$blood_timer=2", "5 0"]
    check(L_, "end: YOU WON")
    L_ += ["1 0 !$end_kind=1"]
    L_ += ["1 %d" % (L | R | B), "1 0"]
    press(L_, B)
    check(L_, "end: THE BLOOD MOON WON (redrawn)")
    L_ += ["1 0 !$end_kind=2"]
    L_ += ["1 %d" % (L | R | B), "1 0"]
    press(L_, B)
    check(L_, "end: THE LAST MOON WON (redrawn)")
    L_ += ["1 0 !$end_kind=0"]
    L_ += ["1 %d" % (L | R | B), "1 0"]
    press(L_, B)
    check(L_, "end: THE FARM IS LOST (redrawn)")
    # the stats page with the biggest numbers
    press(L_, 8)   # to the title
    w_a, w_b = 999 | 65535 << 16, 65535 | 3 << 16
    hist = ",".join("$hist_a+%d=%d,$hist_b+%d=%d" % (4 * i, w_a, 4 * i, 65535 | (i % 4) << 16) for i in range(5))
    L_ += ["1 0 !$runs_played=99999,$best_a=%d,$best_b=%d," % (w_a, w_b) + hist]
    press(L_, DOWN)
    press(L_, A)
    check(L_, "STATS, biggest numbers")
    return "\n".join(L_) + "\n"


def main():
    path = os.path.join(HERE, ".menus.txt")
    with open(path, "w") as f:
        f.write(script())
    out = subprocess.run(["sh", os.path.join(HERE, "run.sh"), path], capture_output=True, text=True, cwd=HERE).stdout
    rows = [ln for ln in out.splitlines() if ln.startswith("[")]
    assert len(rows) == len(checks) * len(ROWS), "%d peek lines for %d checks" % (len(rows), len(checks))
    bad = 0
    for k, label in enumerate(checks):
        problems = []
        for i, r in enumerate(ROWS):
            words = [int(p.split("=")[1]) & 0xFFFFFFFF for p in rows[k * len(ROWS) + i].split()]
            cells = []
            for w in words:
                cells += [w & 0x3FF, (w >> 16) & 0x3FF]
            text = "".join("." if t == 0 else " " if t == SPACE else "#" for t in cells[:30])
            if cells[BOX_L] not in (SPACE,) or cells[BOX_R] not in (SPACE,):
                if any(cells[c] for c in range(BOX_L, BOX_R + 1)):   # a window row
                    problems.append("row %2d touches the edge: |%s|" % (r, text))
            if any(cells[c] for c in list(range(0, BOX_L)) + list(range(BOX_R + 1, 30))):
                problems.append("row %2d outside the box:  |%s|" % (r, text))
        if problems:
            bad += 1
            print("FAIL", label)
            for p in problems:
                print("    " + p)
    print("%d states checked, %d with text on or past the edge" % (len(checks), bad))
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
