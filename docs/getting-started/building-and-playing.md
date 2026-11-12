# Building and playing: the user-facing guide

> **Purpose:** how to build from your own disc and play it, with SSA and Giants as source-build examples (controls, portal, Toy Box) · **Status:** current (2026-10-26) · **Tags:** none; disc identities are [SSA] and [Giants]

Native PC, Mac, Linux and Xbox (Dev Mode) ports of **Skylanders: Spyro's Adventure** (SSA) and **Skylanders: Giants**
(Xbox 360), made by static recompilation: the game's PowerPC code and Xenos shaders are translated into C++ and host
shaders, on your own machine, from your own disc. A new runtime stands in for the Xbox 360 (kernel, graphics through
Metal, Vulkan or D3D12, audio, input, the Portal of Power). Players install all six games through **Arkeyan Launcher**, which downloads the player kit and builds the game
([launcher documentation](../release/launcher/README.md)). This source-build guide uses SSA and Giants as examples;
current versions and tested platforms are in the [project README](../../README.md).

## You need your own disc

This repository contains **no game data** and never will: no disc image, no game files, no translated code, no
shaders, no artwork. You need a disc image (`.iso`) that you dumped yourself from your own Xbox 360 disc of
the game. The supported releases (the program checks the image and tells you if it is another one):

| Game | Redump name | Size | Title ID |
|---|---|---|---|
| SSA | "Skylanders - Spyro's Adventure (USA, Europe) (En,Fr,De,Es,It,Nl,Sv,No,Da,Fi)" (default.xex version 0.0.0.2) | 8 738 846 720 bytes | 415608AB |
| Giants | "Skylanders - Giants (USA, Europe) (En,Fr,De,Es,It,Nl,Pt,Sv,No,Da,Fi)" (XGD3) | 8 738 846 720 bytes | 415608DA |

Everything the build derives from your disc (`private/`, including `private/<game>/ppc/` and the shader cache) and every executable or package it
produces contains the game's code. **Keep it to yourself; never share it.** Figure dumps (`.sky`, `.bin`, `.dmp`) are
copies of your own toys: the same applies.

## Building

The build translates the game from your disc, then compiles it. It needs an internet connection the first time
(submodules, FFmpeg's source, the Windows toolchain), about 3 GB of free disk space for a macOS build (more for the
Windows and Linux toolchains, and about 18 GB more while making an Xbox package), and some time: from a fresh clone
the whole macOS build took about 8 minutes on an M1 Pro (clone 1 min, game data 3 min, configure 1 min, compile
2.5 min at `-j4`); the Windows cross build takes 10 to 30 minutes. Later builds only recompile what changed.

Get the source with its submodules:

    git clone --recurse-submodules <repository URL> Arkeyan
    cd Arkeyan

(If you cloned without `--recurse-submodules`: `git submodule update --init --recursive`.)

Once per clone: `git config core.hooksPath tools/git-hooks` (checks the format, the identity and the staged files before each commit).

In the commands below, `"<disc>.iso"` is the path to your disc image (keep the quotes: the file name has spaces).

