# Arkeyan Launcher's Collection page

> **Purpose:** what does Arkeyan Launcher's Collection page show and do, and where is the shared library it uses documented? · **Status:** current (2026-10-26) · **Tags:** none

The page's own detail: Arkeyan Launcher's `docs/collection_ui.md` and `figures_overlap.md`. Everything else is the runtime's,
shared with the launcher:

| Subject | Page |
|---|---|
| The collection folder, `collection.json`, locks, migration, the catalogue, icons | [collection.md](../../runtime/host/collection.md) |
| The library's API for a UI (namespace `collection`) | [collection_api.md](../../runtime/host/collection_api.md) |
| Figure data, the tag, the in-game Toy Box (controls, wheel, rendering, scripts) | [figures.md](../../runtime/host/figures.md) |
| The Toy Box and figure management UX design (seats, sections, the wheel) | [toybox_ux.md](../../runtime/host/toybox_ux.md) |
| Each game's pictures from the disc (portraits, logos) | [game_art.md](../../runtime/host/game_art.md) |

## The shared library in Arkeyan Launcher
`src/shared/collection/`, `src/shared/figures/`, `src/shared/art/`, `resources/collection/catalogue.json` and
each game's art manifest (a copy of `games/<id>/art.json`, under Arkeyan Launcher's resources folder) are copies of this repo's `runtime/host/` library, made by `tools/sync_skylauncher.sh`
(the file list: [README.md](README.md), "What passes between the two repos"). **Change them in this repo, where the
tests are** (`ArkeyanCollectionTest`, `ArkeyanFigureTest`, `ArkeyanArtTest`, `ArkeyanSavesTest`), then sync. The launcher must not write a
figure whose lock is held by a live game (`LockInfo::Busy()`): the library refuses it.

## The Collection page (the designer's look, the Toy Box's organisation)
Code: Arkeyan Launcher `src/launcher/ui/collection_*` (`collection_page`, `collection_model`, `collection_figure`, `collection_add`, `collection_manage`).

Design: [toybox_ux.md](../../runtime/host/toybox_ux.md) (one model for both apps); Arkeyan Launcher `docs/collection_ui.md`. Header COLLECTION (count, `+ Add a figure`, `...`: Trash, Backups, Import and export, Unknown toys, Tidy, the folder). **PORTAL AT THE NEXT START** with a selector of the games that can start with prepared seats (SSA, Giants, Swap Force): the seats in a row (seat, picture, name, level for that game; X to take off). **The bar** (2026-10-27): the search field first (the smart search: names, elements, games, toy types, versions, "level 10", EN and FR, one typo tolerated), one **Filters** button (a small panel: **Element**, **Game**, **Type**, **Sort by** Recent / Name / Level / Element / Game, **Clear all**), and under it, when something is on, the tags with an x, Clear all and the count of figures found; the Toy Box has the same ([toybox_ux.md](../../runtime/host/toybox_ux.md) "Search, filters and sort"). **The shelf**: every figure once, favourites first (a star), then recent in the selected game, then by element; P1/P2/"On" on what is on the portal; "Not for <game>" greyed at the end. Click a seat (or LB/RB), then a figure: it goes there ("Choose a figure for Player 1 · only what Spyro's Adventure reads"); a click on a card otherwise opens its **sheet**: picture, name, "Level 10 (15)", gold and play time, last played, three actions (**Put on the portal**, **Edit** (name and stats, the selected game's limits; Giants' hat picker), **Favourite**), the rest behind `...` (duplicate, back up, restore a backup, show in the folder, reset and delete with a red **Hold** bar), the file and numbers behind **Details**. **A Swap Force swapper is one card** (two linked dumps: `collection.md`, "Swappers"; `Cards()`/`Group()`): one figure on the shelf, one click puts both halves on its seat (`portal.txt`: Swap Force's six slots, P1 0 and 1, P2 2 and 3, Magic item 4, Adventure 5; a non-swapper takes the first slot of its seat; `SeatFirstSlot`/`SlotSeat`; a bottom whose top is on the portal too is not a seat when read back), its sheet has "Change the bottom" / "Change the top" (`ChangeHalf`; a lone half offers it too), and Delete, Export and the lock test act on both halves (`Card::Halves()`, `Card::Busy()`). Swap Force's data folder is `SwapForce/` (`datadir::GameFolderName`). Swap Force can be prepared like SSA and Giants (`SessionFile` not empty). Seats = `portal.txt` in the game's data folder (`WriteSeats`/`ReadSeats`). A signature of the folder is compared every 1.5 s; while the game runs: "ON THE PORTAL NOW", seats read-only, "Playing now" on its figures.

**A swapper's picture** is its two halves stacked, as the game's swap screen (`collection::SwapperPicture`, shared with the Toy Box; [game_art.md](../../runtime/host/game_art.md)): on the shelf's cards, the seats, the sheet and Add a figure (`CollectionPage::DrawSwapper`, falling back to the top's picture with the bottom small when a half has no picture from the disc). The sheet names a mixed swapper as the game does (`CardName`/`SwapperName`: "Magna Jet"), writes "Top: X · Bottom: Y" ("Haut : X · Bas : Y"), and a figure never played shows "New: never played" with no "Last played" row. Test hooks: `SSA_LAUNCHER_TEST_SWAPPERS=1` (all 16 swappers and four mixed ones seeded, shots `53-` to `55-`), `SSA_LAUNCHER_TEST_LOGO=1` (each game's page, `logo-<game>`), `SSA_LAUNCHER_TEST_SIZE=<w>x<h>`. The logo is never drawn above its own pixels.

**Tests** (Arkeyan Launcher `tests/launcher_test.cpp`, run by `ArkeyanLauncherTest`): the swapper cards (a swapper once on the shelf whichever half a search or a filter matches; one seat and two lines of `portal.txt` for it, a bottom first in the file still one seat; Change the bottom; Delete and Undelete of both halves; a lone half deleted alone), Swap Force's game row, its disc and its pictures (`SKY_LAUNCHER_TEST_SWAPFORCE_ISO`, [updates_release.md](updates_release.md)). Screenshots `SKY_LAUNCHER_TEST_COLLECTION=1`: `42` to `45-swapforce-choose-player-1`, and `47-search-element` to `52-sort-name` (the search, the Filters panel, the tags, a sort). The search and the sort through `CollectionModel::Shelf` are checked too.

## Picture selection

The launcher and Toy Box use one shared selector: reverse catalogue order, newest cached game first, independent of
the viewed game. Dedicated variant art wins over newer plain art. Both icon sizes follow that order; large pictures
prefer artwork within the chosen game. Individual swapper halves use their own half image, then whole-figure art
(a bottom can use its matching top's entry), then a generated icon. Only two genuine half images are composited at
the waist, and plain halves never replace a dedicated variant picture. Existing caches need no re-extraction or version change. The full path audit and synthetic test coverage
are in [game_art.md](../../runtime/host/game_art.md#picture-lookup-audit).
