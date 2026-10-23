"""Crowfield's music (v0.9): songs written as notes, turned into src/music.h, plus WAV previews.

Songs: the title, a normal day, the dusk shop, a blood moon, the final nights, and (v0.9.1) the last shop and
the work day before the final nights. Caleb picks each tune.
Each song has two voices that loop together:
  melody -> GBA sound channel 2 (square wave, plucked envelope)
  bass   -> GBA sound channel 3 (4-bit wave, a round tone)
The sound clips keep DirectSound A, so music never cuts a sound effect.
Notes are "NAME OCTAVE LENGTH": "E5 2" is E in octave 5 for 2 steps; "r 4" is a rest of 4 steps.
A step is one eighth note; frames_per_step sets the tempo (60 frames a second). Bars are 8 steps; "|" is
only there to read them and is ignored.

Run: python3 tools/make_music.py           (writes src/music.h; build.sh runs it)
     python3 tools/make_music.py --wav DIR (also writes one preview .wav per song into DIR)
"""
import math
import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "src", "music.h")

SONGS = {
    # The title (v0.9.1, Caleb's pick): "Harvest Waltz", a slow waltz in F, swaying like a barn dance.
    'title': {
        "frames_per_step": 12, "env": (9, 6), "bar": 6,
        "melody": """
            C5 4 A4 2 | F5 4 E5 2 | D5 2 E5 2 F5 2 | C5 6 |
            A4 4 C5 2 | A#4 4 G4 2 | A4 2 G4 2 E4 2 | F4 6 |
            C5 4 A4 2 | F5 4 G5 2 | A5 2 G5 2 F5 2 | D5 6 |
            C5 4 A4 2 | A#4 2 A4 2 G4 2 | E4 4 G4 2 | F4 6 |
        """,
        "bass": """
            F2 2 C3 4 | F2 2 C3 4 | A#2 2 F3 4 | F2 2 C3 4 |
            F2 2 C3 4 | C3 2 G2 4 | C3 2 G2 4 | F2 6 |
            F2 2 C3 4 | D3 2 A2 4 | A#2 2 F3 4 | D3 2 A2 4 |
            F2 2 C3 4 | A#2 2 F3 4 | C3 2 G2 4 | F2 6 |
        """,
    },
    # A normal day (v0.9.1, Caleb's pick): "Pitchfork Polka", a bouncy, brave G major polka.
    "day": {
        "frames_per_step": 9, "env": (10, 4),
        "melody": """
            D5 1 r 1 D5 1 E5 1 D5 2 B4 2 | C5 1 r 1 C5 1 D5 1 C5 2 A4 2 | B4 1 C5 1 D5 1 G5 1 F#5 2 E5 2 | D5 4 r 4 |
            G5 1 r 1 G5 1 A5 1 G5 2 E5 2 | F#5 1 r 1 F#5 1 G5 1 F#5 2 D5 2 | E5 1 F#5 1 G5 1 E5 1 D5 2 F#5 2 | G5 4 r 4 |
        """,
        "bass": """
            G2 2 D3 2 G2 2 D3 2 | D3 2 A2 2 D3 2 A2 2 | G2 2 D3 2 C3 2 A2 2 | D3 2 A2 2 D3 2 F#2 2 |
            G2 2 D3 2 G2 2 B2 2 | D3 2 A2 2 D3 2 C3 2 | C3 2 E3 2 D3 2 D2 2 | G2 2 D3 2 G2 4 |
        """,
    },
    # The dusk shop (v0.9.1, Caleb's pick): "Sunny Rows", a bright C major tune over long, still bass notes.
    'dusk': {
        "frames_per_step": 12, "env": (8, 7),
        "melody": """
            E5 2 G5 2 A5 2 G5 2 | E5 3 D5 1 C5 4 | D5 2 E5 2 G5 2 E5 2 | D5 8 |
            E5 2 G5 2 A5 2 C6 2 | A5 3 G5 1 E5 4 | D5 2 C5 2 D5 2 E5 2 | C5 8 |
        """,
        "bass": """
            C3 8 | A2 8 | F2 8 | G2 8 |
            C3 8 | A2 8 | G2 8 | C3 8 |
        """,
    },
    # A blood moon (v0.9.1, Caleb): the first final-nights tune, D minor over a pounding bass.
    'blood': {
        "frames_per_step": 8,
        "melody": """
            D5 1 D5 1 F5 1 A5 1 G#5 2 A5 2 | F5 1 E5 1 D5 2 C#5 4 | D5 1 F5 1 A5 1 D6 1 C#6 2 A5 2 | A#5 2 A5 2 G5 2 E5 2 |
            F5 2 E5 2 D5 2 C#5 2 | D5 2 E5 2 F5 2 G5 2 | A5 2 G5 1 F5 1 E5 2 C#5 2 | D5 6 r 2 |
        """,
        "bass": """
            D2 1 D3 1 D2 1 D3 1 D2 1 D3 1 D2 1 D3 1 | A2 1 A3 1 A2 1 A3 1 A2 1 A3 1 A2 1 A3 1 |
            D2 1 D3 1 D2 1 D3 1 D2 1 D3 1 D2 1 D3 1 | A#2 1 A#3 1 A#2 1 A#3 1 G2 1 G3 1 G2 1 G3 1 |
            A2 1 A3 1 A2 1 A3 1 A2 1 A3 1 A2 1 A3 1 | A#2 1 A#3 1 A#2 1 A#3 1 G2 1 G3 1 G2 1 G3 1 |
            A2 1 A3 1 A2 1 A3 1 A2 1 A3 1 A2 1 A3 1 | D2 1 D3 1 D2 1 D3 1 D2 1 D3 1 D2 2 |
        """,
    },
    # The final nights (v0.9.1, Caleb's pick): "The Flock Descends", Crow King's opening turned into a falling,
    # chasing A minor line; tense and faster than the rest.
    "final": {
        "frames_per_step": 7, "env": (12, 3),
        "melody": """
            E6 1 D6 1 C6 1 B5 1 A5 2 E5 2 | F5 1 E5 1 D5 1 C5 1 B4 4 | E6 1 D6 1 C6 1 B5 1 A5 2 C6 2 | B5 6 G#5 2 |
            A5 1 G#5 1 A5 1 B5 1 C6 2 A5 2 | D6 1 C6 1 B5 1 A5 1 G#5 2 E5 2 | F5 1 G#5 1 A5 1 B5 1 C6 1 B5 1 A5 1 G#5 1 | A5 4 E5 4 |
        """,
        "bass": """
            A2 2 A2 1 A2 1 A2 2 A2 2 | D3 2 D3 1 D3 1 E3 2 E3 2 | A2 2 A2 1 A2 1 A2 2 A2 2 | E2 2 E2 1 E2 1 E2 2 E2 2 |
            F2 2 F2 1 F2 1 F2 2 F2 2 | D3 2 D3 1 D3 1 E3 2 E3 2 | D3 2 D3 2 E3 2 E3 2 | A2 2 E2 2 A2 4 |
        """,
    },
    # The 3rd final night (Caleb): the same notes, grungier. A thin, buzzy melody (12.5% duty) doubled a little out
    # of tune on channel 1, a louder sawtooth bass, and a noise crunch on the beat (channel 4).
    "final3": {
        "frames_per_step": 7, "env": (12, 3), "duty": 0, "double": 3, "bass_wave": "saw", "bass_loud": True, "noise": "x.o.x.oo",
        "melody": """
            E6 1 D6 1 C6 1 B5 1 A5 2 E5 2 | F5 1 E5 1 D5 1 C5 1 B4 4 | E6 1 D6 1 C6 1 B5 1 A5 2 C6 2 | B5 6 G#5 2 |
            A5 1 G#5 1 A5 1 B5 1 C6 2 A5 2 | D6 1 C6 1 B5 1 A5 1 G#5 2 E5 2 | F5 1 G#5 1 A5 1 B5 1 C6 1 B5 1 A5 1 G#5 1 | A5 4 E5 4 |
        """,
        "bass": """
            A2 2 A2 1 A2 1 A2 2 A2 2 | D3 2 D3 1 D3 1 E3 2 E3 2 | A2 2 A2 1 A2 1 A2 2 A2 2 | E2 2 E2 1 E2 1 E2 2 E2 2 |
            F2 2 F2 1 F2 1 F2 2 F2 2 | D3 2 D3 1 D3 1 E3 2 E3 2 | D3 2 D3 2 E3 2 E3 2 | A2 2 E2 2 A2 4 |
        """,
    },
    # The last shop before the final nights (Caleb's pick): "Dawn Must Come", dark E minor lifting into G major.
    'last_shop': {
        "frames_per_step": 11, "env": (11, 7),
        "melody": """
            E5 4 F#5 2 G5 2 | B5 6 A5 2 | G5 2 F#5 2 E5 2 D5 2 | E5 8 |
            C6 4 B5 2 A5 2 | B5 4 G5 4 | A5 2 B5 2 C6 2 D6 2 | B5 8 |
        """,
        "bass": """
            E3 8 | G2 8 | C3 4 D3 4 | E3 8 |
            C3 8 | G2 8 | A2 4 D3 4 | G2 8 |
        """,
    },
    # The work day before the final nights (Caleb's pick): "Crow King", an A minor march with dotted steps.
    'last_day': {
        "frames_per_step": 9, "env": (11, 6),
        "melody": """
            A4 3 C5 1 E5 4 | D5 3 C5 1 B4 4 | C5 3 D5 1 E5 2 A5 2 | G#5 8 |
            A5 3 G5 1 F5 2 E5 2 | F5 3 E5 1 D5 2 C5 2 | B4 2 C5 2 D5 2 B4 2 | A4 8 |
        """,
        "bass": """
            A2 4 E3 4 | G2 4 D3 4 | F2 4 C3 4 | E2 4 B2 4 |
            F2 4 C3 4 | D3 4 A2 4 | E2 4 G#2 4 | A2 8 |
        """,
    },
}
SONG_ORDER = ["title", "day", "dusk", "blood", "final", "last_shop", "last_day", "final3"]
# Each song may also set (v0.9.1):
#   "env": (volume 0-15, fade step 0-7) for the melody; step 0 holds the note, 7 fades slowest
#   "bar": steps per bar (6 for a waltz)
#   "duty": the melody's square shape, 0 = 12.5% (thin, buzzy), 1 = 25%, 2 = 50% (round; the default)
#   "double": play the melody on channel 1 too, this many frequency-register steps sharp (a gritty chorus)
#   "bass_wave": "sine" (round; the default) or "saw" (rough); "bass_loud": the bass at full volume, not half
#   "noise": a pattern per step on channel 4, repeating: "x" a heavy crunch, "o" a light tick, "." nothing
DEFAULT_ENV = (9, 5)

