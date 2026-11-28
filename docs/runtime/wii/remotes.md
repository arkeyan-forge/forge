# The Wii host's remotes: remote 1 from the host's input

> **Purpose:** how does the game read its remotes on the host, and how does a test press a button? · **Status:** current (2026-11-26) · **Tags:** [V]

## What the game asks
The game reads its remotes through the SDK's two libraries above the Bluetooth stack: **WPAD** (each remote's
connection and reports) and **KPAD** (the samples a game reads each frame: the buttons held, pressed and released, the
accelerometer, the pointer). The game's input manager calls `WPADProbe(remote, &type)` for each remote every frame: a
remote found (0, or busy) of a known type counts as connected, and only then does it call `KPADRead(remote, statuses,
16)` for its samples.

The host serves neither the Bluetooth stack nor the remotes' reports: `WUDInit` is answered as started, and the host
answers `WPADProbe` and `KPADRead` itself ([`runtime/wii/os.cpp`](../../../runtime/wii/os.cpp), bound to the game's
addresses in `games/<id>/addresses.h`).

## Remote 1
- `WPADProbe`: remote 1 found, a remote with no extension (type 0); the others not (`WPAD_ERR_NO_CONTROLLER`, type 253).
- `KPADRead`: one sample for remote 1, none for the others. The sample is a `KPADStatus` (0xF0 bytes in this SDK):
  the buttons held (0x00), pressed since the last sample (0x04) and released (0x08), the accelerometer at rest (gravity
  along -Y, 0x0C-0x14, and its magnitude, 0x18), the device type (0x5C) and no error (0x5D); the rest zero (no pointer).
- The buttons are WPAD's bits: Left 0x0001, Right 0x0002, Down 0x0004, Up 0x0008, + 0x0010, 2 0x0100, 1 0x0200,
  B 0x0400, A 0x0800, - 0x1000, Home 0x8000.

The host's keyboard and first controller give remote 1's buttons, the remote held upright, each time the window's
events are read ([`runtime/wii/video.cpp`](../../../runtime/wii/video.cpp)); they add to the script's:

| Remote | Keyboard | Controller |
|---|---|---|
| A | Return | south (A, cross) |
| B | Backspace | east |
| 1 | 1 | west |
| 2 | 2 | north |
| + / - | = / - | Start / Back |
| Home | H | Guide |
| D-pad | arrows | D-pad |

## Pressing a button in a test
`SKY_WII_PRESS=<presses>` adds scripted presses, counted in remote 1's reads (one per frame in the game's loop):
`A@120+10,B@300` holds A for reads 120 to 129 and B for read 300 alone. A press is one or more buttons (`A B 1 2 + - H
U D L R`), `@`, the first read, and an optional `+count` (1 by default). A script that does not parse is logged
(`[remote] SKY_WII_PRESS: ...`) and ignored.

## Code and tests
[`runtime/wii/remote.{h,cpp}`](../../../runtime/wii/remote.h): the script's parser and remote 1's samples (the
host's buttons with the script's, pressed and released against the last sample). `ArkeyanWiiRemoteTest` (in `ctest`):
the parser and its errors, a press's sample sequence, the host's buttons.

## Not yet
The pointer (the sensor bar), the accelerometer's motion, the extensions (the Nunchuk, the Wii Wheel's sideways hold),
remotes 2 to 4, the speaker and the rumble.
