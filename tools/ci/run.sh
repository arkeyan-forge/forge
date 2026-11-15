#!/usr/bin/env bash
# The host-side checks, run on a Mac (docs/getting-started/testing.md, "Checks"): no game data, no disc. In Docker
# (tools/ci/checks.Dockerfile, amd64): clang-format and clang-tidy on the changed files, the host tests on Linux, with
# AddressSanitizer, and on Windows (llvm-mingw) under Wine. Natively: the host tests on macOS.
#
#   tools/ci/run.sh [--quick] [--only job,job] [--base REF] [--tidy-all] [--serial] [--force]
#
# Jobs: format, tidy, macos, linux, sanitizers, windows, run at the same time (--serial: one after the other). --quick
# runs format and macos (the pre-push hook). The changed files are those since --base (default: the merge base with
# origin/main). --tidy-all lists clang-tidy's findings in every file, as a report. A job that passed on the same tree
# (commits, uncommitted and untracked files but the docs, submodule commits, options) is skipped; --force runs it
# again. Logs: .scratch/checks/<job>.log. Exit code 0: every job passed. The game checks stay tools/test_all.sh.
set -uo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root" || exit 1
jobs=""; base=""; serial=0; force=0; export TIDY_ALL=0
while [ $# -gt 0 ]; do
    case $1 in
        --quick) jobs=format,macos ;;
        --only) shift; jobs=${1:?--only needs a list of jobs} ;;
        --base) shift; base=${1:?--base needs a ref} ;;
        --tidy-all) TIDY_ALL=1 ;;
        --serial) serial=1 ;;
        --force) force=1 ;;
        *) echo "usage: tools/ci/run.sh [--quick] [--only job,job] [--base REF] [--tidy-all] [--serial] [--force]" >&2
            exit 2 ;;
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
# files pathspec...: the tracked and untracked files those match, with their contents.
files() {
    git ls-files -s -- "$@"
    git diff HEAD --binary -- "$@"
    git ls-files --others --exclude-standard -- "$@"
    git ls-files -z --others --exclude-standard -- "$@" | xargs -0 git hash-object --
}
# What the jobs check; a job that passed on the same state is skipped. The docs change no result: *.md is left out,
# except the notices the builds ship.
state=$({
    files . ':(exclude,glob)**/*.md'
    files ':(glob)**/THIRD_PARTY_NOTICES.md'
    git submodule status
    echo "$base $TIDY_ALL"
} | shasum | cut -c1-40)
# amd64 for every container job: the build runs XenosRecomp's dxc-bin (x86_64 only), and Wine runs x86_64 programs.
native=linux/amd64
# One volume per checkout and job keeps each build incremental.
key=$(printf '%s' "$root" | shasum | cut -c1-8)

image() { # platform
    docker build -q --platform "$1" -t "arkeyan-checks-${1#linux/}" -f tools/ci/checks.Dockerfile tools/ci >/dev/null
}

container() { # job platform
    local image=arkeyan-checks-${2#linux/}
    docker run --rm --platform "$2" -e CHANGED -e TIDY_ALL -v "$root:/repo:ro" -v "arkeyan-checks-$key-$1:/work" \
        "$image" /repo/tools/ci/job.sh "$1"
}

macos() {
    # No SKY_WERROR: plume's Metal header and Mac-only unused code still warn (2026-11-15); linux and windows keep it.
    cmake -S . -B build-ci-macos -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DSKY_WERROR=OFF -DSKY_TESTS_ONLY=ON || return 1
    cmake --build build-ci-macos --parallel 4 || return 1
    ctest --test-dir build-ci-macos --output-on-failure
}

# Parallel jobs would build the shared image at the same time: it is built once, first.
case ",$jobs," in
    *,format,* | *,tidy,* | *,linux,* | *,sanitizers,* | *,windows,*)
        image $native >"$logs/image.log" 2>&1 ||
            { echo "error: image build failed: see $logs/image.log" >&2; exit 1; } ;;
esac
# run_job job command: the log, the result and, on a pass, the state it passed on.
run_job() {
    local start=$SECONDS result
    if $2 >"$logs/$1.log" 2>&1; then
        echo "$state" >"$logs/$1.pass"; result=PASS
    else
        rm -f "$logs/$1.pass"; result=FAIL
    fi
    echo "$result $((SECONDS - start))" >"$logs/$1.result"
}

# Background jobs ignore Ctrl-C in a script: stop the whole process group.
trap 'trap - TERM; kill -TERM 0; exit 130' INT TERM
names=(); runs=(); pids=()
for job in ${jobs//,/ }; do
    case $job in
        macos) run=macos ;;
        format | tidy | linux | sanitizers | windows) run="container $job $native" ;;
        *) echo "error: unknown job $job" >&2; exit 2 ;;
    esac
    rm -f "$logs/$job.result" "$logs/$job.parallel.log"
    names+=("$job"); runs+=("$run")
    if [ $force -eq 0 ] && [ "$(cat "$logs/$job.pass" 2>/dev/null)" = "$state" ]; then
        echo "SKIP 0" >"$logs/$job.result"
        pids+=("")
        continue
    fi
    run_job "$job" "$run" &
    pids+=($!)
    [ $serial -eq 0 ] || wait $!
done

failed=0
summary=""
again=()
for i in "${!names[@]}"; do
    job=${names[$i]}
    [ -z "${pids[$i]}" ] || wait "${pids[$i]}"
    read -r result took <"$logs/$job.result" || { result=FAIL; took=0; }
    case $result in
        SKIP) line=$(printf '%-11s SKIP (passed on this tree)' "$job") ;;
        *) line=$(printf '%-11s %s (%ss)' "$job" "$result" "$took") ;;
    esac
    echo "$line"
    summary+="$line"$'\n'
    [ "$result" != FAIL ] && continue
    if [ $serial -eq 0 ]; then again+=("$i"); else failed=1; tail -20 "$logs/$job.log" | sed 's/^/    /'; fi
done
# Timing checks can miss their deadlines while the other jobs load the machine: a job that failed among them runs again
# alone, its first log kept as <job>.parallel.log.
for i in ${again[@]+"${again[@]}"}; do
    job=${names[$i]}
    mv "$logs/$job.log" "$logs/$job.parallel.log"
    run_job "$job" "${runs[$i]}"
    read -r result took <"$logs/$job.result" || { result=FAIL; took=0; }
    line=$(printf '%-11s %s alone (%ss; failed among the others: %s)' "$job" "$result" "$took" "$job.parallel.log")
    echo "$line"
    summary+="$line"$'\n'
    [ "$result" = PASS ] || { failed=1; tail -20 "$logs/$job.log" | sed 's/^/    /'; }
done
printf '%s' "$summary" >"$logs/summary.txt"
[ $failed -eq 0 ] && echo "all jobs passed" || echo "failed: see $logs/<job>.log"
exit $failed
