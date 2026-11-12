#!/bin/sh
# Builds the player kits: dist/skylanders-kit-<platform>/ and its .zip (skylanders-kit-<platform>.zip, the only name the
# release carries, docs/release/launcher/updates_release.md).
# A kit holds Arkeyan-Builder, XenonRecomp, XenosRecomp, the
# prebuilt runtime of every game (everything but the game code: SDL, plume, FFmpeg, ... as static libraries and objects),
# the headers and command templates that compile the player's ppc/ against it, each game's config, dxc, fonts and the
# licences. Nothing from the game: the player's Arkeyan-Builder derives the game code and shaders from their disc, and picks the
# game by the SHA-256 of its default.xex (docs/release/launcher/kit_contract.md).
#
#   tools/make_kit.sh [windows] [macos] [linux]        (default: windows and macos)
#   KIT_GAMES="ssa" tools/make_kit.sh macos            (a kit of some games only; default: the six games)
#
# Needs a checkout with submodules, private/<game>/ppc/ppc_config.h and ppc_context.h of every game (SSA: tools/prepare_game.sh;
# Giants, Swap Force, Trap Team: Arkeyan-Builder's output, docs/release/launcher/kit_contract.md; the runtime is
# compiled against these two headers: the memory layout constants and XenonRecomp's context; no game code), and for
# Windows llvm-mingw 20260922 (LLVM_MINGW_ROOT or private/toolchains, as tools/build_windows.sh) and the minimal LGPL
# FFmpeg (FFMPEG_WINDOWS, else private/ffmpeg/windows-x64, built when missing). The Linux kit is built in Docker
# (tools/linux/kit.Dockerfile, amd64: zig's clang for glibc 2.34, the compiler the player's Arkeyan-Builder downloads); its
# FFmpeg goes to private/ffmpeg/linux-x64. JOBS (default 6) sets the parallelism.
set -eu

root=$(cd "$(dirname "$0")/.." && pwd)
jobs=${JOBS:-6}
platforms=${*:-windows macos}
cd "$root"

games=${KIT_GAMES:-ssa giants swapforce trapteam superchargers imaginators}
for g in $games; do
    for header in ppc_config.h ppc_context.h; do
        [ -f "private/$g/ppc/$header" ] || { echo "error: no private/$g/ppc/$header: translate the $g disc once (docs/release/launcher/kit_contract.md)" >&2; exit 1; }
    done
    # The player's game folder gets this game's config, hashes and name from games/<g>/game.txt; its hashes must be the
    # ones the runtime's disc check (games/<g>/info.cpp) accepts.
    [ -f "games/$g/game.txt" ] || { echo "error: no games/$g/game.txt" >&2; exit 1; }
    sed -n 's/^version=//p' "games/$g/game.txt" | grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+$' || { echo "error: games/$g/game.txt has no version=X.Y.Z" >&2; exit 1; }
    for sha in $(sed -n 's/^xex_sha256=//p' "games/$g/game.txt" | tr ',' ' '); do
        grep -q "$sha" "games/$g/info.cpp" || { echo "error: games/$g/game.txt lists a default.xex hash that games/$g/info.cpp does not" >&2; exit 1; }
    done
    # The loaded images of those releases: Arkeyan-Builder accepts another edition with one of them (edition_finder.md).
    for sha in $(sed -n 's/^image_sha256=//p' "games/$g/game.txt" | tr ',' ' '); do
        grep -q "$sha" "games/$g/info.cpp" || { echo "error: games/$g/game.txt lists an image hash that games/$g/info.cpp does not" >&2; exit 1; }
    done
    grep -q '^image_sha256=' "games/$g/game.txt" || { echo "error: games/$g/game.txt has no image_sha256=" >&2; exit 1; }
done
# One XenonRecomp context for the kit: the player's XenonRecomp writes the same ppc_context.h for every game.
first=${games%% *}
for g in $games; do
    cmp -s "private/$g/ppc/ppc_context.h" "private/$first/ppc/ppc_context.h" ||
        { echo "error: private/$g/ppc/ppc_context.h differs from $first's: translate the discs again with one XenonRecomp" >&2; exit 1; }
