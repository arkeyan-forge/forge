# Components of this kit and their licences

Each component's licence text is in `components/` (the full texts, with copyright lines, are also in
`THIRD_PARTY_NOTICES.md`). "Where" says what carries it: **lib** = static libraries and objects the player's game is
linked from (`lib/`), **tool** = a program in the kit (`bin/`, `Arkeyan-Builder`), **file** = data or headers.

| Component | Licence | Where | File in `components/` |
|---|---|---|---|
| ArkeyanRuntime (runtime, Arkeyan-Builder, launcher UI) | MIT | lib, tool | `ArkeyanRuntime-MIT.txt` |
| SDL 3 (with bundled HIDAPI BSD option, SunPro libm, yuv_rgb) | zlib; HIDAPI BSD-3-Clause; libm SunPro permissive; yuv_rgb BSD-3-Clause | lib | `SDL-zlib.txt` |
| plume | MIT | lib | `plume-MIT.txt` |
| AMD FidelityFX SDK (FSR 3.1: host code and shaders) | MIT | lib | `FidelityFX-SDK-MIT.txt` |
| metal-cpp (plume contrib) | Apache-2.0 | lib (macOS) | `metal-cpp-Apache-2.0.txt` |
| D3D12MemoryAllocator, VulkanMemoryAllocator, volk | MIT | lib | `D3D12MemoryAllocator-MIT.txt`, `VulkanMemoryAllocator-MIT.txt`, `volk-MIT.txt` |
| Vulkan-Headers | Apache-2.0 / MIT | lib | `Vulkan-Headers-Apache-2.0.txt` |
| Dear ImGui | MIT | lib | `imgui-MIT.txt` |
| stb | MIT / public domain | lib | `stb-MIT.txt` |
| miniz 1.15 | Unlicense (public domain) | lib | `miniz-Unlicense.txt` |
| {fmt} | MIT | lib, tool | `fmt-MIT.txt` |
| xxHash | BSD-2-Clause | lib, tool | `xxHash-BSD-2-Clause.txt` |
| zstd | BSD-3-Clause (or GPL-2.0, BSD chosen) | lib, tool | `zstd-BSD-3-Clause.txt` |
| simde | MIT | file (`include/simde`), lib | `simde-MIT.txt` |
| tiny-AES-c | Unlicense (public domain) | lib | `tiny-AES-c-Unlicense.txt` |
| TinySHA1 | ISC | lib | `TinySHA1-ISC.txt` |
| Xenia (adapted code) | BSD-3-Clause | lib | `Xenia-BSD-3-Clause.txt` |
| XenonRecomp (XenonUtils, recompiler) | MIT | lib, tool | `XenonRecomp-MIT.txt` |
| XenosRecomp | MIT | tool | `XenosRecomp-MIT.txt` |
| tomlplusplus | MIT | tool | `tomlplusplus-MIT.txt` |
| smol-v | MIT / public domain | tool, lib | `smol-v.txt` |
| DirectX Shader Compiler (dxc) | NCSA (LLVM Release License) and third-party notices | tool | `DirectXShaderCompiler-LICENSE.TXT` |
| llvm-mingw runtime (libc++, libc++abi, libunwind, compiler-rt) | Apache-2.0 with LLVM exception | Windows tools and lib | `llvm-mingw-LICENSE.TXT` |
| zig 0.17.0 (downloaded by Arkeyan-Builder on Linux, not in the kit: clang/LLVM, libc++, glibc and Linux headers and stubs) | MIT; its bundled parts keep theirs (LLVM: Apache-2.0 with LLVM exception; glibc: LGPL-2.1-or-later; Linux headers: GPL-2.0 with the syscall note) | — (the player's compiler) | `zig-MIT.txt` |
| LLVM libc++, libc++abi, libunwind, compiler-rt (zig's, linked statically) | Apache-2.0 with LLVM exception | Linux tools and the game | `llvm-libcxx-LICENSE.TXT` |
| zlib (Ubuntu 24.04's libz.a) | zlib | lib (Linux) | `zlib-License.txt` |
| MinGW-w64 runtime and winpthreads | ZPL-2.1 / MIT / public domain | Windows tools and lib | `MinGW-w64-runtime.txt` |
| Outfit, Bungee fonts | SIL OFL 1.1 | file (`res/fonts`) | `OFL-Outfit.txt`, `OFL-Bungee.txt` |
| FFmpeg 8.1.2 (WMA Pro decoder only; `libavcodec`, `libavutil`) | **LGPL-2.1-or-later** | lib, tool (Arkeyan-Builder) | `FFmpeg-COPYING.LGPLv2.1.txt` |
| libmspack (`lzxd.c`) | **LGPL-2.1** | lib, tool (inside XenonUtils) | `libmspack-COPYING.LIB.txt` |

## The (L)GPL parts

Two components are not permissive. They are the only ones, and what the licences ask is done:

- FFmpeg and libmspack (LGPL): linked statically into the player's game on their machine from `lib/`. The objects and
  libraries are in the kit, so the player can relink with a modified library; the FFmpeg source as released but for its `tests/` directory (`ffmpeg-8.1.2-without-tests.tar.xz`; the original is checked by SHA-256 in `ffmpeg-build.sh`),
  its build script (`ffmpeg-build.sh`) and the rest of the source (the repository) are provided. libmspack's source is
  in `libmspack-source.tar.gz` (a submodule of XenonRecomp's, so not in `XenonRecomp-source.tar.gz`). Arkeyan-Builder also links FFmpeg statically: `relink/` contains its objects, dependency libraries, link recipe and relink script so the player can replace FFmpeg in the shipped tool.
