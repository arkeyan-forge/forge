# Wii runtime: what a Wii game needs from the host

> **Purpose:** what does the forge already have for a Wii game (SuperChargers Racing), and where is each piece? · **Status:** current (2026-11-23) · **Tags:** none

## What it is
The seventh game, Skylanders SuperChargers Racing, is a Wii game (issue #82). A Wii game links its SDK statically, so
there is no import table: the console is answered by hooks on the SDK's functions, found by symbol in the game's
executable. This area grows one piece per pull request, in the order of #82: the disc, the paired-single
instructions, the game's executable, the operating system, the GX renderer, the audio, the input and the portal.

| Piece | State |
|---|---|
| Disc: RVZ images, partitions, file system, hash check | done: [disc_rvz.md](disc_rvz.md) |
| Everything else | not started (#82) |

## Where the code is
[`runtime/disc/`](../../../runtime/disc/): `rvz.{h,cpp}` (the image), `wii_disc.{h,cpp}` (partitions, files, hashes),
`sha1.{h,cpp}`. [`vfs.cpp`](../../../runtime/vfs.cpp) opens an RVZ like any other disc.

## How to test it
`ArkeyanRvzTest` (in `ctest` and `tools/test_all.sh`) builds synthetic images; `ArkeyanRvzTest --disc <image.rvz>`
lists a real disc's files and checks its hashes.

## Gotchas
- Only RVZ images, compressed with Zstandard (Dolphin's default) or not at all: no ISO or WBFS, which would need the
  console's key, and no bzip2 or LZMA.

## Pages
- [disc_rvz.md](disc_rvz.md): the RVZ image, the Wii disc and its hash tree.
