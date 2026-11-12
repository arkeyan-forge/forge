# Day-1 tool

> **Purpose:** what does `tools/day1.sh` do and how is it run? · **Status:** current (2026-10-26) · **Tags:** [Xbox] [Lux]

Every other tool of this repo (recompilers, sigmatch, shader tools, builds, kit) is in the table of
[docs/tools/README.md](README.md). Isolation variables for any game run:
[testing.md](../getting-started/testing.md).

## day1.sh
`tools/day1.sh <game> <iso>` (run from anywhere): steps 1-2 of [docs/porting/day1.md](../porting/day1.md).
1. Extracts `default.xex` (`tools/shaders/xdvdfs.py`), runs `xexdump` (sections, imports, `image.bin`), hashes the XEX
   (`default.xex.sha256`).
2. Makes SSA's signatures from SSA's image (`private/image.bin`) with `tools/sigmatch/sigmatch.py make`.
3. Runs `sigmatch match` against the new disc (`sigmap.json`, `sigmatch.txt`) and lists the entries below confidence 1.0.

- Writes **only** into `private/<game>/day1/` (git-ignored, next to CMake's `private/<game>/ppc/`).
- Uses this repo's tools; `ARKEYAN_WORKSHOP=<checkout>` takes them from another checkout.
- Needs `private/build-host-tools` (xexdump) and `private/image.bin` (SSA's image): `tools/prepare_game.sh` on the SSA
  disc, once.
- Last run on the SSA disc as `ssa` (2026-10-24, before the move into this repo): 293/293 found, 291 at 1.0, XEX hash =
  the known one.
