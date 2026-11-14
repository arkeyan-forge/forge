#!/bin/sh
# Fetch a public llvm-mingw archive, verify it before extraction, and print its installation directory.
# Both host archives are pinned below; LLVM_MINGW_SHA256 can override the digest for an explicit local check.
# The Linux pin was verified against the release asset's GitHub SHA-256 digest.
set -eu

root=$(cd "$(dirname "$0")/../.." && pwd)
version=20260922
case $(uname -s) in
    Darwin) default_archive=llvm-mingw-$version-ucrt-macos-universal.tar.xz ;;
    Linux) default_archive=llvm-mingw-$version-ucrt-ubuntu-22.04-x86_64.tar.xz ;;
    *) echo "error: unsupported llvm-mingw host" >&2; exit 1 ;;
esac
archive=${LLVM_MINGW_ARCHIVE:-$default_archive}
case "$archive" in
    llvm-mingw-$version-ucrt-macos-universal.tar.xz)
        checksum=${LLVM_MINGW_SHA256:-52e5f5a7b131021d0c39a37a38fa380a1da7885cd04bd61afd0cd4ecfb8bc1f3}
        ;;
    llvm-mingw-$version-ucrt-ubuntu-22.04-x86_64.tar.xz)
        checksum=${LLVM_MINGW_SHA256:-bb7bb7654b33d5aa8712acb837c963b2e0c56352560c76105270a3268c665c21}
        ;;
    *) echo "error: unsupported llvm-mingw release asset" >&2; exit 1 ;;
esac
printf '%s\n' "$checksum" | LC_ALL=C grep -Eq '^[0-9a-f]{64}$' ||
    { echo "error: LLVM_MINGW_SHA256 must be a lowercase SHA-256 digest" >&2; exit 1; }

deps=${KIT_DEPS:-$root/private}
downloads=$deps/toolchains/download
mkdir -p "$downloads"
if [ ! -f "$downloads/$archive" ]; then
    curl --proto '=https' --tlsv1.2 -fL --retry 3 -o "$downloads/$archive.part" \
        "https://github.com/mstorsjo/llvm-mingw/releases/download/$version/$archive"
    mv "$downloads/$archive.part" "$downloads/$archive"
fi
printf '%s  %s\n' "$checksum" "$downloads/$archive" | shasum -a 256 -c - >&2
# Always extract the checked archive, including on a repeated invocation.
tar -xJf "$downloads/$archive" -C "$deps/toolchains"
installation=$deps/toolchains/${archive%.tar.xz}
[ -x "$installation/bin/x86_64-w64-mingw32-clang" ] && [ -f "$installation/LICENSE.TXT" ] ||
    { echo "error: llvm-mingw archive is missing the compiler or license" >&2; exit 1; }
printf '%s\n' "$installation"
