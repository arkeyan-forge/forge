# XAM content and saves

> **Purpose:** how do XAM content packages, the device selector and saves work? · **Status:** current (2026-10-26) · **Tags:** [Xbox] [Lux] [SSA] [SwapForce]

Code: [`kernel/xam_content.cpp`](../../../runtime/kernel/xam_content.cpp), [`kernel/files.cpp`](../../../runtime/kernel/files.cpp)
(mounts), [`kernel/overlapped.cpp`](../../../runtime/kernel/overlapped.cpp). Class: the XAM calls are **[Xbox]**; the
names and files the title chooses are **[Lux]** / **[SSA]**.

## The model

The console has one **hard drive** (device id 1, type 1) backed by a host folder. A **content package** (a save game) is a
sub-directory `<save root>/<type %08X>/<XCONTENT_DATA file name>/` which `XamContentCreateEx` **mounts under a drive name** the
title chooses (`lux` for the Lux games) for `NtCreateFile` to read and write (`lux:\GLOBAL.DAT`).

| Save root | Value |
|---|---|
| `SKY_SAVE_DIR` if set | `<dir>` itself (**tests: always a new empty folder**, the menu presses assume no save) |
| else (0.3.1 layout) | `<Skylanders data root>/SSA/saves` (`datadir::Game("ssa") / "saves"`; macOS `~/Library/Application Support/Skylanders/SSA/saves`, Windows `%APPDATA%\Skylanders\SSA\saves`; UWP `LocalState`). Before 0.3.1: `SDL_GetPrefPath("SSARecomp","SSARecomp") + "saves"`; the first start moves it ([../platforms/data_folders.md](../../getting-started/platforms/data_folders.md)) |

The drive is "present" while its folder exists or can be created; without it, `XamShowDeviceSelectorUI` answers
`ERROR_CANCELLED` (1223).

## Calls

- **`XamShowDeviceSelectorUI(user, content type, flags, bytes, device id out, overlapped)`**: like the console when a single
  device can hold the content and `XCONTENTFLAG_FORCE_SHOW_UI` is not set, it **picks the hard drive without any UI**
  (`*deviceId = 1`) and completes the overlapped. The game polls the overlapped through `XGetOverlappedResult`
  (`InternalLow`, `0x3E5` = pending): **publish `InternalLow` last** ([threads_sync.md](threads_sync.md#xoverlapped-completion-kerneloverlappedcpp)).
- **`XamContentCreateEx(user, root name, XCONTENT_DATA*, flags, disposition out, license mask out, cache size, content size, overlapped)`**
  - the **overlapped is the 9th argument, at `r1+0x54`**;
  - `XCONTENT_DATA` (0x134 bytes): `+0x00` device id, `+0x04` content type, `+0x08` display name `WCHAR[128]`, `+0x108` file name
    `char[42]`, 2 bytes padding;
  - creation flags **low nibble = CreateFile dispositions**: 1 CREATE_NEW, 2 CREATE_ALWAYS, 3 OPEN_EXISTING, 4 OPEN_ALWAYS,
    5 TRUNCATE_EXISTING; create/truncate-always replace an existing package with an empty one (`remove_all`);
  - results: `ERROR_PATH_NOT_FOUND` (3) for OPEN_EXISTING of a missing package, `ERROR_ALREADY_EXISTS` (183) when the root name is
    already mounted (the console's error for this is unverified), `ERROR_DEVICE_NOT_CONNECTED` (1167) for another device id,
    `ERROR_INVALID_NAME` (123); disposition out 1 = created, 2 = opened; license mask 0;
  - logs `[content] create lux: "<file name>" type 0x... flags ... -> result (disposition n)`. Content "errors" in logs are
    **probes** (D38): `flags 0x13 -> 3` is the game checking whether a slot exists, then it creates it (`0x12`, disposition 1) and
    reopens (`0x13`, disposition 2).
- `XamContentClose(root name, overlapped)`: unmounts (`ERROR_NOT_FOUND` 1168 if it was not mounted).
- `XamContentGetDeviceState(device, overlapped)`; `XamContentGetDeviceData(device, XDEVICE_DATA*)`: `+0x00` id, `+0x04` type, `+0x08`
  total bytes, `+0x10` free bytes (`fs::space`), `+0x18` `WCHAR[28]` "Hard Drive".
- `XamContentCreateEnumerator(user, device, content type, flags, items per enumerate, buffer size out, handle out)`
  **[SwapForce]**: an `XamEnumerator` (`kernel/xam_enumerator.h`) of one `XCONTENT_DATA` per `<save dir>\<type>\<name>`
  folder, sorted by name, display name = file name (the host keeps no other); device 0 means any device; users 1-3 give
  `ERROR_NO_SUCH_USER`; buffer size = items per enumerate x 0x134. `XamEnumerate` hands them out, then
  `ERROR_NO_MORE_FILES`. Swap Force lists its saved games (type 1) this way, 4 at a time, before it opens one: an empty
  list would hide existing saves. Logs `[content] enumerate type 0x1 on device 1: n package(s)`.
- Save user index plays no part (one profile).

## What the Lux title saves [Lux] [SSA]

Names built with **`vsprintf`** (the only caller; it was a stub, so names were empty): `Skylanders - Global Data`, `Skylanders - Slot 1..4`.
Files: `GLOBAL.DAT` (196 bytes), `SLOT.DAT`, `SLOT.NFO`. Saves are stamped with **local time**: XAPI's `GetTimeZoneInformation`
(`sub_822E4A18` [SSA]) reads XConfig 3/1-3/7 and 3/12, `FileTimeToSystemTime` calls `RtlTimeToTimeFields`.
The save manager singleton and its state machine are in [lux/boot_saves.md](../../engines/lux/boot_saves.md).

## Gotchas

1. The overlapped completion must exist for **every** XAM call that takes one, even synchronous ones.
2. `XamContentCreateEx` runs on the title's thread; the APC for the completion routine is queued on **that** thread.
3. Two games sharing one save folder collide: the multi-game plan wants a per-title-ID folder (architecture step 5).
4. Saves are flushed on every `NtWriteFile` (`stream.flush()`), so `std::_Exit` at quit loses nothing.
