# What was stubbed, and the stubs that bit us

> **Purpose:** which imports are stubbed, which stubs cost time, and which runtime guards exist? · **Status:** current (2026-10-26) · **Tags:** [Xbox] [SSA]

**Rule: a stub that "succeeds" without writing its outputs is worse than a missing function.** The weak fallback in
`kernel/stubs.cpp` makes every import return 0 and log `[stub] Name (lr, r3, r4)` **once**. After every new screen run
`grep '\[stub\]' log | sort -u`.

## Stubs that cost real time (all fixed by implementing the outputs) [Xbox] (unless marked)

| Symptom | Stub | What it must do | Where |
|---|---|---|---|
| Title music at -96 dB while SFX play (D9) | `XMsgInProcessCall` (XMP) + `XamGetSystemVersion` 0 | write controller 0 / locked 0 through the pointers; version 2.0.17559.0 | [xam.md](xam.md) |
| 22 s wait after START (STATE_GAME_FINALCHECKS, D25) | `XNetGetTitleXnAddr`, XConfig | return `XNET_GET_XNADDR_NONE`, write the 36-byte XNADDR; write XConfig outputs | [xam.md](xam.md) |
| Country from stack garbage | `ExGetXConfigSetting` | write value + required size, return a status | [xam.md](xam.md) |
| Crash reading guest `0x00000008` | `XamUserCreateAchievementEnumerator` | write a handle and the buffer size; `XamEnumerate` -> no more files | [xam.md](xam.md) |
| Empty save names | `vsprintf` | real Microsoft formatting | [xam_content_saves.md](xam_content_saves.md) |
| Wrong save time stamps / crash | `RtlTimeToTimeFields` | fill TIME_FIELDS | [kernel_exports.md](kernel_exports.md) |
| Device selector loops | overlapped `InternalLow` published early | publish last | [threads_sync.md](threads_sync.md) |
| Wwise banks never load | `ReadFileEx` completions | user APCs in alertable waits | [threads_sync.md](threads_sync.md) |
| XMA silent | `MmMapIoSpace`, XMA buffers | guest address of the physical allocation; contexts in the 64 KB window | [audio_xma_xaudio.md](audio_xma_xaudio.md) |
| Hang after a level load (holds the portal lock) | `NtWaitForMultipleObjectsEx` with 0 handles | **fail** with `STATUS_INVALID_PARAMETER_1`, never block | [threads_sync.md](threads_sync.md) |
| 120-500 fps, movies at 6 fps | fences retire instantly, no vblank | pace Swap on the emulated vblank | [video_vblank.md](video_vblank.md) |
| Movies stall | `KeTimeStampBundle` not ticking | tick every ms | [threads_sync.md](threads_sync.md) |
| Music "silent" after XMP fix? | Wwise `XamNotifyCreateListener` | waitable handle | [xam.md](xam.md) |

## Stubs that are fine for SSA (re-check per game)

- Video driver (`Vd*`, 24 of them): the game's D3D calls them during device creation; the hooked/null-GPU layer owns the result
  ([video_vblank.md](video_vblank.md)).
- UI: `XamShowSigninUI`, `FriendsUI`, `MessagesUI`, `KeyboardUI`, `MessageBoxUI(Ex)`, `DirtyDiscErrorUI`: never shown.
- Live: `XamSession*`, `XNetConnect`, `XNetRegisterKey`, `XNetServerToInAddr`, `NetDll_select`, ...: no network.
- Voice: `XamVoice*`. Timers: `NtCreateTimer/NtSetTimerEx/NtCancelTimer`. `NtQueryDirectoryFile`, `NtReadFileScatter`, `NtCancelIoFile`.
- Debug: `DbgPrint`, `DbgBreakPoint`, `KeBugCheck*`, `HalReturnToFirmware`: a `[stub]` line here is a **fatal path**.
- Other: `sprintf`/`_vsnprintf` (only D3D and the movie player link them), `RtlUnwind`/`RtlCaptureContext` (SEH is not used at run time),
  `XamLoaderLaunchTitle/TerminateTitle`, `XamAlloc/XamFree` (not called), `XamGetExecutionId`.

## Weak by design: unknown run-time imports

`XexGetProcedureAddress` answers `STATUS_ORDINAL_NOT_FOUND` and logs `ordinal 0x... not provided` for anything but the two portal
ordinals. A later game that looks up more ordinals at run time (lights, speaker, new XAM exports) will show it there.

## Other runtime guards that behave like stubs

- **`SsaBadIndirectCall`** (`indirect_call_guard.cpp`, forced into every generated (`private/<id>/ppc/`) unit through `-include`): an indirect call whose target is
  outside the code range, misaligned, or without a recompiled function is **logged once per (lr, target) and skipped with `r3 = 0`**:
  `[guard] bad indirect call to 0x... skipped: lr .. r3 .. (*r3 ..) r31 .. (*r31 ..) thread N "name"` (r3 is usually `this`, its first word
  the vtable). Before it, such a call read the table out of bounds and jumped to a garbage host pc (SIGBUS in `sub_829B71D0`, a billboard render callback).
- **`UnmappedFunction`**: "Indirect call to unrecompiled guest code (ctr=.. lr=..)" **aborts**: a missing function boundary (run
  `find_switch_tables.py` again and add the entry to `functions`).
- **Main heap free guard** (`patches.cpp`, `sub_828A25F8` [SSA]): a double free or a free next to a corrupt block is skipped and logged
  `[heap] free of 0x.. skipped (reason), lr ..` (max 32 lines), the block leaks ([gotchas.md](../../porting/gotchas.md)).
