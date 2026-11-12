# Tools

> **Purpose:** which tools turn a disc into a port, build it and ship the kit, and where is each one documented? · **Status:** current (2026-10-26) · **Tags:** [Xbox] [Lux] [Alchemy]

Index of the whole tree: [../README.md](../README.md).

## What it is
Everything that is not the runtime: the two recompilers (code and shaders), the analysis tools for a new binary, the
per-game derive step, the platform builds, the regression harness, Arkeyan-Builder and the player kit. Run every tool from
the repository root; Python tools with **`python3 -I`** (isolated: no import from the current directory). The flow,
from the disc to a running port: [toolchain/README.md](toolchain/README.md).

| Piece | What | Detail |
|---|---|---|
| **XenonRecomp** (hedge-dev, MIT) | PPC -> C++ (`private/<id>/ppc/`, ~160 MB for SSA, from the player's disc) | `thirdparty/XenonRecomp` + [`patches/xenonrecomp.patch`](../../patches/xenonrecomp.patch); [toolchain/xenonrecomp.md](toolchain/xenonrecomp.md) |
| **XenosRecomp** (squidbus fork 4906992, MIT) | Xenos shaders -> MSL / SPIR-V / signed DXIL | `thirdparty/XenosRecomp` + [`patches/xenosrecomp.patch`](../../patches/xenosrecomp.patch); [toolchain/xenosrecomp.md](toolchain/xenosrecomp.md) |
| **plume** (MIT, pinned e48a55b) | RHI: Metal, Vulkan, D3D12 | `thirdparty/plume` + `patches/plume.patch`, `patches/plume_queue_order.patch`; [../runtime/gpu/plume_backends.md](../runtime/gpu/plume_backends.md) |
| `tools/xexdump` | XEX sections, imports, the loaded `image.bin` | built into `private/build-host-tools`; [toolchain/analysis_tools.md](toolchain/analysis_tools.md) |
| `tools/find_switch_tables.py` | switch tables + extra `functions` (packed leaves) | [toolchain/xenonrecomp.md](toolchain/xenonrecomp.md) |
| `tools/sigmatch/sigmatch.py` | find SSA's hooks in another build by masked signature | [toolchain/sigmatch.md](toolchain/sigmatch.md); inventory [`docs/giants/ssa_hooks.json`](../giants/ssa_hooks.json) (metadata only) |
| `tools/ppcinfo.py` (`--game <id>`), `tools/xmatch.py` | disassembly queries; match functions of another game against SSA's | [toolchain/analysis_tools.md](toolchain/analysis_tools.md) |
| `tools/shaders/` (`xdvdfs.py`, `lsa.py`, `ucode.py`, `giants_extract.py`, `swapforce_extract.py`, `build_cache.sh`, `check_dxil.sh`, `extract_dxil.py`, `compare_cache.py`) | disc reader, shader archives, census, the shader cache | [toolchain/analysis_tools.md](toolchain/analysis_tools.md), [../runtime/gpu/shaders.md](../runtime/gpu/shaders.md) |
| `tools/prepare_game.sh` | SSA's derive step (xexdump, XenonRecomp, shader cache) | [toolchain/prepare_build.md](toolchain/prepare_build.md) |
| `tools/toy_tables.py` | the toy, variant and upgrade tables of a player's disc (Giants' toy definitions, SSA's toy pictures, both games' upgrades) into `private/<game>/toy_tables.json` | [../runtime/host/collection.md](../runtime/host/collection.md#the-games-own-toy-tables) |
| `tools/build_windows.sh`, `tools/build_linux.sh`, `tools/linux/`, `tools/windows/` | Windows cross build, Linux build and tests in Docker | [toolchain/prepare_build.md](toolchain/prepare_build.md), [../getting-started/platforms/README.md](../getting-started/platforms/README.md) |
| `tools/xbox/` | UWP packaging (paused) | [`tools/xbox/README.md`](../../tools/xbox/README.md) |
| `tools/regress/kit_isolation.sh` | a kit-built game (Arkeyan-Builder, then the app) writes only in scratch folders | [../getting-started/testing.md](../getting-started/testing.md) |
| `tools/test_all.sh`, `tools/regress/` | the regression harness, both games | [../getting-started/testing.md](../getting-started/testing.md) |
| `tools/pipelines/` (`tour.sh`, `tour_giants.sh`, `tour_summary.py`, `merge_lists.py`; [SSA] `tour_ssa_story.sh` + `story_summary.py`, the story tour, and `ssa_ending.sh`, the ending and credits: [games/ssa/docs/status.md](../../games/ssa/docs/history.md#story-tour-2026-10-28-ssa-v)), `tools/perf/` (`level_run.sh`, `frames.py`) | pipeline baseline from a level tour; the SSA story tour; frame times | [../runtime/gpu/pipeline_cache.md](../runtime/gpu/pipeline_cache.md), [../porting/debugging.md](../porting/debugging.md) |
| `tools/arkeyan-builder/`, `tools/make_kit.sh`, `tools/kit_scan.sh` | the player-side build driver, the kit, the kit's hygiene scan | [arkeyan-builder.md](arkeyan-builder.md), [../release/launcher/kit_contract.md](../release/launcher/kit_contract.md) |
| `tools/arkeyan-builder/edition*`, `fingerprint*` (`--edition-report`), `tools/editions/from_report.py` | the edition finder: a disc's report, header-only editions accepted, a disc's addresses by fingerprints | [edition_finder.md](edition_finder.md) |
| `tools/arkeyan-builder/hd_*` (`--hd-textures`) | HD textures: our own 2x pass over the player's disc into their texture pack | [hd_textures.md](hd_textures.md) |
| `tools/ffmpeg/build.sh` | the minimal LGPL FFmpeg (WMA Pro only) | [../release/licensing.md](../release/licensing.md) |
| `tools/sync_skylauncher.sh` | copy the shared library into a Arkeyan Launcher checkout | [../release/launcher/README.md](../release/launcher/README.md) |
| `tools/check_docs.py`, `tools/docs_move.py` | broken doc paths; move a page and rewrite its references | [../README.md](../README.md) |
| `tools/day1.sh` | day 1 of a new disc (extract, xexdump, sigmatch) | [`docs/tools/day1.md`](day1.md) |

## Where the code is
`tools/` (scripts and host tools), `thirdparty/{XenonRecomp,XenosRecomp,plume}` (submodules) with our changes in
`patches/` (applied by the root `CMakeLists.txt` at configure time), each game's XenonRecomp config in
`games/<id>/config/` ([`SSA.toml`](../../games/ssa/config/SSA.toml), [`Giants.toml`](../../games/giants/config/Giants.toml)).

## How to test it
- After a change to a patch: reset the submodules (`git -C thirdparty/{plume,XenosRecomp,XenonRecomp} checkout -- .`),
  then cmake and ninja; after a XenosRecomp change, **rebuild the shader cache** (`tools/shaders/build_cache.sh`, on
  the Mac so it holds AIR + SPIR-V + DXIL).
- After a XenonRecomp change: regenerate `private/<id>/ppc/` and loop until XenonRecomp reports no unknown instruction
  and no unhandled jump table; then run `tools/test_all.sh` (both games).
- sigmatch: `python3 -I tools/sigmatch/test_sigmatch.py`.
- The kit: `tools/kit_scan.sh <kit dir>`, then a player-side run (`tools/linux/kit_test.sh` for Linux).

## Gotchas
- **Fences**: our XenonRecomp patch turns `lwsync`/`sync`/`eieio` into fences (1773 sites in SSA); without them, a rare
  heap corruption on ARM64 (D1). Also missing instructions, `blrl`, `vupkd3d128` FLOAT16, ARM64 `setcsr`, the XEX
  loader, x64-safe `divw`/`divd` (they trap on x64, not on ARM64). `ByteSwap(float)` outside the patch converts
  numerically: swap the bits.
- **Config [SSA]**: save/restore helpers and `setjmp`/`longjmp` found by byte pattern (sigmatch gives the 10
  `*_address` lines); `invalid_instructions` (0x00000000 padding and the 8-byte EH data words in front of
  `__CxxFrameHandler` / `__C_specific_handler` functions); `functions` (136 entries for code outside `.pdata`);
  210 switch tables in `SSA_switch_tables.toml` (Giants: 321); every `*_as_local` option true since 2026-11-04 ([toolchain/xenonrecomp.md](toolchain/xenonrecomp.md#registers-as-locals)), `skip_*` false.
- **Boundaries**: at run time every "Indirect call to unrecompiled guest code (ctr=...)" is a missing boundary (packed
  leaves without `.pdata`: 27 on SSA); add it to `functions` and rerun XenonRecomp.
- **sigmatch confidence**: 1.0 strict unique, 0.9 loose unique; below that, check by hand (`ppcinfo.py asm`).
  `config_function` entries are SSA's layout: drop and regenerate them.
- **XenosRecomp**: untranslatable shaders are skipped instead of aborting; the patch adds point sprites, pulled
  vertices, DEC3N normals, Metal `-fpreserve-invariance` and one DXIL variant per specialization.
- **Nothing from the disc in git**: `private/` holds every derived file (`image.bin`, `ppc/`, the shader cache,
  signatures).

## Pages
| Page | Answers |
|---|---|
| [toolchain/README.md](toolchain/README.md) | The pipeline from `default.xex` to a running port, and the toolchain pages. |
| [toolchain/xenonrecomp.md](toolchain/xenonrecomp.md) | XenonRecomp: config option by option, patch hunks, switch tables, boundaries, guard. |
| [toolchain/xenosrecomp.md](toolchain/xenosrecomp.md) | XenosRecomp: the shader translator patch, hunk by hunk. |
| [toolchain/sigmatch.md](toolchain/sigmatch.md) | sigmatch: finding hooks in another build; the 293-entry inventory. |
| [toolchain/analysis_tools.md](toolchain/analysis_tools.md) | `xexdump`, `ppcinfo`, `find_switch_tables`, `xmatch`, `xdvdfs`, `lsa`, `ucode`, recipes. |
| [toolchain/prepare_build.md](toolchain/prepare_build.md) | `prepare_game.sh`, the CMake options, the prebuilt library, the platform builds. |
| [arkeyan-builder.md](arkeyan-builder.md) | How does Arkeyan-Builder build the game on the player's machine (Linux: zig)? How is a kit made? |
