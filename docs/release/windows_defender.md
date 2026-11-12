# Windows Defender: evidence and release options

> **Purpose:** why Defender may flag our Windows programs, what 0.9.6 changed, and the signing options left · **Status:** current (2026-11-11) · **Tags:** none

Scope: the supplied workshop and `private/launcher`, inspected offline on 2026-11-10. The reported detection is
`Trojan:Win32/Bearfoos.A!ml`; the owner has already submitted false positives to Microsoft. That report is the starting
point, not an independently reproduced verdict. `private/audit-windows-090.md` is absent. There are no release PE files
here to inspect, and the dependency submodule directories are empty. The following rankings are hypotheses about
plausible contributors, not Microsoft's feature weights or a finding that the programs are malicious.

## Evidence and likelihood

| Factor | Assessment | Evidence and interpretation |
| --- | --- | --- |
| Unsigned releases and limited reputation | Likely contributor, inferred from the build | The Windows job in `private/launcher/.github/workflows/build.yml` uses clang-cl and copies the executable directly to `dist/`; neither it nor `tools/make_kit.sh` contains an Authenticode signing step. The macOS `codesign` calls do not sign Windows PE files. A separately signed published artifact cannot be ruled out without inspecting it. A changing locally built game has its own hash. |
| Thin VERSIONINFO | Possible contributor | Before this change, the launcher's CompanyName was empty, the builder and games lacked CompanyName/LegalCopyright, and XenosRecomp had partial metadata. XenonRecomp had no first-party resource attachment in our build. Descriptive metadata identifies the software to people and scanners; it does not authenticate a publisher or guarantee a different verdict. |
| Missing application manifest | Unlikely for launcher, builder, games and XenosRecomp; possible for XenonRecomp | `skylauncher.rc.in`, `arkeyan-builder.rc.in`, `SSARecomp.rc` and `xenosrecomp.rc.in` already referenced manifests. Games built by the kit inherit the manifest object through `kit_commands.py`. This change adds XenonRecomp's manifest and makes ordinary privileges explicit. |
| llvm-mingw, clang/lld and static runtimes | Possible contributor in combination | `cmake/toolchains/x86_64-w64-mingw32-clang.cmake` selects llvm-mingw 20260922, UCRT and `-static`; `tools/make_kit.sh` uses it for tools and games. The launcher instead uses clang-cl and the static MSVC runtime. These are legitimate toolchains; replacing them is no demonstrated cure. |
| Stripped symbols | Possible contributor, weak evidence alone | `strip_kit()` in `tools/make_kit.sh` applies `llvm-strip --strip-all` to our three kit executables and strips debug information from libraries/objects. This is existing release packaging, not proof of packing or obfuscation. Do not strip more to address a detection; retain a private diagnostic build when investigating it. |
| PE sections and entropy | Possible, unmeasured | `cmake/embed_fonts.cmake`, runtime shader caches and the launcher's embedded fonts/catalogue increase the amount of non-code data. No release PE is available to measure entropy or section flags/names. No packer step was found in the inspected build scripts. High entropy and unusual sections must not be claimed as observed. |
| Timestamps and reproducibility | Unlikely as an isolated cause | The MinGW toolchain already uses `-mno-incremental-linker-compatible` for object timestamps and `--no-insert-timestamp` for PE headers. `make_kit.sh` also passes these flags when overriding compiler flags, maps source paths, and sets `ZERO_AR_DATE=1`. `ObjectList()` sorts game objects before linking. These help comparison; they are not full reproducibility evidence. |
| Download followed by execution | Likely contributor | `updater.cpp::DownloadVerified()` fetches release assets and `GameInstaller::Launch()` in `game_install.cpp` runs the extracted Arkeyan-Builder. `main.cpp::DownloadToolchain()` downloads the compiler, checks a pinned SHA-256, and extracts it before compilation. On Windows the builder uses WinHTTP and in-process ZIP extraction, not a downloaded shell script. |
| Writing executable files and running them | Likely contributor | `main.cpp::Link()` writes the game executable. `AddMovieShaders()` then runs that new game headlessly to collect runtime shaders and can relink it afterwards. This is documented work, logged and limited by a timeout, but resembles an installer/compiler chain to behavioural detection. |
| Self-update | Likely contributor | `UpdateLauncher()` downloads a verified replacement next to the running application in `.skylauncher-update`; `ReplaceInPlace()` renames the previous executable to `.old`, installs the new file and supports rollback. The UI requests a restart. This is a real update operation; there is no need to add a batch/PowerShell replacement helper. |
| Process creation | Possible contributor | Builder `process.cpp::Run()` uses `CreateProcessW`, redirected logs, an explicit inherited-handle list, `CREATE_NO_WINDOW` and timeout/cancel handling for its own child. Launcher `game_install.cpp` uses SDL processes and a Windows job for cancellation. Launcher `updater.cpp` starts the system curl client. These calls have legitimate purposes; a console-free UI worker is not antivirus evasion. No remote-thread injection or anti-debugging call was found in the inspected launcher/builder/platform sources. |
| Network at startup | Possible contributor | `ui/launcher_ui.cpp` checks `update.auto_check` (default `1`) and calls `StartUpdateCheck(false)`; `updater.cpp::Check()` fetches release JSON. The setting is already visible in `ui/update_page.cpp`. This is a check, not automatic download/execution of an update; installation actions initiate the tool downloads. |
| Kit layout and per-user writes | Possible contributor | `make_kit.sh` assembles Arkeyan-Builder, `bin/` translators and DXC DLLs, prebuilt `lib/`, compile/link templates, headers, resources, licences and `relink/`. `game_install.cpp` puts the compiler in the launcher's data folder; the builder writes `arkeyan-work/` and the game beside the chosen output. Portable writable locations are expected for this design, but the executable/library/compiler chain needs clear documentation. |
| Runtime memory APIs | Unlikely on this evidence | `runtime/platform/platform_win32.cpp` reserves/commits guest memory with VirtualAlloc and changes it to read/write, read-only or no-access with VirtualProtect. The inspected file contains no PAGE_EXECUTE allocation. These memory services are not evidence of injected or decrypted host code; removing them would break the runtime. |

