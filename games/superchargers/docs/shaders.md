# SuperChargers' shaders: the disc and the cache

> **Purpose:** where SuperChargers' shaders are on the disc, their format, and how its shader cache is built · **Status:** current (2026-11-04) · **Tags:** [Xbox] [Alchemy] [SuperChargers] [V]

## On the disc
All of them in one archive, `/archives/shaders_xenon.pak` (681983 bytes), as Swap Force's, but in new formats [V]:
- **IGA v11, big-endian**: the magic reads `\x1aAGI` (as Swap Force's v10), version 11 (as Trap Team's, which is little-endian), header 0x38,
  u32 name hashes, then 16-byte entries `{ ?, offset, size, mode }` (Swap Force's order), the name table's position as a 64-bit value **high
  word first** at 0x28 and its size at 0x30, LZMA chunks `[u16 n, little-endian][props 5D 00 80 00 00][n bytes]` every 0x800 (the header's
  word at 0x10 is 0x800). 105 entries, `Temporary/BuildServer/xenon/Output/shaders/<Name>.igz`.
- **IGZ v9** in each entry: Swap Force's and Trap Team's section table at 0x18 (`{ offset, size }` every 16 bytes) and tagged records in
  section 0 (`TMET`, `ZSTM`, `NHMT`, `BTVR`, `RTSR`, `SFOR`, `DIPR`, `NHMR`, `TOOR`, `MANO`). The v7 method finds the containers: each D3D
  header (`10 2A 11 00/01`) takes the next physical block of its size from `NHMT`.

Result [V]: **1207 distinct containers, 345 vertex and 862 pixel**, all byte for byte among the 1215 a run's `SKY_DUMP_SHADERS` gave (the 8
others are built at run time: Bink's and the game's own).

## How we extract it
1. Disc containers: Arkeyan-Builder's `tools/arkeyan-builder/igz_shaders.cpp`, through the runtime's archive reader
   (`runtime/host/art/alchemy_archive.cpp`), with `shaders=igz:/archives/shaders_xenon.pak` in `games/superchargers/game.txt`.
   The fixtures: `ArkeyanArtTest` (every IGA layout, both byte orders of v11) and `ArkeyanBuilderTest` (IGZ v6 to v9); on the real archive,
   `TRAPTEAM_ARC=<shaders_xenon.pak> ArkeyanArtTest` opens it as a verified v11 and unpacks its 105 entries.
2. Run-time containers: a run with `SKY_DUMP_SHADERS=<dir>`, the intro movies played (the Bink shaders come with the first logo).
   A player's install does the same in its `movies` step: `movie_shaders=8` in `game.txt` (the game's own six at start, Bink's two with the
   first logo, 2.6 s or more later) makes it wait for all eight; without the target a slow start missed Bink's and every movie was black
   (2026-11-06, [render_bugs.md](../../../docs/runtime/gpu/render_bugs.md)) [V].
3. `SKY_GAME=superchargers tools/shaders/build_cache.sh <disc> <folder>` with the disc's and the dump's containers merged by content;
   configure again and relink. Day 1's cache (dumps only): 1214 of 1215 translate; `203D384226E0FD37.ps` is rejected by the Metal
   compiler [V].
