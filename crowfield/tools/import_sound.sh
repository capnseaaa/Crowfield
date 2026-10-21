#!/bin/sh
# Copy the chosen clips out of the downloaded packs into sound/*.wav (mono, 16-bit, 22050 Hz).
# tools/synth_sound.py makes whoosh.wav and poof.wav (no pack had them).
# Run once after picking a clip; the build does not need ffmpeg (tools/make_sound.py reads sound/*.wav).
# The packs live in sound/source/ (not committed). Where each pack came from: sound/CREDITS.md.
set -e
cd "$(dirname "$0")/.."
S=sound/source
clip() {   # clip <name> <source file>
    ffmpeg -v error -y -i "$2" -ac 1 -ar 22050 -sample_fmt s16 "sound/$1.wav"
    echo "sound/$1.wav <- $2"
}
clip hit       $S/impact/Audio/impactPunch_medium_000.ogg
clip caw       $S/crow/crow_caw.wav
clip hurt      $S/impact/Audio/impactPunch_heavy_000.ogg
clip plant     $S/impact/Audio/footstep_snow_001.ogg
clip harvest   $S/interface/Audio/pluck_002.ogg
clip noseeds   $S/interface/Audio/error_004.ogg
clip knockdown $S/rpg/Audio/dropLeather.ogg