## Priorities, changes and limits

1. **Follow up the existing Microsoft submission: high potential effect, low engineering cost.** Keep the submission ID,
   exact distributable executable hash, release/tag, Defender engine/security intelligence version and detection time.
   Provide the download-to-compile/update explanation above. Reproduce with current security intelligence and the exact
   release; a successful review of one hash does not promise approval of every future build. Distinguish Defender malware
   classification from SmartScreen download reputation. Only distributable software is in this investigation; do not
   collect or redistribute a player's game or personal files.
2. **Complete honest executable identity: small cost, modest/uncertain detection effect; implemented.**
   `arkeyan-builder.rc.in`, `game.rc.in`, `version_resource.h`, `xenosrecomp.rc.in` and the launcher's `skylauncher.rc.in`
   now contain CompanyName, product, description, file/product versions and copyright. `Arkeyan project` names a project,
   not an invented legal company or the game's original publisher. Copyright strings follow the existing licences and
   `THIRD_PARTY_NOTICES.md`; translator metadata explicitly identifies our kit distribution and retains upstream authors.
   Game descriptions identify the Arkeyan runtime. Binary file names and game identities stay compatible.
3. **Make manifests explicit: small cost; implemented.** `CMakeLists.txt` enables the RC language and attaches the new
   `tools/arkeyan-builder/xenonrecomp.rc.in` to XenonRecomp. The builder uses the kit version defined in the root project.
   `SSARecomp.manifest` and the launcher's `skylauncher.manifest` declare `asInvoker`, `uiAccess=false`; UTF-8 and the
   launcher's DPI settings remain present. Games built by the kit keep the existing prebuilt manifest object: adding a
   second one in `VersionResource()` would duplicate resource 24/1. `runtime/CMakeLists.txt` now also includes the game
   VERSIONINFO in direct UWP builds. UWP package identity/version remains the separate Xbox packaging path; the UWP build
   was not validated here.
