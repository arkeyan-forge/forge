# XenonRecomp: config, every option, every patch hunk, boundaries, fences

> **Purpose:** how is XenonRecomp configured for a game, and what does each hunk of our patch fix? · **Status:** current (2026-11-04) · **Tags:** [Xbox] [SSA]

Upstream: `thirdparty/XenonRecomp` (hedge-dev, MIT). Our changes: [`patches/xenonrecomp.patch`](../../../patches/xenonrecomp.patch) (1333 lines, applied by the root CMake at configure time; after a merge that touches patches:
`git -C thirdparty/XenonRecomp checkout -- .` then cmake + ninja). SSA's config: [`games/ssa/config/SSA.toml`](../../../games/ssa/config/SSA.toml), [`SSA_switch_tables.toml`](../../../games/ssa/config/SSA_switch_tables.toml),
[`SSA_symbols.txt`](../../../games/ssa/config/SSA_symbols.txt). Giants': [`games/giants/config/Giants.toml`](../../../games/giants/config/Giants.toml). The patch is **[Xbox]** (every game), the numbers **[SSA]**. No `midasm_hook` is used anywhere.

## What it produces and how it is used

`XenonRecomp <toml> <ppc_context.h>` writes `ppc/` (`ppc_recomp.*.cpp` ~160 MB, `ppc_func_mapping.cpp`, `ppc_config.h`, `ppc_context.h`): every function as `void f(PPCContext& ctx, uint8_t* base)`; **generated, never edited, never committed** (it derives
from the game). Each PowerPC instruction is a comment above its C++, so grep works on the output. Details that matter at run time: loads/stores byte-swap; vector registers are reversed as whole 16-byte vectors; FPU denormals are managed per instruction through a
stored state; **indirect calls go through a 64-bit-pointer table indexed by `(addr - CODE_BASE) * 2` placed after the image** ([../xbox360/memory.md](../../runtime/xbox360/memory.md)); virtual calls land in `UnmappedFunction` if the target was not seen as a function start.
Functions are **weak** so `PPC_FUNC(sub_X)` overrides them ([../xbox360/threads_sync.md](../../runtime/xbox360/threads_sync.md)). A tool `XenonAnalyse` finds jump tables for some games; ours does not match SSA's compiler (see `find_switch_tables.py` below).
`ppc_config.h` gives `PPC_IMAGE_BASE/SIZE`, `PPC_CODE_BASE/SIZE`. **Rebuild every prebuilt `libArkeyanRecompLib.a` when `private/<id>/ppc/` changes** (a stale one crashed the main menu: `Indirect call to unrecompiled guest code ctr=0x826854A8`).

## `config/SSA.toml` option by option

| Option | SSA value | Meaning |
|---|---|---|
| `file_path` | `../private/default.xex` | the player's XEX (paths relative to the TOML) |
| `out_directory_path` | `../ppc` | must exist |
| `switch_table_file_path` | `SSA_switch_tables.toml` | the jump tables (210) |
| `skip_lr`, `skip_msr` | false | skipping the link register / MSR (an optimisation valid only if the game does not use exceptions; we keep them off: `blrl` needs `lr`) |
| `ctr_as_local`, `xer_as_local`, `reserved_as_local`, `cr_as_local`, `non_argument_as_local`, `non_volatile_as_local` | all **true** (every game, 2026-11-04) | CTR, XER, the reservation, the CR fields, r0/r2/r11/r12 and the non-volatile registers become each function's locals instead of `PPCContext` fields: [Registers as locals](#registers-as-locals) |
| `restgprlr_14_address` / `savegprlr_14_address` | `0x825F5CE0` / `0x825F5C90` | CRT register restore/save helpers; byte patterns (public README): restgprlr `e9 c1 ff 68` (`ld r14,-0x98(r1)`), savegprlr `f9 c1 ff 68` |
| `restfpr_14_address` / `savefpr_14_address` | `0x825F647C` / `0x825F6430` | patterns `c9 cc ff 70` / `d9 cc ff 70` |
| `restvmx_14` / `savevmx_14` / `restvmx_64` / `savevmx_64` | `0x825FF3D8` / `0x825FF140` / `0x825FF46C` / `0x825FF1D4` | patterns `39 60 fe e0 7d cb 60 ce` / `... 61 ce` / `39 60 fc 00 10 0b 60 cb` / `... 61 cb` |
| `longjmp_address` | `0x825F8430` | restores f14-f31/r13-r31 from the jmp_buf and calls `RtlUnwind` when an unwind function is set (find it by calls to `RtlUnwind`) |
| `setjmp_address` | `0x825FECA0` | mirror stores (`stfd f14, 0(r3)` ...) behind a hook-pointer check; sits just after longjmp |
| `invalid_instructions` | `{0x00000000, 4}` padding; `{0x825FFC00, 8}` **`__CxxFrameHandler` + FuncInfo** in front of C++ EH functions; `{0x829E6114, 8}` **`__C_specific_handler` import thunk + scope table** in front of SEH functions; `{0x8260CA80, 8}` a custom handler + null data in front of `0x8260CA5C` | data words XenonRecomp must skip instead of decoding as code (re-find the 3 handler entries: sigmatch `config_data`) |
| `functions` | **146** `{ address, size }` | functions outside `.pdata` the analyzer cannot size: leaf functions with jump tables, `bdz/bdnz` chains, adapters ending in `b <next>`, vtable targets merged into a neighbour, **packed leaf functions** |

