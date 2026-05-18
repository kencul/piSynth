#!/usr/bin/env bash
# Measures CPU utilization under 8-voice load for each effect in isolation
# and all effects combined. Requires the synth running and a browser tab open
# at http://<hostname>.local:9002 to keep the WebSocket active.
#
# Setup:
#   1. Start the synth.
#   2. Open the browser UI — keeps the WebSocket and FFT pipeline active.
#   3. Load the virtual MIDI driver:  sudo modprobe snd-virmidi
#      The synth auto-connects within a few seconds.
#   4. Install sysstat if not present:  sudo apt install sysstat
#
# Usage:
#   ./scripts/benchmark_cpu.sh
#
# Six passes run in sequence: idle baseline, then dry, reverb only, chorus
# only, delay only, and all effects. Each saturates all 8 voices and samples
# pidstat for DURATION seconds. The first WARMUP_S seconds of each pass are
# discarded to exclude voice ramp-up and parameter smoother transients.

set -euo pipefail

DURATION=30      # seconds of pidstat sampling per pass
WARMUP_S=2       # leading samples to discard per pass
RETRIGGER_S=5    # re-fire notes every N seconds to counteract Karplus-Strong decay

NOTES=(36 40 43 47 52 55 59 64)  # 8 distinct pitches; spread avoids voice-steal collisions

# ── dependency checks ─────────────────────────────────────────────────────────

VIRMIDI=$(amidi -l 2>/dev/null | awk '/Virtual/ { print $2; exit }')
if [[ -z "$VIRMIDI" ]]; then
    echo "Error: VirMIDI not found."
    echo "       Run:  sudo modprobe snd-virmidi"
    echo "       Then wait a few seconds for the synth to auto-connect and re-run."
    exit 1
fi

if ! command -v pidstat &>/dev/null; then
    echo "Error: pidstat not found."
    echo "       Run:  sudo apt install sysstat"
    exit 1
fi

SYNTH_PID=$(systemctl show -p MainPID pi-synth.service 2>/dev/null | cut -d= -f2)
if [[ -z "$SYNTH_PID" || "$SYNTH_PID" == "0" ]]; then
    echo "Error: pi-synth.service is not running."
    echo "       Run:  sudo systemctl start pi-synth.service"
    exit 1
fi

# ── helpers ───────────────────────────────────────────────────────────────────

send_cc()       { amidi -p "$VIRMIDI" -S "$(printf 'B0 %02X %02X' "$1" "$2")"; }
send_note_on()  { amidi -p "$VIRMIDI" -S "$(printf '90 %02X 64'   "$1"      )"; }
send_note_off() { amidi -p "$VIRMIDI" -S "$(printf '80 %02X 00'   "$1"      )"; }

all_notes_off() { for n in "${NOTES[@]}"; do send_note_off "$n"; done; }

RETRIGGER_PID=""
TMPRESULTS=$(mktemp /tmp/cpu_bench_XXXXXX)

cleanup() {
    [[ -n "$RETRIGGER_PID" ]] && kill "$RETRIGGER_PID" 2>/dev/null || true
    all_notes_off
    rm -f "$TMPRESULTS"
}
trap cleanup EXIT

# ── effect presets ────────────────────────────────────────────────────────────
# CC map: 15=decay  22=chorus_rate  23=chorus_depth  24=chorus_mix
#         25=delay_time  29=delay_fb  1=delay_mix
#         26=reverb_room  27=reverb_cutoff  28=reverb_mix
#
# zero_effects disables all three mix controls before each pass so only the
# target effect contributes load.

zero_effects() { send_cc 24 0; send_cc 1 0; send_cc 28 0; }

preset_dry() {
    zero_effects
    send_cc 15 127   # max decay — voices sustain past the DURATION window
}

preset_reverb() {
    zero_effects
    send_cc 15 127
    send_cc 26 127; send_cc 27 127; send_cc 28 127
}

preset_chorus() {
    zero_effects
    send_cc 15 127
    send_cc 22 127; send_cc 23 127; send_cc 24 127
}

preset_delay() {
    zero_effects
    send_cc 15 127
    send_cc 25 127; send_cc 29 127; send_cc 1 127
}

preset_all() {
    send_cc 15 127
    send_cc 26 127; send_cc 27 127; send_cc 28 127
    send_cc 22 127; send_cc 23 127; send_cc 24 127
    send_cc 25 127; send_cc 29 127; send_cc 1  127
}

# ── sampling ──────────────────────────────────────────────────────────────────

