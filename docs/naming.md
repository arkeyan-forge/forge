# Project and product names

> **Purpose:** current names, their history and the identifiers retained for compatibility · **Status:** current (2026-11-12) · **Tags:** none

The project is **Arkeyan**. Its runtime is **ArkeyanRuntime**, its player application is **Arkeyan Launcher**,
and its kit build driver is **Arkeyan-Builder**. **SSA** means Spyro's Adventure, one of the six games.
The licence holder is **the Arkeyan contributors**.

The project was called SSARecomp, then Skylanders-Recomp, before Arkeyan. The launcher was Skylauncher.
The builder was SSABuild through 0.5.x and SkyBuild from 0.6.0 to 0.7.2; it became Arkeyan-Builder in 0.8.0.
The shared libraries, tests and build variables adopted Arkeyan names on 2026-11-09.

## Compatibility identifiers

These names remain because installed files, settings or package identities depend on them:

| Identifier | Reason |
|---|---|
| `SKY_*`, `SKY_LAUNCHER_*`; older `SSA_*`, `SSA_LAUNCHER_*` aliases | Public environment variables; the older spelling is read when the current one is unset (`runtime/host/env.h`) |
| Cached CMake `SSA_<X>` options | Migrated once to `ARKEYAN_<X>`; `SSA_ISO` still names SSA's disc |
| `SSARecomp`, `SGRecomp`, `SSFRecomp`, `TTRecomp` executable names | The launcher accepts older installs until a rebuild writes the game's full title |
| `SSARecomp` data folder and `SSARecomp.log` | Migration inputs; new logs are `Arkeyan.log` |
| `SSABuild`, `ssabuild-work`, `ssabuild-ready.txt` | Older kit executable, work folder and compiler marker; existing caches remain usable |
| `skylauncher-game.log`, `skylauncher-game.prev.log`, `Skylauncher` data folder | Existing launcher settings, support logs and migration inputs |
| `org.ssarecomp.SSARecomp` | Stable macOS bundle identity preserves caches and Dock entries |
| Xbox `SSARecomp.*`, `CN=SSARecomp Local`, packaged `SSARecomp.exe` | Stable package identity permits upgrades |
| `SSARecomp.rc`, `SSARecomp.manifest` | Existing Windows resource filenames in `runtime/platform/win32/` |
| Catalogue runtime field `"SSARecomp"`, `libSSARecompLib.a` | Older recorded installs and locally built game libraries |
| `SSAP`, `SSAK` | On-disk pipeline magic and guest binding marker |
| `SSA_symbols`, `SSA_switch_tables`, `private/ssa/` | Spyro's Adventure-specific inputs, not shared product names |
| `tools/sync_skylauncher.sh`, `namespace skybuild`, `SKYBUILD_*`, `SkylauncherCore`, `SKYLAUNCHER_VERSION` | Existing source filenames and internal identifiers |

Names of other projects in source attribution remain their published names.
`tools/rename_arkeyan.py` maintains the source-identifier migration; new prose uses the current product names.
