#!/usr/bin/env bash
# Every toy a game accepts, one isolated run each (docs/runtime/host/figures.md; the per-game accepts lists in
# resources/collection/catalogue.json). SSA's list is the catalogue's `ssa` accepts; Giants and Swap Force take every toy
# whose own game their `accepts_games` names. Swap Force's two halves are run as the swappers the game builds from them
# (a top and its bottom, one card): 16 runs, not 32 loose halves.
#
#   tools/regress/figures.sh <ssa|giants|swapforce|trapteam> <disc .iso> <work dir> [--variants] [--seconds N] [<type> ...]
# Trap Team maps to catalogue id trapteam, includes its known tags through that game's year (including Elite and
# mini tags), and pairs older swappers. Unmapped trap/villain data is listed as SKIP, never fabricated. Trap Team work
# must be below private/; its generator/checker calls also use the isolation block.
#
# With no <type> arguments it runs every toy (~80 s each on SSA, ~100 s on Giants and Swap Force); arguments run just
# those: a toy type, a Swap Force swapper as `<top>+<bottom>` or either half, or `--variants` on SSA for every variant the
# catalogue lists with a tag of its own.
#
# Per toy: a figure made by `ArkeyanFigureTest --make` (characters at 1000 XP, 100 gold; an SSA character's odd toy types get a
# Giants part, so "Giants' area untouched" is checked on real data; magic items, adventure packs and sidekicks go beside a
# Gill Grunt, which the game needs), then an isolated run that reaches SSA's level_027 (the fast recipe,
# docs/getting-started/testing.md), Giants' tutorial or Swap Force's tutorial, then `ArkeyanFigureTest --roundtrip` on every
# figure: for SSA nothing outside the first pair of areas changed (identity, toy type and variant, extra areas, trailers),
# the checksums are valid, and the bytes the game wrote are known fields; for Giants and Swap Force the identity stays,
# both pairs stay valid and a written figure gets its extra area (`--roundtrip-later`, as those games write them). A
# character is also expected to have been written (first use). The portal log must show each toy placed in its slot and
# read by the game. A frame is kept as <label>.png
# when `sips` or Pillow can convert it. Results: <work dir>/results.tsv (label, verdict, detail). Never touches the
# player's files: every run uses scratch folders under <work dir>. One game at a time, `nice`.
set -u
root=$(cd "$(dirname "$0")/../.." && pwd)
[ $# -ge 3 ] || { sed -n '2,6p' "$0"; exit 2; }
game=$1; iso=$2; work=$3; shift 3
case $game in
    ssa|giants|swapforce|trapteam) ;;
    *) echo "unknown game: $game (ssa, giants, swapforce or trapteam)"; exit 2 ;;
esac
[ -f "$iso" ] || { echo "no such disc image: $iso"; exit 2; }
case $game in
    ssa) build=build; runtime="Skylanders Spyro's Adventure"; default_seconds=80 ;;
    giants) build=build-giants; runtime="Skylanders Giants"; default_seconds=100 ;;
    swapforce) build=build-swapforce; runtime="Skylanders Swap Force"; default_seconds=100 ;;
    trapteam) build=build-trapteam; runtime="Skylanders Trap Team"; default_seconds=130
        work=$(python3 -I "$root/tools/regress/trapteam.py" work "$work") || exit 2 ;;
esac
exe="${FIGURE_EXE:-$root/$build/ArkeyanRuntime/$runtime}"
if [ "$game" = trapteam ] && [ ! -x "$exe" ] && [ -z "${FIGURE_EXE:-}" ]; then
    exe="$root/$build/ArkeyanRuntime/$runtime.app/Contents/MacOS/$runtime"
fi
[ -x "$exe" ] || { echo "build $runtime first ($exe)"; exit 2; }
figtest=$(find "$root/$build" -name ArkeyanFigureTest -type f -perm -u+x 2>/dev/null | head -1)
[ -n "$figtest" ] && [ -x "$figtest" ] || { echo "build ArkeyanFigureTest first"; exit 2; }
mkdir -p "$work"; work=$(cd "$work" && pwd); : > "$work/results.tsv"; : > "$work/names.tsv"

