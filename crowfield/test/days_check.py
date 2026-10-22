#!/usr/bin/env python3
"""Check the v0.5 day table (docs/V0.5_PLAN.md) in the game: for each test day, skip to that day's dusk,
start the day, read today's crow queue, and check the total and the mix of levels against the plan's ranges.
Run from crowfield/test: python3 days_check.py   (writes and runs .days.txt)"""
import os, re, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
DAYS = [1, 5, 9, 10, 11, 15, 19, 20, 24, 25, 28, 30, 33, 34, 35, 36, 37, 38, 39, 40, 41, 45, 50, 60]
ROUNDS = 2   # each day twice, for two random rolls


def plan(d):
    """(total range, {level: range}) from the plan; level 1 black, 2 red, 3 purple, 4 gold."""
    if d >= 40:
        lo = min(22, 12 + (d - 40) // 2)
        hi = min(24, lo + 2)
        return (lo, hi), {4: (lo, hi), 1: (0, 0), 2: (0, 0), 3: (0, 0)}
    lo = 2 + d // 2 + (2 if d >= 35 else 0)      # v0.5.1: a few more from Day 35
    hi = min(30, lo + (2 if d < 10 else 3 if d < 30 else 4))
    p = (1 + (d - 20) // 3, 1 + (d - 20) // 3 + (d >= 25)) if d >= 20 else (0, 0)
    g = (1 + (d - 30) // 3, 1 + (d - 30) // 3 + (d >= 33)) if d >= 30 else (0, 0)
    if d < 10:
        return (lo, hi), {1: (lo, hi), 2: (0, 0), 3: p, 4: g}
    if d < 25:
        r = (1 + (d - 10) // 2, 2 + (d - 10) // 2 if d > 10 else 1)
        return (lo, hi), {1: (max(0, lo - r[1] - p[1]), hi - r[0] - p[0]), 2: r, 3: p, 4: g}
    b = (lo // 5, hi // 3)
    return (lo, hi), {1: b, 2: (0, hi - b[0] - p[0] - g[0]), 3: p, 4: g}


def script():
    lines = ["1 0", "1 0 !$spawning=1,$blood_next=9999"]   # no blood moons here: blood.txt tests those
    for _ in range(ROUNDS):
        for d in DAYS:
            # end the current day: no crows to come or alive, dusk in 2 frames; then NEXT DAY into day d
            lines += ["1 0 !$day_left=0,$crow_state=0,$crow_state+4=0,$crow_state+8=0,$crow_state+12=0",
                      "1 0 !$day_done=2,$day=%d" % (d - 1), "6 0",
                      "1 128", "1 0", "1 128", "1 0", "1 128", "1 0", "1 1", "1 0",
                      "1 0 @$day,$day_left,$shop_open",
                      "1 0 @" + ",".join("$day_queue+%d" % (4 * k) for k in range(10)),
                      "1 0 @" + ",".join("$day_queue+%d" % (4 * k) for k in range(10, 20)),
                      "1 0 @" + ",".join("$day_queue+%d" % (4 * k) for k in range(20, 30)),
                      "1 0 !$day_left=0"]   # stop it spawning while we read the next day
    return "\n".join(lines) + "\n"


def main():
    with open(os.path.join(HERE, ".days.txt"), "w") as f:
        f.write(script())
    out = subprocess.run(["sh", "run.sh", ".days.txt"], cwd=HERE, capture_output=True, text=True).stdout
    rows = [[int(v) for v in re.findall(r"=(-?\d+)", line)] for line in out.splitlines() if "=" in line]
    bad = 0
    for i in range(0, len(rows), 4):
        day, left, shop = rows[i]
        queue = (rows[i + 1] + rows[i + 2] + rows[i + 3])[:left]
        (lo, hi), mix = plan(day)
        count = {lv: queue.count(lv) for lv in (1, 2, 3, 4)}
        ok = lo <= left <= hi and shop == 0 and all(mix[lv][0] <= count[lv] <= mix[lv][1] for lv in mix)
        bad += not ok
        print("day %2d: %2d crows (plan %d-%d)  black %d red %d purple %d gold %d  %s" % (
            day, left, lo, hi, count[1], count[2], count[3], count[4], "ok" if ok else "OUT OF PLAN"))
    print("%d days checked, %d out of plan" % (len(rows) // 4, bad))
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
