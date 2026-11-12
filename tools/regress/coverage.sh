#!/usr/bin/env bash
# Automatic coverage of one game: every level (about 45 s each), every figure, then the credits, one game at a time.
#
#   tools/regress/coverage.sh <ssa|giants|swapforce|trapteam> <disc .iso> <work dir> [--levels a,b] [--figures n,m] [--no-figures] [--no-credits]
# Trap Team: --list prints disc-derived levels and exits without a game; work must be below private/.
#
# The level tour is the game's own tools/pipelines tour (tour_ssa_story.sh, tour_giants.sh, tour_swapforce.sh) with
# TOUR_SECONDS=45 (Giants' tutorial opens after its title, at ~29 s; COVERAGE_LEVEL_SECONDS overrides); --levels passes a
# comma-separated list of level names (or Swap Force zones) to it. The figures are
# tools/regress/figures.sh, from the game's accept list (Swap Force's swappers included); --figures passes a comma list of
# toy types (a Swap Force swapper as `<top>+<bottom>` or either half). The credits are tools/pipelines/ending.sh.
# --no-figures skips the figure tour, even with --figures; --no-credits skips the credits.
#
# Writes <work dir>/coverage.tsv: a `# game <id>` line, then the columns `kind name verdict lowest_fps detail`. kind is
# level, figure or credits; verdict is ok, crash (a fault or a [guard] line) or not_reached; lowest_fps is the smallest
# number of frames in any one-second window of the run (first to last frame drawing >= 60 % of the busiest draw counts,
# for a level or figure; every frame for credits); detail lists that run's [guard] lines, faults, script errors and
# missing-file lines from its log.
# The runs and logs stay under <work dir>. Never touches the player's files; one game at a time, `nice`.
set -u
root=$(cd "$(dirname "$0")/../.." && pwd)
usage() { sed -n '2,17p' "$0"; exit 2; }
[ $# -ge 3 ] || usage
game=$1; iso=$2; work=$3; shift 3
case $game in
    ssa|giants|swapforce|trapteam) ;;
    *) echo "unknown game: $game (ssa, giants, swapforce or trapteam)"; usage ;;
esac
[ -f "$iso" ] || { echo "no such disc image: $iso"; exit 2; }
levels=""; figures=""; do_figures=1; do_credits=1; list_only=0
while [ $# -gt 0 ]; do
    case $1 in
        --levels) shift; levels=${1:-} ;;
        --figures) shift; figures=${1:-} ;;
        --no-figures) do_figures=0 ;;
        --no-credits) do_credits=0 ;;
        --list) [ "$game" = trapteam ] || usage; list_only=1 ;;
        *) echo "unknown option: $1"; usage ;;
    esac
    shift
done
if [ "$list_only" -eq 1 ]; then
    exec bash "$root/tools/pipelines/tour_trapteam.sh" --list "$iso"
fi
if [ "$game" = trapteam ]; then
    work=$(python3 -I "$root/tools/regress/trapteam.py" work "$work") || exit 2
fi
mkdir -p "$work"; work=$(cd "$work" && pwd)
out=$work/coverage.tsv
printf '# game %s\nkind\tname\tverdict\tlowest_fps\tdetail\n' "$game" > "$out"
level_seconds=${COVERAGE_LEVEL_SECONDS:-45}
[ "$game" != trapteam ] || level_seconds=${COVERAGE_LEVEL_SECONDS:-130}

row() { # <kind> <name> <verdict> <fps> <detail>
    local detail
    detail=$(printf '%s' "${5:-}" | tr '\t\n' '  ' | cut -c1-2000)
    printf '%s\t%s\t%s\t%s\t%s\n' "$1" "$2" "$3" "${4:-0}" "$detail" >> "$out"
}
add() { # <kind> <name> <hint> <log> <frames> [<prefix detail>]
    local result verdict fps detail
    result=$(python3 -I "$root/tools/regress/coverage_scan.py" "${4:-}" "${5:-}" "$1" "$3" "${6:-}")
    IFS=$'\t' read -r verdict fps detail <<< "$result"
    row "$1" "$2" "$verdict" "$fps" "$detail"
}

echo "== $game: levels (${level_seconds} s each)"
lwork=$work/levels; rm -rf "${lwork:?}"; mkdir -p "$lwork"
level_list=${levels//,/ }
case $game in
    ssa)
        if [ -n "$level_list" ]; then
            TOUR_SECONDS=$level_seconds "$root/tools/pipelines/tour_ssa_story.sh" "$iso" "$lwork" $level_list > "$lwork/tour.log" 2>&1
        else
            TOUR_SECONDS=$level_seconds "$root/tools/pipelines/tour_ssa_story.sh" "$iso" "$lwork" > "$lwork/tour.log" 2>&1
        fi ;;
    giants)
        TOUR_SECONDS=$level_seconds "$root/tools/pipelines/tour_giants.sh" "$iso" "$lwork" 1 $level_list > "$lwork/tour.log" 2>&1 ;;
    swapforce)
        TOUR_KEEP=1 TOUR_SECONDS=$level_seconds "$root/tools/pipelines/tour_swapforce.sh" "$iso" "$lwork" $level_list > "$lwork/tour.log" 2>&1 ;;
    trapteam)
        TOUR_SECONDS=$level_seconds bash "$root/tools/pipelines/tour_trapteam.sh" "$iso" "$lwork" $level_list > "$lwork/tour.log" 2>&1 ;;
esac
tour_status=$?
if [ -s "$lwork/summary.tsv" ]; then
    GAME=$game python3 -I - "$lwork/summary.tsv" > "$lwork/parsed.tsv" <<'PY'
