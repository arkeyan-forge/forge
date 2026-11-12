# Day 1: the Giants disc arrives (the worked example)

> **Purpose:** the exact commands that took the Giants disc from an ISO to a first boot with logs, as a worked example for the next game · **Status:** current (2026-10-26) · **Tags:** [Xbox] [Giants]

This page is how it went for Giants, a procedure to follow for any game, with
the commands updated to today's layout. Everything runs from the repo root, on the Mac, with SSA's game data already
prepared (`tools/prepare_game.sh` on the SSA disc: it builds the host tools and writes `private/image.bin`, which
sigmatch reads to make SSA's signatures). Giants files go in `private/giants/`, never in `private/` (SSA's). Steps 1
and 2's sigmatch part are scripted: `tools/day1.sh giants private/discs/<Giants>.iso` writes
`private/giants/day1/` (`default.xex`, `xexdump.txt`, `default.xex.sha256`, `ssa_sigs.json`, `sigmap.json`,
`sigmatch.txt`). Known problems and their fixes: [problem_catalogue.md](problem_catalogue.md); Giants' own boot notes:
[games/giants/docs/boot.md](../../games/giants/docs/boot.md).

```sh
GISO="<Giants disc>.iso"
T=private/build-host-tools          # XenonRecomp and xexdump, built by tools/prepare_game.sh
mkdir -p private/giants
```

## 1. Extract and identify the XEX

```sh
python3 -I tools/shaders/xdvdfs.py "$GISO" private/giants /default.xex
$T/tools/xexdump/xexdump private/giants/default.xex private/giants > private/giants/xexdump.txt
head -20 private/giants/xexdump.txt                 # base, entry, sections (.text, BINK, RADCODE...)
shasum -a 256 private/giants/default.xex            # the GameRelease row (games/giants/info.cpp)
```

Title ID, media ID and version: the launcher reads them (`ReadExecutionInfo`, `host/launcher.cpp`). Before a game has
its `GameRelease` row, the SSA build given its ISO says "This disc image is another game (title ID XXXXXXXX)": that
line is the title ID. Record region, size and Redump name (Giants: `games/giants/info.cpp`, `games/giants/game.txt`).

## 2. sigmatch, then XenonRecomp's first pass

sigmatch first, because it gives the CRT helper addresses XenonRecomp needs. SSA's signatures are game bytes: they are
made from SSA's image into `private/` and never committed (`docs/giants/ssa_hooks.json` is metadata only).

```sh
python3 -I tools/sigmatch/sigmatch.py make docs/giants/ssa_hooks.json --image private/image.bin \
    --sigs private/giants/ssa_sigs.json
python3 -I tools/sigmatch/sigmatch.py match docs/giants/ssa_hooks.json private/giants/image.bin \
    --sigs private/giants/ssa_sigs.json --out private/giants/sigmap.json --toml > private/giants/sigmatch.txt
grep -c " 1.0 " private/giants/sigmatch.txt; sort -k4 -n private/giants/sigmatch.txt | head -40   # weakest first
```

- Confidence 1.0/0.9: take it. Below 0.9 or `check by hand`: compare the asm,
  `python3 -I tools/ppcinfo.py asm <SSA addr>` against `python3 -I tools/ppcinfo.py --game giants asm <Giants addr>`
  (`--game giants` reads `private/giants/{image.bin,xexdump.txt,ppc/}`).
- `config` entries (`*_address`): the `--toml` lines at the end are the `[main]` block of the new config.
- `config_function` entries are SSA's layout: drop them, regenerate.

The config is now committed: `games/giants/config/Giants.toml` and `Giants_switch_tables.toml` (no game bytes: addresses
only). It uses the kit's layout (`../private/default.xex`, output `../ppc`), so run XenonRecomp on a copy laid out the
same way, as `tools/prepare_game.sh` does for SSA:

```sh
W=private/giants/ppc-gen; rm -rf "$W"; mkdir -p "$W/config" "$W/private" "$W/ppc" private/giants/ppc
cp games/giants/config/* "$W/config/"; ln -s "$PWD/private/giants/default.xex" "$W/private/default.xex"
$T/thirdparty/XenonRecomp/XenonRecomp/XenonRecomp "$W/config/Giants.toml" \
    thirdparty/XenonRecomp/XenonUtils/ppc_context.h > private/giants/xenonrecomp.log 2>&1
cp "$W"/ppc/* private/giants/ppc/; grep -ciE "unrecognized|error|switch" private/giants/xenonrecomp.log
```

How the config was made the first time (for the next game): start from `games/ssa/config/SSA.toml`; the 10 `*_address`
lines from sigmatch's `--toml` output; `functions = [ ]` at first; `invalid_instructions`: keep the padding entry,
re-find the 3 handler entries (`ssa_hooks.json` `config_data`); then
`python3 -I tools/find_switch_tables.py <image.bin> <xexdump.txt> <functions.toml> > <Game>_switch_tables.toml`, paste
the functions into the config, run XenonRecomp, and run `find_switch_tables.py` again with `ppc/ppc_func_mapping.cpp` as
its 4th argument (pass 2).

