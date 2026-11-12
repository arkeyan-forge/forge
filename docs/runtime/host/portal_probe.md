# `"Skylanders Spyro's Adventure" --portal-probe`: is a real portal reachable?

> **Purpose:** what does `--portal-probe` print (the JSON contract for "Test the portal")? · **Status:** current (2026-11-10) · **Tags:** [Xbox]

For the launcher's "Test the portal" button. The runtime looks for real Portals of Power on every transport it has,
tries to open and query each one, prints the result as JSON lines on **stdout**, and exits with **0**. It needs no disc
image, opens no window, reads no settings that matter and writes no log file. It takes about 1 s per portal found and write
method (each query waits up to 1 s for an answer), and well under 1 s with none.

Code: `PortalProbeMain()` in `runtime/host/portal.cpp`, `ProbeHidPortals()` in `host/portal_hid.cpp`,
`ProbeUsbPortals()` in `host/portal_usb.cpp`.

## For a person: the block on stderr

Before the JSON lines, stderr gets a short block, made to be copied into a bug report. For a HID portal it shows each
way of sending a command (on Windows `output-report`, `HidD_SetOutputReport`: SET_REPORT over the control pipe, then
`hid-write`, `SDL_hid_write`: WriteFile, the interrupt OUT endpoint; on macOS `usb-control`, a USB control SET_REPORT to the
device, then `hid-write`, [portal.md](portal.md#backends-xbox); on Linux `hid-write`, then `usb-control` through libusb,
which takes the portal's interface from the kernel until the probe closes it), and what the portal
answered to a reset (`R`) and a status query (`S`), the first bytes of each reply:

```
portal probe: 1 portal(s) found
  hid 1430:0150 "Spyro Porta": opened, interface 0, usage page FF00, output report 33 bytes
    output-report  R: ok, 'R' 02 18 00 00 00 00 00 00 in 9 ms; S: ok, 'S' 00 00 00 00 01 00 00 00 in 6 ms
    hid-write      not needed (output-report works)
    => ok, the game writes with output-report
```

On Windows, once a method answers, `hid-write` is not tried (`not needed (<method> works)`): a portal that doesn't
take it leaves the write pending, and hidapi's close freed that write's OVERLAPPED while Windows could still complete
it, which overwrote the probe's strings (issue #41, a Wii Trap Team portal: `the game writes with output-\u0001...`).

(Illustrative bytes.) The game tries the methods in the same order and keeps the first that works. On Windows run it
from a terminal: `"Skylanders Spyro's Adventure.exe" --portal-probe` (or `> probe.txt 2>&1` to keep it).

## Output contract

One JSON object per line, in this order:

1. Zero or more **transport lines**, for a transport that cannot be used at all on the machine it runs on (macOS never
   prints the `usb` one: its Xbox portals go through IOKit; Windows only if `winusb.dll` cannot be loaded, never a libusb
   line, [portal.md](portal.md)). Linux, 0.9.6 (the launcher words it for players; `SKY_LIBUSB` is only in the game log):

   ```
   {"transport":"usb","available":false,"status":"error: libusb-1.0 is not installed (install the libusb-1.0 package)"}
   ```

2. Zero or more **portal lines**, one per portal found:

   ```
   {"kind":"hid","vid":"1430","pid":"0150","product":"Spyro Porta","opened":true,"status":"ok"}
   {"kind":"usb","vid":"1430","pid":"1f17","product":"Xbox 360 portal","opened":false,"status":"error: cannot open (LIBUSB_ERROR_NOT_SUPPORTED)"}
   ```

3. Exactly one **final line**, always last, even when nothing was found:

   ```
   {"done":true,"count":1}
   ```

Fields:

| Field | Type | Meaning |
| --- | --- | --- |
| `kind` | string | `"hid"`: Wii, Wii U, PS3, PS4 and PC portals (USB HID, 1430:0150, through SDL's hidapi). `"usb"`: Xbox 360 portals (1430:1F17, 24C6:FA00, or an Activision device with an Xbox 360 interface) and Xbox One portals (1430:09AA, 1430:09AB, or an Activision device with a GIP interface), through libusb-1.0 (IOKit on macOS); on Windows also a portal on a driver Arkeyan cannot reach, with the reason. A line `{"note": "..."}` (Windows) names each device that may be a portal (portal.md, "Xbox portals by what they are"). |
| `vid`, `pid` | string | USB vendor and product id, 4 lowercase hex digits. |
| `product` | string | The device's product string (HID), or the model (`"Xbox 360 portal"`, `"Xbox One portal (experimental)"`, `"Activision device"`). May be empty. |
| `opened` | bool | The runtime could open the device. |
| `write` | string | HID only: the write method that got an answer to the reset (`"output-report"`, `"usb-control"` or `"hid-write"`), empty if none. |
| `methods` | object | HID only: for each method tried, `"R: <result>; S: <result>"` (`ok, <reply bytes> in <n> ms` or `error: ...`). |
| `details` | string | USB only (0.9.5): what the device is, for a bug report: every interface and alternate setting with its endpoints (`"interface 0 FF/5D/01: 81 interrupt 32 bytes, 02 interrupt 32 bytes"`), on Linux `; kernel drivers: 1-3:1.0 xpad` (the driver each interface had, from sysfs), on Windows `driver WinUSB`. From 0.9.6 the steps follow, each after `; `: an Xbox 360 portal's start request and its reply (`start request C1 01 0100: zeros (20 bytes)`), then the first packets each way (`out 1: 0B 14 52 (32 bytes, zeros after)`, `in 1: 0B 14 52 01 28 (32 bytes, zeros after)`); an Xbox One portal on Windows' own driver: `driver xboxgip, reached through Windows.Gaming.Input.Custom; handed over by Windows ...; out: 52 ...; in: 52 ...`. The launcher's Copy puts the first part on a `device:` line and each step on its own line. |
| `id`, `model`, `verdict`, `verdict_reason` | string | Only when the portal answered the reset: its id as `"01 28"`, the model's name from the table of ids (`"unknown portal"` if it is not in it), what Spyro's Adventure makes of it (`"works"`, `"warns"`, `"rejected"` or `"unknown"`) and why. `docs/runtime/host/portal.md`. |
| `status` | string | `"ok"`: opened and it answered a reset command (Xbox One portals: opened and claimed; they are not queried). An Xbox 360 portal gets 1 s for its `R` answer (after the 2-byte header; at the start or after one byte also counts); reports of another kind are skipped. Since 0.9.5 nothing else counts as an answer (0.9.4 took any packet, and silence too: a portal that never answers showed "Real portal", issue #20): `no 'R' answer within 1 s (nothing came back)` or `(<n> other packets, the first: 0B 14 53 ...)`. Otherwise `"error: "` and a reason: `cannot open (...)`, `write failed (...)`, `read failed (...)`, `no answer within 1 s`, `cannot claim interface (...)`, `no portal interface`. Windows (0.9.5): a portal on a driver Arkeyan cannot use is a `usb` line, `opened` false, with the reason (`it uses the WinUSB driver, not Windows' own HID driver ...`, `an Xbox portal (driver xusb22): ... libusb-1.0, which Windows does not have`, `no driver is installed for it`; [portal.md](portal.md#real-portals-on-windows-the-driver)). On Linux an access error adds `; Linux needs the portal's udev rule: ` and the rule (the kit's `README.txt`). |
| `transport` | string | `"hid"` or `"usb"`, on transport lines only. |
| `available` | bool | Always `false` on transport lines (a usable transport prints no line). |
| `done`, `count` | bool, number | Final line: `count` is the number of portal lines. |

The launcher should parse each line on its own, use the lines that have `kind` (portals), show transport lines as a
note, stop at `done`, and ignore keys it does not know: later versions may add keys, never remove or rename these.
Strings are JSON-escaped (UTF-8). A portal is usable by the game when its `status` is `"ok"`. With no portal line, the
message is "no portal found". The exit code is 0 whatever was found (non-zero only if the runtime itself failed to
start, e.g. an old runtime without this option exits 2 with "unknown option --portal-probe" on stderr).

Do not run the probe while the game is running: the game holds the portal (on Windows a HID device can be opened
twice, but both would read its reports).

## Example

```
$ "Skylanders Spyro's Adventure" --portal-probe
{"transport":"usb","available":false,"status":"error: libusb-1.0 is not installed (install the libusb-1.0 package)"}
{"done":true,"count":0}
```

## The game's own log

In play, the portal's steps are in `Arkeyan.log` (docs/settings.md), lines starting with `[portal]`:
`[portal] mode auto`, `[portal] hid: enumerated 1430:0150 "..." usage page FF00 ...` (or `none found`),
`[portal] hid: opened ... (interface 0, output report 33 bytes)` (or `cannot open ...`), `[portal] USB HID portal`,
`[portal] USB HID: connected`, `[portal] hid: wrote 'R' with output-report (33-byte output report)`,
`[portal] hid: read the first report, 'R' ...`. A failing portal logs `[portal] hid: write of 'R' failed (...)` and
`[portal] USB HID: lost (...); trying again after 2 s, then up to 10 s apart` once, and stays quiet until a write works
(`[portal] USB HID: working again`).
`[portal] mode` is `SKY_PORTAL`, which Arkeyan Launcher sets from `[portal] mode` of `launcher.ini` (`auto`, `emulated`,
`hid`, `usb`; the runtime also takes `real` and `none`).