NOTE_INDEX = {"C": 0, "C#": 1, "D": 2, "D#": 3, "E": 4, "F": 5, "F#": 6, "G": 7, "G#": 8, "A": 9, "A#": 10, "B": 11}


def parse(text):
    """-> list of (frequency or 0 for a rest, steps)"""
    tokens = text.replace("|", " ").split()
    out = []
    for name, steps in zip(tokens[0::2], tokens[1::2]):
        if name == "r":
            out.append((0.0, int(steps)))
            continue
        pitch, octave = name[:-1], int(name[-1])
        midi = 12 * (octave + 1) + NOTE_INDEX[pitch]
        out.append((440.0 * 2 ** ((midi - 69) / 12), int(steps)))
    return out


def square_reg(f):   # channel 1/2 frequency register: f = 131072 / (2048 - n)
    return 0 if f == 0 else max(0, min(2047, round(2048 - 131072 / f)))


def wave_reg(f):     # channel 3 with a 32-sample wave: f = 65536 / (2048 - n)
    return 0 if f == 0 else max(0, min(2047, round(2048 - 65536 / f)))


def check(name, song):
    m = sum(s for _, s in parse(song["melody"]))
    b = sum(s for _, s in parse(song["bass"]))
    assert m == b, f"{name}: melody is {m} steps, bass is {b}; they must loop together"
    bar = song.get("bar", 8)
    assert m % bar == 0, f"{name}: {m} steps is not whole bars of {bar}"


