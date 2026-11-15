#!/bin/sh
# One check inside the container of tools/ci/run.sh (tools/ci/checks.Dockerfile): format, tidy, linux, sanitizers or
# windows. The checkout is mounted read only at /repo and copied into the job's volume (/work), so rebuilds are
# incremental; no game data is copied (private/ stays out). CHANGED lists the changed files (format, tidy);
# TIDY_ALL=1 checks every file of the compile database and only reports.
set -eu

job=${1:?usage: job.sh format|tidy|linux|sanitizers|windows}
mkdir -p /work/src
rsync -a --delete --exclude .git --exclude '/build*' --exclude /private --exclude /dist --exclude /.scratch \
    /repo/ /work/src/
cd /work/src
tests="-DSKY_TESTS_ONLY=ON -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++"
# Left out under amd64 emulation (Rosetta), all run natively by the macos job (2026-11-15): the Toy Box link's timed
# socket exchanges miss their deadlines.
emulated='ArkeyanToyboxLinkTest'

case "$job" in
    format)
        files=$(printf '%s\n' "${CHANGED:-}" | grep -E '\.(cpp|h|hpp)$' |
            grep -vE '^(thirdparty|private|ArkeyanRecompLib|lib)/' || true)
        [ -z "$files" ] || clang-format --dry-run --Werror --style=file $files
        echo "format: $(printf '%s' "$files" | grep -c . || true) file(s)"
        ;;
    tidy)
        cmake -S . -B /work/build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo $tests -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
        cmake --build /work/build --parallel 4
        # Changed files must be clean. TIDY_ALL lists the findings of every file by check without failing: the older
        # code still has findings, paid down file by file.
        python3 - <<'PY'
import json, os, subprocess, sys
db = {os.path.relpath(e["file"], os.getcwd()) for e in json.load(open("/work/build/compile_commands.json"))}
ours = {f for f in db if f.endswith(".cpp") and not f.startswith(("thirdparty/", "private/", "ArkeyanRecompLib/", "build"))}
every = os.environ.get("TIDY_ALL") == "1"
files = sorted(ours) if every else sorted(set(os.environ.get("CHANGED", "").split()) & ours)
print(f"clang-tidy: {len(files)} file(s)")
if files and every:
    out = subprocess.run(["run-clang-tidy", "-quiet", "-p", "/work/build"] + files, capture_output=True, text=True).stdout
    found = {line for line in out.split("\n") if ": warning: " in line or ": error: " in line}
    checks = {}
    for line in found:
        name = line.rsplit("[", 1)[-1].rstrip("]") if line.endswith("]") else "other"
        checks[name] = checks.get(name, 0) + 1
    for name, count in sorted(checks.items(), key=lambda item: -item[1]):
        print(f"{count:6} {name}")
    print(f"clang-tidy report: {len(found)} distinct finding(s)")
elif files:
    sys.exit(subprocess.run(["run-clang-tidy", "-quiet", "-p", "/work/build", "-warnings-as-errors=*"] + files).returncode)
PY
        ;;
    linux)
        cmake -S . -B /work/build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DSKY_WERROR=ON $tests
        cmake --build /work/build --parallel 4
        ctest --test-dir /work/build --output-on-failure -E "^($emulated)\$"
        ;;
    sanitizers)
        cmake -S . -B /work/build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DSKY_SANITIZE=address $tests
        cmake --build /work/build --parallel 4
        ctest --test-dir /work/build --output-on-failure -E "^($emulated)\$"
        ;;
    windows)
        # The kit's toolchain, not MSVC, so the tests build what players get (tools/build_windows.sh, kit.txt).
        toolchain=$(KIT_DEPS=/work/deps sh tools/ci/fetch_llvm_mingw.sh)
        export WINEPREFIX=/work/wine
        [ -d "$WINEPREFIX/drive_c/windows" ] || xvfb-run -a wineboot -i
        # libc++ and libunwind of llvm-mingw, found by Wine on WINEPATH (Z: is Wine's /).
        export WINEPATH="Z:$(printf '%s' "$toolchain/x86_64-w64-mingw32/bin" | tr / '\\')"
        export CPATH=/work/src/cmake/toolchains/mingw-case
        cmake -S . -B /work/build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DSKY_WERROR=ON -DSKY_TESTS_ONLY=ON \
            -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/x86_64-w64-mingw32-clang.cmake -DLLVM_MINGW_ROOT="$toolchain" \
            -DCMAKE_CROSSCOMPILING_EMULATOR=wine
        cmake --build /work/build --parallel 4
        # Also left out under Wine (2026-11-15), each passing on Windows and run natively by the macos job: file locks
        # and sharing modes (Collection, VersionResource), and Rosetta refusing the write watch's exception path
        # ("invalid gdt selector index").
        xvfb-run -a ctest --test-dir /work/build --output-on-failure \
            -E "^($emulated|ArkeyanCollectionTest|ArkeyanVersionResourceTest|ArkeyanWriteWatchTest)\$"
        ;;
    *) echo "error: unknown job $job" >&2; exit 2 ;;
esac