import sys
game = __import__("os").environ["GAME"]
for line in open(sys.argv[1], errors="replace"):
    if not line.strip():
        continue
    f = line.rstrip("\n").split("\t")
    if f[0] in ("chapter", "level", "zone"):
        continue
    if game == "ssa":
        if len(f) < 10:
            continue
        name, opened, faults, verdict = f[1], f[2], f[3], f[9]
    else:
        if len(f) < 7:
            continue
        # Swap Force appends the frame's brightness after the verdict; take the verdict wherever it is.
        verdict = next((x for x in reversed(f)
                        if x in ("ok", "no level", "fault", "hang", "black", "no frames")), f[-1])
        name, opened, faults = f[0], f[1], f[3]
    crashed = faults not in ("", "0") or verdict == "fault"
    reached = opened == "1"
    hint = "crash" if crashed else ("ok" if reached and verdict not in ("hang", "no level") else "not_reached")
    if game == "trapteam" and verdict in ("black", "no frames"):
        hint = "not_reached"
    print(name, hint, name.replace("/", "_"), sep="\t")
PY
    while IFS=$'\t' read -r name hint runname; do
        [ -n "${name:-}" ] || continue
        # Giants' tour also loads its credits level; the credits row below is the one that reports it.
        [ "$game" = giants ] && [ "$name" = credits_giants ] && continue
        log=$lwork/runs/$runname/game.log
        frames=$lwork/runs/$runname/frames.txt
        [ -f "$log" ] || log=$lwork/logs/$runname.log
        # SKY_LEVEL starts a Trap Team arena alone, without its base mesh (Arena_Mesh_*.bld, the Havok world): the fault
        # in sub_8264B3F0 is the redirect's (games/trapteam/docs/status.md, "Arenas in the tour").
        prefix=""
        if [ "$game" = trapteam ] && [[ $name == arena_challenge_* ]] &&
            grep -q 'guest function: sub_8264B3F0' "$log" 2>/dev/null; then
            prefix="SKY_LEVEL artefact, not a player bug: the arena runs without its Arena_Mesh (no physics world)"
        fi
        add level "$name" "$hint" "$log" "$frames" "$prefix"
    done < "$lwork/parsed.tsv"
else
    tail_line=$(tail -3 "$lwork/tour.log" 2>/dev/null | tr '\n' ' ')
    row level "all" not_reached 0 "the level tour wrote no summary (exit $tour_status): $tail_line"
fi

if [ $do_figures -eq 1 ]; then
    echo "== $game: figures"
    fwork=$work/figures; rm -rf "${fwork:?}"; mkdir -p "$fwork"
    figure_list=${figures//,/ }
    if [ -n "$figure_list" ]; then
        "$root/tools/regress/figures.sh" "$game" "$iso" "$fwork" $figure_list > "$fwork/figures.log" 2>&1
    else
        "$root/tools/regress/figures.sh" "$game" "$iso" "$fwork" > "$fwork/figures.log" 2>&1
    fi
    if [ -s "$fwork/results.tsv" ]; then
        python3 -I - "$fwork/results.tsv" "$fwork/names.tsv" > "$fwork/parsed.tsv" <<'PY'
import sys
names = {}
try:
    for line in open(sys.argv[2], errors="replace"):
        fields = line.rstrip("\n").split("\t")
        if len(fields) == 2:
            names[fields[0]] = fields[1]
except OSError:
    pass
for line in open(sys.argv[1], errors="replace"):
    f = line.rstrip("\n").split("\t")
    if len(f) < 3:
        continue
    print(f[0], names.get(f[0], f[0].replace("_", " ")), "ok" if f[1] == "PASS" else "not_reached", f[2], sep="\t")
PY
        while IFS=$'\t' read -r label name hint detail; do
            [ -n "${label:-}" ] || continue
            log=$fwork/$label/game.log
            frames=$fwork/$label/frames.txt
            add figure "$name" "$hint" "$log" "$frames" "$detail"
        done < "$fwork/parsed.tsv"
    else
        tail_line=$(tail -3 "$fwork/figures.log" 2>/dev/null | tr '\n' ' ')
        row figure "all" not_reached 0 "the figure runs wrote no results: $tail_line"
    fi
fi

if [ $do_credits -eq 1 ]; then
    echo "== $game: credits"
    cwork=$work/credits; rm -rf "${cwork:?}"; mkdir -p "$cwork"
    if [ "$game" = trapteam ]; then
        printf '%s\n' 'Trap Team: no credits route verified; no game launched for credits' > "$cwork/ending.txt"
        st=2
    elif [ -n "${COVERAGE_CREDITS_SECONDS:-}" ]; then
        ENDING_SECONDS=$COVERAGE_CREDITS_SECONDS "$root/tools/pipelines/ending.sh" "$game" "$iso" "$cwork" > "$work/credits-ending.log" 2>&1
        st=$?
    else
        "$root/tools/pipelines/ending.sh" "$game" "$iso" "$cwork" > "$work/credits-ending.log" 2>&1
        st=$?
    fi
    reason=$(cat "$cwork/ending.txt" 2>/dev/null || tail -1 "$work/credits-ending.log" 2>/dev/null)
    if [ -f "$cwork/game.log" ]; then
        [ $st -eq 0 ] && hint=ok || hint=not_reached
        add credits credits "$hint" "$cwork/game.log" "$cwork/frames.txt" "${reason:-}"
    else
        row credits credits not_reached 0 "${reason:-no credits run}"
    fi
fi

ok=$(grep -c $'\t''ok\t' "$out" || true)
crash=$(grep -c $'\t''crash\t' "$out" || true)
missed=$(grep -c $'\t''not_reached\t' "$out" || true)
echo "coverage: $out ($(( $(wc -l < "$out") - 2 )) rows: $ok ok, $crash crash, $missed not reached)"
exit 0