Loop: run `find_switch_tables.py` (switch tables and the functions list), rerun XenonRecomp, until the log has no
unknown instruction and no unhandled jump table. Every "Indirect call to unrecompiled guest code" at run time later is a
missing function boundary: add it to `functions` ([problem_catalogue.md](problem_catalogue.md)).

## 3. Imports against the runtime

```sh
comm -23 <(awk '$1=="import"{sub("__imp__","",$3);print $3}' private/giants/xexdump.txt | sort -u) \
         <(sed -n 's/^IMPORT(\(.*\))/\1/p' runtime/kernel/import_list.inc | sort -u)    # imports we lack
comm -13 <(awk '$1=="import"{sub("__imp__","",$3);print $3}' private/giants/xexdump.txt | sort -u) \
         <(sed -n 's/^IMPORT(\(.*\))/\1/p' runtime/kernel/import_list.inc | sort -u)    # SSA-only (harmless)
```

On SSA the first list is empty (218 imports, 218 known). Each missing one: implement it in `runtime/kernel/*.cpp`
(our own code: Xenia and the other projects are read-only references), add it to `import_list.inc`. A stub must write its outputs
(D5 in [gotchas.md](gotchas.md): weak stubs cost more than missing functions; Giants' extra imports are in `runtime/kernel/imports_giants.cpp`).

## 4. sigmatch results into hooks

For each hook in `sigmap.json`: the address goes into `games/giants/addresses.h` (a zero is "unknown"; the hook
contract: [../architecture/hooks.md](../architecture/hooks.md)), on a `games/giants` branch. Check the D3D device offsets in the matched D3D
functions (`ssa_hooks.json`, `not_signed.struct_offsets`). Data globals come out of the `via 0x...` function.

## 5. Shaders

Giants (Alchemy) keeps its shaders in compressed `.arc`/`.bld` archives and none in the XEX
([games/giants/docs/shaders.md](../../games/giants/docs/shaders.md)). The cache is built from a run-time dump
(`SKY_DUMP_SHADERS=<dir>`, a first run with the cache missing) or from the disc extractor:

```sh
python3 -I tools/shaders/giants_extract.py "$GISO" private/giants/shaders/disc [<runtime dump dir>]
SKY_GAME=giants tools/shaders/build_cache.sh "$GISO" <dump dir>    # writes private/giants/ppc/shader_cache.cpp
```

SSA for comparison: shaders.lsa + effects.lsa + 81 shaders embedded in the XEX + the movie player's (dumped at run time):
"1115 of 1119 shaders translated" (4 skipped: 2 XEX-embedded with no constant table, 2 from shaders.lsa the Metal
compiler rejects). Giants: 4444 containers (50 of them Bink 2, only in the dumps).

## 6. First boot with logs

```sh
cmake -S . -B build-giants -G Ninja -DCMAKE_BUILD_TYPE=Release -DARKEYAN_GAME=giants && nice ninja -C build-giants -j4 ArkeyanRuntime
R=$PWD/private/giants/run1; mkdir -p "$R"
SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null \
SKY_SETTINGS=$R/none.ini SKY_LAUNCHER_INI=$R/none.ini SKY_COLLECTION=$R/collection SKY_SAVE_DIR=$R/save \
SKYLANDERS_DATA=$R/data SKY_LOG=$R/game.log \
SKY_PORTAL_LOG=1 nice "./build-giants/ArkeyanRuntime/Skylanders Giants" "$GISO" > "$R/log" 2>&1 & pid=$!
(sleep 60; kill $pid; sleep 3; kill -9 $pid) & wait $pid
grep -E "\[stub\]|unrecompiled|Indirect call|SIGBUS|SIGSEGV|assert" "$R/log" | sort | uniq -c | sort -rn | head -30
```

## Expected blockers and how to read them

| Log | Meaning | Action |
|---|---|---|
| `Indirect call to unrecompiled guest code 0x8xxxxxxx` | a function XenonRecomp did not see | add to `functions` in `games/giants/config/Giants.toml`, rerun step 2 |
| `[stub] Name` repeating, hang at start | a kernel/XAM call returns success without outputs | implement properly (the 22 s start-up hang on SSA) |
| crash in `sub_` near heap code | missing fences or a bad hook address | check `patches/xenonrecomp.patch` applied, then every hook with confidence < 1 |
| black screen, frames counted | ring/command-buffer hooks not matched | `D3DDevice_*` entries in `sigmap.json`, device offsets |
| movies frozen, no frames | Bink hooks (`BinkDoFrame`, `Movie_Update`) or missing movie shaders | dump with `SKY_DUMP_SHADERS`, rebuild cache |
| silence | XMA engine hooks (two engines: XAudio2 and Wwise copies) | `Xma*` entries; engine count may differ |
| portal not detected | different command set (lights) | `SKY_PORTAL_LOG=1`, compare with `docs/runtime/host/portal_probe.md` |
| dark level, no lights | light pre-pass depth readback (`LightTile_ClassifyJob`: 2 copies in SSA) | check which copy is the worker's |
