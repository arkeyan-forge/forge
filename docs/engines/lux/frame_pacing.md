# Frame pacing, the variable step, and the 60 fps findings

> **Purpose:** how the game measures time, and what 30 and 60 fps need · **Status:** current (2026-10-28) · **Tags:** [Lux] [SSA]

Detail: [engine_notes.md](engine_notes.md) §3.4 and §4.11. Class **[Lux]** (expected in other Lux games: verify the clamp), **[SSA]** addresses.

## How the game measures time [V]

- **Step factor F** (`sub_827F6628`, called each frame from `sub_827F6850`): reads **`mftb`**, takes the time since the previous frame in 60ths of a second (`ticks / timebase frequency (0x82BA1F30) * 60`) and smooths it:
  **`F += 0.3 * (dt*60 - F)`, clamped to `]0, 3]`**. Stored in **`0x82B62CA4`** (starts at 1.0) and copied to `clock+196`, where `clock` = **`0x82C7CD20`** (getter `sub_82856AA8`).
- **Game delta** = `F * [clock+188] * [clock+184]` (`sub_821491F8`; `clock+148` non-zero = paused), so a frame steps `F/60 s`: **F = 2 at 30 fps, 1 at 60 fps**.
- **Clamps**: maximum F = 3 = **1/20 s per frame** (below 20 fps the game slows down: measured 15 fps -> 0.754 s of game time per second = 15 x 0.05). No minimum besides 0, so **60 fps needs no game patch**.
- **Shader and scene time count vblanks, not frames**: the vblank callback `sub_82631968` increments `0x82BB4FD8`; `sub_82631A20` snapshots it in `0x82BB4FCC` each frame; `sub_82630B78` writes `scene+1016 = vblanks * 16.67 (or 20) * 0.001` seconds, which `sub_8285B738`
  splats into shader constants. Independent of the frame rate, but it would **speed up if the emulated vblank ran faster than 60 Hz**: keep it at 60 Hz.
- Wall-clock timeouts (menus, attract: `sub_828024B8`, `sub_82802890`, `sub_828034C8`, `sub_827DCB70`) use `mftb` directly: frame-rate independent.
- No "1/120 target frame time" found; the 1/30 constant `0x820A602C` is used by LOD/streaming tables (`sub_8263FD90`), not the step. ~26 functions load the 1/60 constant `0x820A5F84` and were **not audited one by one**.
- NGL frame statistics (`0x82BA8258`: frame time `+0x5C` from D3D's swap timing `dev+21856 * dev+21872`, fps `+72`) are only read by the debug overlay.

## How we pace [Xbox]

`D3DDevice_Swap` -> `WaitForFlip(device+0x3630)` ([../xbox360/video_vblank.md](../../runtime/xbox360/video_vblank.md)). The title asks for interval 2 (30 fps). **`fps_limit=60` shows the interval-2 frames at every vblank** (`vblanks == 2 -> 1`): the game keeps its speed because F follows the measured time. While the frames miss their blank, 60 falls back to 30 (no judder; [video_vblank.md](../../runtime/xbox360/video_vblank.md#pacing-waitforflippresentinterval-in-the-swap-hook)).
Measurements (shared M1 Pro, load 20-26): title/menus 59.7-60.0 fps at 60 (29.8-30.1 at 30), F 1.00-1.01 at 60 vs 1.95-2.08 at 30; game seconds per wall second 1.000-1.001; Hugo's intro 40.82 s vs 40.84 s; movies 4.96/10.43/8.61/10.52 s vs 4.93/10.38/8.53/10.46 s.
Heap/data clock scan: at 30 and 60 fps 39 of 42 growing values identical. **Gameplay at a real 60 fps was not observed** (level_027 rendered 20-31 fps on the test machine): fixed per-frame steps may exist.
`fps_limit` values: 30, 60 (0.9.4; before, also 120, 144, 240, 0 = no limit, now read as 60); above 60 the host paced the frames ([video_vblank.md](../../runtime/xbox360/video_vblank.md), "Above 60"): F is 0.5 at 120, there is no minimum.
A player's `launcher.ini` with `fps_limit=60` once skewed tests: **set `SKY_SETTINGS`** in every test.

## Things that can mislead

- Late frames: F is smoothed (0.3 per frame), so after a sudden drop the game runs slightly slow for a few frames, as at 30.
- The audio bank-load throttle runs twice as often at 60 (harmless). Movies present each frame twice.
- Without frame pacing the title ran at 120-500 fps (fences retire instantly) and movies at 6-7 fps (D26).
