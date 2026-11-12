# The title's Portal of Power driver (what the game expects)

> **Purpose:** what SSA's portal driver sends and expects · **Status:** current (2026-10-26) · **Tags:** [Lux] [SSA]

Our side: `runtime/kernel/portal.cpp`, `runtime/host/portal*.cpp` (backends: [portal.md](../../runtime/host/portal.md); crypto and tag format: [figures.md](../../runtime/host/figures.md)). Class **[Lux]** protocol (shared by the later games with additions), **[SSA]** addresses.

- **Lookup**: `sub_82615A68` calls `XexGetProcedureAddress` for **xam ordinals `0x4A1` / `0x4A2`** (`XamInputNonControllerGetRaw` / `SetRaw`) and stores the pointers at `0x82B7FA68` / `0x82B7FA6C`; it **retries on every poll while missing**.
- **Portal thread** `sub_8264ABB8`: every **12 ms** calls `sub_826488C0`: one `GetRaw`, then **at most one** `SetRaw`. Driver object `0x82C4EF4C`, vtable `0x820BA298`: [2] input handler `0x82648178`, [3] command builder `0x82647AB0`; state at `+28`.
  The title's own portal log is `sub_826434D8` (printf-like; `SKY_PORTAL_LOG=1` echoes it): `[Portal] Decrypted:` lines.
- **`GetRaw(DWORD* state, DWORD* len /*in: 32*/, BYTE* buf)`**: return 0 = success, non-zero = "disconnected" (resets the protocol state machine); `*state` non-zero = new data; the title **skips `buf[0..1]`** (the 360 header) and parses `buf+2`, `len-2`.
  **`SetRaw(DWORD len, BYTE* buf)`**: `buf = 0B 14 <cmd...>`. Our shim: `ErrorInvalidParameter (0x57)` when state/buffer is null or `capacity <= 2` or `> 32`; `ErrorDeviceNotConnected (0x48F)` when no portal.
- **Commands** (letter first): `R 00` reset/version; `A 00` / `A 01` activate (deactivate/activate); `S` status; `Q (n<<4|slot) block` query (read a 16-byte block); `W (1<<4|slot) block data16` write; `C r g b` light colour (no answer); `FA` (360 dongle).
  **Later games add `J`, `L`, `M`, `V` (lights, speaker) [?]** (Giants, Alchemy, sends only `C r g b`: [status.md](../../../games/giants/docs/status.md)): log with `SKY_PORTAL_LOG=1` before assuming; the emulated portal ignores unknown commands (`[portal] emulated portal ignores command 0x..`).
- **Answers expected**:
  - `R`: version `(b1<<8 | b2)` **>= `0x011F`** or the portal is "out of date" (ours: `02 1B`);
  - `A`: `act batt 77` (battery <= 164 = "too low"; ours `0xFF`, trailer `0x77`);
  - `S`: `<LE32: 2 bits per slot (bit 0 present, bit 1 changed since the last report)> counter active` (16 slots; counter byte increments; active byte 1/0). **Any `S` with active = 0 resets the protocol, so an inactive portal must stay silent** (reads return nothing until `A 01`);
  - `Q (1<<4|slot) block data16`, or `Q 01 ...` on failure; `W (1<<4|slot) block` echo.
- **Block layout the title reads on placement**: blocks 0-1, both progress areas (8-16, 36-44) and the trailers: **32 of 64 blocks, mask `88889FF88889FF8B`** (used by the Toy Box's real-portal view). The title reads a newly placed figure within **0.1 s**, also in cutscenes and in play.
- **Accepted toys**: `sub_82643DA0` accepts toy types 0-32 and the legendaries 404, 416, 419, 430; `sub_82643BE8` accepts the magic items 200-207, the adventure packs 300-304 and **505, 514, 519, 526 (Giants-era sidekicks known to SSA's code)**. Character internal names from `sub_829BC850` (AirDragon = Whirlwind ... Cynder; `DarkSpyro` = 28).
  Variants (block 1 `0x1C`): no check found [?].
- **Figure key derivation** `sub_82643958` (constant at `0x8264398C`): `MD5(block0 || block1 || block number || " Copyright (C) 2010 Activision. All Rights Reserved. ")`, AES-128-ECB ([figures.md](../../runtime/host/figures.md)).
- **Hardware**: Xbox 360 portal USB `1430:1F17` (also `24C6:FA00` in Xenia's list), interrupt endpoints IN `0x81` / OUT `0x02`, **2-byte header** on commands; Wii / Wii U / PS3 / PS4 / PC portals: HID `1430:0150` (every HID model, SSA to Imaginators), commands by SET_REPORT, **no header**; Xbox One portal: GIP
  (Santroller capture: VID 1430 PID `09AB`, custom message `0x21`, length 30; device type `Activision.Xbox.Skylanders.Portal`, interface `{1F7788A2-B860-9B40-3626-EC56C7C45256}`) [?] (untested).
- Gotcha: a `WideLoad` wait with the portal lock held hung the main thread once ([str_levels_loading.md](str_levels_loading.md)); a real portal's blocking HID write on the title's thread froze the game ~1 s every ~2 s on Windows: **real portal I/O now runs on its own thread** (`AsyncPortal`, retries backing off to 10 s).
