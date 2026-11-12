#!/usr/bin/env bash
# Day 1 for a game: identify the disc and match SSA's hooks, writing only into private/<game>/day1/ (git-ignored, next
# to CMake's private/<game>/ppc/).
#   tools/day1.sh <game> <disc .iso>        e.g. tools/day1.sh giants private/discs/giants.iso
# Wraps steps 1-2 of docs/porting/day1.md (extract the XEX, xexdump, sigmatch). It runs the tools of this repo
# (ARKEYAN_WORKSHOP names another checkout to take them from) and writes nothing outside private/<game>/day1/.
set -euo pipefail
[[ $# -eq 2 ]] || { echo "usage: $0 <game> <disc .iso>" >&2; exit 2; }
game=$1; iso=$2
[[ $game =~ ^[a-z0-9]+$ ]] || { echo "bad game id: $game" >&2; exit 2; }
[[ -f $iso ]] || { echo "no such disc image: $iso" >&2; exit 2; }
here=$(cd "$(dirname "$0")/.." && pwd)
ws=${ARKEYAN_WORKSHOP:-$here}
[[ -d $ws ]] || { echo "checkout not found: $ws (set ARKEYAN_WORKSHOP)" >&2; exit 1; }
ws=$(cd "$ws" && pwd)
xexdump=$ws/private/build-host-tools/tools/xexdump/xexdump
[[ -x $xexdump ]] || { echo "no xexdump: run tools/prepare_game.sh once in $ws" >&2; exit 1; }
[[ -f $ws/private/image.bin ]] || { echo "no SSA image in $ws (needed for the signatures): run tools/prepare_game.sh on the SSA disc" >&2; exit 1; }
iso=$(cd "$(dirname "$iso")" && pwd)/$(basename "$iso")
out=$here/private/$game/day1
mkdir -p "$out"

cd "$ws"
echo "[1/3] extract and identify the XEX"
python3 -I tools/shaders/xdvdfs.py "$iso" "$out" /default.xex
"$xexdump" "$out/default.xex" "$out" > "$out/xexdump.txt"
shasum -a 256 "$out/default.xex" | cut -d' ' -f1 > "$out/default.xex.sha256"
head -20 "$out/xexdump.txt"
echo "default.xex sha256: $(cat "$out/default.xex.sha256")"

echo "[2/3] SSA signatures (from SSA's image) into $out/ssa_sigs.json"
python3 -I tools/sigmatch/sigmatch.py make docs/giants/ssa_hooks.json --image "$ws/private/image.bin" \
    --sigs "$out/ssa_sigs.json" > "$out/sigmatch_make.txt" 2>&1

echo "[3/3] sigmatch against this disc"
python3 -I tools/sigmatch/sigmatch.py match docs/giants/ssa_hooks.json "$out/image.bin" --sigs "$out/ssa_sigs.json" \
    --out "$out/sigmap.json" --toml > "$out/sigmatch.txt"
echo "entries at confidence 1.0: $(grep -c ' 1.0 ' "$out/sigmatch.txt" || true)"
echo "below 1.0 (check by hand):"; grep -E "confidence|^[a-zA-Z_]+ +0x" "$out/sigmatch.txt" | grep -v " 1.0" | grep -v "# confidence 1.0" | head -20 || true
echo "done. Next: docs/porting/day1.md step 2 (XenonRecomp config)."
