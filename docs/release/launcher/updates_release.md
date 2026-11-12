# Updates, kit download, the release flow, CI

> **Purpose:** how does Arkeyan Launcher update itself and the kit, and how is a release published? (the one page of the release steps) · **Status:** current (2026-10-26) · **Tags:** none

Detail in Arkeyan Launcher's own `docs/updates.md`; the checks before a tag: [../release.md](../release.md). The user publishes; **Arkeyan Launcher pushes only when asked; a force-push or release deletion needs the user** (give them the exact `! command`).

## The updater (`src/launcher/updater.{h,cpp}`, window side `ui/update_page.cpp`)
No telemetry: the only request is `GET https://api.github.com/repos/<repo>/releases/latest`, then the asset. HTTPS goes through **`curl`** (`curl.exe` ships with Windows 10/11; macOS and most Linux have it): `--proto =https --tlsv1.2`, 15 s connect timeout, no redirect to plain http.
1. **Check** at **every** start (`[update] last_check` keeps the time), unless `[update] auto_check=0` or `SKY_LAUNCHER_NO_UPDATE_CHECK`; and on demand in Settings > About > Check for updates. Offline: silent.
2. The tag (`vX.Y.Z`) is compared as semver with the built version (`SKYLAUNCHER_VERSION`, the CMake cache variable the CI sets from the tag). Pre-releases and tags that are not versions are never offered.
3. A **banner** pushes the page down 56 px: "Update available: vX.Y.Z", **Update now**, **What's new** (the release's notes), **Later**.
4. **Update now**: the asset for this OS is downloaded next to the running program (`.skylauncher-update/`), its SHA-256 checked against the release's **`SHA256SUMS`** asset, then swapped in. **A release without `SHA256SUMS`, or a wrong hash, is refused and deleted.**
5. Then **Restart now**.

| OS | Asset | Swap |
|---|---|---|
| Windows | `ArkeyanLauncher.exe` | the running exe is renamed `<its name>.old`, the new one moved in, the old deleted at the next start |
| macOS | `ArkeyanLauncher-macOS.zip` (the `.app`) | replaced if its folder is writable (`.old` kept until the next start); otherwise `ArkeyanLauncher-macOS.dmg` is downloaded, verified and opened |
| Linux | `ArkeyanLauncher-x86_64.AppImage` | the AppImage (`$APPIMAGE`) is replaced the same way; not an AppImage: the release page opens |
If the second move fails the first is undone (`ReplaceInPlace`); a copy that is not installed (a build folder) opens the release page. `SKY_LAUNCHER_UPDATE_URL` points at a fake `latest.json` (tests; `file://` works).

## The game kit as a release asset
`[update] kit_repo=owner/name` (default `Arkeyan-Launcher`). **First install**: with no kit, INSTALL downloads the release tagged `v<launcher version>`, else the latest; the zip is verified against that release's `SHA256SUMS` (a mismatch or missing line refuses it), unpacked into `kit/<release tag>/`. **Game updates** (2026-10-27, Arkeyan Launcher 0.4.3): each game has its own version (`version=` in `games/<id>/game.txt`: SSA 0.9.0, Giants 0.8.0, Swap Force 0.4.0; 1.0 = the game at 100 %, each fix bumps the last number), carried by the kit (`kit.txt` `game_version.<id>`) and said by one hidden line of the release's notes, `<!-- games: ssa=0.9.1 giants=0.8.1 -->` ([kit_contract.md](kit_contract.md)). A game whose version there is higher than the built one shows **UPDATE** (with its download and time) in place of PLAY and a small link **Play without updating**; one click fetches the kit of the release the notes belong to if needed and rebuilds the game (incremental). **No tools are ever downloaded in the background** (the old sidebar "Downloading tools" line is gone); the launcher's own update stays offered at start.

