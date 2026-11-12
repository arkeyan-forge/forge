# Release: decisions, identity, hygiene and the checklist

> **Purpose:** what may be published and how, which identity and hygiene rules apply, and what is checked before a release? · **Status:** current (2026-11-12) · **Tags:** none

The release steps and the CI are in [launcher/updates_release.md](launcher/updates_release.md); the kit contract in
[launcher/kit_contract.md](launcher/kit_contract.md); how a kit is made in [../tools/arkeyan-builder.md](../tools/arkeyan-builder.md);
the licences in [licensing.md](licensing.md).

## Decisions
- v0.1 = public GitHub, **source and kit only**: the player supplies the ISO and builds on their machine;
  `private/<id>/ppc/`, the shader cache and game files are never distributed. Licence MIT. Publishing is a separate release step.
- Why local recompilation: it is the only option that keeps "nothing derived from the game leaves the player's
  machine"; prebuilt translated code would be a derivative of copyrighted code with real exposure (similar projects are
  tolerated; tolerance is not a licence). The launcher ships the open runtime, XenonRecomp, the per-game config and
  gets a compiler; the first build takes 5-45 min and happens once.
- The pipeline baseline (`games/<id>/pipelines/baseline.zst`: hashes and render states only) is the one disc-derived
  artifact the user allowed to ship.
- The leaked 2011 prototype PDBs are not used (user decision). No outside code: Xenia, Unleashed Recompiled,
  Dolphin, RPCS3 and every other project are read-only references; the Xenia-derived (BSD-3) parts already in `runtime/`
  keep their attribution ([licensing.md](licensing.md)).

## Public source and identity

The Arkeyan forge contains the runtime, six game adapters and Arkeyan-Builder. Arkeyan Launcher is a separate application.
Public files must contain no personal names, account details, home-directory paths or machine names.
The licence holder is the Arkeyan contributors. Keep third-party attribution and licence notices intact.

## Hygiene of a kit
- `make_kit.sh` refuses any file that could hold game data (`.xex`, `.iso`, `ppc_recomp*`, `shader_cache.cpp`,
  `.lsa`, `image.bin`).
- Every compile maps the checkout path to `.` (`-ffile-prefix-map`, `-fmacro-prefix-map`, `-fdebug-prefix-map`);
  binaries and libraries are stripped.
- **`tools/kit_scan.sh <kit dir>` must pass** (`make_kit.sh` runs it before zipping): it fails on the build machine's
  user or paths, the words of the local list `private/kit_scan_words.txt` (untracked), and e-mail addresses; binaries
  are read with `strings` (UTF-16 too), archives decompressed first, file names count.
  In the Linux kit's container the user has no name, so `make_kit.sh` passes the host's (`KIT_SCAN_USER`).

## Checklist before tagging
1. Kits rebuilt from this repo's `main` with the kit `version` the release expects (`version=` in `make_kit.sh`).
2. `kit_scan.sh` passes on every kit; `tools/test_all.sh` passes for the games in the release; `ArkeyanLauncherTest` passes.
3. The kits tested end to end (disc -> `@done` -> the game starts) on each platform; the Windows kit on a real PC.
4. The launcher's `launcher.ini` migration read through.
5. dxc: its licence text is in every kit (decided 2026-10-26, [licensing.md](licensing.md)); check it matches the shipped version.
6. Arkeyan Launcher's README and the release notes updated; then the steps of
   [launcher/updates_release.md](launcher/updates_release.md).
