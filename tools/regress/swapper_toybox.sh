#!/bin/bash
# Swap Force: a swapper is ONE card in the Toy Box and both its tags go on the portal ([SwapForce];
# docs/runtime/host/toybox_ux.md "Swappers", games/swapforce/docs/status.md). The collection holds one linked swapper
# (Blast Zone: a top, toy 2004, and a bottom, toy 1004). The run starts the game on an empty portal, takes the Toy Box
# at the portal prompt, puts the card on player 1 (both tags: slots 0 and 1), then takes it off and puts it on again.
# Checks the log: one card, both slots, the game took it and opened both halves' archives, taken off, put on again.
#
#   SWAPFORCE_ISO=<Swap Force .iso> [SWAPPER_ART_DATA=<data folder>] tools/regress/swapper_toybox.sh [<run name>]
#                                                                                       (about 2 minutes; build-swapforce/)
# SWAPPER_ART_DATA: a scratch data folder where the launcher's extraction already ran (ArkeyanArtTest with SWAPFORCE_ISO and
# SKYLANDERS_DATA=<it>: <it>/SwapForce/portraits/artworks holds the swapper halves' pictures). It is copied into the run's data, and
# the check also wants the joined picture (the game itself only reads the portraits; the launcher extracts the large pictures).
#
# Isolated like every run (docs/getting-started/testing.md): scratch collection, save, data, settings; offscreen video,
# disk audio; kills only its own pid. Everything is under private/runs/<name>. Exit 0: all checks pass.
set -u
root=$(cd "$(dirname "$0")/../.." && pwd)
name=${1:-swapper_toybox}
iso=${SWAPFORCE_ISO:?set SWAPFORCE_ISO to the Swap Force disc image}
exe="${LIVE_EXE:-$root/build-swapforce/ArkeyanRuntime/Skylanders Swap Force}"
make=${FIGURE_TOOL:-$root/build-swapforce/ArkeyanRuntime/ArkeyanFigureTest}
R=$root/private/runs/$name
rm -rf "${R:?}"; mkdir -p "$R/collection" "$R/save" "$R/data"
[ -n "${SWAPPER_ART_DATA:-}" ] && cp -R "$SWAPPER_ART_DATA/SwapForce" "$R/data/"

# One new swapper: two dumps, linked in collection.json ("partner" on both; docs/runtime/host/collection.md), and a loose
# bottom (Fire Kraken) for "Change the bottom".
for toy in 2004 1004 1005; do "$make" --make "$R/collection/new_$toy.sky" $toy > /dev/null || { echo "cannot make the dumps"; exit 2; }; done
cat > "$R/collection/collection.json" <<'EOF'
{
  "format": 1,
  "figures": {
    "new_2004.sky": { "partner": "new_1004.sky", "origin": "created" },
    "new_1004.sky": { "partner": "new_2004.sky", "origin": "created" }
  }
}
EOF

# A every 2 s through the title, Story Mode, the save slot (to the portal prompt: no B, it would close the Toy Box).
press="a:14,a:16,a:18,a:20,a:22,a:24,a:26"
# The prompt opens the Toy Box: Return puts the first card on P1 (the swapper: the loose bottom comes after it); F6 opens
# the Toy Box again (its cursor is on the loose bottom: Left), Delete takes the card off the seat, Return puts it back. Then
# F2 (the figure page of the card under the cursor; the Toy Box is still open), Down x3 to "Change the bottom", Return
# (the halves), Return (the loose bottom).
script="Return:75,F6:88,Left:89,Delete:90,Return:92,shot=swapper:94,F2:100,Down:101,Down:102,Down:103,Return:104,shot=half:105,Return:107,shot=changed:109"
env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null \
  SKYLANDERS_DATA="$R/data" SKY_SETTINGS="$R/none.ini" SKY_LAUNCHER_INI="$R/none.ini" SKY_COLLECTION="$R/collection" \
  SKY_SAVE_DIR="$R/save" SKY_LOG="$R/game.log" SKY_SKIP_MOVIES=1 SKY_PRESS="$press" SKY_PORTAL=emulated \
  SKY_PORTAL_LOG=1 SKY_TOYBOX_SCRIPT="$script" SKY_TOYBOX_SHOTS="$R/shots" \
  nice "$exe" --vsync 0 --fps-limit 30 "$iso" > "$R/log" 2>&1 &
pid=$!; echo $pid > "$R/pid"
(sleep "${SWAPPER_SECONDS:-125}"; kill $pid 2>/dev/null; sleep 3; kill -9 $pid 2>/dev/null) > /dev/null 2>&1 &
timer=$!
wait $pid; { kill $timer && wait $timer; } 2>/dev/null

log=$R/game.log; [ -f "$log" ] || log=$R/log
fails=0
check() { # <what> <fixed text> [<count>]
    local n; n=$(grep -cF -- "$2" "$log" "$R/log" | awk -F: '{s += $NF} END {print s}')
    if [ "$n" -ge "${3:-1}" ]; then echo "ok:   $1"; else echo "FAIL: $1 (wanted '$2' x${3:-1}, saw $n)"; fails=$((fails + 1)); fi
}
check "the Toy Box lists the 3 dumps as 2 cards (the swapper is one)" "[toybox] 2 cards (a swapper is one)"
check "the card goes on P1 with both tags (slot 0, slot 1)" "P1: Blast Zone on slot 0 and slot 1 (bottom)"
check "top on the portal"    "new_2004.sky (toy type 2004) in slot 0"
check "bottom on the portal" "new_1004.sky (toy type 1004) in slot 1"
[ -n "${SWAPPER_ART_DATA:-}" ] && check "the card is drawn as its two halves joined" "[toybox] swapper picture Blast Zone (top) + Blast Zone (bottom): joined"
check "the game took the card" "the game took Blast Zone"
check "the game opened the top's archive"    "body_fireknight.pak"
check "the game opened the bottom's archive" "legs_fireknight.pak"
check "taken off as one card" "P1: Blast Zone taken off"
check "put on again (second time)" "P1: Blast Zone on slot 0 and slot 1 (bottom)" 2
check "Change the bottom" "Halves changed"
check "the changed card goes back on P1" "P1: Blast Kraken on slot 0 and slot 1 (bottom)"
if python3 -I - "$R/collection/collection.json" <<'PY'
import json, sys
f = json.load(open(sys.argv[1]))["figures"]
sys.exit(0 if f["new_2004.sky"].get("partner") == "new_1005.sky" and f["new_1005.sky"].get("partner") == "new_2004.sky" and not f["new_1004.sky"].get("partner") else 1)
PY
then echo "ok:   collection.json: the top is linked to the new bottom, the old bottom is loose"; else echo "FAIL: collection.json links after the change"; fails=$((fails + 1)); fi
[ "$fails" = 0 ] && echo "swapper Toy Box check: PASS" || echo "swapper Toy Box check: $fails FAILED (logs in $R)"
exit $((fails != 0))
