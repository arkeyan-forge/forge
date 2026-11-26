#!/usr/bin/env bash
# The online session on a bad network, the required checks of docs/runtime/host/netplay.md ("Step 6"): two instances of
# one game on this computer, in lockstep over the loopback, each run with a simulated network (SKY_NETPLAY_TEST_NET) or a
# slowed frame (SKY_NETPLAY_TEST_SLOW), then their checkpoint hashes, frame rates and logs compared.
#
#   tools/regress/netplay_matrix.sh <runtime executable> <disc .iso> <work dir> [--seconds N] [--fps 30|60] [<case> ...]
#
# Cases (all when none is named): clean, rtt50, rtt150, rtt250, loss2, loss5, cut5, cut60, slow. Each side delays what it
# sends by half the round trip. A case passes when every checkpoint both games reached is equal, neither session ended
# before the run did, the frame rate holds (25 at 30 FPS, 50 at 60, except `slow` and the 250 ms case), a 5-second cut
# reconnects, and a 70-second cut asks the player at the minute without ending, then reconnects. Results: <work dir>/results.tsv. Every run is isolated
# (scratch settings, saves, data, collection, offscreen video, disk audio); no figure is placed.
set -euo pipefail
[ $# -ge 3 ] || { sed -n 2,14p "$0"; exit 2; }
exe=$1 iso=$2 work=$3
shift 3
seconds=70 fps=30 cases=()
while [ $# -gt 0 ]; do
    case $1 in
        --seconds) seconds=$2; shift 2 ;;
        --fps) fps=$2; shift 2 ;;
        *) cases+=("$1"); shift ;;
    esac
done
[ ${#cases[@]} -gt 0 ] || cases=(clean rtt50 rtt150 rtt250 loss2 loss5 cut5 cut60 slow)
mkdir -p "$work"
results="$work/results.tsv"
printf 'case\tverdict\tdetail\n' > "$results"

spec() {
    case $1 in
        clean) echo "" ;;
        rtt50) echo "delay=25,jitter=5" ;;
        rtt150) echo "delay=75,jitter=15" ;;
        rtt250) echo "delay=125,jitter=20" ;;
        loss2) echo "delay=25,jitter=5,loss=2" ;;
        loss5) echo "delay=25,jitter=5,loss=5" ;;
        cut5) echo "delay=25,cut=30000-35000" ;;
        cut60) echo "delay=25,cut=25000-95000" ;;
        slow) echo "" ;;
        *) echo "unknown case $1" >&2; exit 2 ;;
    esac
}

# run <dir> <host|join> <net spec> <slow ms>: one isolated instance, stopped after $seconds.
run() {
    local dir=$1 role=$2 net=$3 slow=$4
    rm -rf "${dir:?}"
    mkdir -p "$dir/collection" "$dir/save" "$dir/data"
    printf '[video]\nfps_limit=%s\nresolution_scale=1\n' "$fps" > "$dir/settings.ini"
    (
        export SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null \
            SKY_SETTINGS="$dir/settings.ini" SKY_LAUNCHER_INI="$dir/settings.ini" SKY_COLLECTION="$dir/collection" \
            SKY_SAVE_DIR="$dir/save" SKYLANDERS_DATA="$dir/data" SKY_LOG="$dir/game.log" \
            SKY_LAUNCHER_NO_UPDATE_CHECK=1 SKY_PORTAL=emulated SKY_SKIP_MOVIES=1 \
            SKY_NETPLAY_DETERMINISTIC=1 SKY_NETPLAY_DELIVERY=frame SKY_NETPLAY_SCHEDULE=cooperative \
            SKY_NETPLAY_PASSWORD=matrix SKY_NETPLAY_FPS="$fps" SKY_NETPLAY_TEST=1 \
            SKY_NETPLAY_HASH="$dir/hash.txt" SKY_NETPLAY_HASH_EVERY=30
        [ -n "$net" ] && export SKY_NETPLAY_TEST_NET="$net"
        [ "$slow" != 0 ] && export SKY_NETPLAY_TEST_SLOW="$slow"
        if [ "$role" = host ]; then export SKY_NETPLAY_HOST=47660; else export SKY_NETPLAY_JOIN=127.0.0.1:47660; fi
        nice "$exe" "$iso" > "$dir/stderr.log" 2>&1 &
        local pid=$!
        (sleep "$seconds"; kill "$pid" 2>/dev/null || true; sleep 5; kill -9 "$pid" 2>/dev/null || true) &
        wait "$pid" || true
    )
}

median_fps() {
    grep -a "fps; frame median" "$1" | tail -n +2 | sed -E 's/^.*\[perf\] ([0-9.]+) fps.*$/\1/' | sort -n |
        awk '{ v[NR] = $1 } END { if (NR) print v[int((NR + 1) / 2)]; else print 0 }'
}

base_seconds=$seconds
for case in "${cases[@]}"; do
    net=$(spec "$case")
    # The minute-long cut: lost at 28 s, asked at 88 s, back at 95 s.
    seconds=$base_seconds
    [ "$case" = cut60 ] && [ "$seconds" -lt 115 ] && seconds=115
    slow=0
    [ "$case" = slow ] && slow=$((fps == 60 ? 12 : 25))
    host="$work/$case-host" guest="$work/$case-guest"
    run "$host" host "$net" 0 &
    sleep 2
    run "$guest" join "$net" "$slow"
    wait
    detail="" verdict=PASS
    same=$(python3 -I - "$host/hash.txt" "$guest/hash.txt" <<'EOF'
import sys
def load(path):
    try:
        return dict(line.split()[:2] for line in open(path) if line.strip())
    except OSError:
        return {}
a, b = load(sys.argv[1]), load(sys.argv[2])
common = sorted(set(a) & set(b), key=int)
print(len(common), sum(a[k] != b[k] for k in common))
EOF
)
    read -r compared differing <<< "$same"
    [ "$compared" -ge 20 ] && [ "$differing" -eq 0 ] || verdict=FAIL
    detail="$compared checkpoints, $differing differing"
    for side in "$host" "$guest"; do
        if grep -aq "session ended\|session refused" "$side/stderr.log" &&
            ! grep -a "session ended" "$side/stderr.log" | grep -q "other player quit"; then
            verdict=FAIL
            detail="$detail; $(basename "$side") ended early: $(grep -a 'session ended\|session refused' "$side/stderr.log" | head -1)"
        fi
    done
    low=$((fps == 60 ? 50 : 25))
    for side in "$host" "$guest"; do
        rate=$(median_fps "$side/game.log")
        detail="$detail; $(basename "$side") ${rate} FPS"
        if [ "$case" != slow ] && [ "$case" != rtt250 ] && [ "$case" != cut60 ] &&
            awk -v r="$rate" -v l="$low" 'BEGIN { exit !(r < l) }'; then
            verdict=FAIL
        fi
    done
    case $case in
        cut5) grep -aq "reconnected after" "$host/stderr.log" || { verdict=FAIL; detail="$detail; no reconnection"; } ;;
        cut60)
            grep -aq "asking the player" "$host/stderr.log" || { verdict=FAIL; detail="$detail; the player was not asked"; }
            grep -aq "reconnected after" "$host/stderr.log" || { verdict=FAIL; detail="$detail; no reconnection"; } ;;
    esac
    printf '%s\t%s\t%s\n' "$case" "$verdict" "$detail" | tee -a "$results"
done
! grep -q "	FAIL	" "$results"
