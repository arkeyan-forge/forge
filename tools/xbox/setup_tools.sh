#!/usr/bin/env bash
# Fetches and builds what the Xbox pipeline needs, into private/xbox/ (or $SKY_XBOX_TOOLS):
#   toolchain/llvm-mingw   llvm-mingw (clang, lld, libc++, UCRT; Apache-2.0 with LLVM exception), used by
#                          cmake/toolchains/uwp-x64-llvm-mingw.cmake
#   bin/makemsix           makemsix from Microsoft's msix-packaging (MIT), with patches/makemsix-store-iso.patch
#   bin/osslsigncode       osslsigncode (GPL-3.0, run as a separate program) to sign the package
# macOS only for now. Needs git, curl, cmake, ninja, the Xcode command line tools and Homebrew's openssl@3.
# Running it again skips what is already there.
set -euo pipefail

LLVM_MINGW_VERSION=20260922
LLVM_MINGW_ASSET=llvm-mingw-$LLVM_MINGW_VERSION-ucrt-macos-universal.tar.xz
LLVM_MINGW_SHA256=52e5f5a7b131021d0c39a37a38fa380a1da7885cd04bd61afd0cd4ecfb8bc1f3
MSIX_PACKAGING_COMMIT=25a65f5c1690930813bcc10cdf1d59fa865f2bb1
OSSLSIGNCODE_TAG=2.14
JOBS=4

here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../.." && pwd)"
out="${SKY_XBOX_TOOLS:-${SSA_XBOX_TOOLS:-$root/private/xbox}}"

[[ "$(uname -s)" == Darwin ]] || { echo "error: setup_tools.sh supports macOS only for now" >&2; exit 1; }
for tool in git curl cmake ninja shasum; do
    command -v "$tool" >/dev/null || { echo "error: $tool not found" >&2; exit 1; }
done
openssl_root="$(brew --prefix openssl@3 2>/dev/null || true)"
[[ -n "$openssl_root" && -d "$openssl_root/include/openssl" ]] ||
    { echo "error: Homebrew openssl@3 not found (brew install openssl@3)" >&2; exit 1; }
mkdir -p "$out/dl" "$out/toolchain" "$out/src" "$out/build" "$out/bin"

# --- llvm-mingw ---------------------------------------------------------------------------------------------------
if [[ ! -x "$out/toolchain/llvm-mingw/bin/x86_64-w64-mingw32uwp-clang" ]]; then
    archive="$out/dl/$LLVM_MINGW_ASSET"
    if [[ ! -f "$archive" ]] || ! shasum -a 256 "$archive" | grep -q "^$LLVM_MINGW_SHA256 "; then
        echo "Downloading $LLVM_MINGW_ASSET"
        curl -fL --retry 3 -o "$archive.part" \
            "https://github.com/mstorsjo/llvm-mingw/releases/download/$LLVM_MINGW_VERSION/$LLVM_MINGW_ASSET"
        mv "$archive.part" "$archive"
    fi
    shasum -a 256 "$archive" | grep -q "^$LLVM_MINGW_SHA256 " ||
        { echo "error: $archive does not match the pinned SHA-256" >&2; exit 1; }
    rm -rf "$out/toolchain/llvm-mingw" "$out/toolchain/${LLVM_MINGW_ASSET%.tar.xz}"
    tar -xJf "$archive" -C "$out/toolchain"
    mv "$out/toolchain/${LLVM_MINGW_ASSET%.tar.xz}" "$out/toolchain/llvm-mingw"
fi
echo "llvm-mingw: $out/toolchain/llvm-mingw"

