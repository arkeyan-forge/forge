#!/usr/bin/env bash
# Makes the pipeline baseline (docs/runtime/gpu/pipeline_cache.md): every level of the disc in a headless, silent run of the
# fast recipe (docs/getting-started/testing.md) started there with SKY_LEVEL, a different figure each run, walking, attacking and opening
# the pause menu; then merges every run's pipelines.bin with the current baseline.
#
#   tools/pipelines/tour.sh <disc .iso> <work dir> [<figure folder>] [<parallel runs, default 2>]
#
# Each run: ~2 min 10 s, its own save folder <work dir>/runs/<level> (with a copy of the figures), log next to it.
# Writes <work dir>/merged.bin; then: zstd -19 --rm <work dir>/merged.bin -o games/ssa/pipelines/baseline.zst
set -euo pipefail

root="$(cd "$(dirname "$0")/../.." && pwd)"
iso="$1" work="$2" figures="${3:-$root/private/figures}" parallel="${4:-2}"
exe="$root/build/ArkeyanRuntime/Skylanders Spyro's Adventure"
seconds=125
mkdir -p "$work/runs"

# Menus (new game, slot 1), B through the level's cutscene, then play from 62 s: A (dialogs, jump), the left stick in
# four directions, X, Y, B; the pause menu at 100-112 s.
press=$(python3 -I - <<'PY'
p = ["start:1", "start:2", "start:3", "start:4", "start:5"]
p += [f"a:{t}" for t in (6, 7.5, 9, 10.5, 12, 13.5, 15, 16.5, 18, 19.5, 21, 22.5, 24, 25.5, 27)]
p += [f"b:{t}" for t in range(30, 62, 2)]
dirs = ["lup", "lleft", "ldown", "lright"]
t, i = 62.0, 0
while t < 200:
    if not 98 <= t < 114:
        p += [f"a:{t:g}", f"{dirs[i % 4]}:{t + 0.3:g}:2.5", f"x:{t + 0.6:g}", f"x:{t + 1.2:g}", f"y:{t + 1.8:g}",
              f"b:{t + 2.4:g}", f"y:{t + 3.0:g}:1"]
        i += 1
    t += 4
p += ["start:100", "down:101.5", "a:103", "right:105", "b:107", "down:108", "a:109", "b:111", "start:112.5"]
print(",".join(p))
PY
)

levels=$(python3 -I "$root/tools/shaders/xdvdfs.py" "$iso" | grep -o '/levels/[a-z0-9_]*/mission.str' | cut -d/ -f3 \
    | grep -v -x -e frontend -e mytest)
figs=(); while IFS= read -r f; do figs+=("$f"); done < <(ls "$figures" | grep '\.sky$' | grep -v '^new_')

run() # <level> <figure>
{
    local dir="$work/runs/$1"
    rm -rf "$dir"; mkdir -p "$dir"; cp -R "$figures" "$dir/figures"
    SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null SKY_SAVE_DIR="$dir" \
        SKY_SKIP_MOVIES=1 SKY_PRESS="$press" SKY_PORTAL=emulated SKY_FIGURES="$dir/figures" SKY_PORTAL_PLACE="$2" \
        SKY_LEVEL="$1" "$exe" "$iso" > "$dir.log" 2>&1 & local pid=$!
    (sleep $seconds; kill $pid 2>/dev/null; sleep 3; kill -9 $pid 2>/dev/null) & wait $pid || true
    echo "$1 ($2): $(grep -c 'built at draw time' "$dir.log" || true) new pipelines (not in the list yet)"
}

i=0
for level in $levels; do
    run "$level" "${figs[$((i % ${#figs[@]}))]}" &
    i=$((i + 1))
    if (( i % parallel == 0 )); then wait; fi
done
wait

lists=()
[[ -f "$root/games/ssa/pipelines/baseline.zst" ]] && lists+=("$root/games/ssa/pipelines/baseline.zst")
for level in $levels; do [[ -f "$work/runs/$level/pipelines.bin" ]] && lists+=("$work/runs/$level/pipelines.bin"); done
python3 -I "$root/tools/pipelines/merge_lists.py" "$work/merged.bin" "${lists[@]}"
