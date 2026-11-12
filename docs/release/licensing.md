# Licensing

> **Purpose:** what may be distributed, and what each third-party component obliges us to do? · **Status:** current (2026-10-26) · **Tags:** none

The full notices: `THIRD_PARTY_NOTICES.md` at the repository root; the kit's per-component table:
`resources/kit-licenses/COMPONENTS.md`; what the kit ships: [launcher/kit_contract.md](launcher/kit_contract.md)
("Licences in the kit").

## What may be distributed
- **This project's code** and its build outputs (the game-title executable or app), with the required
  third-party notices.
- **Third-party notices**: `THIRD_PARTY_NOTICES.md`, copied into every distributable (the macOS `.app`
  `Contents/Resources/`, the Windows build output, the Xbox `.msix` packages, the kit's `res/` and `licenses/`).
- The pipeline baseline (`games/<id>/pipelines/baseline.zst`: hashes and render states only), the one disc-derived
  artifact the user allowed to ship.

## What must never be distributed
- **The game ISO / disc image.** The project runs from the player's own copy and never includes game data.
- **`private/<id>/ppc/`** (the PowerPC code translated from the game): not committed, not distributable.
- **The shader cache** (`private/<id>/ppc/shader_cache.cpp` and anything derived from it): generated from the
  player's disc.
- **Game assets, saves, profiles, figure dumps, frame dumps, logs**, a built game or package, or any other derived
  game data.

## FFmpeg (LGPL-2.1-or-later)
- Used for XMA / WMA Pro audio decoding in `runtime/apu/xma_decoder_ffmpeg.cpp`.
- **Default build**: a minimal static LGPL FFmpeg 8.1.2 built by `tools/ffmpeg/build.sh` (only the `wmapro` decoder,
  no `--enable-gpl` / `--enable-nonfree`; source URL and SHA-256 pinned in the script). CMake builds it into the build
  tree when `ARKEYAN_FFMPEG_ROOT` is not set; `-DARKEYAN_FFMPEG_ROOT=<prefix>` uses a prefix built by the same script.
- **Licence enforcement**: the build checks `avcodec_license()` / `avcodec_configuration()` (and library strings) at
  configure time and refuses a GPL or non-free FFmpeg. **Homebrew's FFmpeg is GPL: never link it.**
  `-DARKEYAN_FFMPEG=OFF` builds without music and sound effects (`apu/xma_decoder_null.cpp`).
- **Obligations**: the player links FFmpeg on their own machine from the kit's static libraries and objects, so they
  can relink; the kit carries the FFmpeg source (without `tests/`) and its build script (`THIRD_PARTY_NOTICES.md`,
  "How the LGPL is met"). Whoever distributes binaries statically linked against LGPL FFmpeg must meet LGPL 2.1+
  (relinkable objects, corresponding source).

## Other third-party components
- **XenonRecomp, XenosRecomp** (MIT): code and shader recompilation. XenonRecomp decodes with our own PowerPC decoder
  (`tools/xenonrecomp/ppc_decoder.cpp`, MIT); the GNU binutils disassembler it came with (GPL) is no longer built, so no
  GPL code is in any binary. Inside XenonRecomp: **libmspack** (`lzxd.c`, LGPL-2.1).
- **plume** (MIT) with contrib (metal-cpp Apache-2.0, D3D12MemoryAllocator / VulkanMemoryAllocator / volk MIT,
  Vulkan-Headers Apache-2.0 / MIT).
- **SDL3** (zlib) with embedded HIDAPI (BSD / GPL / original, at the user's choice as configured), SunPro libm,
  yuv_rgb (BSD-3-Clause).
- **Dear ImGui, stb, miniz** (MIT / public domain): the Toy Box and the launcher.
- **fmt, xxHash, simde, tomlplusplus, tiny-AES-c, TinySHA1, zstd, smol-v**: MIT-like; see `THIRD_PARTY_NOTICES.md`.
- **llvm-mingw runtime** (Apache-2.0 with LLVM exceptions; MinGW-w64 ZPL / MIT): linked statically into the Windows
  and Xbox executables; downloaded by Arkeyan-Builder on Windows, not shipped. On Linux, **zig** (MIT) is downloaded and its
  libc++ linked statically.
- **DirectX Shader Compiler (dxc)** (LLVM Release License / NCSA): shader compilation on non-Apple builds; the kit ships
  it next to XenosRecomp. Its licence text (the upstream `LICENSE.TXT`, with the third-party notices) is copied from Microsoft's repository
  because `dxc-bin` has none, and is in every kit (`licenses/components/DirectXShaderCompiler-LICENSE.TXT`, listed in
  `COMPONENTS.md`). **Decided 2026-10-26 by the user: "we add the licence text"**; the remaining check (that the text matches the shipped version) is an internal task.
- **makemsix** (MIT) and **osslsigncode** (GPL-3.0, a separate program on the build machine, never linked or shipped):
  Xbox packaging.
- **libusb-1.0** (LGPL): loaded at run time by `host/portal_usb.cpp` for Xbox portals; the build does not link it.
- **Fonts**: Outfit and Bungee (OFL).
- **Xenia code** (BSD-3-Clause) adapted with attribution in named files: `d3d/xenos.*`, `apu/xma_context.*`,
  `host/portal_usb.cpp`, `host/launcher_assets.cpp`, `host/art/xenos_texture.cpp` (the tiling, also in Arkeyan Launcher's
  `src/shared/art/`), `kernel/xmp.cpp` (all under `runtime/`).

## Projects used for reference only
Unleashed Recompiled, Dolphin, RPCS3 and the like may be read for reference but **must not** have their code copied
into this tree. The leaked 2011 prototype PDBs are not used (user decision).
