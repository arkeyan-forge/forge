# Arkeyan on Xbox (Dev Mode)

> **Purpose:** how is SSA built, packaged, installed and debugged on an Xbox in Dev Mode? · **Status:** current (2026-10-26); the Xbox port is paused (last run 2026-10-23) · **Tags:** [Xbox] [SSA]

The Xbox build is a UWP app: the same runtime as on PC and Mac, with its own host (`runtime/host/uwp`): a D3D12 swap
chain on the app's window, the gamepad through Windows.Gaming.Input, sound through XAudio2. It is installed on a retail
Xbox One or Xbox Series X|S switched to **Dev Mode**, as one signed `.msix` package that holds the executable **and
your disc image** (`game.iso`, about 8.7 GB, stored uncompressed). One x64 package serves both consoles (the code
stays within the Xbox One's Jaguar CPU instructions).

The package contains your game: keep it for your own console, never share it.

Status (2026-10-23): the fixed 8.7 GB `.msix` installs and starts on an Xbox Series X (Dev Mode, app type Game):
D3D12, the shader cache, threads, kernel and files work. The first run crashed when the intro movie started (a game
draw on D3D12 with a pipeline built from SPIR-V; fixed: D3D12 now skips the game's draws, see `docs/getting-started/platforms/xbox_uwp.md`).
Both install methods work (section 5); the network share needs no upload for a new build.

Since the D3D12 work (`docs/runtime/gpu/plume_backends.md`, merged) the game's draws render on D3D12 too (signed DXIL in the shader cache),
checked under vkd3d-proton but not yet on the console. In `Arkeyan.log`, a pipeline the console rejects shows as
`CreateGraphicsPipelineState failed with error code ...` (its draws are skipped). The log also says `[gpu] D3D12 DRED on`,
`[gpu] first present WxH` once a frame reached the screen, and, if the GPU device is lost, `[gpu] D3D12 device removed:
reason ...` followed by the DRED breadcrumbs and page fault, and `[gpu] video memory ...` (budget and usage): send
those lines. A cache built before that has no
DXIL: `tools/prepare_game.sh` rebuilds it, then rebuild `build-uwp`.

What differs from PC on Xbox:
- **Button glyphs**: the Xbox One / Series pad's (View and Menu instead of Back and Start, drawn by the runtime);
  `[input] glyphs=xbox360` in `launcher.ini` brings back the game's own Xbox 360 art (`docs/getting-started/settings.md`).
- **No launcher**: the settings come from `LocalState\launcher.ini` (below).

### Console settings

At the first start the app writes `LocalState\launcher.ini` with the console defaults; edit it through Device Portal
(section 7) and restart the app. The keys are those of `docs/getting-started/settings.md` (`[video]`, `[input] glyphs`, `[ssa]` language and voices);
there is no command line.

| Setting | Console default |
|---------|-----------------|
| input | the gamepad (Windows.Gaming.Input; no keyboard) |
| glyphs | `xboxone` (also `xbox360`, `xbox`, `playstation`, `nintendo`) |
| `resolution_scale` | **2 on Xbox Series X\|S, 1 on Xbox One** (from `AnalyticsInfo.DeviceForm`; the scale is done by the shared `gpu/` code, so D3D12 has it as Metal does) |
| `anisotropy`, `vsync` | 1, on |
| `[portal] mode` | `auto`: a real portal answering at start, otherwise the emulated one; `real`: wait for a real portal (plug it in at any time); `emulated` |

If the Series renders badly or slowly at x2, set `resolution_scale=1`.

`[gpu]` (since 1.0.2488.849), to bisect rendering problems without a new package:
- `thread=off` (default): the single-threaded renderer that first showed the game; `thread=on`: the GPU thread.
- `pipeline_library=off` (default): on, the D3D12 pipeline cache (`pipelines.d3d12`) removed the device at start on an
  Xbox Series X. If it is turned on and a run dies while opening it, `pipelines.d3d12.loading` stays behind and later
  runs skip the cache (the file is renamed `.failed`) until that marker is deleted.

## 1. Tools (once, on the Mac)

    tools/xbox/setup_tools.sh

This puts into `private/xbox/` (about 1.5 GB): the llvm-mingw cross compiler, `makemsix` (Microsoft's open-source
packager, patched by `tools/xbox/patches/makemsix-store-iso.patch` so the disc image is stored instead of deflated) and
`osslsigncode` (signing; `setup_tools.sh` adapts it for packages over 4 GB, see section 3). Tools built before
2026-10-23 20:00 lack this: delete `private/xbox/bin` and run the script again. It needs git, curl, cmake, ninja, the Xcode command line tools and Homebrew's `openssl@3`.
No Microsoft SDK or Visual Studio is involved.

The game shaders are compiled with the `dxc` that comes with the XenosRecomp submodule:

    git submodule update --init thirdparty/XenosRecomp/thirdparty/dxc-bin

## 2. Build the Xbox executable

First derive the game data from your disc (`private/ssa/ppc/` and the shader cache), as for every platform:

    tools/prepare_game.sh "<your disc>.iso"

Then:

    cmake -S . -B build-uwp -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/uwp-x64-llvm-mingw.cmake \
          -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS_RELEASE="-O2 -DNDEBUG" -DCMAKE_CXX_FLAGS_RELEASE="-O2 -DNDEBUG"
    ninja -C build-uwp -j4 ArkeyanRuntime
    tools/xbox/check_uwp_imports.sh "build-uwp/ArkeyanRuntime/Skylanders Spyro's Adventure.exe"

The first build compiles the translated game code for x64 (about 10 minutes). A Windows desktop build of the same
`private/ssa/ppc/` can be reused instead with `-DARKEYAN_PPC_LIBRARY=<build-win64>/ArkeyanRecompLib/libArkeyanRecompLib.a`.
`check_uwp_imports.sh` must print `OK`: every Windows function the program uses must exist on the console.

### Xbox Series X|S build

`-DARKEYAN_UWP_MARCH=x86-64-v3` (a separate build tree, e.g. `build-uwp-series`, compiling `private/ssa/ppc/` itself) makes a package for
the Series CPU (AVX2, FMA, BMI2): faster, and it closes at once on an Xbox One (`Arkeyan.log` says why). The default
(`btver2`) runs on both consoles.

## 3. Make the package

    tools/xbox/package.sh --exe "build-uwp/ArkeyanRuntime/Skylanders Spyro's Adventure.exe" --iso "<your disc>.iso" --out SSARecomp.msix

- The first run creates your signing certificate in `~/Library/Application Support/SSARecomp/xbox-signing`
  (`--cert-dir` to put it elsewhere) and reuses it afterwards. Keep that folder: packages signed with it install over
  each other. `SSARecomp.cer` there is the public part, for installing the package on a Windows PC.
- Each package gets a higher version number automatically, so it installs as an update. `--version a.b.c.d` forces one.
- At the end it prints the package's **family name** (`SSARecomp.SpyrosAdventure_<id>`): the folder name of its data
  on the console.

`--no-iso` instead of `--iso` makes a small package without the game (it starts, logs `no disc image` and stops):
a quick way to check that a console accepts our packages at all.

**Packages over 4 GB.** The disc image is the first file, so every other file starts past 4 GB and needs the zip64
form. makemsix writes it as MakeAppx does: in the central directory, only the values that overflow move into the
zip64 extra field (the image's two sizes; the offset alone for the files after it), and every local header has no
extra field and a 24-byte data descriptor. osslsigncode 2.14 read that extra field as if all values were there and
wrote the offset of the `AppxSignature.p7x` it appends (past 4 GB) in 32 bits; `setup_tools.sh` fixes both, keeping
the minimal form (offset only, version needed 4.5). The package that failed on the console (`0x8007000B`) had been
made with an earlier fix that wrote all three values into every zip64 extra field, and a signature entry marked
version 2.0 with a zip64 field: the likely cause. Checked here: `osslsigncode verify`, the CRC of every file and every
block of `AppxBlockMap.xml` (Python's zipfile; `makemsix unpack` itself cannot read a file over 4 GB, its hash stream
keeps the low 32 bits of the size).

### Disk space

The output folder's disk needs about **18 GB free** while packaging: the unpacked package and the signed one exist
together for a moment (2 x 8.7 GB). Afterwards only the 8.7 GB package remains. The disc image itself is not copied
when it is on the same disk as the output (hard link). `package.sh` checks the free space first and says how much is
missing.

If the Mac's disk is short:
- write the package to an external drive (`--out /Volumes/<drive>/SSARecomp.msix`); the 18 GB are then needed there
  only (exFAT or APFS both work, the disc image is copied to that drive for the time of packaging);
- free space on the Mac: empty the Trash, delete old build folders (`build*/`, `private/build*`), Xcode's
  `~/Library/Developer/Xcode/DerivedData`, `brew cleanup`; macOS also shows big files in System Settings > General >
  Storage.

## 4. Prepare the console (once)

1. **Dev Mode**: install the *Xbox Dev Mode* app from the Store on the console, start it and note the activation code;
   enter it in Partner Center (Xbox devices) with your developer account; then *Switch and restart* in the app.
   The console boots into **Dev Home**.
2. **Device Portal**: in Dev Home, *Remote Access Settings*: enable *Xbox Device Portal*, set a user name and a
   password. Dev Home shows the address, `https://<console IP>:11443`. Open it in a browser on the Mac (accept the
   certificate warning: the console signs its own).
3. **Storage**: the package needs about 9 GB on the console's internal storage; Dev Home shows what is free.

Leaving Dev Mode is done from Dev Home; retail games are not playable while in Dev Mode.

## 5. Install

### A. Register from a network share (recommended for the full game)

The console runs the app from a folder shared by the Mac (a "loose" layout): nothing is uploaded or signed, and a new
build is only a new `SSARecomp.exe` copied over the old one.

1. **The folder** (once): the package's files without its packaging files, plus the disc image. From a small package:

       tools/xbox/package.sh --exe "build-uwp/ArkeyanRuntime/Skylanders Spyro's Adventure.exe" --no-iso --out private/xbox/out/SSARecomp-small.msix
       mkdir -p private/xbox/loose/SSARecomp && cd private/xbox/loose/SSARecomp
       unzip ../../out/SSARecomp-small.msix -x AppxBlockMap.xml AppxSignature.p7x '\[Content_Types\].xml'
       ln "<your disc>.iso" game.iso      # a hard link: no copy (same disk)

   It holds `AppxManifest.xml`, `Assets/`, `SSARecomp.exe`, `THIRD_PARTY_NOTICES.md` and `game.iso`. To update:
   copy the new `"build-uwp/ArkeyanRuntime/Skylanders Spyro's Adventure.exe"` as `SSARecomp.exe` over the old payload and restart the app.
2. **Share it** from the Mac: *System Settings* > *General* > *Sharing* > *File Sharing* on, *(i)*: add the
   `private/xbox/loose` folder; *Options*: *Share files and folders using SMB* on, and tick your user under
   *Windows File Sharing* (it asks for your Mac password). The share is `\\<Mac IP>\loose`.
3. **Register it**: Device Portal, *Home* > *My games & apps* > *Add* > *Register from network share* (the wording varies
   a little): path `\\<Mac IP>\loose\SSARecomp`, your Mac user name and password. The console reads the game from the
   share while it runs: use a wired network, and keep the Mac awake with the share on.

### B. Install the package

In Device Portal, *Home* > *My games & apps* > *Add*: choose `SSARecomp.msix`, then *Next* and *Start*. It needs no
dependency package. Uploading 8.7 GB takes a while: use a wired network and keep the page open. If Device Portal asks
for a certificate, give it `SSARecomp.cer` (from the signing folder); Dev Mode should not need it. On a Windows PC the
same package installs once `SSARecomp.cer` is in *Local Machine > Trusted People*.

### Then, either way

Then set the app type to **Game** (games get the full D3D12 and more memory and CPU; apps are limited, and on Xbox One
apps only get D3D11): in Dev Home, highlight *Spyro's Adventure (SSARecomp)*, press the View button (two squares),
*View details*, and change *App type* to *Game* (menu names may differ a little between console updates).

An update is installed the same way, over the previous version: your saves are kept. Uninstalling deletes them.

## 6. Play

Start it from Dev Home (or *My games & apps*). The gamepad works as on the Xbox 360. B is a game button here: the app
handles it, so it does not send you back to Home. To quit: Xbox button, then *Quit* on the app.

**Real portal** (untested on a console yet, `docs/getting-started/platforms/xbox_uwp.md`):
- Plug the portal into a USB port of the console **before starting the app** (with `mode=auto`). With `[portal]
  mode=real` in `launcher.ini` it can be plugged in later, but then there is no emulated portal.
- The **Xbox One portal** (sold with SuperChargers and Imaginators on Xbox One) is the one most likely to work: it is an
  Xbox accessory, reached through Windows.Gaming.Input.
- The **PS3/PS4/Wii/Wii U/PC portals** (USB HID) may or may not be allowed on the console; try it.
- The **Xbox 360 portal** cannot work: the console has no driver for Xbox 360 USB devices.
- Then send the `[portal]` lines of `Arkeyan.log` (section 7): they list the devices the console shows the app,
  whether access was granted, and the first portal reports.

**Emulated portal**: put figure dumps (`.sky`, `.bin`, `.dmp`, 1 KB tag images of your own figures) into the app's
`LocalState\figures` folder (below), then put them on the portal with the Toy Box. The portal starts as you left it
(the figures and their slots, kept in `LocalState\portal.txt`); the first time it is empty. A real portal, when one
is used, replaces all of this.

**Toy Box** (the in-game figure menu, as on PC and Mac): **hold View (the two squares) and press Y** to open or close
it. The game gets no input while it is open. D-pad or left stick: move; A: choose; X: put on / take off the portal;
Y: the figure's sheet (level, gold, experience, upgrades... saved into its dump); B: back (closes it from the grid);
LB/RB: element filter; LT/RT: Skylanders / magic items / adventure packs; **Menu (the three lines): Clear Portal**,
every figure off at once (the button is also at the bottom, next to the hints). It lists the dumps of
`LocalState\figures` (the emulated portal also creates a new dump there for every character that has none) with
the portraits from your disc, extracted at the first opening into `LocalState\portraits` (a coloured disc with
initials when a toy has none). There is no keyboard, so nicknames cannot be typed.

## 7. Logs and saves

In Device Portal, *File explorer* > *LocalAppData* > `SSARecomp.SpyrosAdventure_<id>` > *LocalState*:
- `Arkeyan.log`: everything the runtime prints (rewritten at each start). Download it to report a problem.
- `launcher.ini`: the settings (above).
- `saves\`: the saves (as on PC).
- `portal.txt`: the figures on the emulated portal when the game was last left (one `<slot> <path>` line each).
- `figures\`: figure dumps for the emulated portal (created at the first start; upload files into it).
- `portraits\`: the Toy Box's portraits, extracted from the disc image at its first opening (a cache: deletable).

## 8. When something goes wrong

- *The app closes at once*: look at `Arkeyan.log`. `no disc image` means the package has no `game.iso`;
  `cannot commit 4096 MB up front` is only a note (the console's memory budget is smaller than the guest's address
  space; memory is then committed as the game uses it).
- *The install fails for lack of space*: free space on the console (uninstall other Dev Mode apps) or in Dev Home's
  storage settings.
- *A black screen with sound*: the app type is probably still *App* (Xbox One apps have no D3D12): set it to *Game*.
- *The real portal is not used* (`[portal] emulated portal` in the log): look at the `[portal]` lines above it.
  `no HID portal ... visible` and no `game controller` line: the console does not show the portal to the app (try
  another USB port, or another portal kind). `access denied`: the console refuses that device. `GIP message 21 ...
  failed`: the Xbox One portal is seen but refuses the reports. Set `mode=emulated` to stop looking for one.
- *A crash* (`Fault (exception ...)` in the log): each backtrace line is either `0x14xxxxxxx (executable +0x...)`, an
  address in `SSARecomp.exe` at the linker's base, or `<address> <dll>+0x<offset>` for a system module. Symbolize the
  executable's lines with the very `SSARecomp.exe` that ran:
  `private/xbox/toolchain/llvm-mingw/bin/llvm-symbolizer --obj="build-uwp/ArkeyanRuntime/Skylanders Spyro's Adventure.exe" 0x14xxxxxxx ...`

## Files

| File | What it is |
|------|------------|
| `setup_tools.sh` | fetches and builds the tools into `private/xbox/` |
| `package.sh` | makes, signs and checks the `.msix` (what the launcher's "Export for Xbox" will run) |
| `AppxManifest.xml.in` | package manifest template (identity, Xbox and Windows device families, logos, the HID portal capability) |
| `assets/` | placeholder logos (`make_placeholder_logos.py`); no game artwork |
| `check_uwp_imports.sh` | checks that an executable only uses functions of the UWP API surface |
| `tools/xbox/patches/makemsix-store-iso.patch` | makes makemsix store `.iso` files instead of deflating them |
| `testapp/` | a small probe app (D3D12, gamepad, memory, files) used while bringing the platform up |
| `presenttest/` | present experiments, one colour each, in their own small package (`SSARecomp.PresentTest`): which way of presenting reaches the screen |