## Releasing
1. Bump `SKYLAUNCHER_VERSION` in `CMakeLists.txt`, commit.
2. Tag `vX.Y.Z` and **push the tag**: **only a pushed tag matching the version publishes**; branches and pull requests only build.
3. Wait for the CI: it attaches `ArkeyanLauncher.exe`, `ArkeyanLauncher-x86_64.AppImage`, `ArkeyanLauncher-macOS.dmg`, `ArkeyanLauncher-macOS.zip` and **`SHA256SUMS`** (`sha256sum` format, one line per asset, written by the job).
4. Build the kits in this repo (`tools/make_kit.sh windows macos linux`, [../../tools/arkeyan-builder.md](../../tools/arkeyan-builder.md)), then in Arkeyan Launcher `tools/release_kit.sh vX.Y.Z skylanders-kit-windows-x64.zip skylanders-kit-macos-arm64.zip skylanders-kit-linux-x64.zip` (needs `gh`): uploads the kit zips under their own names only (from 0.6.1 no `ssa-kit-<platform>.zip` copies: Arkeyan Launcher 0.4.x and 0.5.x find no update and their players download 0.6.1 by hand; it lives in Arkeyan Launcher and is changed there), downloads the release's `SHA256SUMS`, replaces or appends a line per kit and uploads it again with `--clobber`, then writes each game's version (from the kits' `kit.txt`, the kits must agree) as the hidden `<!-- games: ... -->` line at the end of the release's notes, replacing an earlier one (`DRY_RUN=1` prints the notes and the upload list and changes nothing). **A game fix**: bump its `version=` first, so the kits and the games line say it.
**The kit follows the launcher** (Arkeyan Launcher 0.4.1; its `docs/updates.md`): a launcher never builds with a kit from an older release than its own (before INSTALL or UPDATE, never in the background, it fetches the kit of `v<its version>` or the one the games line names, `[update] kit_fetched` remembers it), and never runs Arkeyan-Builder on a disc its kit's `games=` lacks (the right kit is fetched first); a kit next to the launcher that is older than a downloaded one, or cannot build the game while the downloaded one can, is not used; the player's own (`SKY_LAUNCHER_KIT`, `[launcher] kit=`) is never replaced. Cause (2026-10-26, a Windows PC): 0.4.0 updated itself, the 0.3.1 kit next to it stayed, and its Arkeyan-Builder 0.1.0 refused the Giants disc with "is it Spyro's Adventure?". So **every launcher release must carry the kit of the same tag** (`tools/release_kit.sh`), even when the kit did not change.
A kit-only update is a new release the same way. Kit names must be exactly the ones of [kit_contract.md](kit_contract.md) ("The release assets"): the new name only.

## Swap Force release checklist
Arkeyan Launcher plays Swap Force (`Skylanders Swap Force`) since the launcher that lists it; a launcher release with it must carry a kit whose `games=` lists `swapforce`.
1. Build the kits with Swap Force: `KIT_GAMES="ssa giants swapforce" tools/make_kit.sh windows macos linux`; check `kit.txt` says `games=ssa,giants,swapforce` ([kit_contract.md](kit_contract.md)). The default `KIT_GAMES` of `make_kit.sh` stays `ssa giants` until the user decides.
2. `SKY_LAUNCHER_TEST_GIANTS_ISO=<giants iso> SKY_LAUNCHER_TEST_SWAPFORCE_ISO=<swap force iso> ArkeyanLauncherTest <spyro iso> <scratch>` passes (each disc accepted by its own game, refused by the others, pictures and portraits read into `SwapForce/`).
3. `tools/test_all.sh` passes with `SWAPFORCE_ISO` set; then `tools/release_kit.sh` as above, with the kit of the same tag.

## CI (`.github/workflows/build.yml`)
Triggers: push to `main` and `feat/**`, tags `v*`, pull requests, manual. Jobs: **windows** (`windows-latest`, `ilammy/msvc-dev-cmd`, clang-cl, Ninja, Release, `-DSKYLAUNCHER_VERSION` from the tag), **linux** (`ubuntu-22.04`, glibc 2.35, Clang, a long apt list of X11/Wayland/udev/dbus headers + `librsvg2-bin`, `libfuse2`, AppImage via `linuxdeploy` with `resources/linux/skylauncher.desktop` and an icon rendered from `resources/icons/skylauncher.svg`), **macos** (`macos-latest`, dmg + zip), then the release job. `concurrency` cancels superseded runs.

## Releases so far
0.1.x (CI only); **v0.2.0** (2026-10-24, the first tag release; re-released after the history rewrites of that day:
[../release.md](../release.md)); **v0.3.0** (2026-10-25: INSTALL downloads the kit; collection bugs S-01/S-02/S-03/S-15
fixed, issue #9); **v0.3.1** (2026-10-25, latest: the Linux kit, the Windows portal fix, one `Skylanders` data folder);
**0.4.0** in progress on `main` (with the 0.5.0 multi-game kits). Version rule (issue #7, decided): bump, tag, push the
tag; the CI version comes from the tag.