done
# The kit has its own version: the one Skylauncher expects (docs/release/launcher/kit_contract.md).
version=0.9.6
# Static libraries are byte-identical between the games' builds only if archives carry no timestamps (Apple's ar).
export ZERO_AR_DATE=1
# Nothing of this machine in a kit: every compile maps the checkout's path to ".", so neither __FILE__ nor debug
# info nor an assert message holds it (tools/kit_scan.sh checks the result).
maps="-ffile-prefix-map=$root=. -fmacro-prefix-map=$root=. -fdebug-prefix-map=$root=."
revision=$(git rev-parse --short HEAD)$(git diff --quiet HEAD -- . || echo -dirty)

# Configures <build>/runtime against a game-free ppc directory (the two headers, empty sources) and builds every
# input of ArkeyanRuntime's link but the game code and shader cache libraries.
build_runtime() { # <build dir> <game> <cmake args...>: <build dir>/<game>/runtime and /stub-ppc
    rb=$1/$2
    game=$2
    shift 2
    mkdir -p "$rb/stub-ppc"
    cp "private/$game/ppc/ppc_config.h" "private/$game/ppc/ppc_context.h" "$rb/stub-ppc/"
    : > "$rb/stub-ppc/ppc_func_mapping.cpp"
    : > "$rb/stub-ppc/shader_cache.cpp"
    cmake -S . -B "$rb/runtime" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS_RELEASE="-O2 -DNDEBUG $maps" \
        -DCMAKE_CXX_FLAGS_RELEASE="-O2 -DNDEBUG $maps" -DARKEYAN_KIT_BUILD=ON -DARKEYAN_GAME="$game" -DARKEYAN_PPC_DIR="$root/$rb/stub-ppc" "$@" > "$rb/runtime-cmake.log" 2>&1 ||
        { tail -20 "$rb/runtime-cmake.log"; exit 1; }
    ninja -C "$rb/runtime" -t commands ArkeyanRuntime | tail -1 | tr ' ' '\n' | grep -E '\.(obj|o|res|a)$' |
        grep -v -E '^[-/]' | grep -v -E 'libArkeyanRecompLib|libArkeyanShaderCache' | sort -u |
        (cd "$rb/runtime" && xargs nice ninja -j"$jobs") > "$rb/runtime-build.log" 2>&1 ||
        { tail -30 "$rb/runtime-build.log"; exit 1; }
}

# The runtime of every game (cmake args after the build dir go to each).
build_runtimes() { # <build dir> <cmake args...>
    d=$1
    shift
    for g in $games; do
        build_runtime "$d" "$g" "$@"
    done
}

# Arkeyan-Builder and XenonRecomp (root project, tools only; Arkeyan-Builder links the same static FFmpeg as the games, for --menu-music: the
# callers pass -DARKEYAN_FFMPEG_ROOT), XenosRecomp (its own project; asserts stay on, as in
# tools/shaders/build_cache.sh: they reject bad shaders).
build_tools() { # <build dir> <extra C++ flags for XenosRecomp> <cmake args...>
    b=$1
    xenos_flags=$2
    shift 2
    # -U: XenonRecomp built by clang outside Apple emits weak aliases instead of wrapper functions; every kit
    # emits the wrappers, the form the macOS-built game code always had (tested on every platform).
    cmake -S . -B "$b/tools" -G Ninja -DCMAKE_BUILD_TYPE=Release -DARKEYAN_FFMPEG=ON -DARKEYAN_TOOLS_ONLY=ON \
        -DARKEYAN_KIT_VERSION="$version" -DCMAKE_C_FLAGS="$maps ${tool_flags:-}" -DCMAKE_CXX_FLAGS="-UXENON_RECOMP_USE_ALIAS $maps ${tool_flags:-}" "$@" \
        > "$b/tools-cmake.log" 2>&1 || { tail -20 "$b/tools-cmake.log"; exit 1; }
    nice ninja -C "$b/tools" -j"$jobs" Arkeyan-Builder XenonRecomp > "$b/tools-build.log" 2>&1 ||
        { tail -30 "$b/tools-build.log"; exit 1; }
    cmake -S thirdparty/XenosRecomp -B "$b/xenos" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS_RELEASE=-O2 \
        -DCMAKE_C_FLAGS_RELEASE=-O2 -DBUILD_SHARED_LIBS=OFF -DCMAKE_C_FLAGS="$maps ${tool_flags:-}" -DCMAKE_CXX_FLAGS="$xenos_flags $maps ${tool_flags:-}" "$@" \
        ${xenos_link:+-DCMAKE_EXE_LINKER_FLAGS="$xenos_link"} > "$b/xenos-cmake.log" 2>&1 || { tail -20 "$b/xenos-cmake.log"; exit 1; }
    nice ninja -C "$b/xenos" -j"$jobs" XenosRecomp > "$b/xenos-build.log" 2>&1 || { tail -30 "$b/xenos-build.log"; exit 1; }
}

