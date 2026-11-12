# Analysis tools: xexdump, ppcinfo, xmatch, find_switch_tables, xdvdfs, lsa, ucode, check_dxil, day1

> **Purpose:** which tool answers which question about a game binary or its shaders, and how is it run? · **Status:** current (2026-10-26) · **Tags:** [Xbox] [Lux]

All in [`tools/`](../../../tools/) (run from the repository root). Python tools are run with **`python3 -I`** (isolated:
no import from the current directory). `ppcinfo.py` reads SSA's files by default (`private/image.bin`,
`private/xexdump.txt`, `private/ssa/ppc/ppc_func_mapping.cpp`, `games/ssa/config/SSA_symbols.txt`); with `--game <id>`
another game's (`private/<id>/{image.bin,xexdump.txt,ppc/}`, `games/<id>/config/*_symbols.txt`).

| Tool | What it does | Usage |
|---|---|---|
| `tools/xexdump` (C++, built by `prepare_game.sh` into `private/build-host-tools/tools/xexdump/xexdump`) | decrypts/decompresses a XEX through XenonUtils, writes the loaded image `<out>/image.bin`, prints the layout | `xexdump default.xex <out dir> > xexdump.txt`. Output lines: `base 0x82000000 size 0x00D40000 entry 0x822DF400`, `section <name> <base> <size> flags N`, `import 0x<addr> <name>` (e.g. `__imp__VdSwap`). Everything else reads this file |
| `tools/shaders/xdvdfs.py` | list/extract files from an Xbox 360 ISO (read-only), XDVDFS partition offsets `0x0, 0x2080000, 0xFD90000, 0x18300000` | `xdvdfs.py <iso>` lists; `xdvdfs.py <iso> <outdir> /default.xex /shaders.lsa ...` extracts |
| `tools/ppcinfo.py [--game <id>]` | static queries over the dumped image without symbols (function starts from `ppc_func_mapping.cpp`; a function spans to the next start; names from `SSA_symbols.txt` and imports) | `callers <addr\|import>` (functions that `bl` to an address or import thunk, e.g. `VdSwap`); `callees <addr>`; `strings <addr>` (C strings referenced by `lis/addi` pairs); `dis <addr> [count]` (capstone; no VMX128); `asm <addr>` (XenonRecomp's own disassembly, handles VMX128); `find-string <text>` (functions referencing strings that contain text) |
| `tools/xmatch.py` | matches another game's functions against SSA's (reference tree = a checkout with SSA's `private/`): `callgraph` (seeded by imports), `shape` (instruction-shape similarity), `offsets` (D-form displacements that moved, e.g. the D3D device's) | `xmatch.py <reference tree> <game> callgraph\|shape\|offsets ...` (usage in its header) |
| `tools/find_switch_tables.py` | switch tables + `functions` boundaries ([xenonrecomp.md](xenonrecomp.md)) | `<image.bin> <xexdump.txt> <functions.toml> [<ppc_func_mapping.cpp>] > <G>_switch_tables.toml` |
| `tools/shaders/lsa.py` | reader of the Lux `.lsa` shader archives (format in [shaders.md](../../runtime/gpu/shaders.md)); extracts containers | `lsa.py <file.lsa> [outdir]` |
| `tools/shaders/ucode.py` | feature census of Xenos microcode: which control-flow ops (`CF_NAMES`: Nop, Exec, ExecEnd, CondExec, CondExecEnd, CondExecPred, CondExecPredEnd, LoopStart, LoopEnd, CondCall, Return, CondJmp, Alloc, CondExecPredClean(End), MarkVsFetchDone), fetch kinds (VertexFetch 0, TextureFetch 1, GetTextureBorderColorFrac 16, ComputedLod 17, Gradients 18, Weights 19, SetTextureLod 24, SetGradientsHorz/Vert 25/26), exports | `ucode.py <container files...>`; finds what a translation may silently get wrong (XenosRecomp ignores some instructions) |
| `tools/shaders/build_cache.sh` / `extract_dxil.py` / `check_dxil.sh` / `dxil_check.cpp` | build the shader cache; extract every DXIL container; validate with `dxil.dll` under Wine | [shaders.md](../../runtime/gpu/shaders.md) |
| `tools/sigmatch/sigmatch.py` | find SSA's hooks in another build | [sigmatch.md](sigmatch.md) |
| `tools/pipelines/{tour.sh,merge_lists.py}`, `tools/perf/{level_run.sh,frames.py}` | pipeline baseline, frame times | [pipeline_cache.md](../../runtime/gpu/pipeline_cache.md), [debugging.md](../../porting/debugging.md) |
| `tools/day1.sh <game> <iso>` | steps 1-2 of `docs/porting/day1.md`: extract `default.xex`, xexdump, sha256, make SSA's signatures from SSA's image, run `sigmatch match`; output only in `private/<game>/day1/` | [`docs/tools/day1.md`](../../../docs/tools/day1.md) |

## Recipes

- **Find a function by string**: `ppcinfo.py find-string "incorrect header check"` -> `inflate`; `find-string "nglListSend called while"` -> `nglListSend`.
- **Find who calls a kernel import**: `ppcinfo.py callers VdSwap` -> `D3DDevice_Swap`; `callers VdInitializeRingBuffer`; `callers VdSetGraphicsInterruptCallback`.
- **Check a sigmatch result**: `ppcinfo.py asm <SSA addr>` against the new bytes (for the new game: `ppcinfo.py --game <id> asm <addr>`).
- **Which function is a vtable target**: read the RTTI names with a regex over `image.bin` (`.?AV...@lux@@`), follow the vtable pointer.
- **Write-protect a page to find a CPU reader of a resolve**: that is how `sub_821404D8` was found (temporary instrumentation in the runtime, not committed).
- **Dump run-time shaders**: `SKY_DUMP_SHADERS=<dir>` on one run; `build_cache.sh <iso> <dir>`.
- **Frame capture**: `SKY_D3D_CAPTURE=<s>` and `SKY_DUMP_FRAME=<s> SKY_DUMP_PREFIX=<p>` ([debugging.md](../../porting/debugging.md)).
