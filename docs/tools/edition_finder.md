# Edition finder

> **Purpose:** what a player's disc of an edition we don't have is, whether it runs a supported release's code, and where its functions are · **Status:** current (2026-11-10, 0.9.5) · **Tags:** [Xbox] [Lux] [Alchemy]

The owner has PAL discs only; players have others (Trap Team US, media ID 44D8BCCF, Imaginators US, 2C277630: public
issue #21). Every check used to accept one exact `default.xex` (its SHA-256) and refused the rest as `WrongVersion`.

## 1. The report: `Arkeyan-Builder --edition-report <disc> [--kit <kit>]`
Plain text on stdout, ready to paste in a "New disc edition" issue (`tools/arkeyan-builder/edition.{h,cpp}`,
`EditionReportCommand` in `main.cpp`): the kit's version, the game (by title ID, from the kit's `game.txt`), title ID,
media ID, version, region (security info `0x178`), the languages its files hold (`art::LanguagesOnDisc`), `default.xex`'s
size and SHA-256, the **loaded image**'s size, base and SHA-256 (decrypted, decompressed basefile), then per supported
release "this very disc", "same game code" or "different game code", and a `Result:` line. Ids, sizes and hashes only: no
byte of the game, no path, no name. Works without a kit (then "not in this kit"). The launcher's **Find my edition** runs
it (Arkeyan Launcher `docs/launcher_ui.md`).

## 2. Header-only editions are accepted everywhere
Swap Force's USA/Australia and Europe discs, Trap Team's two European ones, have the same loaded image byte for byte; only
the XEX header differs. So each release now also carries its image's SHA-256, and a disc whose `default.xex` is unknown but
whose loaded image is a supported release's is that release (its label from its own region in the builder's log, its
languages from its own files):

| Where | What |
|---|---|
| `games/<id>/info.cpp` | `GameRelease::imageSha256` (`runtime/host/launcher.h`); the runtime's `CheckGameSource` compares it after the xex hash |
| `games/<id>/game.txt` | `image_sha256=` (same order as `xex_sha256=`), `releases=` (labels, `;`), `title_id=`; `tools/make_kit.sh` checks them against `info.cpp` |
| Arkeyan-Builder | `IdentifyGame`: xex hash, else title ID and image hash (`FindGameByImage`, `game_config.h`) |
| Arkeyan Launcher | `src/launcher/launcher.cpp` and `launcher_games.cpp`, the same field and order |

The loader of XenonUtils trusts the XEX header: `XexImageReadable` (`runtime/host/xex_title.h`, the launcher's copy in
`launcher.cpp`) checks the header, security and file format infos, the first LZX block and the import table are in bounds
before an unverified file is decrypted (the next blocks are checked by their hash chain inside the loader). The image's
hashes, from our six discs: SSA `f53c42d4…`, Giants `43a64a6d…`, Swap Force `f6648b7e…`, Trap Team `265c807d…`,
SuperChargers `f2c26c01…`, Imaginators `4f793c38…` (in each `info.cpp`).

## 3. Other code: where its functions are
`tools/arkeyan-builder/fingerprint.{h,cpp}`. At kit time `tools/make_kit.sh` runs `ArkeyanEditionPrints <disc>
private/libs/<id>/libSSARecompLib.a games/<id>/addresses.h private/editions/<id>.txt` (the library's `sub_XXXXXXXX` names
are every function's start) and ships the file as `config/<id>/fingerprints.txt`. One line per `GAME_HOOK_` and per
constexpr that is an image address: hashes only (FNV-1a), never instruction bytes:
- the function's words, trailing zero padding dropped, with what a link changes masked: `b`/`bl` targets, `lis`
  immediates, and the low half of every addi, ori, D-form load or store whose base a `lis` of the function set;
- the hash of its first four words with every such low half masked (candidates fast);
- each callee's length and hash in call order (the call graph tells same-code functions apart);
- a code address inside a function (a call site, a return address): that function, if its hash is unique, and the offset;
- a data address: the first function of a unique hash that builds it with a `lis` pair, and the pair's instruction indexes.

Coverage of our own discs (each found again at its place): SSA 46 of 48 (`LanguageIndex` has no unique referencing
function; `LightTile_ClassifyJob` has a twin, so it is "ambiguous" anywhere), Giants 33/33, Swap Force 40/40, Trap Team
33/33, SuperChargers 39/39, Imaginators 38/38 [V].

`--edition-report` on a disc of other code searches every fingerprint (`fingerprint::Find`) and adds "Addresses on this
disc: N of M found", one line per address (ours, theirs, confidence: `sure` one match, `call graph` the one candidate whose
callees all match, `likely` the best of several), then "Not found". ~1 s per game.

`python3 -I tools/editions/from_report.py <report> [--write]` turns a pasted report into the release entry (both
`info.cpp` files, the `game.txt` lists) and, with addresses, `games/<id>/editions/<media ID>/addresses.h`: the game's
header with that disc's addresses, `// check: likely` on the weaker ones, the missing ones commented out. A build for
that edition: `-DARKEYAN_EDITION=<media ID>` (`runtime/CMakeLists.txt` puts that folder before the game's).

**Next step [?]:** the kit ships one prebuilt runtime per game, built with its `addresses.h`; a disc of other code needs
its own (built with `ARKEYAN_EDITION`), picked by Arkeyan-Builder from the disc's media ID, and its XenonRecomp config's
addresses (function bounds, switch tables, mid-asm hooks in `games/<id>/config/*.toml`) re-found the same way. Until
then such a disc stays refused; its report says what we need.

## How to verify
- `build/tools/arkeyan-builder/ArkeyanEditionTest . <scratch>` (in `tools/test_all.sh`; discs optional): the report's
  text; every `game.txt` against its `info.cpp`; Swap Force's disc, then its executable under another media ID, version
  and region (accepted, same image), with one data byte changed (refused), Trap Team's under a Swap Force header (refused);
  fingerprints: all 40 of Swap Force found in place, all found in a shifted copy (64 words inserted, branches fixed: 24
  moved, 16 kept) [V]; Giants' found in Swap Force's code: 13, all equal to Swap Force's `addresses.h` [V]; Swap Force's
  in Trap Team's code: 15, all equal to Trap Team's [V]. Not found is the honest answer for functions that changed.
- `ArkeyanRuntimeLauncherTest <SSA disc>`: the SSA executable under another header is `Ok`, with one data byte changed
  `WrongVersion`. Arkeyan Launcher's `ArkeyanLauncherTest <disc>`: the same, and **Find my edition** against the fake builder.
- `python3 -I tools/editions/from_report.py --self-test`.
