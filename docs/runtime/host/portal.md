# The portal: protocol, backends, light, identity, prompt watcher

> **Purpose:** how does the runtime talk to a real or emulated Portal of Power, which portal does it pick, and how does it notice the game asking for a figure? · **Status:** current (2026-11-11) · **Tags:** [Xbox] [Lux] [Alchemy] [SSA] [Giants] [SwapForce]

Code: `runtime/kernel/portal.cpp` (the xam ordinals), `runtime/host/portal*.cpp` (backends, identity, prompt),
`games/<id>/<id>_portal_prompt.*` and `games/<id>/adapter.cpp` (each game's prompt reader). The tag on a figure:
[figures.md](figures.md). The `--portal-probe` JSON contract: [portal_probe.md](portal_probe.md). What the title's
driver expects: [portal_title_side.md](../../engines/lux/portal_title_side.md). Platform notes:
[windows.md](../../getting-started/platforms/windows.md) (HID write method),
[xbox_uwp.md](../../getting-started/platforms/xbox_uwp.md) (console routes).

## Portal protocol [Lux] [V] (the title's disassembly and a run)

`GetRaw(DWORD* state, DWORD* len, BYTE* buf)` / `SetRaw(DWORD len, BYTE* buf)` on xam ordinals **0x4A1 / 0x4A2** (Xbox 360 reports start with a 2-byte header `0B 14` on commands, skipped by the title on input; removed/added by `kernel/portal.cpp`, so backends speak **bare protocol**: reports of at most 32 bytes, command letter first):
`R 00` reset/version (answer `(b1<<8|b2) >= 0x011F`; the emulated portal answers its game's Xbox 360 model, `01 28` for SSA and `01 48` for Giants: "The portal's identity" below), `A 00/01` activate (answer `act batt 77`; battery <= 164 = "too low"), `S` status (`LE32` 2 bits per slot: bit0 present, bit1 changed since the last report; counter; active), `Q (n<<4|slot) block` query, `W (1<<4|slot) block data16` write, `C r g b` portal light colour (no answer; see "Portal light"), `FA` (360 dongle). **An inactive portal must stay silent** (any `S` with active = 0 resets the protocol). Block 0 and sector trailers are never written by the emulated portal. Later games add `J L M V` (lights, speaker) [?]: log with `SKY_PORTAL_LOG=1` first; the emulated portal logs each unknown command letter once (`[portal] emulated portal ignores command 0x..`).

## The speaker [TrapTeam] [V] (2026-11-02)
A trap's voice uses the portal's speaker: `M 01` on, sound blocks, `M 00` off. **Trap Team reads no figure until `M` is answered**, and asks again without end: unanswered, a Skylander swapped while a trap is on the portal is never read and the game asks for a Skylander. The emulated portal answers `M <flag> 00 19`. Regression: `tools/regress/trapteam_swap.sh`.

- **The blocks, measured** (the tutorial with Wet Walter's trap, `SKY_PORTAL_LOG=1`): 32-byte `SetRaw` reports, header `0B 17` (commands
  use `0B 14`), **30 payload bytes, all audio**: two 4-bit G.721 codes per byte, **low nibble first**, 60 samples at 8 kHz per block.
  One line: `M 01`, 766 blocks over 5.75 s (133 a second, one every 7.5 ms, 2.2 to 16 ms apart), `M 00`. Silence is `FF` bytes; code 0
  never appears, as the recommendation's coder never sends it. The bytes would read as commands (`R` resets, `W` writes a figure), so
  `kernel/portal.cpp` takes every `0B 17` report out of the command path and hands it to `PortalSpeakerSound`.
- **Where it plays: never twice** (`ChooseSpeakerRoute`, `host/portal_speaker.*`). A real portal whose model has a speaker
  (`PortalModelHasSpeaker`: the Traptanium ids `02 18`, `02 1B`, `02 19`, `02 27`, from its answer to `R`) and whose backend takes sound
  (`PortalBackend::TakesSound`) plays it; any other portal (the emulated one, a Spyro's Adventure, Giants or Swap Force portal, an id not
  known yet) leaves it to the computer; the switch off sends it nowhere. Logged on each change: `[portal] speaker: the portal plays the
  sound` / `the computer plays the sound` / `off (Portal sound)`.
- **Each backend's format.** Xbox 360 USB: the console's own report, `0B 17` and the 30 bytes, on the OUT endpoint. HID (Wii, Wii U, PS):
  decoded by the backend and sent as 16-bit little-endian PCM, 32 samples (64 bytes) per `SDL_hid_write`, the format the HID Traptanium
  portal is said to take [?] (Dolphin and Cemu as leads); a portal that refuses a block (Windows' HID writes are the descriptor's 33 bytes)
  logs `[portal] hid: the portal refused a sound block (...)` and the computer plays the rest. Xbox One (GIP): its sound message is not
  known, so the computer plays it [?]. `AsyncPortal` queues the blocks with the commands, in order; a refused block never drops the portal.
- **The computer's stream** (`SpeakerStream`, one producer, one consumer, no allocation, no lock): the title's portal thread decodes
  each block into a ring of 8192 samples (`M 01` resets the decoder; `M 00` lets the queue play out; the trap leaving the portal, a toy
  type 210 to 220 gone from the status report, or the switch going off empties it at once); `HostAudioSubmit` (and the XAudio2 twin)
  adds it to both channels before the master volume, upsampled 8 to 48 kHz by linear interpolation. 40 ms are queued before a line
  starts; a producer more than half a second ahead skips forward. So the F3 volume, `SKY_AUDIO_DUMP` and the disk audio driver all see it.
- **Checked in the game's own mix** (2026-11-02, `SDL_AUDIO_DRIVER=disk` to a file, the swap regression): the line decodes to speech
  (lag-1 correlation 0.81, spectral flatness 0.08, 85 % of the energy in 300 Hz-1 kHz, voiced frames 0.71 periodic at about 275 Hz;
  the high-nibble-first order gives 0.50 and clips); the decoded line is found in the disk file at 89 standard deviations of the
  cross-correlation, at -27 dBFS under the game's -21 dBFS. Listen files are under `private/speaker/` (not in git).
- **The decoder** `runtime/host/portal_adpcm.{h,cpp}`: 4-bit ITU-T G.721 ADPCM to 16-bit mono PCM, our own code from the recommendation's
  description. Fixed against the real stream: the first pole coefficient adapted the wrong way round (+3/256 when p(k) and p(k-1)
  *agree*), and the speed control now reads the tone flag just updated. `ArkeyanPortalAdpcmTest`: a test-only encoder (analysis by
  synthesis) codes sines; the decoder gives them back at **48.4 dB (440 Hz), 37.1 dB (1 kHz), 39.8 dB (2.5 kHz)** (floor 34 dB; 33.5,
  27.6, 26.1 before the fix); silence stays within 16 LSB; blocks decode like the whole stream.
- **Tests** `ArkeyanPortalSpeakerTest`: the route for every model (emulated, real with and without a speaker, a backend refusing sound, an
  unknown id, the switch off); a vowel-like signal in 30-byte low-nibble-first blocks, paced like the game, comes out of the stream at
  48 kHz at 52 dB SNR; the prebuffer, `M 00` draining, a flush, skipping ahead, the ring's limit, mixing on top of the game.
- **With a real portal** [?] (not tried with hardware here): Trap Team on a portal without a speaker sends `M` to it; if that portal
  does not answer `M`, Trap Team reads no figure while a trap talks (the emulated portal answers it).

A **real portal without a speaker** (any but a Traptanium) is never sent `M`: `host/portal.cpp` answers it as the speaker
would (`M <flag> 00 19`, returned before the device's next report), since such a portal may not answer and Trap Team reads
no figure until it is [?] (not tried on such a portal yet).

## Portal light

Both games [SSA] [Giants]:

- **What the games send.** Only `C r g b` (3 colour bytes, then zeros up to the 32-byte report), nothing else beyond `R A S Q W` [V] (Giants from the title to Chapter 1 with a Giants figure, a LightCore, an SSA figure, sidekick, item and adventure pack, `SKY_PORTAL_LOG=1`; Spyro's Adventure sends it too). Both games send it all the time and **fade** the colour (Spyro's Adventure: 456 commands in 90 s, from the title to level_027; Giants: 2081 in 200 s, steps of 8 per channel from red through green to blue, black while nothing happens).
- **What lights a LightCore figure** [V] (the user, 2026-10-25): LightCore figures are ordinary figures that glow: an LED in the figure, powered by the portal. Nothing on the figure or in the game's commands drives it (a blank LightCore is a plain tag with variant `0x1206`, and only `C` was seen). **The Toy Box emulates LightCores fully**, like any figure; the lit dot on a LightCore's seat, in the portal's colour, is a cosmetic hint. No real hardware is needed to test them.
- **Emulated portal:** accepts `C` (any length, missing bytes are 0), keeps the last colour and the count (`EmulatedPortal::LightColor`, `LightCommands`). **Any portal:** `PortalWrite` records the colour (`PortalLight()`) and logs `[portal] light: #RRGGBB (on|off)` on the first command and each time it turns on or off.
- **A real portal** receives the commands unchanged (the HID and USB backends write every command they are given, `host/portal_hid.cpp`, `portal_usb.cpp`); nothing is filtered by game. [?] not tried with hardware here.
- **Toy Box:** a small swatch in the portal column's title, in the portal's colour in 4 steps per channel (dark while off), and a small dot on a LightCore's seat while the light is on. Code: `Model::lightRgb` (`Tick`), `DrawSeats`. The title's side: [portal_title_side.md](../../engines/lux/portal_title_side.md).

## Backends [Xbox]

Code: `host/portal_backend.h`, `PortalInit` (`host/portal.cpp`), one file per backend (`portal_emulated.cpp`,
`portal_hid.cpp`, `portal_usb.cpp`, `portal_async.cpp`). Chosen by `SKY_PORTAL` (logged as `[portal] mode <m>`); **the
default is `auto`** (`PortalInit`: `getenv("SKY_PORTAL")`, else `"auto"`). On UWP the settings set `SKY_PORTAL`
(`host/uwp/settings_uwp.cpp`).

| Mode | Backend | Notes |
|---|---|---|
| `emulated` (what `auto` falls back to when nothing is plugged in) | **EmulatedPortal**: 16 slots, serves figure dumps from the collection, writes the title's block writes back into the dump, status counter, **starts as the last session left it** (`portal.txt`) | `SKY_PORTAL_PLACE` = exactly these figures (tests) |
| `hid` | **HID**: Wii, Wii U, PS3, PS4, PC portals, USB HID **`1430:0150`** (every HID model SSA to Imaginators), 32-byte input reports on the interrupt IN endpoint, commands as 32-byte output reports (SET_REPORT), through SDL's hidapi; on Windows `HidD_SetOutputReport`, on macOS a USB control SET_REPORT (below), each with `SDL_hid_write` as fallback; on Windows a portal on WinUSB through `winusb.dll` (0.9.6, "Real portals on Windows: the driver") | usage page `FF00`: enumerate-only-controllers hint lifted for the portal |
| `usb` | Linux: **libusb-1.0** loaded at run time (LGPL, not linked; `SKY_LIBUSB`, `libusb-1.0.so.0`, `libusb-1.0.so`); Windows (0.9.6): **WinUSB** devices through `winusb.dll` and the Xbox One portal through **Windows.Gaming.Input.Custom** ("Xbox portals on Windows without libusb" below; libusb only with `SKY_LIBUSB`); macOS: **IOKit** (`host/portal_usb_mac.cpp`, nothing to install). Xbox 360 portal `1430:1F17` (also `24C6:FA00`, Xenia's list only; interrupt endpoints 0x81 in / 0x02 out, 32-byte reports with the 2-byte header, adapted from Xenia, BSD) and the Xbox One portal (GIP: framing `command, options, sequence (skips 0), LEB128 length, payload`; `1430:09AA` (Trap Team's, 2026-11-10), `1430:09AB` (Santroller's capture) and any other Activision device with a GIP interface, message `0x21` of 30 bytes; **experimental, untested on hardware**; any other `1430` device with an Xbox 360 interface counts too: "Xbox portals by what they are" below) | `SKY_PORTAL_USB=VVVV:PPPP`, `SKY_LIBUSB=<path>` |
| `auto` (**default**) | a real portal plugged in at launch (HID first, then USB), else emulated | |
| `real` | the first real portal found, kept even if unplugged; waits for one | |
| `none` | portal disabled | |
| `stall` | test: a fake real portal that stalls each write 1 s then fails | `ArkeyanPortalAsyncTest` |
| `fake-real` | test: a fake real portal holding the dumps of `SKY_PORTAL_FAKE_REAL`, behind the mixed portal | `ArkeyanPortalTest`, `toybox_window.sh` |

**Real portals run on `AsyncPortal`** (own thread, queues, reopen after failure waiting 2, 4, 8, 10 s): a blocking HID write on the title's portal thread froze the game ~1 s every ~2 s on Windows (Arkeyan Launcher issues #11 and #12; [platforms/windows.md](../../getting-started/platforms/windows.md)). On UWP the routes are GIP through `Windows.Gaming.Input.Custom` and HID through `HidDevice`; the Xbox 360 portal is impossible there ([platforms/xbox_uwp.md](../../getting-started/platforms/xbox_uwp.md)).
**macOS: the commands go as a USB control SET_REPORT** [V] (2026-11-08, a Spyro's Adventure Wii portal, `01 2C`). The HID
driver's output report (`SDL_hid_write`, which is `IOHIDDeviceSetReport`) never reached the portal: `0xE00002ED` ("device not
responding"), then `0xE0005000`, the portal lost, no figure. The same 32 bytes (the command, no report id byte) as a class request
on the default pipe (`bmRequestType 0x21`, `SET_REPORT 0x09`, `wValue 0x0200`, `wIndex` the interface) are answered, and the
answers keep arriving as HID input reports. `portal_hid.cpp` (`WriteMethod::ControlReport`, `usb-control`) finds the USB device
from SDL's path (`DevSrvsID:<registry id>` of the HID service, then up the IOService plane through `IOUSBHostInterface`, whose
`bInterfaceNumber` is the index, to `IOUSBHostDevice`) and sends every command with `IOUSBDeviceInterface182::DeviceRequestTO`
(500 ms timeouts); no `USBDeviceOpen` (class requests need none, so another program holding the portal is no bar), no new
dependency (IOKit was already linked). `SDL_hid_write` stays the fallback; the speaker's PCM still goes by `SDL_hid_write`
(interrupt OUT, untried on a Mac) [?]. Windows is unchanged; Linux got the same method later (below). Pure parts (the request, the path) in
`host/portal_hid_report.h`, tested by `ArkeyanPortalHidReportTest`. Verify: `--portal-probe` shows `usb-control  R: ok, 'R' ...`
and `=> ok, the game writes with usb-control`; a run logs `[portal] hid: wrote 'R' with usb-control`, then `[portal] model: ...`.

**Linux: the original Wii portal through a USB control SET_REPORT** (2026-11-10, issue #20) [?] (untested on hardware;
the calls and the bytes through a fake libusb). A player's Spyro's Adventure Wii portal (`1430:0150`, "Spyro Porta",
`/dev/hidraw2`) opened and sent its first status report, then refused the reset: `[portal] hid: write of 'R' failed
(hid-write: Protocol error)` (`EPROTO`: hidraw's write goes to the interrupt OUT endpoint, which this family stalls; the Wii
Swap Force portal takes it). hidraw has no other way to an output report (`HIDIOCSOUTPUT` goes the same way), so Linux
gets the macOS method through libusb, loaded as for the Xbox portals: `LinuxMethods` = `hid-write`, then `usb-control`
(`host/portal_hid_report.h`, `TryOrder`: the method that worked last first). The first `usb-control` command finds the
USB device behind the hidraw node from sysfs (`/sys/class/hidraw/hidrawN/device`, up to the interface's
`bInterfaceNumber` and the device's `busnum`/`devnum`; any `1430:0150` if that fails), opens it, reads the IN endpoint
from the configuration descriptor and claims the interface with libusb's auto-detach, which takes it from the kernel's
HID driver: the hidraw node goes away, so from then on the portal's reports are read by libusb from the interrupt IN
endpoint (`ControlLink::Read`, 1 ms at least: libusb's 0 is forever) and the speaker's PCM stays with the computer.
Closing releases the interface and libusb gives it back to the kernel. Commands: `21 09 0200 <interface>`, 32 bytes, 500
ms (`host/portal_hid_control.h`, `ControlLink`). Log: `[portal] hid: usb-control: interface 0 claimed through libusb
(...)`, then `[portal] hid: wrote 'R' with usb-control`. It needs `/dev/bus/usb` access: the README's `SUBSYSTEM=="usb",
ATTRS{idVendor}=="1430"` rule (an access error names it). Tests: `ArkeyanPortalHidReportTest` (the order, a fake sysfs, a
fake libusb: the claim, the bytes, the reads, the release, the failures; also built and run in the Linux container). To
verify with that portal: `--portal-probe` shows `hid-write R: error: ...` then `usb-control R: ok, 'R' ...` and `=> ok,
the game writes with usb-control`.

**macOS: the Xbox portals through IOKit** (2026-11-09) [?] (no Xbox portal here; the code paths, the log and the device list
were run). A Mac has no libusb ("USB (Xbox) portals can't be used: libusb-1.0 not found" before), so `portal_usb_mac.cpp`
answers the backend's libusb-shaped calls (`portal_usb::Api`, `host/portal_usb.h`) with IOKit's USB user client, no new
dependency: devices are the `IOUSBHostDevice` services (vendor and product from the registry), `IOUSBDeviceInterface182`
opens the device (`USBDeviceOpen`; another client's exclusive hold is not fatal: the interfaces may still open), reads the
configuration with `DeviceRequestTO` and sets the first one if the device has none; `IOUSBInterfaceInterface190` opens
the interface, whose pipes are mapped to endpoint addresses (`GetPipeProperties`: number, plus 0x80 for IN). IOKit has no
timeout on interrupt pipes (`ReadPipeTO` is for bulk pipes), so each claimed interface gets a thread that keeps a
`ReadPipe` of one packet (the pipe's `wMaxPacketSize`, 0.9.6) pending on its IN pipe and queues the packets (the newest 64); an IN transfer waits on that queue for its
timeout, an OUT transfer is a `WritePipe` (32 bytes, taken at once by a portal). A stall is cleared twice, then is an
error; closing aborts the pipes, which ends the pending read. IOKit's results map to libusb's codes
(`ErrorFromIoReturn`), so the backend's logic is the same everywhere. Log: `[portal] usb: IOKit (macOS, nothing to
install)`, then `[portal] Xbox 360 USB portal 1430:1F17 connected (interface 0, endpoints 81/02)` and
`[portal] usb: first report (32 bytes): 0B 14 ...`, or `cannot open (...)` / `cannot claim interface 0 (busy (another
program holds it))`. Tests: `ArkeyanPortalUsbTest` (descriptor parsing, the Xbox 360 framing, the error names, the udev
rule; on a Mac the IOKit device list). To verify with a portal: `--portal-probe` lists it with `"status":"ok"` and its
model, and a run logs the lines above.

**Linux: the Xbox portals need their own udev rule** (2026-11-09, issue #20) [V] (the rules, by `udevadm verify`).
libusb opens `/dev/bus/usb/*`, which belongs to root; the kit's rule only gave the HID portals' `/dev/hidraw*`. The kit's
`README.txt` now writes three lines into `/etc/udev/rules.d/70-skylanders-portal.rules`: the HID one, `SUBSYSTEM=="usb",
ATTRS{idVendor}=="1430", MODE="0660", TAG+="uaccess"` (Xbox 360 and Xbox One portals: every `1430` device, `1430:1F17` and
`1430:09AA` included) and the same for `24c6:fa00`. A
device libusb cannot open was silent in the game (only "no Xbox 360 or Xbox One portal found"): it now logs `[portal] USB
portal 1430:1F17: cannot open (LIBUSB_ERROR_ACCESS)` and, for an access error, `[portal] usb: Linux needs this udev rule
...` with the device's rule; the probe's status carries the same rule. The game also logs where libusb came from
(`[portal] usb: libusb-1.0.so.0`), its first write failure of a connection and its first report. Why the player's
launcher said "Real portal: the game will use it" while the game did not use it [?]: Arkeyan Launcher's verdict
(`PortalVerdict`, `support_page.cpp`) trusts `opened`, which the probe sets as soon as libusb opened the device; a claim
or a reset without an answer leaves `opened` true with an `"error: ..."` status, which the launcher hides. The contract
says a portal is usable when its `status` is `"ok"` ([portal_probe.md](portal_probe.md)): the launcher should judge on that
(not changed here: the launcher's repository). The player's log with the lines above tells which step fails.

`"Skylanders Spyro's Adventure" --portal-probe` (`"Skylanders Giants"` for Giants) prints one JSON line per real portal found; the contract is
[portal_probe.md](portal_probe.md). Arkeyan Launcher's "Test the portal" uses it.

## An Xbox 360 portal that takes commands and never answers

Issue #20 (2026-11-10), a player's Linux PC, two Xbox 360 portals (`1430:1F17`, Spyro's Adventure's and Giants'): the game
logged `connected (interface 0, endpoints 81/02)`, no write failure, and never `usb: first report`: the portal took every
command and nothing came back. 0.9.5 added the probe's `R` check and the log lines; 0.9.6 found the likely cause [?]
(unit tests [V], the hardware not yet).

**The cause (0.9.6): every IN transfer asked for 64 bytes of an endpoint whose packets are 32 bytes.** A USB transfer
ends on a short packet or when its buffer is full (USB 2.0 specification, "short packet"; libusb's documentation,
"Packets and overflows"). The portal's reports are full 32-byte packets, so a 64-byte transfer waited for a second one,
and at its 100 ms timeout libusb returned `LIBUSB_ERROR_TIMEOUT` with the first packet in `transferred`, which the read
loop (and the probe) dropped as "nothing yet". Two packets inside 100 ms came back as one 64-byte "report", the second
one lost. The macOS IOKit reader had the same 64-byte `ReadPipe` (two reports joined, never a timeout). Now:
- `FindInterface` keeps each endpoint's `wMaxPacketSize` (`Interface::inSize`, `outSize`); every IN transfer asks for one
  packet (`ReadLength`) and a timeout that carries data counts (`Received`); `ReadPacket` (`host/portal_usb.h`) does
  both, for the backend, the probe and every platform (the IOKit reader reads `GetPipeProperties`' packet size).
- `ArkeyanPortalUsbTest` has a fake portal whose IN endpoint behaves as USB does: the 64-byte read of 0.9.5 times out
  holding the answer, `ReadPacket` gets it. `ArkeyanPortalUsbBackendTest` runs the whole backend (`host/portal_usb.cpp`)
  over that fake: found, claimed, the start request, `R` framed `0B 14 52 00...` (32 bytes), the answer handed to the
  title as `R 01 28` (header removed), then the status, one 32-byte transfer each, the speaker's `0B 17` blocks, the
  release. Both also pass on Linux (arm64 container) and under Wine (`build-win64`).

**What the public notes say, and what the code does with each** (facts only, no code read or copied):
- Endpoints and framing: interrupt IN `0x81`, OUT `0x02`; commands and answers are 32 bytes, zero-padded, with `0B 14`
  before the command letter (skylandersnfc, "Skylanders Portal Demystified"; the Xenia Canary pull request #581
  discussion: "the 360 portals prepend all the packets with a header", the speaker's sound on the same endpoint under
  another header, G.721 samples). Ours matches (`MakeXbox360Packet`; the title's own header, `kernel/portal.cpp`).
- No console handshake: the portal has the console's security chip ("Infineon security chip (method 3, version 1.00)",
  Demystified), but a PC program talks to it without it: Xenia Canary drives the portal through WinUSB (Zadig) and the
  games "see this portal correctly" (#581); "there's no drm on most of the portals" (same discussion). So no
  authentication is attempted.
- Linux's `xpad` driver (`drivers/input/joystick/xpad.c`) binds every vendor `1430` interface of class `FF/5D/01` (or
  `81`) as a gamepad, so it takes the portal's interface 0 at plug-in and sends it an LED packet (`01 03 0N`); libusb's
  auto-detach takes the interface back at the claim (the log lists the drivers before and after the claim). When xpad
  starts reading such a device it makes a vendor request (IN, to the interface: `C1 01 0100 0000`, 20 bytes), because
  "some third-party controllers require this message to finish initialization". The portal is such a device: the backend
  and the probe make the same request once after the claim (`StartXbox360`); a stall is only logged [?] (whether the
  portal needs it is unknown).
- SDL's gamepad code does the same through libusb (its HIDAPI backend claims vendor `1430` interfaces of class
  `FF/5D/01` and `FF/47/D0`): it held interface 0 (`usbfs` in the probe's driver list, "cannot claim interface
  (LIBUSB_ERROR_BUSY)") and listed the portal as a second gamepad (#82). The runtime and the launcher set
  `SDL_HIDAPI_IGNORE_DEVICES` and `SDL_JOYSTICK_BLACKLIST_DEVICES` to the Xbox portals' ids before SDL starts.
- The second interface: none is documented; `FindInterface` takes the Xbox 360 vendor interface (`FF/5D`) whatever its
  number, and the log describes every interface and alternate setting, so a player's log shows any other.

**Issue #23's data (SteamOS, 0.9.5), checked against the fix** [V] (by a fake portal replaying it; the hardware not yet).
Test said `no 'R' answer within 1 s (1 other packets, the first: 0B 14 53 00 00 00 00 E5 01 ...)`: a status report (`S`,
no figure, counter `E5`, active). 0.9.5's probe read 64 bytes, and a transfer only ends early on a short packet, so that
read that ended in success held two full 32-byte packets: the status, then most likely the answer, judged by the first
half only. The next read then waited out the second. The game's own log of the same session (64-byte reads with 100 ms
timeouts) shows nothing for 5 s: the portal sends no steady status (fewer than two packets in any 100 ms), each answer
alone, each dropped in a timeout. So the portal does talk and answers `R`; no other interface or delay is needed for it
(the answer came within the probe's second, on interface 0 alone). With one packet per read, `QueryXbox360Reset`
(`host/portal_usb.h`, the probe) passes over the status and matches the answer after it. The descriptor: interface 0
`FF/5D/01` (81, 02), 1 `FF/5D/03` (83, 04, 85, 05), 2 `FF/5D/02` (86), 3 `FF/FD/13`, all interrupt 32 bytes: the
layout of a wired Xbox 360 controller (gamepad, headset, plug-in, security), the portal's data on interface 0 as
documented. Linux's `xpad` held interface 0 only (`5-1:1.0 xpad, others none`); the claim with libusb's auto-detach
succeeded (the game logged `connected`), so the detach works; the probe now also lists the drivers after the claim.
Tests: `ArkeyanPortalUsbTest` replays it (the status waiting, `R` answered alone: the 64-byte read gives the player's
exact line, `QueryXbox360Reset` gives `ok` with `in 1: 0B 14 53 ...` then `in 2: 0B 14 52 ...`, each with its time after
the write; a portal that only sends the status gives the 0.9.5 message), `ArkeyanPortalUsbBackendTest` the same through
the game's backend (the title gets the status, then the `R` answer).

**The log, per connection** (also on macOS and Windows): `[portal] usb: 1430:1F17 (revision 0100) interface 0 FF/5D/01:
81 interrupt 32 bytes, 02 interrupt 32 bytes`; on Linux `[portal] usb: kernel drivers before the claim: 1-3:1.0 xpad` and
`... after the claim: 1-3:1.0 usbfs`; `[portal] Xbox 360 USB portal 1430:1F17 connected (interface 0, endpoints 81/02,
packets 32/32 bytes)`; `[portal] usb: start request C1 01 0100: <reply>` (or `stalled (the portal does not need it, or
refused it)`); then the first 8 packets each way, `[portal] usb: out 1: 0B 14 52 (32 bytes, zeros after)`, `[portal] usb:
in 1: 0B 14 52 01 28 (32 bytes, zeros after)` (all of them with `SKY_PORTAL_LOG=1`); and once, if the portal took
commands for 5 s and sent nothing, `[portal] usb: nothing came from endpoint 81 in 5 s ...`. The probe's `details` carry
the same steps and its first packets ([portal_probe.md](portal_probe.md)). One log from a player with the portal tells
which step is still wrong.

## Real portals on Windows: the driver

Issue #19 (2026-11-10): three Wii-family portals (Swap Force `0000547`, Trap Team, SuperChargers; Windows names them all
"Spyro Porta") showed "No portal found". The likely cause [V]: 0.9.4's launcher lost every portal that answered (its
reader stopped at the comma inside `"R: ok, 'R' ..."`; fixed in the launcher, with the runtime's real lines as a test). The
other possibility [?]: a driver tool (Zadig, as some emulators ask) bound the portals to WinUSB or libusbK, so Windows no
longer shows them as HID devices. 0.9.5 lists, read only (CfgMgr32: `CM_Get_Device_ID_List`, `CM_DRP_SERVICE`; no new
library), every present USB node of vendor 1430 and `24C6:FA00` with its driver service:
`[portal] windows: USB 1430:0150, driver HidUsb` in the game's log (when the USB transport starts, after the HID one found
nothing, or always in `real` mode), and `WindowsDriverProblem` (`host/portal_usb.h`) says what is wrong:
`1430:0150` on another driver than `HidUsb` ("uninstall the device and its driver in Device Manager, then plug the portal
in again": Windows then binds its own HID driver), no driver at all, an Xbox portal without `libusb-1.0.dll`, or one whose
driver libusb cannot open (it opens WinUSB, libusbK and libusb0 devices). The probe adds those portals as `usb` lines with
the reason; the launcher says it in its own words. Tests: `ArkeyanPortalUsbTest` (the device ids, each driver's answer).
A cross build of `host/portal_usb.cpp` with llvm-mingw compiles; it has not run on a Windows PC
([test_095_windows.md](../../release/test_095_windows.md)).

**0.9.6: a HID portal on WinUSB is used; on libusbK or libusb0, the way back** [?] (unit tests and the cross build; no
such portal on a PC yet). A Windows player's Trap Team Wii portal (`1430:0150`) had been moved to libusb0 by Zadig; after
"uninstall the device and its driver" Windows put libusb0 back at the replug (the driver package stays in the driver
store). Now (`WindowsDriverProblem`, `host/portal_usb.h`):
- **WinUSB**: no problem. When hidapi finds no portal, the HID backend (`host/portal_hid.cpp`, Windows) claims a
  `1430:0150` on WinUSB through `winusb.dll` (`portal_usb::LoadWinusb`, the Xbox portals' calls) and the same
  `portal_hid::ControlLink` as Linux's usb-control (`host/portal_hid_control.h`, built with `winusb`: "WinUSB: ..." in
  its errors, no udev rule): commands as `SET_REPORT` (`21 09 0200 <interface>`, 32 bytes) on the control pipe, reports
  from the interrupt IN endpoint, one packet per read (its `wMaxPacketSize`, also on Linux now); the speaker's sound
  stays with the computer. Log: `[portal] hid: 1430:0150 is on WinUSB (no HID device): interface 0 claimed, commands as
  SET_REPORT, reports from endpoint 81`, then `[portal] hid: wrote 'R' with usb-control`. The probe adds it as a `hid`
  line (`driver WinUSB, ...`, `usb-control  R: ... S: ...`).
- **libusbK, libusb0 or any other driver** (Arkeyan ships neither library): the status gives the way back
  (`WindowsDriverRemoval`): Device Manager > Uninstall device with "Attempt to remove the driver for this device", replug;
  if it is back, in an administrator Command Prompt `pnputil /enum-drivers`, then `pnputil /delete-driver <inf>
  /uninstall /force` (Windows 10 and later), replug; or WinUSB in Zadig. `<inf>` is the package's published name, read
  only from the device's driver key (`CM_Open_DevNode_Key`, `CM_REGISTRY_SOFTWARE`, value `InfPath`), offered only when it
  is a third-party `oem*.inf`; else `oemNN.inf`, found in the list. The Xbox One portal on those drivers gets the same steps
  (back to Windows' Xbox driver); the Xbox 360 one the WinUSB step. The launcher shows the steps one per line, the two
  commands, and **Copy commands** (its `PortalStatusText`, `PortalDriverCommands`).
- **No libusb on Windows in any text**: the probe's transport line is `winusb.dll could not be loaded` there (never seen:
  it ships with Windows), the statuses no longer name `SKY_LIBUSB`, and the launcher drops an older runtime's libusb line
  on Windows and macOS. On Linux the line is `libusb-1.0 is not installed (install the libusb-1.0 package)` (the launcher:
  "Xbox portals need libusb-1.0: install your system's libusb-1.0 package, then test again."); `SKY_LIBUSB` only in the log.
- Tests: `ArkeyanPortalUsbTest` (each driver's text, the inf or not, no `SKY_LIBUSB`), `ArkeyanPortalHidReportTest` (the
  WinUSB link's errors, one-packet reads, the packet size), the launcher's shell test (the steps, the commands, the
  WinUSB portal's verdict, the transport line per platform).

### Xbox portals by what they are; the Xbox One portal on Windows (2026-11-10)
- **Ids** [V] (`portal_usb::KnownXboxPortal`, `ArkeyanPortalUsbTest`): Xbox 360 `1430:1F17` (Spyro's Adventure to Trap Team),
  Xbox One `1430:09AA` (a Trap Team Xbox One portal; Device Manager: `USB\VID_1430&PID_09AA&REV_0227`, "Xbox Gaming
  Device") and `1430:09AB` (Santroller's capture: GIP message `0x21`, 30 bytes, the framing `host/portal_usb.cpp` uses);
  `24C6:FA00` is only in Xenia's table of Xbox 360 portals (kept, never seen). Any other `1430` device (not `0150`) is a
  portal when its interface is the Xbox 360 vendor one (class `FF`, subclass `5D`: Xbox 360 framing) or GIP (`FF 47 D0`:
  Xbox One framing); else the log names it once, `[portal] usb: 1430:XXXX is no portal we know: <interfaces>`, so the next
  player's log gives its id. macOS (IOKit) and Linux (libusb) open `1430:09AA` in 0.9.5 [?] (no such portal here).
- **Every device that may be a portal** (Windows, `host/windows_devices.h`): the probe's `note` lines (the launcher's Test
  shows them) and the game log (`[portal] windows device: ...`, when the USB transport starts) list every present device
  with a `1430`, `24C6` or `0E6F` id, or "Portal", "Skylander", "Spyro", "Traptanium" in its name, and every device under
  one (its HID or GIP child, its audio output), with the driver service and hardware ids.
- **The Xbox One portal on Windows waited for 0.9.6** (the owner's decision, 2026-11-10): below.
- **Its speaker is an audio device** (Windows makes it the default output when it is plugged in): the game and the
  launcher's music never play there ([audio_xma_xaudio.md](../xbox360/audio_xma_xaudio.md), "The output device").

### Xbox portals on Windows without libusb (0.9.6) [?]
Unit tests and the cross build [V]; no Xbox portal has run on a Windows PC yet ([test_096_xbox.md](../../release/test_096_xbox.md)).
Windows has no libusb, and the drivers it binds by itself decide what a program can do:
- **Xbox One portal: Windows.Gaming.Input.Custom, on Windows' own Xbox driver** (`host/portal_gip_win.cpp`, nothing to
  install, no driver change). Windows' GIP driver holds the portal ("Xbox Gaming Device"); the documented user-space route
  to a GIP accessory is a game controller factory registered for its GIP interface
  (`GameControllerFactoryManager.RegisterCustomFactoryForGipInterface`, the portal's `{1F7788A2-B860-9B40-3626-EC56C7C45256}`),
  the route the Xbox build already uses (`host/uwp/portal_backends.cpp`). Windows then calls `CreateGameController` with an
  `IGipGameControllerProvider`: commands go as GIP message `0x21`, 30 bytes (`SendMessage`, the message class tried
  Command, then LowLatency, then StandardLatency, the first accepted kept), reports come in `OnMessageReceived`; Windows
  does the framing, power and acknowledgements. Registered once per process from a thread kept in the multithreaded
  apartment; when a `1430:09AA`/`09AB` node is present the game waits up to 3 s for Windows to hand it over. Microsoft says
  the namespace is "not targeted at apps", and whether Windows calls an unpackaged desktop program's factory is the one
  unknown: the log says it (`[portal] gip: Windows.Gaming.Input.Custom factory ... registered`, `[portal] gip: Windows hands
  over 1430:09AA (firmware ...)`, `[portal] Xbox One portal 1430:09AA connected (GIP, Windows.Gaming.Input.Custom)`, then
  `[portal] gip: out 1: 52 ...`, `in 1: 52 ...`, or the HRESULT that failed). Windows also suspends the input while the
  game is not in front (`[portal] gip: input suspended`). If Windows never hands it over, the cleanest one-time step is
  the Xbox 360 one below (WinUSB): the USB backend then does the GIP framing itself, as on macOS and Linux.
- **Xbox 360 portal: WinUSB, set once.** No driver Windows binds by itself lets a program send the portal commands (the
  Xbox 360 class driver, `xusb22`, serves XInput gamepads; a portal without an MS OS descriptor gets no driver), and a
  driver can only be installed with administrator rights and a signed package, so Arkeyan cannot do it for the player.
  The one-time step, nothing to download: Device Manager, the portal ("Spyro Portal", or an unknown device), Update
  driver, Browse my computer, Let me pick, Universal Serial Bus devices, **WinUsb Device** (Windows' own `winusb.inf`);
  Zadig's WinUSB driver is the same driver. This is what Xenia Canary asks for the same portal (#581). The game then
  opens it itself through `winusb.dll` (`host/portal_usb_win.cpp`, `LoadWinusb`): the present USB nodes whose service is
  `WinUSB` (CfgMgr32), each opened through a device interface (`WinusbInterfaceClasses`: the `DeviceInterfaceGUIDs` or
  `DeviceInterfaceGUID` registry values Zadig writes, then `GUID_DEVINTERFACE_USB_DEVICE`, then the generic WinUSB class
  `{DEE824EF-...}` [?]) with `CreateFile` and `WinUsb_Initialize`; a composite device's other interfaces by
  `WinUsb_GetAssociatedInterface`; `GET_DESCRIPTOR` by `WinUsb_GetDescriptor`; transfers overlapped, aborted at their
  timeout with what had arrived kept, as libusb does. `libusb-1.0.dll` is used only when `SKY_LIBUSB` names it (it also
  opens libusbK and libusb0 devices). Test and the log say what to do for any other driver: `WindowsDriverProblem`, "an
  Xbox 360 portal on the xusb22 driver, through which no program can send it commands. To use it, once, in Device Manager
  ..."; the launcher: "This Xbox 360 portal needs Windows' WinUSB driver, set once: see the README."
- One backend holds both routes (`XboxPortals`, `host/portal_usb.cpp`; the GIP one first). The probe tests the Xbox One
  portal through the GIP route (`ProbeGipPortal`: handed over, `R` sent, its answer) and the WinUSB ones as on Linux.
- Tests: `ArkeyanPortalUsbTest` (the drivers' verdicts, Win32 errors as libusb codes, the interface classes, the revision
  from a hardware id), `ArkeyanPortalUsbBackendTest`, both built for Windows and run under Wine; the runtime's portal
  files compile with llvm-mingw 20260922 (no warning).

## Mixed portal: a real portal and virtual seats

Player 1 on a real figure and Player 2 on a virtual Toy Box figure, or the reverse (the player's ask, 0.9.4). With a real
portal plugged in (`auto`, `real`, `hid`, `usb`), the runtime puts **`MixedPortal`** (`host/portal_mixed.cpp`) between the
game and the `AsyncPortal`: the game sees one portal, the real one's toys and the Toy Box's virtual seats in it.
`SKY_PORTAL_VIRTUAL=0` keeps the real portal alone (the old behaviour).
- **Slots.** A real toy keeps its own slot number when the game's slot is free, else takes the first free one (logged
  `[portal] mixed: the real portal's slot 0 is the game's slot 2`); the virtual figures (an `EmulatedPortal` that is never
  activated) take the slots the real toys leave free (`EmulatedPortal::SetReserved`). Two toys never share a slot.
- **Traffic.** `R`, `A`, the lights, `M` and the speaker are the real portal's. Each status report is the real portal's with
  the real slots renumbered and the virtual slots' bits added (their changes are reported once, as the emulated portal
  does). `Q` and `W` go to the portal that holds the slot (a real toy's under its own slot number, the answer renumbered
  back); a slot nobody holds fails (`0x01`), as a portal answers.
- **A reset** (0.9.5, issue #20) (a stand-in portal [V]; the player's [?]): a real portal forgets its toys on `R` and
  numbers them anew from slot 0, each reported as arriving again (the player's Giants log: Tree Rex in the real slot 1, then
  in slot 0 without being lifted). Giants read Tree Rex, the next reset made it arrive again, and the game asked "Please
  put a Skylander" forever. Now a reset holds every real toy the game read (by its block 0 serial) in its game slot,
  present and unchanged; each toy the real portal reports after it is identified by its serial (a `Q` of block 0 the mixed
  portal asks and never shows the game) and goes back to its slot (`[portal] mixed: the real portal's slot 0 is the game's
  slot 1 again (the same toy)`), or arrives as a new toy; a held slot nobody comes back to within 3 s is reported gone.
  Blocks asked of a held slot get no answer (the title asks again), so nothing is ever written to an unknown toy.
- **Why Giants reset the portal in bursts** [V]: with the emulated portal Giants sends one `R`; behind `AsyncPortal` it
  sent 2,900 in two minutes. A real portal streams status reports; queued in turn, every answer reached the title a poll
  late (it reads one report every 12 ms), the title sent each command two or three times, the answers to the copies piled
  up, and it reset. `AsyncPortal` now gives the answers first and keeps one status report waiting, the latest, with every
  change the others reported (`ArkeyanPortalAsyncTest`, "answers first"): Giants sends one `R` again.
- **Which player.** Players are taken in the order the toys come, as on one real portal: put Player 1 first, then the other.
  When a virtual Player 2 was put on before a real toy arrives, the real toy can sit in a lower slot than the virtual one;
  which player the game then gives it was not checked on hardware [?].
- **The Toy Box** (`toybox::Model::mixed`): the real toys sit on the first free seat they fit, read only, shown as on a
  real portal (their picture once the game read the tag; "Save a copy to My Skylanders" in their options); the other seats
  work as on the emulated portal. Putting a figure on a real toy's seat, swapping players while one is real, are refused
  ("A real figure is there: lift it off the portal first"). The launcher's Toy Box window shows the same (`seat <i> real`
  lines, [toybox_link.md](toybox_link.md)). The virtual seats start empty and are not saved in `portal.txt` (that file is the
  virtual portal's, for the sessions without a real one).
- **Proof without hardware.** `SKY_PORTAL=fake-real` (`CreateFakeRealPortal`): an emulated portal dressed as hardware,
  holding the dumps of `SKY_PORTAL_FAKE_REAL` (`<dump>[@<on>[-<off>]],...`: a toy put on that long after the game first reads
  the portal, lifted at `<off>`), run behind `AsyncPortal` and `MixedPortal` as a HID portal is. Like the players' portals it
  sends status reports before the first reset and renumbers its toys on each reset (`EmulatedPortal::Renumber`);
  `SKY_PORTAL_FAKE_REAL_ID` (`"02 0A 05 08"`) is the id it answers. `tools/regress/portal_renumber.sh [<game>...]` runs it in
  the six games (Giants: the player's Camo, then Tree Rex beside it, Camo lifted): each game reads the real toy, resets the
  portal at most 3 times, loses no toy; Giants reaches "In Game" after Tree Rex with no "Please put a Skylander". Before the
  fix the same Giants run looped on the prompt; after it, 6 of 6 pass (2026-11-10). Trap Team stays
  on its title with `02 0A 05 08` and reads the toy with `02 1B` (the script gives it `02 1B`); what Trap Team wants of a portal is not read [?]. `ArkeyanPortalTest` (`MixedChecks`): reset and
  activate are the real portal's, a real toy and a virtual one in their slots, blocks of each, a slot nobody holds, a real
  toy lifted, a real toy arriving where a virtual one sits (renumbered), a virtual figure never taking a real toy's slot, a
  block written to the real toy, a reset that keeps the places [V]. `MIXED=1 tools/regress/toybox_window.sh giants`: a virtual Whirlwind on Player 1
  from the window, then a real Spyro arriving at 40 s, read by the game as Player 2, the window refused a change of its seat.
- **What the tester checks with a real portal** (never run here): plug a portal in, start a two-player game, put a real
  figure on it (Player 1), then put a virtual figure on Player 2 from the Toy Box window or F6; both must play; lift the real
  one and put another; then the reverse (virtual Player 1 first, then a real figure). Look at the log for the `[portal]
  mixed:` lines, and at the window: the real figure's seat says "Real figure · on the portal" and refuses changes.

## The portal's identity

The USB product string does not name the model: Wii portals of Spyro's Adventure and of Swap Force (`0000547`) both say
"Spyro Porta", some Swap Force ones "Spyro Portal". Every portal model answers the reset command `R` with its own id: `52 <id>`, two id bytes (four on SuperChargers and
Imaginators). The first byte is 1 for Spyro's Adventure and Giants, 2 for Swap Force and later. The table is in
`host/portal_identity.cpp` (public community knowledge, tsapZ/SwapForceRecomp `docs/portal-compatibility.md`, MIT, see
`THIRD_PARTY_NOTICES.md`); the tests (`ArkeyanPortalTest`) check it.

| Id | Portal | Spyro's Adventure | Swap Force [V] |
|---|---|---|---|
| `01 29` | Spyro's Adventure (sold with Wii and PS3) | works | refused |
| `01 28` | Spyro's Adventure, **Xbox 360** (what the emulated portal answers for SSA) | works | refused (run) |
| `01 2C` | Spyro's Adventure (sold with PC and Mac) | works | refused |
| `01 34` | Spyro's Adventure / Giants, 3DS | works | refused |
| `01 3D`, `01 3C` | Giants (PS3, Wii) | works | refused |
| `01 48` | Giants, Xbox 360 (the emulated portal for Giants) | works | refused (run) |
| `02 03` | Swap Force (`02 02`: 3DS; `02 03` is the emulated portal for Swap Force) | works | works (run) |
| `02 18`, `02 1B` | Traptanium (a Wii Trap Team portal answers `02 1B`), `02 19` mobile, `02 27` Xbox One | works | works |
| `02 0A 03 02`, `02 14 03 02` | SuperChargers (Xbox One) | works | works |
| `02 0A 05 08` | Imaginators, and a Wii Swap Force portal (issue #20): the id names the electronics, not the box | works | works |

The names give a console only where its portal is its own hardware (the wireless Xbox 360 portals of the first two games,
3DS, mobile, Xbox One); the wired HID portals are named by model, since the id follows the electronics (a Wii Trap Team
portal answered `02 1B`, listed elsewhere as the Xbox 360 one). [V]

- **What the game checks.** Spyro's Adventure takes the two id bytes as a version, `(b1 << 8 | b2)`, and calls a portal
  under `0x011F` "out of date" (the title's portal driver, [xbox_uwp.md](../../getting-started/platforms/xbox_uwp.md)); it checks nothing else, so every portal
  above passes and so does one the table does not know, as long as it is newer. Giants: its check is not read, but it plays with `01 48` (the emulated portal, to Chapter 1) and
  with `02 0A 05 08` (the stand-in portal, `portal_renumber.sh`), so `PortalVerdictFor` says "works" from `01 1F` up
  (2026-11-10; it said "unknown" before, which a player read as the cause of the scan loop: it was not). **Swap Force**
  [Alchemy] [SwapForce] [V] has the same driver check, then wants a portal whose first id byte is 2 or more: a Spyro's
  Adventure or Giants portal gets "WRONG PORTAL VERSION. An old Portal of Power is connected to the console" and the
  game waits there (the code and the runs: [history.md](../../../games/swapforce/docs/history.md), "Figures and the
  portal"). Trap Team is said to want a Traptanium portal [?].
- **The emulated portal** answers the Xbox 360 portal of the game's own generation (`EmulatedPortalModel`: Spyro's
  Adventure `01 28`, Giants `01 48`, Swap Force `02 03`, later games `02 1B`). `SKY_PORTAL_MODEL=<id>` (e.g. `0128`, an
  id of the table) makes it answer another model, to see what a game does with it. It answered `02 1B` before; Spyro's Adventure accepts both and
  reaches level_027 with `01 28` (`[portal] <- R 52 01 28`, then `A`, `S`, `Q`).
- **A real portal:** the runtime reads the id the first time the game resets it and logs
  `[portal] model: 01 29 Spyro's Adventure portal; Spyro's Adventure: works`; a portal the game would refuse
  adds `[portal] warning: this portal may not work with this game: ...`. The Wii wireless portal sometimes answers
  `90 00` (a known quirk): that is above the game's check and shows as "unknown portal".
- **`--portal-probe`** adds `id`, `model`, `verdict` (`works`, `warns`, `rejected`, `unknown`) and `verdict_reason` to
  each portal line that answered `R` ([portal_probe.md](portal_probe.md)); the launcher's "Test the portal" shows the model.

## Status pacing [Alchemy] [TrapTeam] [V]
An active emulated portal used to answer every read with a status report. Trap Team's "Portal Audio" thread reads until a read returns
nothing, so that never ended and the main thread starved on the lock the thread holds. `EmulatedPortal::SetStatusInterval` spaces the
unprompted status reports (8 ms in `host/portal.cpp`; answers to commands are never held back); a title that polls every 12 ms or
slower gets one per read as before. Test: `ArkeyanPortalTest` ("A status interval"). Other games' runs not repeated yet [?].
([games/trapteam/docs/boot.md](../../../games/trapteam/docs/boot.md))

## The prompt watcher (`[portal] toybox_auto_open`)

When Spyro's Adventure asks for a figure, the Toy Box opens on the right seat by itself. Code: `host/portal_prompt.*`
(the state machine and the classification), the adapter's `ReadPortalPrompt` (`runtime/game.h`), `host/toybox.cpp`
(`FollowPrompt`: opens, closes, logs). One reader per game:
- **SSA** [SSA]: `games/ssa/adapter.cpp` with `games/ssa/ssa_portal_prompt.*` (the addresses below).
- **Giants** [Giants]: `games/giants/adapter.cpp` with `games/giants/giants_portal_prompt.*` reads the variables of the
  game script that runs its portal screens ("Portal Control"), found by a scan, no fixed address
  ([boot.md](../../../games/giants/docs/boot.md), "Portal prompt": `prompt for player 1`, Enter puts the figure,
  `the prompt is gone`).

- **Swap Force** [SwapForce]: `games/swapforce/adapter.cpp` with `games/swapforce/swapforce_portal_prompt.*` reads the
  game's CMagicMoment object (the instance pointer at `0x83131BE0`, its state and player; state 5 or 13 asks that player,
  16 is a "too many" screen; [history.md](../../../games/swapforce/docs/history.md), "The portal prompt").
- **SuperChargers** [SuperChargers]: `games/superchargers/adapter.cpp` with `games/superchargers/superchargers_portal_prompt.*`
  reads the same CMagicMoment object, moved (`0x83764ED0`; state `+0x2C`, reason `+0x168`, player `+0x1FC`). The reason
  VehicleRequired (13) asks for a **vehicle**: the prompt's `vehicle` flag, and the Toy Box opens on the seat `vehicle<N>`
  instead of `p<N>` ([toybox.md](../../../games/superchargers/docs/toybox.md), "The portal prompt"). `SKY_TOYBOX_SCRIPT`'s
  `prompt=1v` fakes one.

Later games add theirs the same way. The SSA reader:

- **Is the screen up?** The prompt object `0x82C569B0` (a function-local static of `sub_82205D18`) has its mode at
  `+56`: 0 in play, 1 and 2 as the screen comes in (2 = the portal screen is up), 3 and 4 while a figure comes in, 5
  on the way out. Seen in a run with no figure: `0 -> 1 -> 2 -> (figure placed) 3 -> 4 -> 5 -> 0`.
- **Who is asked?** While the mode is 2, the text shown is the CRC-32 of its key in `global.lxb`, at
  `[HUD + 3260 + 376] + 432` with the HUD at `0x82C56AF8` (the portal message is embedded in the HUD; the text element
  is a pointer). The hash does not depend on the language: `0x049B83C4` is Player 1, `0x9D92D27E` Player 2; any other
  portal text (the "too many toys" family) is not a request for a figure and opens nothing. The text appears a few
  frames after the mode turns 2 (the hash is 0 first), so a prompt counts after 3 identical polls.
- Source of the addresses: `tsapZ/SSARecomp` `docs/runtime/host/portal.md` (MIT, `THIRD_PARTY_NOTICES.md`); the chain and the
  values were checked in a run here (`SKY_PORTAL_LOG=1` prints `[toybox] portal prompt: mode N, text hash 0x...` on
  every change).
- **What happens:** `[toybox] prompt for player N` and the Toy Box opens on that seat (Player 2: its seat shows "needs a
  2nd controller"). A Toy Box the player has open already is left alone. When the prompt goes away
  (`[toybox] the prompt is gone`) a Toy Box the prompt opened and nobody touched (Home, no dialog) closes; one the player
  closed by hand stays closed for that prompt. A real portal never opens it (its toys are the player's).
- Recipe: `SKY_PORTAL=emulated`, no `SKY_PORTAL_PLACE`, an empty `SKY_SAVE_DIR`, the fast recipe's presses up to the
  `a:27` (no `b` presses: they would close the Toy Box); the prompt comes ~90 s after the launch, with Hugo's lines.
  `SKY_TOYBOX_SCRIPT="Return:92"` then puts the first figure: `[toybox] prompt for player 1`, `P1: Gill Grunt`,
  `the prompt is gone`, `closed`, `the game took Gill Grunt`.

## The portal at a glance, and taking one figure off (Arkeyan Launcher issue #9)

`collection::PortalOverview` (`host/collection/collection.*`, shared with the launcher, tested by `ArkeyanCollectionTest`)
turns the seats into one entry per occupied place: seat tag (`P1`, `P2`, `Item`, `Adventure`, `Trap`, `V1`; `J1`/`J2` in
French), the seat's name, its hardware slot (the Toy Box knows it, the launcher's next session puts seat i in slot i),
the figure and its name; `PortalOverviewLine` joins them: `P1 Gill Grunt · Item Shield`.

- **Toy Box:** the seat column ends with that line ("Nothing on the portal" when empty). Taking one figure off, whatever
  the seat (a player, a magic item, an adventure pack) and without lifting the others: in the seat column
  **Delete** or X (seat options, first button "Take off"); in the list **Delete** on a figure that sits on a seat; on the
  Figure page the first line is "Take off <seat>". The wheel and the seat's swap are unchanged.
- **Arkeyan Launcher's Collection page:** `docs/collection_ui.md` in the Arkeyan Launcher repository.

## The toy's owner in the log (Arkeyan Launcher issue #28)

Whatever the portal, `kernel/portal.cpp` logs a toy's owner (block 6, [figures.md](figures.md) "Owner") when the game reads it
and it changed for that slot: `[portal] slot 0's toy: owner offline 1F3AB27A` (`none`, `Xbox Live <XUID>`, another kind in
hex); with SuperChargers' `[owner] the player's owner id: ...`, a player's log tells a toy the game counts as someone else's.
The owner's write goes through every backend unchanged [V on the emulated portal and on the fake real one behind the mixed
portal and its thread (`SKY_PORTAL=fake-real`, run `live2`): `W` of block 6, its answer, the `Q` that checks it]; with real
hardware [?].