4. **Fail visibly if resources cannot be compiled: small cost; implemented.** `ExecutableResources()` in builder
   `main.cpp` previously warned and returned an empty object path after windres failed, despite its comment promising
   failure. It now removes any failed/stale resource object, stops the build and points to `logs/icon.log`; the Windows
   link always includes the resource object. A retry cannot reuse an object left by the failed resource command.
   A missing disc icon alone remains optional. A failed rebuild can leave an older executable on disk; it must not be
   treated as a newly successful install. Existing VERSIONINFO tests were updated for the new metadata;
   `install_guard_test.cpp` now covers failed/partial/missing resource output, retry and successful caching with a fake
   child runner. These integrated tests require the missing dependencies and remain to be run with the release build.
5. **Improve repeatability: small cost; implemented for the launcher.** Its `CMakeLists.txt` adds `/Brepro` to MSVC-style
   compilation and `/Brepro /INCREMENTAL:NO` to the launcher link, including the clang-cl release workflow. Existing
   MinGW deterministic flags remain as they were. This removes wall-clock object/link variability; it does not prove
   identical releases across paths, toolchain/SDK versions or CI images. Absolute source paths, ZIP file times and the
   mutable `windows-latest` runner still warrant a separate reproducibility comparison. Pinning an image/toolchain and
   comparing two unsigned builds would cost more and needs the release build environment.
6. **Keep the existing integrity and process model: no needless API rewrite.** Pinned compiler hashes, verified
   `SHA256SUMS`, per-file kit checks, direct argument vectors, progress events, logs and cancellation already explain
   the operations. Replacing curl with native HTTP would remove a child process but costs implementation/testing and
   has no established antivirus benefit. Moving from in-place update to a conventional signed installer, or separating
   shader capture into an explicit installation stage, costs more and changes the product flow. Decide these only if
   Microsoft's feedback or reproducible observations justify them. Do not suppress the compiler/shader capture work.

Both repositories must be rebuilt: editing `private/launcher` here does not change the public launcher's
release by itself. Rebuild the kit's runtime objects as well as the tools so the revised manifest reaches local games.
Do not publish these file changes as an antivirus fix proven to remove the detection: that result has not been tested.

## Lasting choices for the owner

No provider terms or prices were fetched: network operations are forbidden for this task. Costs below describe the
work and recurring obligations, not a current quote. The official links are reference destinations to check before a
decision, not pages consulted during this audit.