`setjmp`/`longjmp` on **UWP**: the translated code imports `_setjmp`, which binds to api-ms-win-crt-private: redirected to ucrtapp's own.

## Registers as locals

[Xbox] [SSA] [Giants] [SwapForce] [TrapTeam] [Alchemy] [V] Since 2026-11-04 every game's config turns the six `*_as_local`
options on. Before, every register was a field of `PPCContext`: the compiler wrote each one back to memory before every
call (the callee could read it) and read it again after, and each compare read `ctx.xer` for its SO bit. As locals,
r14-r31, f14-f31 and v14-v31/v64-v127 stay in host callee-saved registers across calls; CR, XER (SO a known 0), CTR,
r0/r2/r11/r12 never reach memory; the calls to the `__savegprlr`/`__restgprlr`/`__savefpr`/`__restvmx` helpers go away
(each function keeps its own non-volatile registers). SSA's executable: 56.1 MB of code -> 46.8 MB.

It relies on a function never reading a register its caller set, other than the ABI's arguments (r3-r10, f1-f13,
v2-v13), r1 and r13. Two kinds of guest functions do, and XenonRecomp keeps them **in the context** (every register a
`PPCContext` field, as before; the patch's `Recompiler::FindContextFunctions`): the save/restore helpers, and the MSVC
**funclets**: a `__finally` block is a separate function that its parent also calls on the normal path, with r12 = the
parent's frame, and it reads the parent's non-volatile registers. Found by their prologue (r12 read before any write;
every other function's first use of r12 is `mflr r12`), plus the config's `context_functions` list. A function with
locals that calls a context function stores its locals to the context before the call, reads them back after, and
puts the context's previous values back (`$CONTEXT_STORE$` / `$CONTEXT_LOAD$`, filled in once the function's locals are
known): a context function further up the stack still owns those fields. `PPCContext` keeps every field
whatever the options, so the runtime's view of it does not change; `ctx.ctr` and `ctx.r31` simply are not kept up to
date any more (the crash line and the two diagnostics that read them check `PPC_CONFIG_*_AS_LOCAL`).

Without that, the first try (2026-11-04) hung SSA at its first frame and faulted Giants at start in
`RtlLeaveCriticalSection` from a `__finally` funclet (`sub_825DCAF8`, r31 = r12 - 144 with r12 a zero local). The
second, without putting the context back, faulted SSA in `sub_8247C780` (a context function): a callee with locals had
stored its own r27 over the context's while calling a funclet.

**`context_functions`** (in `[main]`, written by `tools/xenonrecomp/local_reads.py <ppc dir> --toml`): the checker
translates nothing; on a translation it runs a forward "set on every path" pass over each function's labels and gotos
and lists the functions that read a register before writing it. Not counted: saves of the non-volatile registers to
the stack frame, `mfcr` into r12, XER.SO copied by a compare, the context stores and loads above. `--toml` also follows
the edge after a call that falls into a label (a callee that does not return: an assert, `longjmp`), so it over-lists:
a cold function in the context costs nothing, a missed one breaks the game. What it lists: setjmp's body, `__chkstk`
(r12 = the size to probe), jump-table words decoded as instructions, zeroing idioms (`xor r30,r30,r30`,
`subfe rX,r29,r29`), filter funclets that use the parent's r30/r31, paths after a call that does not return. Per game:
SSA 34, Giants 20, Swap Force 24, Trap Team 20, SuperChargers 32, Imaginators 62; with the funclets, the functions in
the context: SSA 826, Giants 304, Swap Force 14867 (its C++ EH handlers), Trap Team 765, SuperChargers 2166,
Imaginators 2343. After a second translation the checker reports none. **Run it again after any change to a game's
`functions` or switch tables**: translate, `--toml`, replace the list, translate again (an entry that is no longer a
function is ignored).

**Measured** (M1 Pro, scale 2, `tools/perf/game_perf.sh` with `FALLBACK=0` and `--fps-limit 0`, each pair back to back
alone on the machine (`GAMELOCK_EXCLUSIVE`), load 8-17; "guest" is the `[perf]` render thread's "guest code and other",
CPU the process's CPU time per frame from `ps`). Only SSA runs uncapped; the Alchemy games pace themselves at 60 (30 for
Imaginators on this branch), so their frame time cannot fall and the CPU time shows the gain:

| Game, level | before: fps, work median, guest, CPU | after |
|---|---|---|
| SSA `level_008` (3456 draws) | 53.6 fps, 18.5 ms, 10.2 ms, 41.5 ms | **57.9 fps, 16.0 ms, 5.8 ms**, 39.0 ms (now GPU-bound: GPU thread 98 % busy, GPU wait 8.5 ms) |
| Giants `level_203_the_machine` | 60, 7.1 ms, 4.4 ms, 11.8 ms | 60, **5.6 ms, 3.0 ms, 9.3 ms** |
| Swap Force `barrens/brn2` | 60, 16.6 (paced), 10.6, 37.7 ms | 60, p99 19.1 -> 17.2 ms, **29.2 ms** |
| Trap Team `level_hub_2014_01` | 60, 16.6 (paced), 13.1 (with the swap wait), 27.4 ms; busiest thread 15.7 ms | 60, **15.9 ms; busiest thread 5.4 ms** |
| SuperChargers Academy | 60, 16.6 (paced), 11.4, 30.1 ms | 60, **24.1 ms** |
| Imaginators `level_508` | 30, 33.2 (paced), 29.5, 33.0 ms | 30, 52.9 ms: not comparable, this branch's config lacks the swap-wait hook of the 0.9.0 lane (`sub_8268AA40` spins: 1605 of 2197 samples of its thread); the lib before had it |

SSA's same translation at `-O2` (the kit's level): 57.6 fps, guest 6.1 ms (noise against `-O3`); with ThinLTO over the
whole executable: 57.6 fps, guest 5.9 ms, renderer 9.9 ms against 10.4 ms ([below](#compile-options-of-the-game-code)).

`setjmp`: the context is saved and restored as before; a non-volatile local modified after the `setjmp` keeps its host
value after a `longjmp` (the C rule for non-`volatile` locals, which the guest's compiler also follows).

For a player: the config and the patch are in the kit, so Arkeyan-Builder translates and compiles each installed game
once more (a first install's compile time); updates are incremental again after that.

## The patch hunks (what each fixes and why)

| Hunk (file) | What changes | Why |
|---|---|---|
| **Fences** (`recompiler.cpp`) | `lwsync` -> `__atomic_thread_fence(__ATOMIC_ACQ_REL)`, `sync` -> `SEQ_CST`, `eieio` -> `RELEASE`; before: nothing emitted. `dcbst` becomes a no-op | the title's spin locks release with `lwsync; stw 0`: on ARM64 the release was visible before the critical section's stores: rare heap corruption (D1); 1773 fence sites in `private/ssa/ppc/`; `dmb ish` on ARM64, only `sync` costs an `mfence` on x86 |
| **Jump-table index** (`recompiler.cpp`) | `bctr` switches emit `switch (ctx.rN.u32)`, was `.u64` | the game's bounds check (`cmplwi`) is 32-bit, but `addi` on a negative word leaves the upper half set (0xFFFFFFFF + 2 = 0x1_0000_0001): a 64-bit `switch` with `default: __builtin_unreachable()` then reads the table out of range on x86 (arm64's compiler happened to truncate). Giants' Windows build faulted at start (`sub_821C7A70`, a table load 4 GB off); found with the Windows kit under Wine, 2026-10-26 **[V]** |
| **x64-safe divides** | `divw/divwu/divd/divdu` emit `d == 0 ? 0 : d == -1 ? (0 - n) : n / d` (signed) / `d == 0 ? 0 : n / d` (unsigned) | PowerPC leaves the quotient undefined for 0 and INT_MIN/-1; **x64 `div` traps, ARM64 returns 0**: a latent Windows crash macOS hid (1133 regenerated lines) |
| **`blrl`** | was `__builtin_debugtrap()`; now saves the old `lr` in a temp, sets `ctx.lr = next address`, `PPC_CALL_INDIRECT_FUNC(temp)`, marks the FP control state unknown | calls through the link register |
| **`vupkd3d128` FLOAT16** | type 3 (FLOAT16_2): the halves of the last word -> (x, y, 0, 1); type 5 (FLOAT16_4): last two words -> (x, y, z, w) via `PPCHalfToFloat` (`_Float16`) | missing cases |
| **ARM64 `setcsr`** | `msr fpcr, x` needs a 64-bit operand: `uint64_t(csr)` | compile error |
| **XEX loader** (`XenonUtils/xex.cpp`) | image buffer `max(sum of blocks, security->imageSize)`, tail zeroed | blocks can sum to less than the declared size |
| New instructions | `addc`, `addme`, `bdnzt`, `bdzf`, `eqv`, `frsqrte` (`1.0/sqrt`), `lbzux`, `lfdu`, `lfdux`, `lfsu`, `lfsux`, `lhau`, `lhzu`, `lhzux`, `lvehx`, `lwzux`, `stbux`, `stdux`, `stfdu`, `stfsu`, `stfsux`, `sthu`, `sthux`, `subfze`, `vaddsws`, `vcfpuxws128`/`vctuxs` (NaN/negatives -> 0, >= 2^32 -> UINT_MAX: `simde_mm_vctuxs`), `vcmpgtuw` (`simde_mm_cmpgt_epu32`), `vnor`/`vnor128`, `vpkswss(128)`, `vrfip(128)`, `vsel128`, `vsubshs`, `vsububm` | otherwise `__builtin_debugtrap` or wrong results |
| **`vslo`/`vsro` count** (2026-11-02) | the octet count is read into a local before the loop | the loop read `vB.u8[0]` after writing `vD`: with vD == vB, 15 of 16 bytes were wrong, silently. 3 such `vsro` in Giants and in Swap Force (73 sites re-emitted in 4 functions per Alchemy game, 1 file each; none in SSA, none with vD == vB in Trap Team). Test: `ArkeyanXenonRecompTest` (47 of its 56 checks fail before) |
| **CR6 of byte/halfword compares** (2026-11-02) | `vcmpgtsb.`, `vcmpgtsh.`, `vcmpgtub.`, `vcmpgtuh.` set CR6 from the byte mask (`movemask_epi8`, all = 0xFFFF) | the dot forms left CR6 stale for the next all/none branch (a float mask would also miss elements below each word's top byte). None of the four games uses them today; same test |
| **Our decoder** (`XenonUtils/disasm.{h,cpp}`, `XenonUtils/CMakeLists.txt`, `thirdparty/CMakeLists.txt`) | `ppc::Disassemble` calls [`tools/xenonrecomp/ppc_decoder.cpp`](../../../tools/xenonrecomp/ppc_decoder.cpp) (`ARKEYAN_PPC_DECODER_DIR`, set by the root CMake); `thirdparty/disasm` is no longer a subdirectory (only its `ppc-inst.h`, the instruction ids, is read) | binutils' `ppc-dis.c` is GPL and the kit ships XenonRecomp: no GPL code in any binary now (below) |
| **Context functions** (`recompiler.cpp/.h`, `recompiler_config.cpp/.h`, `ppc_context.h`) | `FindContextFunctions` (helpers, r12-reading funclets, the config's `context_functions`) are translated with every register in the context; a call to one from a function with locals stores and reloads those locals; `PPCContext` keeps every field whatever the `*_as_local` options | the `*_as_local` options: [Registers as locals](#registers-as-locals) |

Our translator test, `tools/xenonrecomp/`: `ArkeyanXenonRecompSampleGen` translates invented instructions with the patched sources at build time; `ArkeyanXenonRecompTest` runs the C++ against the PowerPC semantics (clang, native builds only); since 2026-11-04 it also translates a funclet and its parent with the `*_as_local` options and checks the funclet sees the parent's r12 and r30 and the context's own values come back. `tools/xenonrecomp/local_reads.py` is the checker of [Registers as locals](#registers-as-locals).

**Trap outside the patch**: `XenonUtils::ByteSwap(float)` converts numerically. Swap the bits (`memory.h`).

## The PowerPC decoder: `tools/xenonrecomp/ppc_decoder.cpp`

Our own (MIT), written from the public documents only (Book I-III 2.02, the Programming Environments manual, the AltiVec
PEM, the Cell handbook, the public VMX128 descriptions, the processor manuals' SPR tables; each form's code names its
source). It is a **decode tree over the word's fields**, not an opcode table: a `switch` on the primary opcode, then on
the extended-opcode field of the form (X/XL/XFX bits 21-30, XO 22-30 with OE, A 26-30, MD/MDS, VA, VX/VC, the VMX128
forms of opcodes 4-6), reaching the instruction directly; each form's code extracts and prints its own operands
(`Out`), and the instruction's descriptor (`PpcOpcode`: name, id, which operands) is a constant next to that code. It
fills the same `ppc_insn` (`instruction`, `opcode->{name,id,operands}`, `operands[8]`, `op_str`) the
recompiler reads, so `recompiler.cpp` is unchanged and **the `ppc/` comments (`// name operands`) are byte for byte the
old ones**. It reproduces the old translator's choices, which are not the ISA's: it also decodes POWER and the PPC403/405
instructions outside opcode 4, such as `dlmzb` (data words inside functions are printed too); extended mnemonics only where the fields they fix
select them (`mr`, `not`, `li`, `rotlwi`, `clrlwi`, `clrldi`, never `slwi`/`srwi`); bc hints from BO's "at" bits
but bclr/bcctr from the older y bit (`beqlr+` for BO 01101); RA = 0 printed `0` or `r0` per instruction; an optional
operand at zero is left out unless another optional one is set (`mtfsf 93,f11,0,1`). On primary opcode 4, none of the
PPC405 multiply-accumulate forms (`machhw`, `macchw`, `nmachhw`, `mulchw`...) is decoded: every word with bit 27 set is
`vsldoi128` (review R6, 2026-11-03: they hid 2,555,904 of its 33,554,432 encodings, 7.6 %, which came out unknown; no
word of the four games was one, so no generated code changed) [V] (`ArkeyanPpcDecoderTest` sweeps every encoding).

Proof (2026-11-02): [`decoder_compare.sh`](../../../tools/xenonrecomp/decoder_compare.sh) builds the old decoder in
`private/decoder-compare/` from the submodule and the patch of commit `6be5136` (local only, never committed) and
compares name, id, operand values, operand list and text: **0 differences on the code sections of the four games**
(SSA 2,337,110 words, Giants 1,257,511, Swap Force 3,352,382, Trap Team 1,640,907, at their addresses), and **0 on all 2^32 words** at 0x82000000 (the
same comparison over every word, what `--all` runs).
The four games retranslated with the new XenonRecomp: `ppc/` **byte-identical** to the old one's (and to `private/<id>/ppc/`),
logs identical. Rebuilt as the decode tree (same day), the same proof again: 0 differences on all 2^32 words and on the
four games, `ppc/` byte-identical; and faster, one thread, best of 5: **15.5 to 41.8 million words per second** on the
four games' code, **14.3 to 29.3** on random words (the table searched each primary opcode's entries in order).
`ArkeyanPpcDecoderTest` (unit test): 196 checks on words built from the encodings (every form, fields at
their limits). A decoder change: `ArkeyanPpcDecoderTest`, then `decoder_compare.sh` on the four `default.xex` and `--all`.

## Function boundaries: `tools/find_switch_tables.py`

Usage: `python3 -I tools/find_switch_tables.py private/image.bin private/xexdump.txt private/functions.toml [ppc/ppc_func_mapping.cpp] > config/<G>_switch_tables.toml`, paste `functions.toml` into the config's `functions = [ ... ]`, rerun XenonRecomp, **loop until the log has no unknown
instruction and no unhandled jump table**. How it works:
- XenonAnalyse's fixed patterns do not match this compiler's jump tables (reordered `slwi/addi`, interleaved nops). For every `bctr` the tool finds the **bounds check** (`cmplwi crN, rI, N` + `bgt crN`, or `ble crN, dispatch; b default`, or a distant check upstream when several paths reach the dispatch block, with
  register aliases `mr`) and **concretely simulates the instructions to the `bctr` for each index 0..N**, reading tables from the image. Output `[[switch]] base = ..., r = ..., default = ..., labels = [...]` (210 tables for SSA).
- It also emits `functions` for code outside `.pdata`: leaf functions containing a jump table (the analyzer stops at the `bctr`), `bdz/bdnz` chains (small switches on CTR), adapters ending in `b <next function>`, code reached only through function pointers in data (vtables) merged into a neighbour, and
  **packed leaf functions**: gaps without `.pdata` split after every unconditional terminator, then pieces a branch or jump table crosses joined again. The optional 4th argument (a previous mapping) excludes pointer targets that already are functions.
- **Packed leaves without `.pdata`** (27 on SSA: vtable stubs/accessors reached only through pointers built at run time, e.g. `0x826854A8` size 4): choosing a save slot called one -> `Indirect call to unrecompiled guest code (ctr=0x826854A8 lr=0x8267EE10)` (D13). **Run the tool before the first boot and treat every such message as a missing function boundary**
  (look at ctr: is it inside `.pdata` coverage?).

## The indirect-call guard (`ArkeyanRecompLib/indirect_call_guard.h`)

Force-included (`-include`) into every `private/<id>/ppc/` translation unit (`ArkeyanRecompLib/CMakeLists.txt`), overriding the generated `PPC_CALL_INDIRECT_FUNC` (an `#ifndef` in `ppc_context.h`, so `private/<id>/ppc/` is not regenerated). Fast path: one rotate-and-compare
(`__builtin_rotateright32(target - CODE_BASE, 2) < CODE_SIZE/4`: the rotate moves the two alignment bits to the top, so a misaligned target is out of range too), the table load, a null test. Else `SsaBadIndirectCall` logs and skips (`r3 = 0`).
A prebuilt library built before this change has no guard: rebuild `private/<id>/ppc/` once. Other compile options of `ArkeyanRecompLib`: `-fno-strict-aliasing -Wno-unused-label -Wno-unused-variable`; **clang only** (the code needs `__builtin_assume`, `__builtin_rotateleft*`; Homebrew's GCC mingw cannot build it); fmt's `format.h` needs `<cstdlib>` force-included under libc++/mingw.


## Compile options of the game code

[Xbox] [V] (2026-11-04) What the game code is compiled with, per platform, and what was tried:
- **Optimisation level**: the kit's commands (and so the player's build) are `-O2`, a developer's Release build `-O3`.
  SSA `level_008`, the same translation: 57.6 fps / guest 6.1 ms at `-O2`, 57.9 / 5.8 at `-O3`; one 4 MB file compiles
  6 % faster at `-O2` with the same code size. Kept at `-O2`.
- **CPU**: macOS arm64 is `apple-m1` (the compiler's default for arm64 macOS, every Apple Silicon Mac); Windows x64
  `-march=btver2 -mno-sse4a` (AVX, F16C, BMI1, MOVBE: one `movbe` per guest load or store); Linux x64 `x86-64-v2` since
  2026-11-04 (it was SSE2: [linux.md](../../getting-started/platforms/linux.md)).
- **LTO**: ThinLTO over the whole SSA executable: guest 5.9 ms against 5.8, renderer 9.9 against 10.4 ms: within the
  noise of a shared machine, for a link that each player's compiler (Apple clang, llvm-mingw, zig) must then do on
  100-400 MB of game code. Not used.
- **PGO**: not used, by decision. A profile is derived from running the game (per-function counts keyed by the
  translated code's own hashes): game data, so it cannot ship in the kit or in git; a player-side instrumented run then
  a second compile is too much for an install, and profiles are tied to one compiler version.
