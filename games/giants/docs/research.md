# Giants: public research

> **Purpose:** what public sources say about Giants, and how much of it the disc confirmed · **Status:** current (2026-10-26; research of 2026-10-25, checked against the disc) · **Tags:** [Giants] [Alchemy]

Public facts only; no game data. Our own findings on the disc outrank every line here: [boot.md](boot.md),
[status.md](status.md).

## Sources
[W-Giants] https://en.wikipedia.org/wiki/Skylanders:_Giants · [XC-1218] https://github.com/xenia-canary/game-compatibility/issues/1218 ·
[XP-1518] https://github.com/xenia-project/game-compatibility/issues/1518 · [GR] https://github.com/TheBiemGamer/GiantsRecomp ·
[TTR] https://github.com/tsapZ/TrapTeamRecomp · [COL] https://github.com/tsapZ/SkylandersCoreOfLight ·
[Wiki-Portal] https://skylanderswiki.com/wiki/Portal_of_Power · [Wilson-gist] https://gist.github.com/skylandersNFC/5c9fb3debc11c1ea194ed6ddc9fb8faf ·
[Kneppers] https://marijnkneppers.dev/posts/reverse-engineering-skylanders-toys-to-life-mechanics/ ·
[Activision-compat] https://support.activision.com/skylanders-superchargers/articles/skylanders-superchargers-toy-compatibility

The other ports and tools: [prior_art.md](../../../docs/porting/prior_art.md); cross-game tables:
[franchise.md](../../../docs/porting/franchise.md).

## Facts
### Release
- Oct 17 2012 (AU), Oct 19 (EU), Oct 21 (NA) on 3DS, PS3, Wii, **Xbox 360**; Wii U Nov 18/30 2012. [1] ([W-Giants])
- Developer: Toys for Bob (lead); Vicarious Visions did the Wii U port, n-Space the 3DS. [1] ([W-Giants])
- **Xbox 360 version exists** and is a full version (Metacritic 80). No reduced features found. No Xbox One version
  (the console came out a year later). [1]
- **Title ID `415608DA`** (Xenia Canary report, no media ID given). [1] ([XC-1218]).
  Same `415608xx` family as SSA (`415608AB`). The version "v1.0 (USA/EU)" is the only one a fan port accepts. [1] ([GR])
- Media ID, TUs, DLC, disc size: **not found** publicly (our disc: media `52FFE94B`, XGD3, 8.74 GB: [status.md](status.md)).

### Engine
- Wikipedia says "Vicarious Visions Alchemy" [1] ([W-Giants]). Before the disc we treated it as unreliable and
  expected Lux; **our boot proved Alchemy** (2026-10-25: no `.lsa`/`.str`/`.lxb`, IGA/IGZ archives, none of SSA's
  engine hooks match): [boot.md](boot.md), [docs/engines/alchemy/README.md](../../../docs/engines/alchemy/README.md).
- A fan port ([GR]) is a recompilation with the ReXGlue SDK (Xenia-derived); the same author's other ports are
  "scaffolded from GiantsRecomp, SSARecomp, SwapForceRecomp" [1] ([TTR]).
- Changes from SSA: new toy types (Giants, LightCore), improved Battle Mode with alter egos. [1] ([W-Giants])
  No Kinect use found. Split-screen/online: not found.

### Portal hardware
- Uses the original Portal of Power (4 figures at a time). The SSA and Giants portals are interchangeable. [1] ([Wiki-Portal])
- Xbox 360 portal: wired USB, **VID 0x1430, PID 0x1F17**, 0B 14 prefix before the command, 0x20-byte reports, interrupt
  endpoints 1 in / 2 out, contains an Infineon security chip. [1] ([Wilson-gist]); a Windows user ran Giants with an Xbox 360
  portal through Zadig/libusb [1] ([XC-1218]). Our `runtime/host/portal_usb.cpp` drives it ([portal.md](../../../docs/runtime/host/portal.md)).
- Wii-type portals: VID 0x1430 PID 0x0150 (HID). [2] ([Wilson-gist], [Kneppers])
- Protocol: no new commands are known for Giants over SSA [inf]; confirmed by our runs (only `C r g b`, see Confidence).
- Which portal works: any original portal, Swap Force portal, Traptanium portal for the platform's family. [1] ([Wiki-Portal])

### Toys
- New types: **Giants** (large figures), **LightCore** (glow on the portal, flash-bomb attack). Alter egos in Battle Mode. [1] ([W-Giants])
- Count: over 40 new toys, 16 new Skylanders incl. 8 Giants. [1] ([W-Giants])
- Tag: MIFARE Classic 1K (64 blocks, 16 sectors); Giants variant ID has game nibble 1 in block 1 offset 0x0C. [1] ([Kneppers])
  [V] 2026-10-26: Giants itself reads only the low 12 bits (repose, alternate deco, LightCore, deco id) and checks them
  against its toy definitions ([figures.md](../../../docs/runtime/host/figures.md#the-variant-id-what-each-game-reads-ssa-giants)).
  Data areas, checksum types and AES blocks as SSA's (our `figure_*` code), so the figure code should need little. [inf]
  [V] 2026-10-26: the upgrade field is SSA's, plus the Wow Pow (the extra area's +0x06 bit 0 [V], 2026-10-27,
  bought at Persephone);
  `Init_Setup.bld` sets up every save field, each character archive names its upgrades, `global.bld` prices them
  ([figures.md](../../../docs/runtime/host/figures.md#upgrades-ssa-giants)).
- Compatibility: a toy works in its release game and every later one; Giants accepts SSA toys (Series 1) and Giants toys,
  not later toys. [2] ([Activision-compat], [Wiki-Portal])

### Saves and online
- No online service needed for the story mode that we know of. [inf] Check for `XamUser`/`XNet` imports on the disc.

### Port risks
- Xenia Canary: **playable**, labels `state-playable`, `tech-skylanders-portal-required`; no known issues listed. [1] ([XC-1218])
  (SSA was in "menus" state, with XAM input problems, on Xenia in 2020 [1] ([XP-1518]).)
- Fan port runs Story mode on Windows (D3D12); blank screen fix `gpu_allow_invalid_fetch_constants`, Linux red tint,
  cutscenes heavy. [1] ([GR]) Those are Xenia-GPU issues, not ours.
- Unusual features: none found. Surround 5.1 audio is mentioned by the fan launcher. [1] ([COL])

## Confidence
**[2]** two independent sources, **[1]** one source, **[inf]** inferred. What the disc and our runs settled (2026-10-26):
- [V] Title ID `415608DA`, media ID `52FFE94B` (`games/giants/info.cpp`).
- [V] The engine is **Alchemy**, not Lux (the [inf] "Lux expected" was wrong; Wikipedia was right).
- [V] The portal protocol: Giants sends only `C r g b` (no `J L M V`); LightCore figures are ordinary figures that glow
  ([history.md](history.md), "Figures").
- [V] The tag format: a second pair of areas holds Giants' experience (level 15) and the hat
  ([figures.md](../../../docs/runtime/host/figures.md), "Giants' additions").
- [V] Giants accepts SSA toys and Giants toys (every toy type run to Chapter 1).
- [V] The disc's title lists 11 languages; it holds 12 text and voice languages: those 11 plus Latin-American Spanish
  (`HISPANIC`), every voice line in each ([boot.md](boot.md#voice-language)).
- Still [1]/[inf]: release dates, the 3DS and Wii U ports, Xenia's playable state, the fan port's issues.
