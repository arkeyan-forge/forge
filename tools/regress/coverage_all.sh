#!/usr/bin/env bash
# Coverage of all six games: every entry of each game's catalogue, read from its disc (levels and zones, hubs,
# challenges, arenas, modes, credits, every toy type of its accept list), one isolated run each, 30 s of play with the
# hero moving once the entry is reached (docs/status/COVERAGE.md).
#
#   tools/regress/coverage_all.sh <work dir below private/> [--games a,b] [--fsr] [--resume] [--jobs N] [--play S]
#                                [--only <regex on names or areas>]
#
# The discs: . ../isos.env (SSA_ISO, GIANTS_ISO, SWAPFORCE_ISO, TRAPTEAM_ISO, SUPERCHARGERS_ISO, IMAGINATORS_ISO). Games
# run in the order giants, ssa, swapforce, trapteam, superchargers, imaginators (--games picks some). Each run waits for
# a slot of ../gamelock.sh (COVERAGE_LOCK overrides); --jobs runs (default 2) at once. A slot taken serves up to 8
# runs in a row for COVERAGE_LEASE seconds (600), so a busy lock's queue is waited for once per lease. --fsr: every run with
# SKY_UPSCALER=fsr3. --resume keeps the rows already in each coverage.tsv and runs the rest (after a stop, Ctrl-C or
# kill: the runs in progress stop their games). --play: seconds of play per entry (default 30; Swap Zone pieces and
# lockpick puzzles 20; an entry that runs but cannot count as covered 15). COVERAGE_FIGURE_BATCH (20): figures per game
# run, each COVERAGE_FIGURE_SECONDS (8) on the portal.
#
# Writes <work dir>/<game>/catalogue.tsv (area, kind, name, route, arg, cap, reason), <work dir>/<game>/coverage.tsv
# (`# game <id>`, then kind, name, verdict, lowest_fps, detail, area, seconds; verdict ok, crash, hang, not_reached or
# not_testable), the log lines that matter (logs/) and one frame (shots/) per entry; each run's folder is deleted.
# Ends with one line per game and the wall time (also appended to <work dir>/summary.txt). Then:
#   tools/regress/status_report.py <work dir>/*/coverage.tsv --catalogue <work dir> ... (docs/status/STATUS.md, COVERAGE.md)
set -u
# macOS: no idle sleep while the run lasts (a sleeping Mac stops the games; their entries then time out).
if command -v caffeinate > /dev/null 2>&1; then
    exec caffeinate -i python3 -I "$(cd "$(dirname "$0")" && pwd)/coverage_all.py" "$@"
fi
exec python3 -I "$(cd "$(dirname "$0")" && pwd)/coverage_all.py" "$@"
