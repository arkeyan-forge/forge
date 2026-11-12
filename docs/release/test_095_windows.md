# 0.9.5 on a Windows PC with a Wii / PS3 / PC portal: the owner's checklist

> **Purpose:** the test the owner runs on his own PC before 0.9.5 ships: the portal test, a game with real figures, the logs · **Status:** current (2026-11-10) · **Tags:** none

Why: issue #19 (three Wii-family portals on Windows, every one "No portal found" in 0.9.4) and issue #20 (Giants'
"Please put a Skylander" loop on a Wii Swap Force portal, logs that could not be told apart) were fixed without a Windows
PC or that portal here. What changed: [portal.md](../runtime/host/portal.md) ("Mixed portal", "Real portals on Windows:
the driver"), the launcher's `docs/support.md` ("The last run").

## What to put on the PC (nothing to install)
Built from the 0.9.5 branches, both in one folder (say `Desktop\Arkeyan095\`):
- `ArkeyanLauncher.exe`: the `windows` artifact of the launcher's GitHub Actions run of `feat/095` (each push builds it);
- `skylanders-kit-windows-x64\`: `tools/make_kit.sh windows` here, the zip unpacked. A kit beside the launcher is the one
  it uses (the launcher's `docs/install.md`, "The kit"), so INSTALL builds 0.9.5 games from it.

## The checklist (about 20 minutes)
1. **Device Manager**, the portal plugged in: under *Human Interface Devices* it is a *USB Input Device*; its Properties >
   Driver > Driver Details show `HidUsb.sys`. Write down its name and driver.
2. **Test the portal**: start `ArkeyanLauncher.exe`, Settings > Controls > Portal: *Real portal*, then **Test**. Expected:
   a *Found* chip with the portal (`Spyro Porta` or `Spyro Portal`, HID 1430:0150) and *Real portal: the game will use it*.
   Press **Copy** and keep the text.
3. **A game with real figures**: INSTALL Spyro's Adventure (or Giants) if needed, then PLAY with one figure on the portal.
   The game must take it; lift it, put another one: the game must take that one too. In Giants, at the first *Please put a
   Skylander on the Portal of Power*, put a figure: the game must go on to play (0.9.4 asked again and again).
4. **The logs**: quit the game. Settings > Support: the *Game log* row says *Last run: <the game>, Today <time>*. Press
   **Copy game log**: the text has one `===== Game log: <the game>, last written ... =====` heading, and its first log line
   ends with `started <date> <time> (local time)`.
5. **Report a problem** > Save to Desktop.

In the copied log, the portal lines: `[portal] windows: USB 1430:0150, driver HidUsb`, `[portal] hid: opened 1430:0150 ...`,
`[portal] model: <id> <model>; <game>: works`. A line `[portal] windows: 1430:0150: it uses the ... driver` means a
driver tool moved the portal off Windows' own driver: the launcher's Test says how to give it back.

## The Xbox One portal (Trap Team's, `1430:09AA`), on Windows and on the Mac
- **Windows**: plugged in, Settings > Controls > Portal > **Test**. Expected: a `usb 1430:09aa "Xbox One portal"` line
  with *Arkeyan talks to Xbox One portals on Windows from the next version* (it is not used in 0.9.5), and lines
  `windows device: USB\VID_1430&PID_09AA\... "Xbox Gaming Device", driver <service>`. Copy the text: the driver
  service is what 0.9.6 needs.
- **Sound**: start Trap Team with the speakers or headphones playing, then plug the Xbox One portal in. The sound must
  stay where it was. The game log has `[audio] output: "<your speakers>"` at the start, and after the plug
  `[audio] skipped "<the portal's output>", the new default: a portal's speaker; still "<your speakers>"` (and `[audio] output now: ...` only if the default changed
  to another real output). The launcher's music behaves the same.
- **Mac**: the same portal, Test: expected `usb 1430:09aa "Xbox One portal (experimental)": opened, ok`; then Trap Team
  with a figure on it. In the log: `[portal] Xbox One USB (experimental) portal 1430:09AA connected (interface ...)`,
  `[portal] usb: first write accepted ...` and `[portal] usb: first report ...`; the figure must appear in the game.
  If it does not, the log's `[portal] usb:` lines are what we need.

## The title logo (Trap Team: the white glow on NVIDIA)
Build 80b0488d's dump (every draw at 40 s) showed the cause: on NVIDIA every cube map read 0, so the logo's metal rim,
lit mostly by its cube-map reflection, came out at half the Mac's and the glow kept nothing of it. This build binds each
cube map through a cube view on every GPU ([plume_backends.md](../runtime/gpu/plume_backends.md), "A texture's default
view follows the resource"). On the NVIDIA GPU: a shot of the title with D3D12 and one with Vulkan (F3 > Graphics API,
next start); is the white glow around the logo back? Reflections may also look brighter in other games' levels on NVIDIA:
a shot of one level of any game is welcome. If the glow is still missing, the same dump as before, in a Command Prompt
in the folder of `Skylanders Trap Team.exe` (Manage > Open folder), left on the title until 70 s, then quit; send the
game log and the `ttdump` folder:
```
set SKY_DUMP_FRAME=40
set SKY_DUMP_FLOATS=1
set SKY_DUMP_DRAWS=1
set SKY_DUMP_ALPHA=1
set SKY_DUMP_PREFIX=%USERPROFILE%\Desktop\ttdump\d
"Skylanders Trap Team.exe" "<the Trap Team disc image>.iso"
```
We check the lighting's output on the rim (`d_135d`, where `d_87d` is red): about 134 on the Mac with this build
(`private/h095/cls.py`; 77 on the PC before)
([status.md](../../games/trapteam/docs/status.md#the-title-logos-black-outline-on-a-windows-pc-2026-11-10)).

## What to send back
For each step: passed or what was seen instead. Then the Copy of step 2, the game log of step 4, the report file of step 5,
and the name and driver of step 1. Nothing else is needed; the report is already cleaned of names and folders.
