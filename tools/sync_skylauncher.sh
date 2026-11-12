#!/usr/bin/env bash
# Copies the figure and collection library (runtime/host/figures, runtime/host/collection), the disc picture readers
# (runtime/host/art, docs/runtime/host/game_art.md), the environment-variable helper (runtime/host/env.h), the catalogue, the embed and join CMake scripts and the games' art
# manifests (games/<id>/art.json) into a Skylauncher checkout (docs/release/launcher/README.md), or with --check only
# lists the files that differ. The copies in this repo are the reference: they have the tests.
#   tools/sync_skylauncher.sh [--check] <skylauncher dir>
set -euo pipefail
check=0
[[ "${1:-}" == "--check" ]] && { check=1; shift; }
[[ $# -eq 1 && -d "$1/src" ]] || { echo "usage: $0 [--check] <skylauncher dir>" >&2; exit 2; }
here="$(cd "$(dirname "$0")/.." && pwd)"
target="$1"

pairs=()
for f in figure_catalog figure_crypto figure_data figure_tag; do
    pairs+=("runtime/host/figures/$f.h:src/shared/figures/$f.h" "runtime/host/figures/$f.cpp:src/shared/figures/$f.cpp")
done
for f in json catalogue collection icons datadir save_backup game_data file_list portable_pack upgrades search progress achievements; do
    pairs+=("runtime/host/collection/$f.h:src/shared/collection/$f.h" "runtime/host/collection/$f.cpp:src/shared/collection/$f.cpp")
done
for f in xenos_texture alchemy_archive lux_images game_art game_text disc_languages; do
    pairs+=("runtime/host/art/$f.h:src/shared/art/$f.h" "runtime/host/art/$f.cpp:src/shared/art/$f.cpp")
done
pairs+=("runtime/host/dir_list.h:src/shared/dir_list.h" "runtime/host/env.h:src/shared/env.h")
pairs+=("runtime/host/runtime_names.h:src/shared/runtime_names.h")
pairs+=("resources/collection/catalogue.json:resources/collection/catalogue.json" "cmake/embed_file.cmake:cmake/embed_file.cmake")
# Each game's art manifest (where its pictures are on the disc), joined and built in by cmake/join_json.cmake.
pairs+=("cmake/join_json.cmake:cmake/join_json.cmake")
for manifest in "$here"/games/*/art.json; do
    id=$(basename "$(dirname "$manifest")")
    pairs+=("games/$id/art.json:resources/games/$id/art.json")
done

status=0
for pair in "${pairs[@]}"; do
    from="$here/${pair%%:*}" to="$target/${pair#*:}"
    if cmp -s "$from" "$to"; then
        continue
    fi
    if [[ $check -eq 1 ]]; then
        echo "differs: ${pair#*:}"
        status=1
    else
        mkdir -p "$(dirname "$to")"
        cp "$from" "$to"
        echo "copied: ${pair#*:}"
    fi
done
exit $status