def write_header():
    lines = ["// Generated by tools/make_music.py. Do not edit by hand.", "#pragma once", ""]
    lines.append(f"#define NUM_SONGS {len(SONG_ORDER)}")
    for i, name in enumerate(SONG_ORDER):
        lines.append(f"#define SONG_{name.upper()} {i}")
    # A note is one u16 for the frequency register (0 = rest) and one u8 for its length in steps.
    for name in SONG_ORDER:
        song = SONGS[name]
        check(name, song)
        for voice, reg in (("melody", square_reg), ("bass", wave_reg)):
            notes = parse(song[voice])
            lines.append(f"static const unsigned short {name}_{voice}_freq[{len(notes)}] = {{ "
                         + ", ".join(str(reg(f)) for f, _ in notes) + " };")
            lines.append(f"static const unsigned char {name}_{voice}_len[{len(notes)}] = {{ "
                         + ", ".join(str(s) for _, s in notes) + " };")
    lines.append("static const unsigned short *const song_freq[NUM_SONGS][2] = {")
    lines += [f"    {{ {n}_melody_freq, {n}_bass_freq }}," for n in SONG_ORDER]
    lines.append("};")
    lines.append("static const unsigned char *const song_len[NUM_SONGS][2] = {")
    lines += [f"    {{ {n}_melody_len, {n}_bass_len }}," for n in SONG_ORDER]
    lines.append("};")
    lines.append("static const unsigned short song_count[NUM_SONGS][2] = {")
    lines += [f"    {{ {len(parse(SONGS[n]['melody']))}, {len(parse(SONGS[n]['bass']))} }}," for n in SONG_ORDER]
    lines.append("};")
    lines.append("static const unsigned short song_env[NUM_SONGS] = { "   # channel 2 envelope bits
                 + ", ".join(str((SONGS[n].get("env", DEFAULT_ENV)[0] << 12) | (SONGS[n].get("env", DEFAULT_ENV)[1] << 8))
                             for n in SONG_ORDER) + " };")
    lines.append("static const unsigned char song_duty[NUM_SONGS] = { "
                 + ", ".join(str(SONGS[n].get("duty", 2)) for n in SONG_ORDER) + " };")
    lines.append("static const unsigned char song_double[NUM_SONGS] = { "
                 + ", ".join(str(SONGS[n].get("double", 0)) for n in SONG_ORDER) + " };")
    lines.append("static const unsigned short song_bass[NUM_SONGS] = { "   # channel 3: bank (1 = saw) and volume bits
                 + ", ".join(str((0x40 if SONGS[n].get("bass_wave") == "saw" else 0) | (0x100 if SONGS[n].get("bass_loud") else 0))
                             for n in SONG_ORDER) + " };")
    lines.append("static const char *const song_noise[NUM_SONGS] = { "
                 + ", ".join('"%s"' % SONGS[n]["noise"] if SONGS[n].get("noise") else "0" for n in SONG_ORDER) + " };")
    lines.append("static const unsigned char song_fps[NUM_SONGS] = { "
                 + ", ".join(str(SONGS[n]["frames_per_step"]) for n in SONG_ORDER) + " };")
    # the bass wave: 32 samples, 4 bits each, one smooth sine cycle
    wave = [min(15, max(0, round(7.5 + 7 * math.sin(2 * math.pi * i / 32)))) for i in range(32)]
    packed = [(wave[2 * i] << 4) | wave[2 * i + 1] for i in range(16)]
    lines.append("static const unsigned char bass_wave[16] = { " + ", ".join(str(v) for v in packed) + " };")
    saw = [i // 2 for i in range(32)]   # the rough bass: a sawtooth ramp
    packed = [(saw[2 * i] << 4) | saw[2 * i + 1] for i in range(16)]
    lines.append("static const unsigned char saw_wave[16] = { " + ", ".join(str(v) for v in packed) + " };")
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w") as f:
        f.write("\n".join(lines) + "\n")
    print("wrote", os.path.relpath(OUT, ROOT))
    return wave


def write_wav(name, song, wave, path, loops=2):
    """A preview close to the GBA: the melody's square (its duty and fade), the bass wave (sine or saw), and
    the v0.9.1 extras (a doubled melody, noise hits)."""
    import random
    rate = 32768
    step_s = song["frames_per_step"] / 60.0
    duty = (0.125, 0.25, 0.5, 0.75)[song.get("duty", 2)]
    saw = [i // 2 for i in range(32)]
    bwave = saw if song.get("bass_wave") == "saw" else wave
    bvol = 0.6 if song.get("bass_loud") else 0.35
    vol, fstep = song.get("env", DEFAULT_ENV)
    out = []
    for voice in ("melody", "bass"):
        buf = []
        for _ in range(loops):
            for f, steps in parse(song[voice]):
                n = int(steps * step_s * rate)
                f2 = 0
                if voice == "melody" and f and song.get("double"):
                    f2 = 131072 / (2048 - min(2047, square_reg(f) + song["double"]))
                for i in range(n):
                    t = i / rate
                    if f == 0:
                        buf.append(0.0)
                    elif voice == "melody":
                        level = vol if fstep == 0 else max(0.0, vol - t * 64 / fstep)   # one volume step per fstep/64 s
                        env = 0.45 * level / 9
                        v = env * (1 if (t * f) % 1 < duty else -1)
                        if f2:
                            v = 0.7 * v + 0.7 * env * (1 if (t * f2) % 1 < 0.5 else -1)
                        buf.append(v)
                    else:
                        buf.append(bvol * (bwave[int((t * f) % 1 * 32)] / 7.5 - 1))
        out.append(buf)
    n = min(len(out[0]), len(out[1]))
    mix = [out[0][i] + out[1][i] for i in range(n)]
    pat = song.get("noise")
    if pat:
        rng = random.Random(1)
        per = int(step_s * rate)
        for k in range(n // per):
            c = pat[k % len(pat)]
            if c == ".":
                continue
            amp, decay = (0.5, 0.06) if c == "x" else (0.22, 0.025)
            for i in range(min(per, int(decay * rate))):
                if k * per + i < n:
                    mix[k * per + i] += amp * (1 - i / (decay * rate)) * (rng.random() * 2 - 1)
    mix = [max(-1, min(1, v)) for v in mix]
    data = b"".join(struct.pack("<h", int(v * 20000)) for v in mix)
    with open(path, "wb") as f:
        f.write(b"RIFF" + struct.pack("<I", 36 + len(data)) + b"WAVEfmt "
                + struct.pack("<IHHIIHH", 16, 1, 1, rate, rate * 2, 2, 16) + b"data" + struct.pack("<I", len(data)) + data)
    print("wrote", path)


def main():
    wave = write_header()
    if "--wav" in sys.argv:
        d = sys.argv[sys.argv.index("--wav") + 1]
        os.makedirs(d, exist_ok=True)
        for name in SONG_ORDER:
            write_wav(name, SONGS[name], wave, os.path.join(d, f"crowfield_{name}.wav"))


if __name__ == "__main__":
    main()
