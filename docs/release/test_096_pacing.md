# 0.9.6 frame pacing on a Windows PC at 144 Hz: the owner's checklist

> **Purpose:** the check the owner runs on his PC (1920x1080, 144 Hz) before 0.9.6 ships: the drop to 30 and VSync · **Status:** current (2026-11-11) · **Tags:** none

Why: a player (Ryzen 5 5500, RTX 3050, fullscreen, 60 FPS, scale x4) found VSync and "Drop to 30 FPS when needed" made the
game worse, and the owner's run dropped to 30 and back within seconds. What changed: [video_vblank.md](../runtime/xbox360/video_vblank.md)
(the drop), [plume_backends.md](../runtime/gpu/plume_backends.md) (`plume_present_latest.patch`). This Mac has no 144 Hz
display and no swap chain in its offscreen runs: these steps are the only proof on a real display.

## Setup
A 0.9.6 kit and launcher, as for [test_095_windows.md](test_095_windows.md). Trap Team or Swap Force at 60 FPS, scale x2
or more, fullscreen. Keep the default `fps_auto_fallback` (do not touch the toggle before step 1).

## The checklist (about 20 minutes)
1. **The default**: start the game once, quit. The game's log has `[video] drop to 30 FPS when needed: off by default
   (discrete GPU)`; the launcher's Video tab shows the toggle off.
2. **VSync's mode**: VSync on, Direct3D 12. The log has `[gpu] vsync on: display 144.00 Hz, 60 FPS, not a multiple: the
   newest frame at each refresh`. Play 2 minutes in a level: the motion should look no worse than VSync off, without
   tearing. Every 10 s a line `[perf] display 144 Hz, vsync the newest frame at each refresh: ... frames shown, for 1
   refresh ..%, 2: ..%, 3: ..%`: expected about 60 % for 2 and 40 % for 3 (2.4 refreshes a frame), almost nothing at 1
   or 4. Send three of these lines.
3. **The same with Vulkan** (Graphics API: Vulkan): the `[gpu] vsync on` line says the same (or `FIFO` if the driver has no
   mailbox mode); Vulkan prints no `[perf] display` line.
4. **120 Hz**: set the display to 120 Hz in Windows, start again: `[gpu] vsync on: display 120.00 Hz, 60 FPS: FIFO`; the
   `[perf] display` lines show about 100 % for 2 refreshes. Compare the motion with step 2. Set 144 Hz back.
5. **The drop to 30, on**: turn the toggle on, scale x4, the busiest level you know. If the game cannot hold 60, the log
   says `drop to 30 FPS until they fit again (held 30 s at least)` once, then at most every 30 s or more, never within a
   few seconds; with a lighter scene, `back to 60 FPS` only after a while. Before 0.9.6 it switched every few seconds.
   Send the `[video]` lines.
6. **A recording** (optional): the F3 menu's performance log during step 2, then the same with VSync off: the two CSVs go
   to `private/win/` beside the 0.9.5 ones.

## What to send back
The log lines of steps 1 to 5 (the launcher's Support page, *Copy* on the game log), and a sentence on the motion at
144 Hz with VSync on, VSync off and 120 Hz.
