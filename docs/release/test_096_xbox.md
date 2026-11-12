# 0.9.6 with an Xbox portal: the owner's checklist (macOS, Windows)

> **Purpose:** the test the owner runs with his Trap Team Xbox One portal on a Mac and a Windows PC (and an Xbox 360 portal if one is at hand) before 0.9.6 ships · **Status:** current (2026-11-11) · **Tags:** [Xbox]

Why: no Xbox portal has run with 0.9.6. What changed: [portal.md](../runtime/host/portal.md) ("An Xbox 360 portal that
takes commands and never answers", "Xbox portals on Windows without libusb"). Never put a figure you care about on the
portal while testing writes: reading is enough here.

## What to put on each computer
Built from the 0.9.6 Xbox portal work (runtime and launcher): the launcher, and a kit made here with
`tools/make_kit.sh <platform>` beside it (the launcher uses a kit beside it), so INSTALL builds 0.9.6 games. Install Trap
Team (the portal's own game) and Spyro's Adventure.

## Mac, Trap Team Xbox One portal (about 10 minutes)
1. Plug the portal in. Launcher: Settings > Controls > Portal: *Real portal*, then **Test**. Expected: *Found*, "Xbox One
   portal (experimental) (USB 1430:09AA)" and *Real portal: the game will use it*. **Copy** and keep the text: its
   `device:` line gives the interfaces (a GIP one, `FF/47/D0`).
2. PLAY Trap Team with one figure on the portal: the game must take it; lift it and put another.
3. Settings > Support > **Copy game log**. Keep the `[portal]` lines: `[portal] usb: IOKit ...`, `[portal] usb: 1430:09AA
   (revision ...) interface ...`, `[portal] Xbox One USB (experimental) portal 1430:09AA connected (...)`, the `out 1:`
   ... `in 1:` packets.

## Windows PC, the same portal (about 15 minutes)
1. **Device Manager**: the portal is an "Xbox Gaming Device"; Properties > Driver > Driver Details: write down the driver
   (`xboxgip.sys` expected). Leave it on that driver.
2. **Test** as on the Mac. Expected: "Xbox One portal (experimental)" with *Found* and the model. Copy: the lines `driver
   xboxgip, reached through Windows.Gaming.Input.Custom`, `handed over by Windows`, `out: 52 ...`, `in: 52 ...`. If it says
   "Windows didn't hand the Xbox One portal to the game", keep the copy: that is the one unknown of this route.
3. PLAY Trap Team with a figure, the game in front (Windows stops the portal's input while the game is not). Then **Copy
   game log**. The lines that matter: `[portal] gip: Windows.Gaming.Input.Custom factory ... registered`, `[portal] gip:
   Windows hands over 1430:09AA (firmware ...)`, `[portal] Xbox One portal 1430:09AA connected (GIP, ...)`, `[portal] gip:
   out 1: ...`, `[portal] gip: in 1: ...`, and any `message 21 class N failed (0x...)` or `gip: message XX` line.
4. Only if step 2 failed: give the portal to WinUSB (Device Manager > Update driver > Browse my computer > Let me pick >
   Universal Serial Bus devices > WinUsb Device), replug it, Test again: the USB route then does the GIP framing itself
   (`[portal] usb: WinUSB ...`, `connected`, `out 1:`, `in 1:`). To undo: Device Manager > Uninstall device (tick "delete
   the driver"), replug.

## An Xbox 360 portal, if one is at hand (Mac, then Windows)
- **Mac**: Test, expected *Found* and the model (`01 28` for Spyro's Adventure's). Copy: `start request C1 01 0100: ...`,
  `out 1: 0B 14 52 ...`, `in 1: 0B 14 52 ...`.
- **Windows**: Test first on Windows' own driver: expected "This Xbox 360 portal needs Windows' WinUSB driver, set once: see
  the README" (the Copy names the driver). Then the WinUsb Device step above, replug, Test: *Found* and the model.
- In both, PLAY Spyro's Adventure with a figure, then Copy game log.

## Windows, a Wii/PS/PC portal on another driver (only if one was moved by Zadig, or to check the steps)
- **WinUSB**: Test: *Found*, "Portal of Power (HID 1430:0150)", `usb-control  R: ok, 'R' ...`, details `driver WinUSB,
  interface 0, reports from endpoint 81`; PLAY: the game log has `[portal] hid: 1430:0150 is on WinUSB (no HID device):
  interface 0 claimed ...` then `wrote 'R' with usb-control`.
- **libusbK or libusb0**: Test shows the steps and the two `pnputil` commands with the package's `oemNN.inf`; **Copy
  commands** copies them. Follow the steps: the portal must come back on HidUsb ("USB Input Device") and Test say *Found*.

## What to send back
The Test copies and the game logs of each step, and which steps passed. One log per failing step is enough to fix it.
