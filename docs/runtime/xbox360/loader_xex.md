# XEX loading, module, imports, host thunks

> **Purpose:** how is the XEX loaded, its imports bound and host thunks installed, and what identifies the title? · **Status:** current (2026-10-26) · **Tags:** [Xbox] [SSA]

Code: [`loader.{h,cpp}`](../../../runtime/loader.cpp), [`kernel/module.cpp`](../../../runtime/kernel/module.cpp), `thirdparty/XenonRecomp/XenonUtils`
(`xex.cpp`, `image.cpp`, `xdbf_wrapper.cpp`). Class **[Xbox]**.

## Load sequence (`LoadXex`)

1. `Image::ParseImage` (XenonUtils) decrypts/decompresses the XEX; **must equal `PPC_IMAGE_BASE` / `PPC_IMAGE_SIZE` of the
   recompiled build** ("default.xex is not the executable this build was recompiled from" otherwise: that is also how a wrong disc
   release is caught at run time; the launcher checks the SHA-256 earlier).
2. Copy the image to guest `base`; fill the lookup table (`UnmappedFunction` everywhere, then `PPCFuncMappings`).
3. `CreateModuleHandle`: a copy of the XEX header in guest memory + an LDR entry (offsets in [kernel_exports.md](kernel_exports.md)).
4. `FindOptionalHeader` keys used: `0x000103FF` import libraries, `0x00020104` TLS info (slot count, raw data address, data size, raw size),
   `0x00020200` default stack size (else 0x40000), `0x00030000` system flags.
5. `ResolveImports`: per import library, records; **a record whose word is `0x00000060` is a thunk** (XenonUtils already rewrote it to
   `nop; nop; nop; blr` and byte-swapped the IAT slots in place): the pending IAT slot is pointed at the thunk, so calls through the IAT land on
   the thunk, which the lookup table maps to `__imp__Name`. A slot with **no** thunk is a *variable* import (`CreateKernelVariable`). Kernel
   names come from `xbox/xboxkrnl_table.inc` (ordinal -> name); a variable of another library is named `?`.
   The loader logs each variable: `[loader] xboxkrnl.exe!KeTimeStampBundle (ordinal N) -> 0x...`.
6. The XEX **XDBF/SPA resource** named after the title ID holds the title name (string 0x8000, "Skylanders") and icon (image 0x8000,
   a 64x64 PNG): the launcher reads them with `ReadTitleMetadata()` (XDBFWrapper trusts its input: only call it on a verified XEX).

## The code layout record (`ARKEYAN_LAYOUT_RECORD`, opt-in, #72)

With the CMake option `ARKEYAN_LAYOUT_RECORD=ON` (default OFF), the runtime no longer compiles in the four layout bounds
(`PPC_IMAGE_BASE/SIZE`, `PPC_CODE_BASE/SIZE`); the translated code keeps them as constants. Code:
[`guest_layout.{h,cpp}`](../../../runtime/guest_layout.h), `guest_layout_macros.h`, `layout/ppc_config.h`,
`guest_layout_record.cpp`.

- **The record**: `g_guestLayoutRecord` (version, the four bounds, the image's SHA-256 or empty). A normal build compiles
  it from its own generated `ppc_config.h` (`guest_layout_record.cpp`, the one runtime source that sees the constants), so
  a game runs with no new input; the kit builder will write its own at install (study, step 3).
- **`LoadXex`** derives the parsed layout (code = the envelope of every code section, gaps included, as
  `Recompiler::Recompile` writes it), then refuses ("default.xex is not the executable this build was recompiled from:
  <why>") on another record version, another image hash (when recorded), another image or code bound, an empty or
  misaligned code range, code outside the image, an image or function table outside `0x80000000`-`0xA0000000` (above
  SystemAlloc, below the physical windows), an entry point or a translated function outside the code. Then it activates
  the layout (once, read-only afterwards) before the table, imports, thunks or threads, and logs `[loader] code layout
  from the build's record`.
- **The consumers are unchanged**: `layout/ppc_config.h` comes before `ppc/` for runtime sources; it includes the
  generated header (`#include_next`, its policy macros as they are) and redefines the four names to read the active layout
  (`unsigned long long`, as generated). `PPC_LOOKUP_FUNC`, `TableBegin/End`, `SetEntryReadOnly`, `CreateHostThunk` (its
  cursor starts at the first call, after `LoadXex`), `GuestCodeAddress`/`CallGuest` and the three memory queries of
  `kernel/memory.cpp` compute the same values. With the option OFF the preprocessed sources are the old ones.
- **Proof** (2026-11-12): `ArkeyanGuestLayoutTest` (invented layouts: envelopes with gaps, every refusal, table entries,
  guard, image queries, read-only granules and thunk slots equal to the constant path; the lookup loop 1.6-1.8 ns compiled
  in, 1.9-2.0 ns from the record, on the Mac); `ArkeyanGuestThreadTest` built ON. All six games reach a level ON (runs in
  [kits_without_disc.md](../../release/launcher/kits_without_disc.md#step-2-done-2026-11-12)).

## XenonUtils XEX loader fix (patch)

Compressed blocks can sum to **less than the declared image size**: the buffer is sized `max(sum, security->imageSize)` and the tail stays
zeroed (`patches/xenonrecomp.patch`, `xex.cpp`).

## Host thunks (`CreateHostThunk`)

For exports the title reaches through a function pointer (`XexGetProcedureAddress`) rather than an import thunk: take **one word out of a run of >= 16
zero words** in the padding between code sections (scanning down from the end of the code range; those addresses are `UnmappedFunction` and read 0),
register the host function at that address in the lookup table (read-only after `LoadXex`: the thunk opens its 64 KB granule for the write). Indirect calls to the address then run the host function. Used for xam ordinals
**0x4A1 / 0x4A2** (the portal). If no padding is left: "No code padding left for a host thunk" (abort).

## Modules

Only `xam.xex` and `xboxkrnl.exe` exist (`XexGetModuleHandle`, `XexLoadImage`): LDR entries with `DllBase` `0x81000000` / `0x80040000`.
`XexGetModuleHandle(NULL)` is the title's own module. `XexCheckExecutablePrivilege(n)` = bit n of the XEX system flags.

## XEX identity facts (SSA) [SSA]

Title ID `415608AB`, media ID `53CE7B97`, version 0.0.0.2, `default.xex` 12 595 200 bytes, SHA-256 `e195f513...46ef7b`, image base `0x82000000`
size `0xD40000`, entry `0x822DF400`, **218 imports** (a private `imports.txt`; Giants adds 33, [kernel_exports.md](kernel_exports.md)), XDK libraries 2.0.20500.0 (XAPILIB, D3DX9, XGRAPHC, XBOXKRNL,
XNET, XONLINE, XMP, XAPOBA, XAUDIO2, XPARTY, **D3D9LTCG**, XMCOREL, XHV2 2.0.20500.1), LIBCMT 2.0.11886.0, MSVC link 10.0.11886 / C1C2 16.0.11886.
The static-library header is XEX optional header `0x200FF`: **compare it line by line on day 1** (a different D3D9LTCG means re-deriving every
`d3d/device.h` offset). Sections: [lux/binary_layout.md](../../engines/lux/binary_layout.md).
