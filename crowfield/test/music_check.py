"""music_check.py rec.wav: check a recording of test/music.txt against the songs in tools/make_music.py.

For each of the five songs it finds where the song starts (near its place in music.txt), then measures, at the
start of each melody note in the first 6 s, the strength of the note's own pitch against the pitches a
semitone above and below (Goertzel). A note counts as heard when its own pitch is the strongest.
Run from crowfield/: HARNESS_WAV=/abs/rec.wav sh test/run.sh test/music.txt && python3 test/music_check.py /abs/rec.wav
"""
import math
import os
import struct
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tools"))
import make_music as mm  # noqa: E402

RATE = 32768
SPF = RATE / 59.7275          # samples per GBA frame
STARTS = [1, 363, 725, 1087, 1449, 1811, 2173, 2535]   # nominal first frame of each song in music.txt


def load(path):
    data = open(path, "rb").read()[44:]
    n = len(data) // 4
    vals = struct.unpack("<%dh" % (2 * n), data[:4 * n])
    return [(vals[2 * i] + vals[2 * i + 1]) / 2 for i in range(n)]


def power(x, a, n, f):
    w = 2 * math.pi * f / RATE
    c = 2 * math.cos(w)
    s1 = s2 = 0.0
    for i in range(a, min(a + n, len(x))):
        s1, s2 = x[i] + c * s1 - s2, s1
    return s1 * s1 + s2 * s2 - c * s1 * s2


def score(x, start_frame, name):
    song = mm.SONGS[name]
    notes = mm.parse(song["melody"])
    t = 0
    heard = total = 0
    for f, steps in notes:
        at = start_frame + t * song["frames_per_step"]
        t += steps
        if at - start_frame > 360 - 8:
            break
        if f == 0:
            continue
        a = int((at + 1) * SPF)
        n = int(min(steps * song["frames_per_step"] - 2, 5) * SPF)
        p = power(x, a, n, f)
        total += 1
        if p > power(x, a, n, f * 2 ** (1 / 12)) and p > power(x, a, n, f * 2 ** (-1 / 12)):
            heard += 1
    return heard, total


def main():
    x = load(sys.argv[1])
    ok = True
    for name, s0 in zip(mm.SONG_ORDER, STARTS):
        best = max(((score(x, s0 + d, name), d) for d in range(-12, 13, 1)), key=lambda r: r[0][0])
        (heard, total), d = best
        print("%-6s %2d of %2d melody notes heard (start %+d frames)" % (name, heard, total, d))
        ok &= heard >= total * 0.8
    print("ok" if ok else "FAIL")


if __name__ == "__main__":
    main()