**SSA or Giants.** One tree builds one game per build folder: the default is SSA (`build/`, executable `Skylanders Spyro's Adventure`);
`-DARKEYAN_GAME=giants` builds Giants (use `build-giants/`; the CMake target is `ArkeyanRuntime`, the executable is
`Skylanders Giants`). The game data step below (`tools/prepare_game.sh`) prepares SSA; Giants' `private/giants/ppc/` is made as
described in [testing.md](testing.md#a-fresh-clone). The Windows, Linux and Xbox scripts build SSA.

### macOS (Apple silicon)

Requirements: an Apple silicon Mac with a recent macOS (tested on macOS 26), the Xcode command line tools (`xcode-select --install`), and
from [Homebrew](https://brew.sh): `brew install cmake ninja`. Python 3 comes with the command line tools.

1. Prepare the game data from your disc (builds the recompiler, translates the code into `private/ssa/ppc/` and the
   shaders into `private/ssa/ppc/shader_cache.cpp`; a few minutes):

       tools/prepare_game.sh "<disc>.iso"

2. Configure and build (the first configure also downloads and builds a minimal FFmpeg, about a minute):

       cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
       ninja -C build ArkeyanRuntime
       # Giants: cmake -S . -B build-giants -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DARKEYAN_GAME=giants
       #         ninja -C build-giants ArkeyanRuntime      (gives "build-giants/ArkeyanRuntime/Skylanders Giants.app")

   Add `-j<n>` to `ninja` to limit the parallel jobs if the machine has little memory.

3. Run it: `open "build/ArkeyanRuntime/Skylanders Spyro's Adventure.app"`, or from a terminal (to see the log)
   `"build/ArkeyanRuntime/Skylanders Spyro's Adventure" "<disc>.iso"`. The first time, the app asks for the disc image and remembers it
   (hold Option at start, or pass `--select-iso`, to choose another one). `"build/ArkeyanRuntime/Skylanders Spyro's Adventure" --version` prints
   the version.

### Windows 10/11 x64

There is no native Windows build from source: the Windows executable is cross-compiled **on a Mac** (Apple silicon)
and copied to the PC (players use Arkeyan Launcher, whose kit compiles on the PC itself). It works on a real PC.

1. On the Mac, with the macOS requirements above:

       tools/build_windows.sh "<disc>.iso"

   It runs `tools/prepare_game.sh`, downloads llvm-mingw 20260922 (checked against its SHA-256) into
   `private/toolchains/`, builds FFmpeg for Windows, then `build-win64`. `JOBS=<n>` sets the parallel jobs (default
   4). The first build takes 10 to 30 minutes.

2. Copy the folder `build-win64/dist/ArkeyanRuntime/` (`"Skylanders Spyro's Adventure.exe"`, `fonts/`, `THIRD_PARTY_NOTICES.md`) and your disc
   image to the PC.

3. On the PC: double-click `"Skylanders Spyro's Adventure.exe"` (it asks for the disc image the first time), or run
   `"Skylanders Spyro's Adventure.exe" "<disc>.iso"` from a terminal. It needs a GPU with a Vulkan 1.2 driver (any recent NVIDIA, AMD or
   Intel driver); without Vulkan it falls back to D3D12. `SKY_GPU_BACKEND=d3d12` or `vulkan` forces one.

Details (backends, the console program, Wine and D3D12 tests): [platforms/windows.md](platforms/windows.md).

### Linux (x86_64, arm64)

Requirements: Docker (on Linux, or on a Mac through OrbStack or Docker Desktop), Python 3, git.

    tools/build_linux.sh "<disc>.iso"                 # for the machine's own architecture
    tools/build_linux.sh "<disc>.iso" --arch amd64    # or for the other one (emulated: much slower)

It runs `tools/prepare_game.sh` on the host, builds an Ubuntu 24.04 image with the toolchain
(`tools/linux/Dockerfile`, about 1.4 GB, nothing from the checkout is copied in) and compiles in a container. The
result is `"build-linux-<arch>/ArkeyanRuntime/Skylanders Spyro's Adventure"` with its `fonts/` folder: run `"./Skylanders Spyro's Adventure" "<disc>.iso"`. It needs
a Vulkan driver.

Without Docker, on a Linux desktop: install the packages listed in `tools/linux/Dockerfile` (clang, CMake, Ninja,
SDL3's X11/Wayland/audio/udev headers, zlib, the Vulkan loader), then

    tools/prepare_game.sh "<disc>.iso"
    tools/ffmpeg/build.sh host $PWD/private/ffmpeg/linux
    cmake -S . -B build-linux -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DARKEYAN_FFMPEG_ROOT=$PWD/private/ffmpeg/linux
    ninja -C build-linux ArkeyanRuntime

(not tried outside the container). Details and the Linux player kit: [platforms/linux.md](platforms/linux.md).

Note: build the shader cache on a Mac if you also build for macOS. A cache built on Linux has no Metal shaders and
breaks the macOS build (`tools/prepare_game.sh "<disc>.iso" --shaders` rebuilds it).

### Xbox One / Series X|S (Dev Mode)

The Xbox build is a UWP app installed on a retail console switched to Dev Mode (a Microsoft Partner Center developer
account is needed for Dev Mode). It is built **on a Mac**. The package holds the executable and your disc image
(about 8.7 GB): never share it. The full guide, including Dev Mode, installation, logs and troubleshooting, is
[`tools/xbox/README.md`](../../tools/xbox/README.md). In short:

    brew install cmake ninja openssl@3
    tools/xbox/setup_tools.sh                       # once: llvm-mingw, makemsix, osslsigncode into private/xbox/
    tools/prepare_game.sh "<disc>.iso"
    cmake -S . -B build-uwp -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/uwp-x64-llvm-mingw.cmake \
          -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS_RELEASE="-O2 -DNDEBUG" -DCMAKE_CXX_FLAGS_RELEASE="-O2 -DNDEBUG"
    ninja -C build-uwp ArkeyanRuntime
    tools/xbox/check_uwp_imports.sh "build-uwp/ArkeyanRuntime/Skylanders Spyro's Adventure.exe"
    tools/xbox/package.sh --exe "build-uwp/ArkeyanRuntime/Skylanders Spyro's Adventure.exe" --iso "<disc>.iso" --out SSARecomp.msix

Then install `SSARecomp.msix` through the console's Device Portal (or register a network share, which avoids the
8.7 GB upload), and set the app type to **Game** in Dev Home.

## Playing

### Controls

A gamepad works as the Xbox 360 controller does (Xbox, PlayStation, Switch and most SDL-supported pads). Keyboard
defaults:

| Action | Key | Action | Key |
|---|---|---|---|
| A | Space | Move | W A S D |
| B | X | Camera | I J K L |
| X | C | D-pad | arrow keys |
| Y | V | LB / RB | Q / E |
| START | Return | LT / RT | 1 / 3 |
| BACK | Backspace (Escape also works as back) | | |

Every key can be rebound (`--key a=Space`, or `key_<action>` in `launcher.ini`). The game's button prompts follow the
device you use last (Xbox, PlayStation, Nintendo glyphs or keyboard keycaps; `--glyphs` forces one).

### Settings

Settings come from the command line, then a few `SSA_*` variables, then `launcher.ini`, then the defaults;
`"Skylanders Spyro's Adventure" --help` lists them all. Everything is in one folder, `~/Library/Application Support/Skylanders/` (macOS),
`%APPDATA%\Skylanders\` (Windows) or `~/.local/share/Skylanders/` (Linux): `launcher.ini`, the figure `Collection/`,
and one folder per game (`SSA/`, `Giants/`) with its saves and log. On the Xbox, the app's `LocalState` folder holds
them. Every option, key and folder: [settings.md](settings.md).

### The Portal of Power

The game needs a portal with figures on it. You can use a real one or the built-in emulated one.

- **Emulated portal** (the default when no real portal is plugged in at start): it reads figure dumps (`.sky`,
  `.bin`, `.dmp`: 1 KB tag images of your own figures) from the figure collection (`Collection/` in the data folder). The first start also creates
  a new, blank dump there for every character that has none. Put figures on it with the Toy Box (below); F7 cycles
  through the dumps and F8 takes every figure off. The portal starts the way you left it.
- **Real portal** (PC and Mac): plug it in before starting the game. The Wii, Wii U, PS3, PS4 and PC portals (USB HID)
  are used directly. The Xbox 360 portal needs libusb-1.0 (`brew install libusb` on macOS, the `libusb-1.0-0` package
  on Linux, `libusb-1.0.dll` next to the executable on Windows). `SKY_PORTAL=real` waits for a portal plugged in
  later; `SKY_PORTAL=emulated` ignores real ones.
- **Xbox**: plug the portal in before starting the app. The Xbox One portal is the most likely to work; HID portals
  may or may not be allowed by the console; the Xbox 360 portal cannot work there. Untested on a console so far.

### The Toy Box

The Toy Box is an in-game menu for the emulated portal: browse your figures (with their portraits, read from your
disc), put them on or take them off the portal, and view or edit a figure's sheet (nickname, hat, level, gold,
experience, upgrades...), saved into its dump. The game is paused for input while it is open.

| | Keyboard | Gamepad |
|---|---|---|
| Open / close | F6 or Tab | hold BACK, press Y (Xbox: hold View, press Y) |
| Move | arrows | d-pad, left stick |
| Choose | Enter, Space | A |
| Put on / take off the portal | X | X |
| Figure sheet | Y | Y |
| Back (closes it from the grid) | Escape, Backspace | B |
| Element filter / value x10 | Shift + left/right | LB / RB |
| Skylanders / magic items / adventure packs | Page Up / Page Down | LT / RT |
| Clear the portal | Delete | START (Xbox: Menu) |

The mouse works too on PC and Mac. More: [`docs/runtime/host/figures.md`](../runtime/host/figures.md).

## Known issues

What is open, per game: [SSA](../../games/ssa/docs/status.md), [Giants](../../games/giants/docs/status.md); per platform:
[platforms/](platforms/README.md). In short: macOS is played daily; Windows works on a real PC; Linux was only tested
in containers; the Xbox port is paused. 60 FPS (`--fps-limit 60`) is experimental. Bink movies are drawn by the GPU
only when the shaders the game builds at run time were dumped first (`SKY_DUMP_SHADERS=<dir>`, then
`tools/prepare_game.sh "<disc>.iso" --shader-dump <dir>`; Arkeyan-Builder does it for players).

Bug reports are welcome: include the log (`Arkeyan.log` in the game's data folder, [settings.md](settings.md#the-game-log)),
the platform and GPU, and what you did.

## For developers

Start at [../README.md](../README.md) (every page, one line each);
[testing.md](testing.md) (isolated runs and the regression harness) and [../porting/debugging.md](../porting/debugging.md)
(environment variables, log tags) are the daily tools.

## Licence

The project's own code is under the [MIT licence](../../LICENSE). Third-party components keep their own licences, listed
with their full texts in [`THIRD_PARTY_NOTICES.md`](../../THIRD_PARTY_NOTICES.md). FFmpeg (LGPL-2.1-or-later, only its WMA
Pro decoder, no GPL parts) is downloaded as source and built on your machine.

## Legal

ArkeyanRuntime is an unofficial, non-commercial fan project. It is not affiliated with, endorsed by or sponsored by
Activision, Toys for Bob or Microsoft. *Skylanders* and *Spyro* are trademarks of Activision
Publishing, Inc.; *Xbox*, *Xbox 360* and *Xbox One* are trademarks of Microsoft Corporation. All other trademarks
belong to their owners. The project contains no game code or assets: you must own the game and supply your own legally
made disc image. Do not use this project to play games you do not own, and do not distribute anything built from
your disc.