# Samples pidstat for DURATION seconds, skips the first WARMUP_S samples, then
# computes mean, median (insertion sort), and std dev. Appends one result line
# to TMPRESULTS: "key mean median std". Prints inline progress on completion.
sample_cpu() {
    local key=$1
    pidstat -p "$SYNTH_PID" 1 "$DURATION" | awk -v key="$key" -v warmup="$WARMUP_S" '
        $NF == "Command" && /%CPU/ { for (i=1;i<=NF;i++) if ($i=="%CPU") cpu_col=i }
        /^[0-9]/ && $NF ~ /synth/ {
            if (!cpu_col) next
            if (skipped < warmup) { skipped++; next }
            n++; samples[n] = $cpu_col
            sum += $cpu_col; sum2 += $cpu_col * $cpu_col
        }
        END {
            if (n == 0) { printf "%s 0.0 0.0 0.0\n", key; exit }
            for (i = 2; i <= n; i++) {
                val = samples[i]; j = i - 1
                while (j >= 1 && samples[j] > val) { samples[j+1] = samples[j]; j-- }
                samples[j+1] = val
            }
            mid = int(n / 2)
            median = (n % 2 == 1) ? samples[mid + 1] : (samples[mid] + samples[mid + 1]) / 2
            mean = sum / n
            std  = sqrt(sum2 / n - mean * mean)
            printf "%s %.1f %.1f %.1f\n", key, mean, median, std
        }' >> "$TMPRESULTS"

    read -r _ mean median std < <(grep "^$key " "$TMPRESULTS" | tail -1)
    printf "done.  Mean: %s%%  Median: %s%%  ±%s%%\n" "$mean" "$median" "$std"
}

# ── passes ────────────────────────────────────────────────────────────────────

run_baseline() {
    printf "  %-32s" "Pass 1/6: Idle baseline..."
    zero_effects
    sleep 0.5
    sample_cpu baseline
}

run_pass() {
    local pass=$1 key=$2 label=$3 preset_fn=$4

    printf "  %-32s" "$pass: $label..."
    "$preset_fn"
    sleep 0.5   # let parameter smoothers settle before voices start

    for n in "${NOTES[@]}"; do send_note_on "$n"; done

    # Re-send note-ons periodically. Karplus-Strong decays naturally; without
    # retrigger voices would go silent well before DURATION seconds elapse.
    ( while true; do
          sleep "$RETRIGGER_S"
          for n in "${NOTES[@]}"; do send_note_on "$n"; done
      done ) &
    RETRIGGER_PID=$!

    sample_cpu "$key"

    kill "$RETRIGGER_PID" 2>/dev/null || true
    RETRIGGER_PID=""
    all_notes_off
    sleep 0.3
}

# ── main ──────────────────────────────────────────────────────────────────────

echo "=== PiSynth CPU Benchmark ==="
echo ""
echo "MIDI port : $VIRMIDI"
echo "Synth PID : $SYNTH_PID"
echo "Duration  : ${DURATION}s per pass (first ${WARMUP_S}s discarded), ${#NOTES[@]} voices"
echo "Ensure    : browser UI open at http://<hostname>.local:9002"
echo ""

run_baseline
run_pass "Pass 2/6" dry    "Dry"          preset_dry
run_pass "Pass 3/6" reverb "Reverb only"  preset_reverb
run_pass "Pass 4/6" chorus "Chorus only"  preset_chorus
run_pass "Pass 5/6" delay  "Delay only"   preset_delay
run_pass "Pass 6/6" all    "All effects"  preset_all

echo ""
echo "=== Summary (8 voices, WebSocket active) ==="
printf "%-18s  %8s  %8s  %8s  %8s\n" "Configuration" "Mean" "Median" "Std Dev" "Delta"
printf "%-18s  %8s  %8s  %8s  %8s\n" "-----------------" "--------" "--------" "-------" "--------"

awk '
BEGIN {
    split("baseline dry reverb chorus delay all", order)
    labels["baseline"] = "Idle (baseline)"
    labels["dry"]      = "Dry"
    labels["reverb"]   = "Reverb"
    labels["chorus"]   = "Chorus"
    labels["delay"]    = "Delay"
    labels["all"]      = "All effects"
}
{ cpu_mean[$1] = $2; cpu_median[$1] = $3; cpu_std[$1] = $4 }
END {
    base = cpu_mean["baseline"]
    for (i = 1; i <= 6; i++) {
        k = order[i]
        delta = (k == "baseline") ? "—" : sprintf("%+.1f%%", cpu_mean[k] - base)
        printf "%-18s  %7.1f%%  %7.1f%%  %6s%%  %8s\n",
            labels[k], cpu_mean[k], cpu_median[k], "±" cpu_std[k], delta
    }
}' "$TMPRESULTS"
