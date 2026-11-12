# macOS (Apple silicon, Metal)

> **Purpose:** how the game is built, bundled, signed, profiled and measured on macOS · **Status:** current (2026-10-26) · **Tags:** none (host platform)

All six games have been played on macOS. Performance findings: [renderer_threads.md](../../runtime/gpu/renderer_threads.md).

## Build
```
tools/prepare_game.sh "<disc>.iso"          # host tools, xexdump, private/ssa/ppc/, shader cache (AIR+SPIR-V+DXIL), ~3-5 min
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo [-DARKEYAN_PPC_LIBRARY=<libArkeyanRecompLib.a>]
ninja -C build ArkeyanRuntime
```
Giants: `-DARKEYAN_GAME=giants` in a `build-giants` folder, target `ArkeyanRuntime`, executable `"build-giants/ArkeyanRuntime/Skylanders Giants"`
([../building-and-playing.md](../building-and-playing.md)).

Requirements: Apple silicon, a recent macOS (tested on 26), the Xcode command-line tools, `brew install cmake ninja`;
the **Metal toolchain** (`xcodebuild -downloadComponent MetalToolchain` on Xcode 26+). From a fresh clone the whole
macOS build took ~8 min on an M1 Pro (clone 1 min, game data 3 min, configure 1 min, compile 2.5 min at -j4).

Output `"build/ArkeyanRuntime/Skylanders Spyro's Adventure.app"` (`MACOSX_BUNDLE`, `resources/macos/Info.plist.in`: placeholder bundle id
`org.ssarecomp.SSARecomp`, `NSHighResolutionCapable`, games category, `LSMinimumSystemVersion` from the deployment
target, `NS*FolderUsageDescription` strings for ISOs in Documents/Desktop/Downloads and external volumes; no
game-controller key needed on macOS), icon from `ARKEYAN_GAME_ICON_DIR` when present, otherwise the neutral disc from `resources/macos/make_icon.py` (no game art; [game_art.md](../../runtime/host/game_art.md#the-app-icon)).
Post-build: ad-hoc `codesign --force --sign - --timestamp=none`, and `"build/ArkeyanRuntime/Skylanders Spyro's Adventure"` becomes a **symlink to
the bundle's binary** (so scripts keep working).

Ad-hoc signatures change with every rebuild: macOS may ask for folder access again. Distribution builds:
`-DCMAKE_OSX_DEPLOYMENT_TARGET=11.0` (or 13.0) for `ArkeyanRecompLib` and SDL alike. The player's compiler is Xcode's
clang plus `metal` (Arkeyan-Builder, [../../tools/arkeyan-builder.md](../../tools/arkeyan-builder.md)).

## Launching and choosing the disc
`"Skylanders Spyro's Adventure.app"` or `"Skylanders Spyro's Adventure" <iso>`; the first run asks for the ISO and remembers it in `launcher.ini` (`[ssa] iso=`);
**hold Option** at start (read with `CGEventSourceFlagsState`: SDL only learns modifiers from window events) or pass
`--select-iso` to choose another; a command-line path is **not saved** (test runs never change the player's choice).

## Behaviour notes
- Window closed without `SDL_EVENT_QUIT` (an NSWindow ordered out): `IsWindowGone()` checks `NSApplication.isHidden`
  (Cmd+H hides the app: not that case), then `_Exit(0)`.
- SDL's `offscreen` video driver has **no Metal view** and, without a graphics flag, SDL loads OpenGL on macOS (EGL
  fails): use `SDL_PROP_WINDOW_CREATE_EXTERNAL_GRAPHICS_CONTEXT_BOOLEAN` (D39 in [../../porting/gotchas.md](../../porting/gotchas.md)).
- **Maintenance sleep** during long unattended runs (lid closed): only a user-activity assertion keeps a full wake:
  `caffeinate -u -t 30`, renewed (D31); idle-sleep assertions (`caffeinate -i/-s`, Amphetamine) did not hold.
- **Profiling**: `sample <pid> 10` works in a level (**`sample <pid> 4 1` freezes the game**);
  `xctrace record --template 'Time Profiler' --attach <pid>` works, `--launch` drops the environment.
- **Cold Metal cache** for pipeline timing: copy `"Skylanders Spyro's Adventure.app"`, change `CFBundleIdentifier`, `codesign -s - --force`,
  run it, then delete the copy and `$(getconf DARWIN_USER_CACHE_DIR)/<that id>`.
- **Perf** (M1 Pro): level_027 at 30 fps at scale 1 and 2 (work median 23-26 ms before the GPU thread, 18-20 ms after),
  55.9 fps with `fps_limit=60` on an idle machine, 0 frames over 100 ms in a 120 s run. **Run before/after pairs at
  the same time**: a load average of 20-50 from other jobs skews everything.
- Data root: `~/Library/Application Support/Skylanders/` ([data_folders.md](data_folders.md)). Test runs never touch it:
  the isolation block of [../testing.md](../testing.md).
