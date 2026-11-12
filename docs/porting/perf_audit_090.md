# Performance audit after 0.9.0 (2026-11-06)

> **Purpose:** where the frame time, the install time and the launcher's start go after 0.9.0, and the gains worth doing · **Status:** current (2026-11-06), measured, no fix yet · **Tags:** [Alchemy] [SSA] [V]

Method: `tools/perf/game_perf.sh` (scale 2, limit 60, fallback off, offscreen, Metal, `SAMPLE=1`), one run alone; the Mac was loaded
by other work (conditions are pessimistic for Trap Team and SSA). Install: the 0.9.0 launcher and kit, isolated, step times from
`install.log`. Launcher: offscreen, scratch data, read-only build.

| Run | fps | p95 / p99 gap (ms) | Limit |
|---|---|---|---|
| Trap Team tutorial | 58.2 | 20.3 / 28.5 | GPU (2.0 ms waited per frame) |
| SSA `level_008` | 50.5 | 29.6 / 47.9 | render thread CPU (18.5 ms of work, no GPU wait; 3456 draws; the depth write-back is 13 % of its samples) |
| Imaginators level_508 / hub | 59.4 / 59.8 | 17.9 / 17.4 | guest code (13-14 ms of 16.7) |
| SuperChargers Academy | 58.9 | 17.2 / 27.1 | guest code (12.8 ms); hitches up to 93 ms |

Install (loaded Mac): SuperChargers 372 s, Imaginators 494 s; the `compile` step is 212 s (57 %) and 398 s (81 %), `shaders` 42-47 s,
the rest under 20 s. Launcher: first frame 0.92-0.99 s; disc art from an empty cache 3.0-3.25 s (614 files, ~6 CPU-s), cached 1.1 s; an
idle page costs ~9 % of a core (something keeps it animating). Not measured: MoltenVK (no Vulkan build of these games here), the
launcher's frame time while scrolling (no counter or scroll hook).

Gains, by impact over effort: (1) a shorter `compile` step (bigger translation units, lower optimisation on cold functions; -30 to -50 %
of the install [?]); (2) SSA `level_008`: do the depth write-back only when read, fewer state hashes (50 -> ~57-60 fps [?]);
(3) the launcher's idle wake-ups; (4) Trap Team's GPU time; (5) the SuperChargers hitches.
