#!/bin/bash
# Swap Force's Toy Box shelf: the search field, the Filters panel, the tags with an x, Clear all, the count and the sort
# (docs/runtime/host/toybox_ux.md "Search, filters and sort"). A collection of 14 dumps (Blast Zone and Doom Stone are
# swappers, one card each: 12 cards on Player 1's shelf); the run starts the game on an empty portal, takes the Toy Box at the portal prompt and types and
# presses the pad through the script. Screenshots of the game's window (offscreen output) go to <run>/shots_*.png; the log's
# "[toybox] shelf:" lines give the count after each step. English by default, French with LANGUAGE=fr.
#
#   SWAPFORCE_ISO=<Swap Force .iso> [LANGUAGE=fr] tools/regress/search_toybox.sh [<run name>]     (about 2 minutes; build-swapforce/)
#
# Isolated like every run (docs/getting-started/testing.md): scratch collection, save, data, settings; offscreen video,
# disk audio; kills only its own pid. Everything is under private/runs/<name>. Exit 0: all checks pass.
set -u
root=$(cd "$(dirname "$0")/../.." && pwd)
language=${LANGUAGE:-en}
name=${1:-search_toybox_$language}
fire=fire; [ "$language" = fr ] && fire=feu
iso=${SWAPFORCE_ISO:?set SWAPFORCE_ISO to the Swap Force disc image}
exe="${LIVE_EXE:-$root/build-swapforce/ArkeyanRuntime/Skylanders Swap Force}"
make=${FIGURE_TOOL:-$root/build-swapforce/ArkeyanRuntime/ArkeyanFigureTest}
R=$root/private/runs/$name
rm -rf "${R:?}"; mkdir -p "$R/collection" "$R/save" "$R/data"
# SWAPPER_ART_DATA (see swapper_toybox.sh): a data folder where the launcher's extraction ran, so the swappers' cards show their stacked pictures.
[ -n "${SWAPPER_ART_DATA:-}" ] && cp -R "$SWAPPER_ART_DATA/SwapForce" "$R/data/"

# toy type and experience: Swap Force's cores, two swappers (a top and a bottom each, linked), an item, an adventure pack, and a
# Giants and a Spyro's Adventure figure (shown, they are read by Swap Force too).
for spec in 3000:197500 3004:40000 3008:90000 3010:120000 3014:0 2004:150000 1004:0 2003:20000 1003:0 3200:0 3300:0 100:50000 16:9000 14:0; do
    toy=${spec%%:*}; xp=${spec##*:}
    "$make" --make "$R/collection/new_$toy.sky" $toy xp=$xp > /dev/null || { echo "cannot make the dump of $toy"; exit 2; }
done
cat > "$R/collection/collection.json" <<'JSON'
{
  "format": 1,
  "figures": {
    "new_2004.sky": { "partner": "new_1004.sky", "origin": "created" },
    "new_1004.sky": { "partner": "new_2004.sky", "origin": "created" },
    "new_2003.sky": { "partner": "new_1003.sky", "origin": "created", "favourite": true },
    "new_1003.sky": { "partner": "new_2003.sky", "origin": "created" }
  }
}
JSON

press="a:14,a:16,a:18,a:20,a:22,a:24,a:26"
# The prompt opens the Toy Box. Typing goes straight into the field ("fire", "level 20", with a typo); Start opens Filters (Right
# changes a value, Down the row: Element Fire, Game Swap Force, Sort Name), B closes the panel, the tag's x and Clear all are clicked.
erase=""; for i in $(seq ${#fire}); do erase+=",Backspace:79.$i"; done # one Backspace per letter (one more would close the Toy Box)
script="shot=home:74,type=$fire:76,shot=fire:78$erase,type=niveau 20:81,shot=level:83"
script+=",Backspace:84,Backspace:84.2,Backspace:84.4,Backspace:84.6,Backspace:84.8,Backspace:85,Backspace:85.2,Backspace:85.4,Backspace:85.6"
script+=",type=swaper:87,shot=typo:89,Backspace:90,Backspace:90.2,Backspace:90.4,Backspace:90.6,Backspace:90.8,Backspace:91"
script+=",pad=start:92,shot=filters:93,pad=right*0.1:94,pad=right*0.1:94.5,pad=right*0.1:95,pad=right*0.1:95.5,pad=right*0.1:96"
script+=",pad=down*0.1:97,pad=right*0.1:98,pad=right*0.1:98.5,pad=right*0.1:99,pad=down*0.1:100,pad=down*0.1:101,pad=right*0.1:102"
script+=",shot=filters_set:103,pad=b:104,shot=tags:105,click=290/254:106,shot=one_tag_less:107,pad=start:108,pad=up*0.1:109,pad=a:110,pad=b:111,shot=cleared:112"
env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null SKY_LANGUAGE=$language \
  SKYLANDERS_DATA="$R/data" SKY_SETTINGS="$R/none.ini" SKY_LAUNCHER_INI="$R/none.ini" SKY_COLLECTION="$R/collection" \
  SKY_SAVE_DIR="$R/save" SKY_LOG="$R/game.log" SKY_SKIP_MOVIES=1 SKY_PRESS="$press" SKY_PORTAL=emulated \
  SKY_PORTAL_LOG=1 SKY_TOYBOX_SCRIPT="$script" SKY_TOYBOX_SHOTS="$R/shots" \
  nice "$exe" --vsync 0 --fps-limit 30 "$iso" > "$R/log" 2>&1 &
pid=$!; echo $pid > "$R/pid"
(sleep "${SEARCH_SECONDS:-120}"; kill $pid 2>/dev/null; sleep 3; kill -9 $pid 2>/dev/null) > /dev/null 2>&1 &
timer=$!
wait $pid; { kill $timer && wait $timer; } 2>/dev/null

log=$R/game.log; [ -f "$log" ] || log=$R/log
fails=0
check() { # <what> <fixed text> [<count>]
    local n; n=$(grep -cF -- "$2" "$log" "$R/log" | awk -F: '{s += $NF} END {print s}')
    if [ "$n" -ge "${3:-1}" ]; then echo "ok:   $1"; else echo "FAIL: $1 (wanted '$2' x${3:-1}, saw $n)"; fails=$((fails + 1)); fi
}
check "the Toy Box lists the 14 dumps as 12 cards for Player 1 (a swapper is one; the item and the adventure pack go to other seats)" "[toybox] 12 cards (a swapper is one)"
check "typing '$fire' finds 2 cards (Fryno, Blast Zone)" "[toybox] shelf: 2 found for \"$fire\""
check "'niveau 20' finds Scratch only" "[toybox] shelf: 1 found for \"niveau 20\""
check "the typo 'swaper' still finds the 2 swappers" "[toybox] shelf: 2 found for \"swaper\""
check "the panel's filters: Fire, Swap Force, sorted by name" "(element 2, game swapforce, type -1, sort name)"
check "removing a tag keeps the others" "(element -1, game swapforce, type -1, sort name)"
check "Clear all" "[toybox] script: shot=cleared"
for shot in home fire level typo filters filters_set tags one_tag_less cleared; do
    if [ -s "${R}/shots_$shot.png" ]; then echo "ok:   shot $shot"; else echo "FAIL: shot $shot is missing"; fails=$((fails + 1)); fi
done
[ "$fails" = 0 ] && echo "search Toy Box check: PASS" || echo "search Toy Box check: $fails FAILED (logs in $R)"
exit $((fails != 0))
