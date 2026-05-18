#!/usr/bin/env bash
# Measures round-trip MIDI-to-audio latency.
#
# Setup:
#   1. Connect a 3.5mm cable from the USB-C DAC headphone out to the USB
#      interface line input.
#   2. Start the synth with output going to the USB-C DAC.
#   3. Set attack to minimum (0.1ms), disable all effects in the UI.
#   4. Load the virtual MIDI driver:  sudo modprobe snd-virmidi
#      The synth auto-connects to it within a few seconds.
#
# Usage:
#   ./scripts/benchmark_latency.sh [--capture-device plughw:X,0]
#
# Run 5 times and average. Each run has ~10-15ms timing uncertainty from
# ALSA capture initialization; see scripts/README.md for details.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

ALSA_INIT_S=0.5    # silence window for ALSA capture to initialize
PRE_TRIGGER_S=2.0  # additional silence before the note fires
SAMPLE_RATE=48000
NOTE_ON="90 3C 64" # note-on: channel 1, middle C, velocity 100
NOTE_OFF="80 3C 00"
CAPTURE_DEV=""

while [[ $# -gt 0 ]]; do
    case $1 in
        --capture-device) CAPTURE_DEV="$2"; shift 2 ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

TMPWAV=$(mktemp /tmp/latency_XXXXXX.wav)
ARECORD_PID=""

cleanup() {
    [[ -n "$ARECORD_PID" ]] && kill "$ARECORD_PID" 2>/dev/null || true
    rm -f "$TMPWAV"
}
trap cleanup EXIT

echo "=== PiSynth Latency Benchmark ==="
echo ""

# Locate VirMIDI port
VIRMIDI=$(amidi -l 2>/dev/null | awk '/Virtual/ {print $2; exit}')
if [[ -z "$VIRMIDI" ]]; then
    echo "Error: VirMIDI not found."
    echo "       Run:  sudo modprobe snd-virmidi"
    echo "       Then wait a few seconds for the synth to auto-connect and re-run."
    exit 1
fi

# Locate USB capture device if not provided
if [[ -z "$CAPTURE_DEV" ]]; then
    CAPTURE_DEV=$(arecord -l 2>/dev/null | awk '/USB/ {
        for (i=1;i<=NF;i++) if ($i=="card") { gsub(/:/, "", $(i+1)); print "plughw:" $(i+1) ",0"; exit }
    }')
    if [[ -z "$CAPTURE_DEV" ]]; then
        echo "Error: No USB capture device found."
        echo "       Plug in your USB interface or pass --capture-device plughw:X,0"
        exit 1
    fi
fi

echo "MIDI port     : $VIRMIDI"
echo "Capture device: $CAPTURE_DEV"
echo "Sample rate   : ${SAMPLE_RATE} Hz"
echo ""

# T0 is stamped at arecord launch to approximate sample 0 of the WAV.
# The ALSA_INIT_S window lets the capture buffer start filling before the note fires.
arecord -D "$CAPTURE_DEV" -f S16_LE -r "$SAMPLE_RATE" -c 1 -q \
    --period-size=1024 --buffer-size=8192 "$TMPWAV" &
ARECORD_PID=$!
T0_NS=$(date +%s%N)

sleep "$ALSA_INIT_S"
printf "Silence baseline..."
sleep "$PRE_TRIGGER_S"

T1_NS=$(date +%s%N)
amidi -p "$VIRMIDI" -S "$NOTE_ON"
printf " note fired..."

sleep 1.5
amidi -p "$VIRMIDI" -S "$NOTE_OFF"
sleep 0.3

kill "$ARECORD_PID" 2>/dev/null || true
wait "$ARECORD_PID" 2>/dev/null || true
ARECORD_PID=""
echo " done."
echo ""

PRE_TRIGGER_ACTUAL=$(awk "BEGIN { printf \"%.6f\", ($T1_NS - $T0_NS) / 1e9 }")
python3 "$SCRIPT_DIR/analyze_onset.py" "$TMPWAV" "$SAMPLE_RATE" "$PRE_TRIGGER_ACTUAL"
