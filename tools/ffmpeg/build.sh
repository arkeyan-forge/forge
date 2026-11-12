#!/bin/sh
# Builds the minimal FFmpeg the runtime uses to decode XMA audio (apu/xma_decoder_ffmpeg.cpp): static libavcodec with
# only the WMA Pro decoder, and libavutil. FFmpeg is LGPL-2.1-or-later; no GPL or non-free part is enabled. Threads
# and everything else stay off: the runtime decodes one frame at a time on the thread that kicks the XMA context.
#
#   tools/ffmpeg/build.sh <target> [install prefix]
#
#   host         the build machine (macOS, Linux)
#   windows-x64  Windows 10+ desktop, with llvm-mingw
#   uwp-x64      Xbox One / Series X|S in Dev Mode (UWP AppContainer), with llvm-mingw: WINAPI_FAMILY_APP, no
#                instruction above -march=btver2 in compiled code (FFmpeg's SIMD is chosen at run time)
#   linux-x64    the Linux player kit, in its image (tools/linux/kit.Dockerfile): zig cc for glibc 2.34 (zig-cc)
#
# The default prefix is private/ffmpeg/<target>; configure the game with -DARKEYAN_FFMPEG_ROOT=<prefix>. Tested with
# Apple clang on macOS arm64 and llvm-mingw 20260922 (the Windows build decodes under Wine, the UWP one passes
# tools/xbox/check_uwp_imports.sh).
# llvm-mingw comes from LLVM_MINGW_ROOT, else private/xbox/toolchain/llvm-mingw. nasm builds the x86 SIMD code
# when it is on PATH (Homebrew: brew install nasm), otherwise plain C is used.
set -eu

VERSION=8.1.2
SHA256=464beb5e7bf0c311e68b45ae2f04e9cc2af88851abb4082231742a74d97b524c
URL="https://ffmpeg.org/releases/ffmpeg-$VERSION.tar.xz"

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
TARGET=${1:?usage: build.sh host|windows-x64|uwp-x64|linux-x64 [prefix]}
PREFIX=${2:-$ROOT/private/ffmpeg/$TARGET}
WORK=$ROOT/private/ffmpeg/work
JOBS=${JOBS:-4}

mkdir -p "$WORK" "$PREFIX"
PREFIX=$(cd "$PREFIX" && pwd)
ARCHIVE=$WORK/ffmpeg-$VERSION.tar.xz
if [ ! -f "$ARCHIVE" ]; then
    curl -fL --retry 3 -o "$ARCHIVE.part" "$URL"
    mv "$ARCHIVE.part" "$ARCHIVE"
fi
ACTUAL=$(shasum -a 256 "$ARCHIVE" | cut -d' ' -f1)
if [ "$ACTUAL" != "$SHA256" ]; then
    echo "error: $ARCHIVE has SHA-256 $ACTUAL, expected $SHA256" >&2
    exit 1
fi

SOURCE=$WORK/ffmpeg-$VERSION
[ -d "$SOURCE" ] || tar -xf "$ARCHIVE" -C "$WORK"
BUILD=$WORK/build-$TARGET-$(uname -s)-$(uname -m) # "host" differs between macOS and a Linux container
rm -rf "$BUILD"
mkdir -p "$BUILD"

set -- \
    --prefix="$PREFIX" \
    --enable-static --disable-shared \
    --disable-programs --disable-doc \
    --disable-avdevice --disable-avformat --disable-avfilter --disable-swresample --disable-swscale \
    --disable-network --disable-autodetect --disable-everything \
    --enable-decoder=wmapro \
    --disable-pthreads --disable-w32threads --disable-os2threads \
    --disable-debug

if command -v nasm > /dev/null; then
    set -- "$@" --x86asmexe=nasm
else
    set -- "$@" --disable-x86asm
fi

case "$TARGET" in
host)
    set -- "$@" --cc="${CC:-cc}" # configure's default is gcc, which the Linux build image does not have
    ;;
linux-x64)
    set -- "$@" --cc=zig-cc
    ;;
windows-x64 | uwp-x64)
    MINGW=${LLVM_MINGW_ROOT:-$ROOT/private/xbox/toolchain/llvm-mingw}
    if [ ! -x "$MINGW/bin/llvm-ar" ]; then
        echo "error: no llvm-mingw in $MINGW (set LLVM_MINGW_ROOT)" >&2
        exit 1
    fi
    TRIPLE=x86_64-w64-mingw32
    CFLAGS=
    if [ "$TARGET" = uwp-x64 ]; then
        TRIPLE=x86_64-w64-mingw32uwp # the wrapper defines WINAPI_FAMILY_APP and links the UWP umbrella libraries
        CFLAGS=-march=btver2
    fi
    set -- "$@" \
        --enable-cross-compile --target-os=mingw32 --arch=x86_64 \
        --cc="$TRIPLE-clang" --cxx="$TRIPLE-clang++" \
        --ar=llvm-ar --ranlib=llvm-ranlib --nm=llvm-nm \
        --strip=llvm-strip --extra-cflags="$CFLAGS"
    PATH=$MINGW/bin:$PATH # by name, so that the configure line (kept in the library) holds no path of this machine
    ;;
*)
    echo "error: unknown target $TARGET" >&2
    exit 1
    ;;
esac

cd "$BUILD"
"$SOURCE/configure" "$@" > configure.log 2>&1 || { tail -30 configure.log; exit 1; }
# The library keeps its configure line, and every object the build directory's path: neither may name this machine.
sed "s|$PREFIX|.|g; s|$ROOT|.|g" config.h > config.h.new && mv config.h.new config.h
# zig's glibc stubs still export sysctl, which glibc 2.32 removed with <sys/sysctl.h>: configure's link test finds it.
[ "$TARGET" = linux-x64 ] && sed -i 's/#define HAVE_SYSCTL 1/#define HAVE_SYSCTL 0/' config.h
echo "CFLAGS += -ffile-prefix-map=$ROOT=. -ffile-prefix-map=$WORK=." >> ffbuild/config.mak
make -j"$JOBS" > make.log 2>&1 || { tail -30 make.log; exit 1; }
make install > install.log 2>&1

# Provenance for the configure-time licence check (cmake/ffmpeg.cmake), and for whoever audits a distribution.
mkdir -p "$PREFIX/share"
{
    echo "version=$VERSION"
    echo "target=$TARGET"
    echo "license=LGPL-2.1-or-later"
    echo "configure=$*"
} > "$PREFIX/share/ssa-ffmpeg.txt"

echo "FFmpeg $VERSION ($TARGET) installed in $PREFIX"
