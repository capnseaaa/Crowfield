#!/usr/bin/env python3
"""Synthesize the clips no pack had: sound/whoosh.wav (sword swing) and sound/poof.wav (crow death).
Original sounds made by this script (CC0). Mono 16-bit 22050 Hz, like tools/import_sound.sh.
Fixed random seed, so the output is the same every run. Run once after editing; the build reads the WAVs.
"""
import math, os, random, struct, wave

RATE = 22050
ROOT = os.path.join(os.path.dirname(__file__), "..")


class SVF:   # state-variable filter: band and low outputs, cutoff set per sample
    def __init__(self, q):
        self.low = self.band = 0.0
        self.damp = 1.0 / q

    def run(self, x, fc):
        f = 2 * math.sin(math.pi * min(fc, RATE * 0.45) / RATE)
        self.low += f * self.band
        high = x - self.low - self.damp * self.band
        self.band += f * high
        return self.band, self.low


def whoosh():
    """Air rushing past: band-passed noise, pitch sweeping up then down, a smooth swell."""
    rnd, svf, n = random.Random(7), SVF(1.3), int(RATE * 0.24)
    out = []
    for i in range(n):
        t = i / n
        fc = 450 + 2300 * math.sin(math.pi * min(t / 0.85, 1.0)) ** 1.5
        env = math.sin(math.pi * t ** 0.8) ** 2
        band, _ = svf.run(rnd.uniform(-1, 1), fc)
        out.append(band * env)
    return out


def poof():
    """A soft cartoon poof: a noise burst that closes down fast, a low thump, then two bright sparkle notes."""
    rnd, svf, n = random.Random(11), SVF(0.8), int(RATE * 0.34)
    out = []
    for i in range(n):
        t = i / RATE
        fc = 300 + 3200 * math.exp(-t / 0.05)
        env = min(1.0, t / 0.004) * math.exp(-t / 0.075)
        _, low = svf.run(rnd.uniform(-1, 1), fc)
        v = low * env * 2.2
        if t < 0.08:   # thump: 170 Hz falling to 70 Hz
            v += 0.5 * math.sin(2 * math.pi * (170 * t - 625 * t * t)) * math.exp(-t / 0.025)
        for start, hz in ((0.07, 1568.0), (0.13, 2093.0)):   # sparkle: G6, then C7
            if t >= start:
                u = t - start
                v += 0.4 * math.sin(2 * math.pi * hz * u) * math.exp(-u / 0.06) * min(1.0, u / 0.003)
        out.append(v)
    return out


def save(name, x):
    top = max(abs(v) for v in x) or 1.0
    path = os.path.join(ROOT, "sound", name + ".wav")
    with wave.open(path, "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(RATE)
        w.writeframes(struct.pack("<%dh" % len(x), *(int(v / top * 30000) for v in x)))
    print("sound/%s.wav (%.2f s, synthesized)" % (name, len(x) / RATE))


if __name__ == "__main__":
    save("whoosh", whoosh())
    save("poof", poof())
