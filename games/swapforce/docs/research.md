# Swap Force: public research

> **Purpose:** what public sources say about Swap Force, and how much of it the disc confirmed · **Status:** current (2026-10-26) · **Tags:** [Xbox] [XDK] [Alchemy] [SwapForce]

## Sources
The public research made before the disc arrived, with its sources: [public background](#public-background-2026-10-25) (2026-10-25).

## Facts
- Title ID `415608E9`: the public source gave it for the European disc; the USA/Australia disc has the same [V] (day 1).
- Engine Alchemy [V] (day 1: no Lux code, the Alchemy logo movie, IGA archives).
- Bink 1 and Bink 2 [V]: `.bik` logos (Activision, Alchemy) and 92 `.bk2` movies on the disc.

## Confidence
**[2]** two independent sources, **[1]** one source, **[inf]** inferred, **[V]** confirmed on the disc or in a run.

## Public background (2026-10-25)

These source notes predate the port. They preserve release, hardware and toy references;
engine, import and runtime findings above take precedence. Confidence: [2] two sources, [1] one, [inf] inferred.

### Release
- NA Oct 13 2013, AU Oct 16, EU Oct 18 on 3DS, PS3, Wii, Wii U, **Xbox 360**; PS4 Nov 2013; **Xbox One Nov 22 2013**. [1] ([W-SF])
- Developer: **Vicarious Visions and Beenox** (n-Space on 3DS), Activision. [1] ([W-SF])
- Xbox 360 version exists, full game (Metacritic 83). No reduced features found. [1]
- **Title ID `415608E9`** (European disc, English + German). [1] ([SFR]). Same `415608xx` family as SSA.
- Media ID, TUs, DLC, disc size: **not found**.

### Engine
- **Alchemy** (Vicarious Visions): the fan port's status log mentions Alchemy reflection fields and Xenon surface objects. [2]
  ([SFR-status], [W-SF]). **Not Lux**, like Giants (Alchemy, our boot 2026-10-25): the second Alchemy game for this repo, at newer format versions ([alchemy/README.md](../../../docs/engines/alchemy/README.md)).
- Middleware seen by the fan port: **Bink 1 and Bink 2** (`KB2f` movies), **XMA audio**, NVIDIA **FXAA 3.11** in the game,
  3D scene 1040x620, HUD 1280x720. [1] ([SFR-status])
- Bink 2 decoder jump table had 4 of 11 cases emitted by the recompiler: needs a switch-table config fix. [1] ([SFR-status])
- Changes from the previous game: swappers, 16 figures at once, Nightmare difficulty lock; Kinect not mentioned anywhere. [1]

### Portal hardware
- New "Swap Force portal": flatter, up to **16 figures** (earlier portals: 4); works with all games except Trap Team's
  speaker features. [2] ([Wiki-Portal], [Kneppers], [W-SF])
- Xbox 360 portal: VID 0x1430 PID 0x1F17 (wired, non-HID, interrupt endpoints). [1] ([Wilson-gist]). A fan port says Xbox 360
  portals "aren't HID devices and would need another backend". [1] ([SFR-status])
- Xbox One portal: GIP; VID:PID **not found**.
- Protocol additions: 16-entry status array (S command) instead of 4 [1] ([Kneppers]); nothing else known. [inf]
- Real USB portals (even an old PS3 portal) work in the fan port after portal library fixes. [1] ([SFR-status])

### Toys
- **Swappers**: top and bottom halves, each with its own tag, each read separately with its own index; 16 tops x 16 bottoms
  = 256 combinations; 8 movement types (Rocket, Spin, Dig, Stealth, Teleport, Bounce, Speed, Climb). [2] ([W-SF], [Kneppers])
- 16 new core, 16 new swappers, 16 reposed older toys. [1] ([W-SF])
- Tag: same MIFARE Classic 1K layout; variant ID high nibble = 2. [1] ([Kneppers]). Half-specific data layout: not documented
  in the sources found.
- Compatibility: Swap Force toys do not work in SSA/Giants, work from Swap Force on. [1] ([Activision-compat])

### Saves and online
- Online services: not found. Fan launcher plans co-op over the internet itself, not the game's. [1] ([COL])

### Port risks
- Xenia: no report found for `415608E9` in the compatibility trackers searched. [inf]
- Fan port progress: boot, save slots, opening movie, tutorial, Woodburrow, swappers work. Fixed: EDRAM overlap
  (black characters) via `execute_unclipped_draw_vs_on_cpu`; vertex buffer rewritten in place after a draw (green logo);
  Bink 2 jump table; save deleted too early after a crash. Open: native-res crash during save load, GPU device loss. [1] ([SFR-status])
- Real risks for our runtime: a different engine means new hook inventory, new render path (the Lux/NGL
  assumptions do not hold; start from Giants' Alchemy work), XMA audio instead of Wwise [inf].
- Stutter on first effect use comes from pipeline creation. [1]

### Sources
[W-SF] https://en.wikipedia.org/wiki/Skylanders:_Swap_Force · [SFR] https://github.com/tsapZ/SwapForceRecomp ·
[SFR-status] https://github.com/tsapZ/SwapForceRecomp/blob/main/docs/STATUS.md · [COL] https://github.com/tsapZ/SkylandersCoreOfLight ·
[Wiki-Portal] https://skylanderswiki.com/wiki/Portal_of_Power · [Wilson-gist] https://gist.github.com/skylandersNFC/5c9fb3debc11c1ea194ed6ddc9fb8faf ·
[Kneppers] https://marijnkneppers.dev/posts/reverse-engineering-skylanders-toys-to-life-mechanics/ ·
[Activision-compat] https://support.activision.com/skylanders-superchargers/articles/skylanders-superchargers-toy-compatibility
