# SSA's pictures from the disc: logo, background, portraits

> **Purpose:** which pictures Arkeyan Launcher and the Toy Box read from a player's SSA disc, and how · **Status:** current (2026-10-26) · **Tags:** [Lux] [SSA]

## On the disc
| Picture | Where | Size, format |
|---|---|---|
| Logo | `uiimage.str`: `skylanderslogo` | 1024x512, DXT5 |
| Background | `uiimage.str`: `fullscreen_000` | 512x512, the painted Skylands sky |
| Portraits | `uigameimage.str`: 37 `toy_<name>_hud` (128 px), 37 `<name>_vs` (512 px), 4 magic items (64 px) | 78 files, DXT5 |
| Title icon | `default.xex`, the XDBF resource | PNG |

The containers are `.str` (`strm` + one zlib stream of records) holding `APKF` images:
[str_levels_loading.md](../../../docs/engines/lux/str_levels_loading.md).

## How we extract it
`runtime/host/art/` (shared with Arkeyan Launcher): the provider, the manifest and the rule for a toy's picture are in
[game_art.md](../../../docs/runtime/host/game_art.md); the portraits and their cache (`<data>/SSA/portraits/`,
`SKY_PORTRAITS`) in [figures.md](../../../docs/runtime/host/figures.md) ("Portraits"). Nothing extracted is shipped.

## Tests
`ArkeyanArtTest` (`runtime/host/art/art_test.cpp`, in `tools/test_all.sh`): the readers on synthetic fixtures, and with
`SSA_ISO` the whole extraction (logo, background, title icon, 78 portraits). In a game run:
`[toybox] 78 portraits in <data>/SSA/portraits` when the Toy Box first opens.

## Is it engine-generic?
The `strm` / `APKF` formats are **[Lux]**: a later Lux game would reuse the readers. Which archive and which names are
game data: one row of `runtime/host/art/game_art.cpp`'s table per game.
