"""final3_check.py: the 3rd final night (v0.9.1, Caleb): no purple or lower crows all night, and in its last
10 s "LAST TEN SECONDS" shows and only white crows (Lv.6) come. Every 10 frames it reads every crow slot's
level, then clears the slots so new crows keep coming. Run from crowfield/test: python3 final3_check.py
"""
import os
import subprocess

HERE = os.path.dirname(os.path.abspath(__file__))
STEP = 10   # frames between reads; each read takes 2 frames (the two clear lines)
L = ["0 0 !0203FFF0=1413829460", "1 0 !$spawning=0", "5 0",
     "1 0 !$dev_god=1,$finale=2,$blood_on=3,$blood_timer=2,$spawning=1", "3 0"]
clear = ["$crow_state+%d=0" % (4 * i) for i in range(24)]
for t in range(0, 2700 // STEP + 2):
    L.append("%d 0 @$blood_timer,$finale,$blood_on," % (STEP - 2) + ",".join("$crow_state+%d" % (4 * i) for i in range(12)))
    L.append("0 0 @" + ",".join("$crow_state+%d" % (4 * i) for i in range(12, 24)))   # a line takes ~20 addresses
    L.append("0 0 @" + ",".join("$crow_lv+%d" % (4 * i) for i in range(12)))
    L.append("0 0 @" + ",".join("$crow_lv+%d" % (4 * i) for i in range(12, 24)))
    L.append("1 0 !" + ",".join(clear[:12]))
    L.append("0 0 !" + ",".join(clear[12:]))
    if t == (2700 - 600) // STEP + 1:
        L.append("0 0 last_ten.ppm")
path = os.path.join(HERE, ".final3.txt")
open(path, "w").write("\n".join(L) + "\n")
out = subprocess.run(["sh", os.path.join(HERE, "run.sh"), path], capture_output=True, text=True, cwd=HERE).stdout
rows = [[int(p.split("=")[1]) for p in ln.split()] for ln in out.splitlines() if ln.startswith("[")]
seen = {}
last_ten = {}
for k in range(0, len(rows) - 3, 4):
    st, lv = rows[k] + rows[k + 1], rows[k + 2] + rows[k + 3]
    timer, fin, on = st[0], st[1], st[2]
    if fin != 3 or on != 1:
        continue
    for i in range(24):
        if st[3 + i] not in (0,):   # a crow came since the last clear
            seen[lv[i]] = seen.get(lv[i], 0) + 1
            if timer <= 600 - STEP:
                last_ten[lv[i]] = last_ten.get(lv[i], 0) + 1
print("night 3 crows by level:", dict(sorted(seen.items())), " in the last 10 s:", dict(sorted(last_ten.items())))
ok = all(lv >= 4 for lv in seen) and set(last_ten) <= {6} and last_ten
print("ok" if ok else "FAIL")