# --- makemsix -----------------------------------------------------------------------------------------------------
# Stock makemsix deflates every file whose extension is not in its list of compressed formats; the patch adds .iso
# to that list, so the 8.7 GB disc image is stored as is (as MakeAppx /nc would).
msix_src="$out/src/msix-packaging"
if [[ ! -x "$out/bin/makemsix" ]]; then
    [[ -d "$msix_src/.git" ]] || git clone https://github.com/microsoft/msix-packaging.git "$msix_src"
    git -C "$msix_src" cat-file -e "$MSIX_PACKAGING_COMMIT^{commit}" 2>/dev/null || git -C "$msix_src" fetch origin
    git -C "$msix_src" checkout -q "$MSIX_PACKAGING_COMMIT"
    msix_patch="$here/patches/makemsix-store-iso.patch"
    git -C "$msix_src" apply --reverse --check "$msix_patch" 2>/dev/null || git -C "$msix_src" apply "$msix_patch"

    msix_build="$out/build/msix-packaging"
    cmake -S "$msix_src" -B "$msix_build" -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel \
        -DCMAKE_TOOLCHAIN_FILE="$msix_src/cmake/macos.cmake" -DMACOS=on -DCMAKE_OSX_ARCHITECTURES="$(uname -m)" \
        -DMSIX_PACK=on -DMSIX_SAMPLES=off -DMSIX_TESTS=off -DSKIP_BUNDLES=off -DUSE_VALIDATION_PARSER=on \
        -DXML_PARSER=xerces -DUSE_MSIX_SDK_ZLIB=on
    cmake --build "$msix_build" -j "$JOBS" --target makemsix

    # makemsix finds libmsix.dylib through an rpath to the build tree: point it next to the executable instead.
    cp "$msix_build/bin/makemsix" "$msix_build/lib/libmsix.dylib" "$out/bin/"
    install_name_tool -delete_rpath "$msix_build/lib" "$out/bin/makemsix" 2>/dev/null || true
    install_name_tool -add_rpath @executable_path "$out/bin/makemsix"
    codesign --force --sign - "$out/bin/libmsix.dylib" "$out/bin/makemsix"
fi
echo "makemsix: $out/bin/makemsix"

# --- osslsigncode -------------------------------------------------------------------------------------------------
osslsigncode_src="$out/src/osslsigncode"
if [[ ! -x "$out/bin/osslsigncode" ]]; then
    [[ -d "$osslsigncode_src/.git" ]] ||
        git clone --depth 1 --branch "$OSSLSIGNCODE_TAG" https://github.com/mtrojnar/osslsigncode.git "$osslsigncode_src"
    # Packages over 4 GB (the disc image comes first): osslsigncode 2.14 reads a zip64 extra field as if every value
    # were in it (makemsix, like MakeAppx, writes only those that overflow: the offset alone for the files after the
    # image), and writes the offset of the signature file it appends past 4 GB in 32 bits. Fix both, keeping the
    # minimal zip64 form (offset only, version needed 4.5).
    if ! grep -q "SSARecomp: zip64" "$osslsigncode_src/appx.c"; then
        perl -pi -e 's/if \(len >= (8|16|24)\) \{/if (len + 4 >= pos + 8) { \/* SSARecomp: zip64 *\//; s/if \(len >= 28\) \{/if (len + 4 >= pos + 4) {/' \
            "$osslsigncode_src/appx.c"
        perl -0pi -e 's|(\n(\s*)zipWriteLocalHeader\(bio, &dummy, &header\);\n)|\n$2if (offset >= UINT32_MAX) /* SSARecomp: zip64 signature entry */\n$2    header.version = 45;$1|; s|(\n(\s*)entry->offsetOfLocalHeader = offset;\n)|$1$2if (offset >= UINT32_MAX) {\n$2    entry->offsetInZip64 = 1;\n$2    entry->extraFieldLen = 12;\n$2}\n|' \
            "$osslsigncode_src/appx.c"
    fi
    [[ $(grep -c "SSARecomp: zip64" "$osslsigncode_src/appx.c") == 6 ]] ||
        { echo "error: could not adapt osslsigncode's appx.c" >&2; exit 1; }
    osslsigncode_build="$out/build/osslsigncode"
    cmake -S "$osslsigncode_src" -B "$osslsigncode_build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DOPENSSL_ROOT_DIR="$openssl_root"
    cmake --build "$osslsigncode_build" -j "$JOBS" --target osslsigncode
    cp "$osslsigncode_build/osslsigncode" "$out/bin/"
fi
echo "osslsigncode: $out/bin/osslsigncode"
