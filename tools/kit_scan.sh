#!/bin/sh
# Fails if anything personal is in a kit: the build machine's user or paths, names from a local list, e-mail
# addresses. Binaries are read with strings (UTF-16 text too: NULs removed), text with grep,
# and compressed archives (the corresponding-source tarballs) are decompressed first. Names of files count as well.
# A kit's files.sha256 (tools/make_kit.sh) must list exactly its other files, with their hashes.
#
#   tools/kit_scan.sh <kit dir>
#
# The addresses and build paths that third parties wrote into their own files are theirs, not ours: e-mail addresses
# are refused outside licenses/, include/simde/ and the notices (upstream copyright lines), and "/Users/<other>" paths in prebuilt
# upstream binaries (dxc, the llvm-mingw runtime) are not looked for, only the words above.
set -u
export LC_ALL=C

[ $# -eq 1 ] && [ -d "$1" ] || { echo "usage: $0 <kit dir>" >&2; exit 2; }
kit=$1
# The words that must never appear: the build machine's user name, plus a local list kept out of the repository
# (private/kit_scan_words.txt, git-ignored, one word per line; KIT_SCAN_WORDS points elsewhere).
# KIT_SCAN_USER names the host's user when the kit is built in a container (make_kit.sh sets it): there `id -un` has
# no name for the user's uid and prints the number, which matches ordinary text.
words=${KIT_SCAN_USER:-$(id -un 2>/dev/null)} || words=
list=${KIT_SCAN_WORDS:-$(dirname "$0")/../private/kit_scan_words.txt}
if [ -f "$list" ]; then
    extra=$(grep -v '^#' "$list" | grep -v '^[[:space:]]*$' | paste -sd'|' -)
    [ -n "$extra" ] && words="${words:+$words|}$extra"
else
    echo "kit_scan: warning: no local word list ($list); checking the user name only" >&2
fi
email='[A-Za-z0-9._%+-]+@[A-Za-z0-9-]+\.[A-Za-z]{2,}'
bad=0
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

report() { # <what> <file>
    echo "kit_scan: $1 in $2" >&2
    sed 's/^/    /' "$tmp/hits" | head -5 >&2
    bad=1
}

# <file> <stream command...>: scans the stream the command prints
scan_stream() { # <label> <command...>
    label=$1
    shift
    "$@" > "$tmp/stream" 2>/dev/null
    { strings -a -n 4 "$tmp/stream"; tr -d '\000' < "$tmp/stream" | strings -a -n 6; } | sort -u > "$tmp/text"
    if grep -Eai "$words" "$tmp/text" > "$tmp/hits"; then report "personal string" "$label"; fi
    case "$label" in
    "$kit/licenses/"* | "$kit/include/simde/"* | "$kit/res/THIRD_PARTY_NOTICES.md") ;;
    *)
        # Text files: any address. Binaries hold look-alikes ("a@b.Type"): only mail providers' domains there.
        if grep -qI . "$label" 2>/dev/null; then pattern=$email; else pattern="[A-Za-z0-9._%+-]+@(gmail|googlemail|outlook|hotmail|live|icloud|me|yahoo|proton|protonmail)\\.[a-z]{2,}"; fi
        if grep -Eao "$pattern" "$tmp/text" > "$tmp/hits"; then report "e-mail address" "$label"; fi
        ;;
    esac
}

find "$kit" -type f | sort > "$tmp/files"
while IFS= read -r f; do
    if printf '%s\n' "${f#"$kit"/}" | grep -Eai "$words" > "$tmp/hits"; then report "personal file name" "$f"; fi
    case "$f" in
    *.tar.gz) scan_stream "$f" gzip -dc "$f" ;;
    *.tar.xz) scan_stream "$f" xz -dc "$f" ;;
    esac
    scan_stream "$f" cat "$f"
done < "$tmp/files"

# The kit's file list (tools/make_kit.sh), when it has one: every other file named once, each with its hash.
if [ -f "$kit/files.sha256" ]; then
    (cd "$kit" && find . -type f ! -name files.sha256 | sed 's|^\./||' | sort) > "$tmp/actual"
    sed 's/^[0-9a-f]\{64\} [ *]//' "$kit/files.sha256" | sort > "$tmp/listed"
    if ! cmp -s "$tmp/listed" "$tmp/actual"; then
        diff "$tmp/listed" "$tmp/actual" | grep '^[<>]' > "$tmp/hits"
        report "a file list that is not the kit's files (< listed only, > not listed)" "$kit/files.sha256"
    elif ! (cd "$kit" && shasum -a 256 -c --quiet --strict files.sha256) > "$tmp/hits" 2>&1; then
        report "a file that differs from files.sha256" "$kit/files.sha256"
    fi
fi

if [ $bad -ne 0 ]; then
    echo "kit_scan: FAILED: $kit" >&2
    exit 1
fi
echo "kit_scan: clean: $kit ($(wc -l < "$tmp/files" | tr -d ' ') files)"