| Option | Cost and trade-off | Decision/status |
| --- | --- | --- |
| Microsoft false-positive submissions | Small operational cost per incident; human review and intelligence propagation have variable delays. Preserve one case per reproducible artifact and follow its result. A verdict is not a contractual allow-list. | Already initiated; continue tracking via the [Microsoft submission portal](https://www.microsoft.com/en-us/wdsi/filesubmission). |
| SignPath Foundation | A sponsored signing route for eligible open-source projects; admission, public source/build requirements and CI integration take work. Eligibility must be checked; the tools' source is not currently public. | Owner already said **not now**. Keep that decision; reconsider when source publication becomes acceptable. [Foundation](https://signpath.org/). |
| Azure Trusted Signing | Managed signing: service charges, identity verification, eligible identity/region and CI authentication/permissions. Verify the current offering/name, eligibility and price before choosing it. Protect signing access and use a trusted timestamp service. | Future owner choice; no subscription or integration created. [Microsoft signing service documentation](https://learn.microsoft.com/en-us/azure/trusted-signing/overview). |
| OV code-signing certificate | Recurring certificate/service fees, publisher validation, hardware/cloud key custody and release integration. Consistent publisher identity authenticates releases but does not guarantee Defender or SmartScreen acceptance. | Request a current quote and check applicant eligibility before buying. [Microsoft Authenticode overview](https://learn.microsoft.com/en-us/windows-hardware/drivers/install/authenticode). |
| EV code-signing certificate | Typically more identity/admin work and cost, plus protected key/service operation. EV is not an antivirus exemption and should not be bought on an instant-reputation promise. | Compare operational requirements with OV/managed signing; no purchase proposed as an immediate remedy. |
| Public SHA-256 hashes | Very small cost; helps users, support and Microsoft identify exact files. A hash next to an unsigned download does not independently authenticate the publisher if that release account is compromised. | Already present: launcher release `SHA256SUMS`, kit entries added by `private/launcher/tools/release_kit.sh`, and extracted-kit `files.sha256` from `make_kit.sh::finish()`. Keep them; sign before generating hashes if signing is adopted. |
| GitHub build attestations | Moderate CI/setup cost; binds digest, source commit and workflow identity for independent verification. Requires suitable repository/plan support and an OIDC-enabled attestation job; adds no Authenticode signature or automatic Defender reputation. | **Not present in the supplied workflows**: root CI only tests, launcher workflow builds/releases, kit script builds locally. Attest the actual CI outputs; a CI job merely attesting a locally uploaded kit would not establish how it was built. [GitHub documentation](https://docs.github.com/en/actions/security-for-github-actions/using-artifact-attestations). |
| Public tools source and verifiable builds | Significant publication, dependency and release work; enables independent review and can support sponsored signing, but is not a classifier exemption. Build comparison needs identical compiler, SDK, inputs, paths/configuration and controlled archive metadata. | Owner already said **later**. Record exact build provenance now; revisit publication later. |

Signing launcher/tools at the publisher can establish a stable publisher identity over releases. It does not sign games
that players compile locally. Never distribute a signing key in the kit. Local game outputs vary by edition, version
and configuration; do not promise that tools' reputation transfers to each executable or upload those outputs as part
of a public release. Certificate validation, download reputation and Defender malware detection are separate checks.

## Verification performed and remaining

Offline checks passed with the installed native clang++ and GNU MinGW windres/GCC:

- A standalone probe compiled `version_resource.h`, checked required identity fields and two/three/four-part numeric
  versions, and emitted a VERSIONINFO for a synthetic title. No game data was used.
- CMake `configure_file()` rendered the builder, local game, XenonRecomp, XenosRecomp and launcher RC templates with a
  synthetic test version. All placeholders were resolved; windres compiled every script.
- Six synthetic Windows PE programs linked successfully: those five resource variants and the kit-game generator
  variant. Both game variants linked the existing separate `SSARecomp.rc` manifest object. Embedded strings were
  checked for all required VERSIONINFO keys/values, UTF-8 and `asInvoker`/`uiAccess=false`; launcher DPI settings survived.
- Compiling each resource twice produced identical resource objects with the installed GNU tools. This does not test
  the pinned llvm-mingw release, the clang-cl `/Brepro` path or reproducibility of the actual applications.
- Changed C++ formatting and `sh -n tools/make_kit.sh` were checked. No git command, network operation or commit ran.

Full application builds and the existing integrated tests could not run: the required SDL, XenonRecomp, XenosRecomp,
plume and other submodules are absent. No real release executable was scanned, signed or run; no Defender result is
claimed. The release build should make the Windows launcher and kit with their actual toolchains, inspect Properties /
Details and manifest resources in launcher, builder, both translators and a locally built game, exercise an install
and update with logs, and verify that a windres failure prevents successful installation. Then compare unsigned builds
and test the exact distributable artifacts with current Defender intelligence, attaching the results to the existing
Microsoft case. Keep any locally compiled game and its inputs private.
