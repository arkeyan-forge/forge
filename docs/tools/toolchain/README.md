# Toolchain: from `default.xex` to a running port

> **Purpose:** how does a disc become native code and shaders, and which page details each step? · **Status:** current (2026-10-26) · **Tags:** [Xbox]

The tools themselves, their gotchas and the rest of `tools/`: [../README.md](../README.md). Day 1 of a new game:
the worked example [../../porting/day1.md](../../porting/day1.md).

## The pipeline
```
disc .iso -(xdvdfs.py)-> default.xex -(xexdump)-> image.bin + xexdump.txt
  -(sigmatch + find_switch_tables.py)-> games/<id>/config/<Game>.toml + <Game>_switch_tables.toml
  -(XenonRecomp)-> private/<id>/ppc/ppc_*.cpp -(clang)-> the game code library, linked with the runtime
disc shaders + embedded + run-time dump (SKY_DUMP_SHADERS) -(XenosRecomp)-> private/<id>/ppc/shader_cache.cpp
```
On the developer's machine SSA's derive step is `tools/prepare_game.sh`; on the player's machine Arkeyan-Builder does the
same for every game of the kit ([../arkeyan-builder.md](../arkeyan-builder.md)).

## Pages
| Page | Answers |
|---|---|
| [xenonrecomp.md](xenonrecomp.md) | the config option by option, the patch hunks (fences, divides, instructions, XEX loader), switch tables and packed leaves, the indirect-call guard |
| [xenosrecomp.md](xenosrecomp.md) | the shader translator patch, hunk by hunk |
| [sigmatch.md](sigmatch.md) | finding SSA's hooks in another build; the full inventory of 293 SSA addresses with role and scope |
| [analysis_tools.md](analysis_tools.md) | `xexdump`, `ppcinfo.py`, `find_switch_tables.py`, `xmatch.py`, `xdvdfs.py`, `lsa.py`, `ucode.py`, the DXIL check, `day1.sh`, recipes |
| [prepare_build.md](prepare_build.md) | `prepare_game.sh`, the CMake options, the prebuilt game code library, the Windows and Linux builds |

Elsewhere: plume patches [../../runtime/gpu/plume_backends.md](../../runtime/gpu/plume_backends.md); the shader ABI
and cache [../../runtime/gpu/shaders.md](../../runtime/gpu/shaders.md).
