#!/usr/bin/env bash
# The host-side checks, run on a Mac (docs/getting-started/testing.md, "Checks"): no game data, no disc. In Docker
# (tools/ci/checks.Dockerfile, amd64): clang-format and clang-tidy on the changed files, the host tests on Linux, with
# AddressSanitizer, and on Windows (llvm-mingw) under Wine. Natively: the host tests on macOS.
#
#   tools/ci/run.sh [--quick] [--only job,job] [--base REF] [--tidy-all]
#
# Jobs: format, tidy, macos, linux, sanitizers, windows. --quick runs format and macos (the pre-push hook). The changed
# files are those since --base (default: the merge base with origin/main). --tidy-all lists clang-tidy's findings in
# every file, as a report. Logs: .scratch/checks/<job>.log. Exit code 0: every job passed. The game checks stay
# tools/test_all.sh.
set -uo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root" || exit 1
jobs=""; base=""; export TIDY_ALL=0
while [ $# -gt 0 ]; do
    case $1 in
        --quick) jobs=format,macos ;;
        --only) shift; jobs=${1:?--only needs a list of jobs} ;;
        --base) shift; base=${1:?--base needs a ref} ;;
        --tidy-all) TIDY_ALL=1 ;;
        *) echo "usage: tools/ci/run.sh [--quick] [--only job,job] [--base REF] [--tidy-all]" >&2; exit 2 ;;
    esac
    shift
done
[ -n "$jobs" ] || jobs=format,tidy,macos,linux,sanitizers,windows
[ -n "$base" ] || base=$(git merge-base HEAD origin/main)
renice -n 10 $$ >/dev/null
logs=$root/.scratch/checks
mkdir -p "$logs"
# Committed and uncommitted changes since the base, deleted files left out.
CHANGED=$( (git diff --name-only --diff-filter=d "$base"; git ls-files --others --exclude-standard) | sort -u)
export CHANGED
# amd64 for every container job: the build runs XenosRecomp's dxc-bin (x86_64 only), and Wine runs x86_64 programs.
native=linux/amd64
# One volume per checkout and job keeps each build incremental.
key=$(printf '%s' "$root" | shasum | cut -c1-8)

container() { # job platform
    local image=arkeyan-checks-${2#linux/}
    docker build -q --platform "$2" -t "$image" -f tools/ci/checks.Dockerfile tools/ci >/dev/null || return 1
    docker run --rm --platform "$2" -e CHANGED -e TIDY_ALL -v "$root:/repo:ro" -v "arkeyan-checks-$key-$1:/work" \
        "$image" /repo/tools/ci/job.sh "$1"
}

macos() {
    # No SKY_WERROR: plume's Metal header and Mac-only unused code still warn (2026-11-15); linux and windows keep it.
    cmake -S . -B build-ci-macos -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DSKY_WERROR=OFF -DSKY_TESTS_ONLY=ON || return 1
    cmake --build build-ci-macos --parallel 4 || return 1
    ctest --test-dir build-ci-macos --output-on-failure
}

failed=0
summary=""
for job in ${jobs//,/ }; do
    case $job in
        macos) run=macos ;;
        format | tidy | linux | sanitizers) run="container $job $native" ;;
        windows) run="container windows linux/amd64" ;;
        *) echo "error: unknown job $job" >&2; exit 2 ;;
    esac
    start=$SECONDS
    printf '%-11s ' "$job"
    if $run >"$logs/$job.log" 2>&1; then result=PASS; else result=FAIL; failed=1; fi
    echo "$result ($((SECONDS - start))s)"
    [ $result = PASS ] || tail -20 "$logs/$job.log" | sed 's/^/    /'
    summary+=$(printf '%-11s %s (%ss)' "$job" "$result" $((SECONDS - start)))$'\n'
done
printf '%s' "$summary" >"$logs/summary.txt"
[ $failed -eq 0 ] && echo "all jobs passed" || echo "failed: see $logs/<job>.log"
exit $failed