# The platform-independent part of a kit.
assemble() { # <build dir> <kit dir> <platform>
    b=$1
    kit=$2
    mkdir -p "$kit/bin" "$kit/config" "$kit/share" "$kit/include" "$kit/res" "$kit/licenses"
    for g in $games; do
        mkdir -p "$kit/config/$g"
        cp games/$g/config/* "$kit/config/$g/"
        cp "games/$g/game.txt" "$kit/config/$g/game.txt"
        [ ! -f "private/editions/$g.txt" ] || cp "private/editions/$g.txt" "$kit/config/$g/fingerprints.txt"
    done
    cp thirdparty/XenonRecomp/XenonUtils/ppc_context.h thirdparty/XenosRecomp/XenosRecomp/shader_common.h "$kit/share/"
    cp ArkeyanRecompLib/indirect_call_guard.h runtime/gpu/shader_cache.h "$kit/include/"
    rsync -a --exclude .git thirdparty/XenonRecomp/thirdparty/simde/ "$kit/include/simde/"
    cp -R resources/launcher/fonts "$kit/res/fonts"
    cp THIRD_PARTY_NOTICES.md "$kit/res/"
    for g in $games; do
        python3 -I tools/arkeyan-builder/kit_commands.py "$g" "$b/$g/runtime" "$b/$g/stub-ppc" "$kit"
    done

    # Arkeyan-Builder now conveys FFmpeg too: keep its objects and full link recipe for replacing the LGPL libraries.
    python3 -I tools/arkeyan-builder/kit_relink.py --pack "$b/tools" "$kit"

    # Licences, and the corresponding source of what the kit ships under the LGPL: FFmpeg (LGPL-2.1-or-later, static
    # libraries the player links). XenonRecomp's source goes with our patch; it decodes with our own PowerPC decoder,
    # so binutils' disassembler (GPL), which nothing builds any more, is left out of the archive.
    cp LICENSE THIRD_PARTY_NOTICES.md "$kit/licenses/"
    cp tools/ffmpeg/build.sh "$kit/licenses/ffmpeg-build.sh"
    # The FFmpeg source as released, but for tests/ (fate reference data: not part of the library, and it holds names
    # that have no place in a kit). The build script downloads and checks the original, by its SHA-256.
    ffsrc=private/ffmpeg/work/ffmpeg-8.1.2.tar.xz
    [ -f "$ffsrc" ] || curl -fL --retry 3 -o "$ffsrc" https://ffmpeg.org/releases/ffmpeg-8.1.2.tar.xz
    if tar --version 2>/dev/null | grep -q GNU; then # the Linux kit image: GNU tar has no @archive input
        xz -dc "$ffsrc" | tar --delete --wildcards 'ffmpeg-8.1.2/tests' | xz > "$kit/licenses/ffmpeg-8.1.2-without-tests.tar.xz"
    else
        tar -cJf "$kit/licenses/ffmpeg-8.1.2-without-tests.tar.xz" --exclude 'ffmpeg-8.1.2/tests' "@$ffsrc"
    fi
    git -C thirdparty/XenonRecomp archive --format=tar.gz --prefix=XenonRecomp/ -o "$kit/licenses/XenonRecomp-source.tar.gz" HEAD \
        -- . ":(exclude)thirdparty/disasm/ppc-dis.c" ":(exclude)thirdparty/disasm/disasm.c" \
        ":(exclude)thirdparty/disasm/dis-asm.h" ":(exclude)thirdparty/disasm/ppc.h" ":(exclude)thirdparty/disasm/CMakeLists.txt"
    # libmspack is a submodule of XenonRecomp's: git archive leaves it out of the archive above, so it has its own (LGPL source).
    git -C thirdparty/XenonRecomp/thirdparty/libmspack archive --format=tar.gz --prefix=libmspack/ \
        -o "$kit/licenses/libmspack-source.tar.gz" HEAD
    cp patches/xenonrecomp.patch "$kit/licenses/XenonRecomp-ssarecomp.patch"

    # One licence file per component (resources/kit-licenses/COMPONENTS.md is the table). A missing source file stops
    # the build: a component must not be added to a kit without its licence.
    c=$kit/licenses/components
    mkdir -p "$c"
    cp resources/kit-licenses/COMPONENTS.md "$kit/licenses/"
    cp resources/kit-licenses/* "$c/"
    rm "$c/COMPONENTS.md"
    x=thirdparty/XenonRecomp/thirdparty
    y=thirdparty/XenosRecomp/thirdparty
    p=thirdparty/plume/contrib
    while read -r name file; do
        [ -f "$file" ] || { echo "error: no licence file $file for $name" >&2; exit 1; }
        cp "$file" "$c/$name"
    done <<LIST
SDL-zlib.txt thirdparty/SDL/LICENSE.txt
plume-MIT.txt thirdparty/plume/LICENSE
FidelityFX-SDK-MIT.txt thirdparty/FidelityFX-SDK/LICENSE.txt
metal-cpp-Apache-2.0.txt $p/metal-cpp/LICENSE.txt
D3D12MemoryAllocator-MIT.txt $p/D3D12MemoryAllocator/LICENSE.txt
VulkanMemoryAllocator-MIT.txt $p/VulkanMemoryAllocator/LICENSE.txt
volk-MIT.txt $p/volk/LICENSE.md
Vulkan-Headers-Apache-2.0.txt $p/Vulkan-Headers/LICENSE.md
imgui-MIT.txt thirdparty/imgui/LICENSE.txt
stb-MIT.txt thirdparty/stb/LICENSE
fmt-MIT.txt $x/fmt/LICENSE
xxHash-BSD-2-Clause.txt $x/xxHash/LICENSE
zstd-BSD-3-Clause.txt $y/zstd/LICENSE
simde-MIT.txt $x/simde/COPYING
tiny-AES-c-Unlicense.txt $x/tiny-AES-c/unlicense.txt
XenonRecomp-MIT.txt thirdparty/XenonRecomp/LICENSE.md
XenosRecomp-MIT.txt thirdparty/XenosRecomp/LICENSE.md
tomlplusplus-MIT.txt $x/tomlplusplus/LICENSE
smol-v.txt $y/smol-v/license.txt
libmspack-COPYING.LIB.txt $x/libmspack/libmspack/COPYING.LIB
OFL-Outfit.txt resources/launcher/fonts/OFL-Outfit.txt
OFL-Bungee.txt resources/launcher/fonts/OFL-Bungee.txt
LIST
    tar -xJOf "$kit/licenses/ffmpeg-8.1.2-without-tests.tar.xz" ffmpeg-8.1.2/COPYING.LGPLv2.1 > "$c/FFmpeg-COPYING.LGPLv2.1.txt"
    if [ "$3" = linux-x64 ]; then
        # The compiler Arkeyan-Builder downloads, and the runtime of the kit's programs (libc++, libc++abi, libunwind and
        # compiler-rt, linked statically by zig); the libz.a of the build image, linked into the game.
        cp /opt/zig/LICENSE "$c/zig-MIT.txt"
        cp /opt/zig/lib/libcxx/LICENSE.TXT "$c/llvm-libcxx-LICENSE.TXT"
        sed -n '/Copyright (C) 1995/,/This notice may not be removed/p' /usr/include/zlib.h > "$c/zlib-License.txt"
        grep -q 'This software is provided' "$c/zlib-License.txt" || { echo "error: no zlib licence in zlib.h" >&2; exit 1; }
    else
        cp "${LLVM_MINGW_ROOT:-private/toolchains/llvm-mingw-20260922-ucrt-macos-universal}/LICENSE.TXT" "$c/llvm-mingw-LICENSE.TXT"
    fi

    {
        echo "platform=$3"
        echo "version=$version"
        echo "build=$revision"
        echo "games=$(echo $games | tr ' ' ',')"
        if [ "$3" = linux-x64 ]; then echo "zig=$zig_version"; else echo "llvm_mingw=20260922"; fi
        for g in $games; do
            echo "game_version.$g=$(sed -n 's/^version=//p' games/$g/game.txt)"
            echo "ppc_config_sha256.$g=$(shasum -a 256 private/$g/ppc/ppc_config.h | cut -d' ' -f1)"
        done
        echo "ppc_context_sha256=$(shasum -a 256 private/$first/ppc/ppc_context.h | cut -d' ' -f1)"
    } > "$kit/kit.txt"
    sed "s/@PLATFORM@/$3/" tools/arkeyan-builder/README.txt.in > "$kit/README.txt"
}

# Symbols and debug info out of what we built (not dxc, which is upstream's), so that a kit holds neither paths nor
# names of this machine; then the kit is scanned (tools/kit_scan.sh) and refused if it does.
strip_kit() { # <kit dir> <windows|macos|linux>
    if [ "$2" = linux ]; then
        for f in "$1/Arkeyan-Builder" "$1/bin/XenonRecomp" "$1/bin/XenosRecomp"; do strip --strip-all "$f"; done
        find "$1/lib" -type f \( -name '*.a' -o -name '*.o' \) -exec strip --strip-debug {} +
    elif [ "$2" = windows ]; then
        strip=$LLVM_MINGW_ROOT/bin/llvm-strip
        for f in "$1/Arkeyan-Builder.exe" "$1/bin/XenonRecomp.exe" "$1/bin/XenosRecomp.exe"; do "$strip" --strip-all "$f"; done
        find "$1/lib" -type f \( -name '*.a' -o -name '*.o' -o -name '*.obj' -o -name '*.res' \) -exec "$strip" --strip-debug {} +
    else
        for f in "$1/Arkeyan-Builder" "$1/bin/XenonRecomp" "$1/bin/XenosRecomp"; do
            strip -x "$f"
            codesign --force --sign - "$f"
        done
        find "$1/lib" -type f \( -name '*.a' -o -name '*.o' \) -exec strip -S {} +
    fi
}

finish() { # <kit dir> <windows|macos|linux>
    kit=$1
    strip_kit "$kit" "$2"
    # macOS metadata: a Windows or Linux player would see these files in the unzipped kit.
    find "$kit" \( -name '._*' -o -name '.DS_Store' \) -delete
    # Every file of the kit with its SHA-256, as shasum prints it (docs/release/launcher/kit_contract.md, "File list"): the
    # launcher checks the extracted kit against it, so a file an antivirus took is named instead of failing a build.
    (cd "$kit" && find . -type f ! -name files.sha256 | sed 's|^\./||' | LC_ALL=C sort | tr '\n' '\0' | xargs -0 shasum -a 256) > "$kit.sha256"
    mv "$kit.sha256" "$kit/files.sha256"
    tools/kit_scan.sh "$kit" || exit 1
    # The FidelityFX SDK is built with a two-byte wchar_t (runtime/gpu/fsr/ffx_port.h): outside Windows it must not call
    # the C++ library's wide strings, which are four-byte (the 0.9.4 candidate's frame generation faulted on them).
    if [ "$2" != windows ] && nm -C "$kit/lib/libArkeyanFfx.a" 2> /dev/null | grep -E ' U .*(basic_string<wchar_t|to_wstring)' | grep -q .; then
        echo "error: $kit/lib/libArkeyanFfx.a calls the C++ library's four-byte wide strings (runtime/gpu/fsr/ffx_port.h)" >&2
        exit 1
    fi
    # No game file or game-derived source may be in a kit.
    if find "$kit" \( -name '*.xex' -o -name '*.iso' -o -name 'ppc_recomp*' -o -name 'shader_cache.cpp' -o -name '*.lsa' \
        -o -name 'image.bin' \) | grep -q .; then
        echo "error: game data in $kit" >&2
        exit 1
    fi
    rm -f "$kit.zip"
    (cd "$(dirname "$kit")" && COPYFILE_DISABLE=1 zip -qr -X "$(basename "$kit").zip" "$(basename "$kit")")
    echo "$kit: $(du -sh "$kit" | cut -f1), $(basename "$kit").zip $(du -h "$kit.zip" | cut -f1)"
}

# The edition finder's fingerprints of each game's addresses.h (docs/tools/edition_finder.md): hashes of our own disc's
# functions, never their bytes, into private/editions/<game>.txt and then the kit. Needs the disc (SSA_ISO...,
# GIANTS_ISO, SWAPFORCE_ISO, TRAPTEAM_ISO, SUPERCHARGERS_ISO, IMAGINATORS_ISO) and the game's library (its function
# starts: private/libs/<game>/libSSARecompLib.a); a game without them ships none, and the
# report of a disc of another code then lists no address.
# Only where the discs are (the Linux kit runs this script again in a container, with neither discs nor a host
# compiler): there the fingerprints made before, in private/editions/, go in the kit as they are.
if [ -n "${SSA_ISO:-}${GIANTS_ISO:-}${SWAPFORCE_ISO:-}${TRAPTEAM_ISO:-}${SUPERCHARGERS_ISO:-}${IMAGINATORS_ISO:-}" ]; then
    # One build folder per system: the Linux kit runs this script again in a container, on the same private/.
    host=private/build-edition-host-$(uname -s)-$(uname -m)
    cmake -S . -B "$host" -G Ninja -DCMAKE_BUILD_TYPE=Release -DARKEYAN_TOOLS_ONLY=ON > "$host.log" 2>&1 &&
        nice ninja -C "$host" ArkeyanEditionPrints >> "$host.log" 2>&1 || { tail -20 "$host.log"; exit 1; }
    mkdir -p private/editions
    for g in $games; do
        case $g in
            ssa) disc=${SSA_ISO:-} ;; giants) disc=${GIANTS_ISO:-} ;; swapforce) disc=${SWAPFORCE_ISO:-} ;;
            trapteam) disc=${TRAPTEAM_ISO:-} ;; superchargers) disc=${SUPERCHARGERS_ISO:-} ;; *) disc=${IMAGINATORS_ISO:-} ;;
        esac
        # Without the disc (the container has none) the fingerprints made before are kept.
        if [ -n "$disc" ] && [ -f "$disc" ] && [ -f "private/libs/$g/libSSARecompLib.a" ]; then
            "$host/tools/arkeyan-builder/ArkeyanEditionPrints" "$disc" "private/libs/$g/libSSARecompLib.a" \
                "games/$g/addresses.h" "private/editions/$g.txt" || exit 1
        else
            [ -f "private/editions/$g.txt" ] ||
                echo "warning: no disc or private/libs/$g/libSSARecompLib.a for $g: its kit has no edition fingerprints" >&2
        fi
    done
fi

for platform in $platforms; do
    case "$platform" in
    windows)
        LLVM_MINGW_ROOT=${LLVM_MINGW_ROOT:-$root/private/toolchains/llvm-mingw-20260922-ucrt-macos-universal}
        [ -x "$LLVM_MINGW_ROOT/bin/x86_64-w64-mingw32-clang" ] ||
            { echo "error: no llvm-mingw at $LLVM_MINGW_ROOT (tools/build_windows.sh downloads it)" >&2; exit 1; }
        ffmpeg=${FFMPEG_WINDOWS:-$root/private/ffmpeg/windows-x64}
        [ -f "$ffmpeg/lib/libavcodec.a" ] || LLVM_MINGW_ROOT=$LLVM_MINGW_ROOT JOBS=$jobs tools/ffmpeg/build.sh windows-x64 "$ffmpeg"
        b=build-kit-windows
        kit=$root/dist/skylanders-kit-windows-x64
        mkdir -p "$b"
        cross="-DCMAKE_TOOLCHAIN_FILE=$root/cmake/toolchains/x86_64-w64-mingw32-clang.cmake -DLLVM_MINGW_ROOT=$LLVM_MINGW_ROOT"
        # shellcheck disable=SC2086 # cross is several arguments
        build_runtimes "$b" $cross -DARKEYAN_FFMPEG_ROOT="$ffmpeg"
        # XenosRecomp's own details and manifest, as Arkeyan-Builder's: a bare program is what antivirus heuristics distrust.
        parts=$(printf '%s' "${version%%-*}" | awk -F. '{ printf "%d,%d,%d,0", $1, $2, $3 }')
        sed -e "s/@PARTS@/$parts/g" -e "s/@VERSION@/$version/g" tools/arkeyan-builder/xenosrecomp.rc.in > "$b/xenosrecomp.rc"
        "$LLVM_MINGW_ROOT/bin/x86_64-w64-mingw32-windres" -I runtime/platform/win32 "$b/xenosrecomp.rc" -O coff -o "$root/$b/xenosrecomp.res.o"
        # shellcheck disable=SC2086
        # Set and unset around the call: sh keeps an assignment made before a function call, and the macOS and Linux
        # builds that follow would link the Windows resource.
        xenos_link="-static -Wl,--no-insert-timestamp $root/$b/xenosrecomp.res.o"
        # -DCMAKE_CXX_FLAGS replaces the toolchain file's flags: the objects of the tools (and the ones shipped in relink/)
        # carry no build time only if the flag comes here too.
        tool_flags="-mno-incremental-linker-compatible"
        build_tools "$b" "-include $root/tools/arkeyan-builder/xenos_mingw.h" $cross -DARKEYAN_FFMPEG_ROOT="$ffmpeg" -DCMAKE_EXE_LINKER_FLAGS="-static -Wl,--no-insert-timestamp"
        unset xenos_link tool_flags
        rm -rf "$kit"
        mkdir -p "$kit"
        assemble "$b" "$kit" windows-x64
        cp "$b/tools/tools/arkeyan-builder/Arkeyan-Builder.exe" "$kit/"
        cp "$b/tools/thirdparty/XenonRecomp/XenonRecomp/XenonRecomp.exe" "$b/xenos/XenosRecomp/XenosRecomp.exe" \
            thirdparty/XenosRecomp/thirdparty/dxc-bin/bin/x64/dxcompiler.dll \
            thirdparty/XenosRecomp/thirdparty/dxc-bin/bin/x64/dxil.dll "$kit/bin/"
        finish "$kit" windows
        ;;
    macos)
        [ "$(uname -s)" = Darwin ] && [ "$(uname -m)" = arm64 ] || { echo "error: the macOS kit is built on an arm64 Mac" >&2; exit 1; }
        b=build-kit-macos
        kit=$root/dist/skylanders-kit-macos-arm64
        mkdir -p "$b"
        # One FFmpeg for every game (built once, by tools/ffmpeg/build.sh).
        ffmpeg=${FFMPEG_MACOS:-$root/private/ffmpeg/macos-arm64}
        [ -f "$ffmpeg/lib/libavcodec.a" ] || JOBS=$jobs tools/ffmpeg/build.sh host "$ffmpeg"
        build_runtimes "$b" -DARKEYAN_FFMPEG_ROOT="$ffmpeg"
        build_tools "$b" "" -DARKEYAN_FFMPEG_ROOT="$ffmpeg"
        rm -rf "$kit"
        mkdir -p "$kit"
        assemble "$b" "$kit" macos-arm64
        # Each game's bundle template: its Info.plist names its executable; the icon is shared.
        for g in $games; do
            mkdir -p "$kit/res/$g"
            name=$(sed -n 's/^runtime=//p' "games/$g/game.txt")
            cp "$b/$g/runtime/ArkeyanRuntime/$name.app/Contents/Info.plist" "$kit/res/$g/"
        done
        cp resources/macos/ArkeyanRuntime.icns "$kit/res/"
        cp "$b/tools/tools/arkeyan-builder/Arkeyan-Builder" "$kit/"
        cp "$b/tools/thirdparty/XenonRecomp/XenonRecomp/XenonRecomp" "$b/xenos/XenosRecomp/XenosRecomp" \
            thirdparty/XenosRecomp/thirdparty/dxc-bin/lib/arm64/libdxcompiler.dylib "$kit/bin/"
        # XenosRecomp finds libdxcompiler.dylib next to itself (its build rpath points into the checkout).
        install_name_tool -add_rpath @executable_path "$kit/bin/XenosRecomp"
        # ... and no rpath may stay that names the build machine: only @-relative ones.
        for f in "$kit/Arkeyan-Builder" "$kit/bin/XenonRecomp" "$kit/bin/XenosRecomp"; do
            otool -l "$f" | awk '/cmd LC_RPATH/ { r = 1; next } r && $1 == "path" { print $2; r = 0 }' | grep -v '^@' |
                while IFS= read -r rpath; do install_name_tool -delete_rpath "$rpath" "$f"; done
        done
        codesign --force --sign - "$kit/bin/XenosRecomp"
        finish "$kit" macos
        ;;
    linux)
        if [ "$(uname -s)" != Linux ]; then
            # From the Mac (or anywhere): the same script in the kit image, the checkout mounted at /src, nothing of it
            # copied into the image.
            docker build -q --platform linux/amd64 -t ssarecomp-kit-linux -f tools/linux/kit.Dockerfile tools/linux > /dev/null
            docker run --rm --platform linux/amd64 --user "$(id -u):$(id -g)" -v "$root:/src" -e JOBS="$jobs" -e KIT_GAMES="$games" -e KIT_SCAN_USER="$(id -un)" \
                ssarecomp-kit-linux tools/make_kit.sh linux
            continue
        fi
        [ "$(uname -m)" = x86_64 ] && [ -x /opt/zig/zig ] ||
            { echo "error: the Linux kit is built in its image (tools/linux/kit.Dockerfile, amd64)" >&2; exit 1; }
        zig_version=$(/opt/zig/zig version)
        ffmpeg=${FFMPEG_LINUX:-$root/private/ffmpeg/linux-x64}
        [ -f "$ffmpeg/lib/libavcodec.a" ] || JOBS=$jobs tools/ffmpeg/build.sh linux-x64 "$ffmpeg"
        b=build-kit-linux
        kit=$root/dist/skylanders-kit-linux-x64
        mkdir -p "$b/zlib/include"
        # The compile and link commands the kit's templates are made from start with "zig c++ -target ...", which
        # Arkeyan-Builder runs with the zig it downloads. SDL's configure finds the X11, Wayland and audio headers after zig's
        # own libc headers (-idirafter: never the image's glibc); zlib is linked statically, its header alone.
        export CC="/opt/zig/zig cc -target x86_64-linux-gnu.2.34" CXX="/opt/zig/zig c++ -target x86_64-linux-gnu.2.34"
        export ASM="$CC"
        cp /usr/include/zlib.h /usr/include/zconf.h "$b/zlib/include/"
        # zig's clang targets the baseline x86-64 (SSE2) when given a -target: simde then emulates every SSSE3/SSE4.1
        # operation the translated VMX code uses (pshufb for each vector load and store) in scalar loops. x86-64-v2
        # (SSSE3, SSE4.1/4.2, POPCNT: every x86-64 CPU since 2009) is the kit's Linux baseline; the flags reach the
        # player's game code through the compile templates (docs/release/launcher/kit_contract.md). zig's spelling:
        # -march=x86-64-v2 reads as the CPU "x86".
        march="-mcpu=x86_64_v2"
        build_runtimes "$b" -DCMAKE_C_FLAGS="$march -idirafter /usr/include" -DCMAKE_CXX_FLAGS="$march -idirafter /usr/include" \
            -DARKEYAN_FFMPEG_ROOT="$ffmpeg" -DZLIB_INCLUDE_DIR="$root/$b/zlib/include" \
            -DZLIB_LIBRARY=/usr/lib/x86_64-linux-gnu/libz.a
        # XenosRecomp finds libdxcompiler.so next to itself (quoted: ninja runs the link through sh); its fmt needs <cstdlib> with zig's libc++ too.
        build_tools "$b" "-include cstdlib" -DARKEYAN_FFMPEG_ROOT="$ffmpeg" -DCMAKE_SKIP_BUILD_RPATH=ON -DCMAKE_EXE_LINKER_FLAGS="-Wl,-rpath,'\$ORIGIN'"
        rm -rf "$kit"
        mkdir -p "$kit"
        assemble "$b" "$kit" linux-x64
        cp "$b/tools/tools/arkeyan-builder/Arkeyan-Builder" "$kit/"
        cp "$b/tools/thirdparty/XenonRecomp/XenonRecomp/XenonRecomp" "$b/xenos/XenosRecomp/XenosRecomp" \
            thirdparty/XenosRecomp/thirdparty/dxc-bin/lib/x64/libdxcompiler.so "$kit/bin/"
        finish "$kit" linux
        ;;
    *)
        echo "usage: $0 [windows] [macos] [linux]" >&2
        exit 1
        ;;
    esac
done