variants=0; seconds=$default_seconds
while [ $# -gt 0 ]; do
    case $1 in
        --variants) variants=1 ;;
        --seconds) shift; seconds=${1:-$default_seconds} ;;
        *) break ;;
    esac
    shift
done
wanted="$*"

if [ "$game" = trapteam ]; then
    [[ "$seconds" =~ ^[0-9]+$ ]] && [ "$seconds" -ge 8 ] || { echo "--seconds must be an integer >= 8"; exit 2; }
    list=$(python3 -I "$root/tools/regress/trapteam.py" figures "$root/resources/collection/catalogue.json" "$@") || exit 2
else
list=$(GAME="$game" VARIANTS=$variants WANTED="$wanted" python3 -I - "$root/resources/collection/catalogue.json" <<'PY'
import json, os, re, sys
c = json.load(open(sys.argv[1]))
game = os.environ["GAME"]
variants = os.environ["VARIANTS"] == "1"
g = next(x for x in c["games"] if x["id"] == game)
toys = {t["type"]: t for t in c["toys"]}
accepted = [toys[t] for t in g["accepts"]] if "accepts" in g else [
    t for t in c["toys"] if t.get("game") in g.get("accepts_games", [])]
rows = []
if game == "swapforce":
    # A swapper is a top (2000-2015) and its bottom (1000-1015), one card on the portal.
    bytype = {t["type"]: t for t in c["toys"]}
    paired = set()
    for t in accepted:
        if t["category"] != "swap_top":
            continue
        b = bytype.get(t["type"] - 1000)
        if b is None or b["category"] != "swap_bottom" or b not in accepted:
            continue
        name = re.sub(r"\s*\(top\)\s*$", "", t["name"])
        rows.append((f'{t["type"]}+{b["type"]}', "swap", name, f'{t["type"]}+{b["type"]}'))
        paired.update((t["type"], b["type"]))
    accepted = [t for t in accepted if t["type"] not in paired]
else:
    paired = set()

for t in accepted:
    kind = "core" if t["category"] in ("core", "giant") else "other"
    if variants and game == "ssa":
        for v in t.get("variants", []):
            if "id" in v:
                rows.append((f'{t["type"]}:{v["id"][2:]}', kind, v["name"], str(t["type"])))
    else:
        rows.append((str(t["type"]), kind, t["name"], str(t["type"])))

wanted = set(os.environ["WANTED"].split())
def selected(key, figs):
    if not wanted:
        return True
    aliases = {key, figs} | set(key.replace("+", " ").split()) | set(re.findall(r"\d+", figs))
    return bool(wanted & aliases)

for key, kind, name, figs in rows:
    if selected(key, figs):
        print(key, kind, name.replace(" ", "_"), figs, sep="\t")
PY
)
fi

ssa_press="start:1,start:2,start:3,start:4,start:5,a:6,a:7.5,a:9,a:10.5,a:12,a:13.5,a:15,a:16.5,a:18,a:19.5,a:21,a:22.5,a:24,a:25.5,a:27,b:30,b:32,b:34,b:36,b:38,b:40,b:42,b:44,b:46,b:48,b:50,b:52,b:54,b:56,b:58,b:60,b:62,b:64,b:66,b:68,b:70,b:72,b:74,b:76,b:78"
case $game in
    ssa)
        press=$ssa_press
        marker='open \levels\level_027\mission.str -> 0x00000000'
        roundtrip=--roundtrip
        extra=(SKY_LEVEL=level_027) ;;
    giants)
        press=$(python3 -I "$root/tools/regress/giants_press.py" "$seconds")
        marker='open \level\level_213_tutotial.arc -> 0x00000000'
        roundtrip=--roundtrip-later
        extra=() ;;
    swapforce)
        press=$(awk 'BEGIN { for (t = 14; t <= 50; t += 2) printf "%sa:%d", (t > 14 ? "," : ""), t }')
        marker='open \archives\tutorial_a.pak -> 0x00000000'
        roundtrip=--roundtrip-later
        extra=() ;;
    trapteam)
        press=$(python3 -I "$root/tools/regress/giants_press.py" "$seconds")
        # Discover the tutorial on this disc; never maintain a disc file/level list in the repository.
        tutorials=$(python3 -I "$root/tools/regress/trapteam.py" levels "$iso") || exit 2
        tutorial=$(printf '%s\n' "$tutorials" | grep -E '^level_.*_tutorial$')
        [ "$(printf '%s\n' "$tutorial" | wc -l | tr -d ' ')" = 1 ] && [ -n "$tutorial" ] \
            || { echo "cannot identify one tutorial on this disc"; exit 2; }
        marker="open \\level\\$tutorial.arc -> 0x00000000"
        roundtrip=--roundtrip-later
        extra=() ;;
esac

record() { printf '%s\t%s\t%s\n' "$1" "$2" "$3" >> "$work/results.tsv"; printf '  %-4s %-14s %s\n' "$2" "$1" "$3"; }
tool() {
    local status
    if [ "$game" = trapteam ]; then
        env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null \
            SKYLANDERS_DATA="$R/data" SKY_SETTINGS="$R/none.ini" SKY_LAUNCHER_INI="$R/none.ini" \
            SKY_COLLECTION="$R/collection" SKY_FIGURES="$R/collection" SKY_SAVE_DIR="$R/save" SKY_LOG="$R/tool.log" \
            "$figtest" "$@"
        status=$?
        if [ "$1" = --make ] && [ "$status" -ne 0 ]; then make_failed=1; fi
        return "$status"
    else
        "$figtest" "$@"
    fi
}
pid=""
stop_trapteam() {
    if [ -n "$pid" ]; then
        kill "$pid" 2>/dev/null || true
        sleep 3
        kill -9 "$pid" 2>/dev/null || true
        wait "$pid" 2>/dev/null || true
        pid=""
    fi
}
if [ "$game" = trapteam ]; then
    trap stop_trapteam EXIT
    trap 'exit 130' INT
    trap 'exit 143' TERM
fi

while IFS=$'\t' read -r key kind name figs; do
    [ -n "${key:-}" ] || continue
    label=$(printf '%s_%s' "${key//:/_}" "$name" | tr -c 'A-Za-z0-9._-' '_')
    if [ "$kind" = skip ]; then
        record "$label" SKIP 'generator/checker does not support this toy/tag (trap/villain areas are unmapped), or tag not in this game catalogue'
        printf '%s\t%s\n' "$label" "$name" >> "$work/names.tsv"
        continue
    fi
    R=$work/$label; rm -rf "${R:?}"; mkdir -p "$R/collection" "$R/save" "$R/snap"
    place=""; files=""; types=""; make_failed=0
    case $kind in
        core)
            # A variant key keeps its tag; the make tool reads the base type and variant. An odd type carries a Giants
            # part, so the SSA run checks that area on real data.
            if [ "$game" = trapteam ]; then
                tool --make "$R/collection/toy.sky" "$key" > "$R/make.txt" 2>&1
            elif [ $(( ${key%%:*} % 2 )) -eq 1 ]; then
                tool --make "$R/collection/toy.sky" "$key" xp=40000 gold=100 hat=6 > "$R/make.txt" 2>&1
            else
                tool --make "$R/collection/toy.sky" "$key" xp=1000 gold=100 > "$R/make.txt" 2>&1
            fi
            files="toy"; types="${key%%:*}" ;;
        swap)
            i=0
            for t in ${key//+/ }; do
                f=half$i
                tool --make "$R/collection/$f.sky" "$t" > "$R/make.txt" 2>&1
                files="$files $f"; types="$types ${t%%:*}"; i=$((i + 1))
            done
            files=${files# }; types=${types# } ;;
        *)
            tool --make "$R/collection/gill.sky" 14 xp=1000 gold=100 > "$R/make.txt" 2>&1
            tool --make "$R/collection/toy.sky" "$key" >> "$R/make.txt" 2>&1
            files="gill toy"; types="14 ${key%%:*}" ;;
    esac
    if [ "$game" = trapteam ] && [ "$make_failed" -ne 0 ]; then
        record "$label" SKIP 'ArkeyanFigureTest --make failed; see the isolated make.txt; no game launched'
        printf '%s\t%s\n' "$label" "$name" >> "$work/names.tsv"
        continue
    fi
    for f in $files; do
        cp "$R/collection/$f.sky" "$R/before_$f.sky" || { echo "cannot make $f ($key)"; exit 2; }
        place="$place${place:+,}$R/collection/$f.sky"
    done
    env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null \
        SKYLANDERS_DATA="$R/data" SKY_SETTINGS="$R/none.ini" SKY_LAUNCHER_INI="$R/none.ini" \
        SKY_COLLECTION="$R/collection" SKY_FIGURES="$R/collection" SKY_SAVE_DIR="$R/save" SKY_LOG="$R/game.log" \
        SKY_FRAME_TIMES="$R/frames.txt" SKY_SKIP_MOVIES=1 SKY_PRESS="$press" SKY_PORTAL=emulated \
        SKY_PORTAL_PLACE="$place" SKY_PORTAL_LOG=1 \
        SKY_GPU_SNAPSHOT="$R/snap/s" SKY_GPU_SNAPSHOT_FROM=1500 SKY_GPU_SNAPSHOT_EVERY=100000 \
        ${extra[@]+"${extra[@]}"} \
        nice "$exe" --vsync 0 --fps-limit 30 "$iso" > "$R/log" 2>&1 &
    pid=$!
    if [ "$game" = trapteam ]; then
        elapsed=0
        while kill -0 "$pid" 2>/dev/null && [ "$elapsed" -lt "$seconds" ]; do sleep 1; elapsed=$((elapsed + 1)); done
        stop_trapteam
    else
        (sleep "$seconds"; kill "$pid" 2>/dev/null; sleep 3; kill -9 "$pid" 2>/dev/null) &
        timer=$!
        wait "$pid" 2>/dev/null
        { kill "$timer"; wait "$timer"; } 2>/dev/null   # a finished run must not leave a timer that could hit a reused PID
    fi
    verdict=PASS; detail=""
    grep -aqF "$marker" "$R/game.log" 2>/dev/null || { verdict=FAIL; detail="the game did not reach its tutorial level; "; }
    i=0
    for f in $files; do
        t=$(printf '%s\n' $types | sed -n "$((i + 1))p")
        grep -aq "\[portal\] placed .*$f.sky (toy type $t) in slot $i" "$R/game.log" 2>/dev/null \
            || { verdict=FAIL; detail="${detail}$f not placed in slot $i; "; }
        grep -aq "\[portal\] -> Q 5[0-9A-F] [0-9A-F]$i " "$R/game.log" 2>/dev/null \
            || { verdict=FAIL; detail="${detail}the game never read slot $i; "; }
        i=$((i + 1))
    done
    grep -aEq "Fault \(|Indirect call to unrecompiled|SIGSEGV|SIGBUS|SIGABRT|assertion failed|\[guard\] bad indirect call" "$R/game.log" 2>/dev/null \
        && { verdict=FAIL; detail="${detail}fault or guard line; "; }
    for f in $files; do
        out=$(tool "$roundtrip" "$R/before_$f.sky" "$R/collection/$f.sky" 2>&1); st=$?
        line=$(printf '%s\n' "$out" | grep '^round trip:' | sed 's/^round trip: //; s/; outside the first pair.*//')
        [ $st -eq 0 ] || { verdict=FAIL; detail="${detail}$f round trip failed ($(printf '%s\n' "$out" | grep FAIL | head -1)); "; }
        detail="$detail[$f] $line; "
        [ "$f" = toy ] && [ "$kind" = core ] && ! printf '%s' "$line" | grep -q 'first pair written' \
            && { verdict=FAIL; detail="${detail}the game did not write the character; "; }
    done
    if command -v sips > /dev/null 2>&1 && [ -f "$R/snap/s_0.ppm" ]; then
        sips -s format png -Z 480 "$R/snap/s_0.ppm" --out "$work/$label.png" > /dev/null 2>&1
    fi
    record "$label" "$verdict" "$detail"
    printf '%s\t%s\n' "$label" "$name" >> "$work/names.tsv"
done <<< "$list"
echo "results: $work/results.tsv ($(grep -c PASS "$work/results.tsv") PASS, $(grep -c FAIL "$work/results.tsv") FAIL)"
[ "$game" != trapteam ] || echo "$(grep -c SKIP "$work/results.tsv") SKIP (listed in results.tsv)"
[ "$(grep -c FAIL "$work/results.tsv")" -eq 0 ]
