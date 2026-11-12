# Xbox present test

> **Purpose:** which way of presenting reaches the Xbox screen, and how is the test app built and read? · **Status:** current (2026-10-26); test 1 ran 2026-10-23 and the game was fixed (A4); the Xbox port is paused · **Tags:** [Xbox]

The game presents to its CoreWindow without errors (30 a second), yet the console keeps showing the splash screen.
This small app finds out which way of presenting reaches the screen. It is its own package,
`SSARecomp.PresentTest` ("Arkeyan Present Test" in Dev Home): it installs next to the game, needs no disc image, and
is about 1 MB.

## Build and package

    ninja -C build-uwp ArkeyanPresentTest
    SKY_PACKAGE_NAME=SSARecomp.PresentTest SKY_DISPLAY_NAME="SSARecomp Present Test" \
        tools/xbox/package.sh --exe build-uwp/ArkeyanRuntime/ArkeyanPresentTest.exe --no-iso --out private/xbox/out/SSARecomp-PresentTest.msix

Install it like the game (Device Portal, *Add*) and set its app type to **Game** too.

## What it shows

Version 2. Each experiment fills the screen with one colour for 3 s; the sequence then repeats from C. The log says
`start <id> <colour>` before each one, its present count, `released` once its swap chain is gone, and `TIMEOUT` when one
still runs after 6 s (a stuck worker thread is abandoned and the next experiment starts).

Present test 1 (2026-10-23): GREY showed; RED, plume as the game made its swap chain then, presented without errors
but was never shown, and the test stopped there. The game now creates its swap chain as the template does (A4).

| # | Colour | Experiment |
|---|--------|------------|
| 0 | GREY | Microsoft's UWP D3D12 template (FLIP_DISCARD, 2 buffers, no flags, Present(1, 0), UI thread), swap chain created **before** the Activated event (once, at start) |
| C | BLUE | raw D3D12, waitable flag waited on each frame, SetMaximumFrameLatency(1) |
| D | YELLOW | raw D3D12, the template as GREY, created after activation |
| E | MAGENTA | raw D3D12, ALLOW_TEARING flag, Present(0, ALLOW_TEARING) (skipped when not supported) |
| F | CYAN | raw D3D12, FLIP_SEQUENTIAL, 2 buffers |
| G | WHITE | raw D3D12, FLIP_DISCARD, 3 buffers |
| H | ORANGE | raw D3D12 template, Present from a **worker** thread |
| I | PURPLE | raw D3D12 template after ApplicationView.SetDesiredBoundsMode(UseCoreWindow) |
| A | RED | **the game before the fix**: plume's old swap chain rebuilt in raw D3D12: waitable flag (not waited on), ALLOW_TEARING flag, Present(1, 0), ResizeBuffers after creation, worker thread |
| A2 | PINK | as A without the waitable flag |
| A3 | BROWN | as A without the tearing flag |
| A4 | GREEN | **the game now**: plume, no waitable or tearing flag, Present(1, 0), no ResizeBuffers, UI thread |
| A5 | TEAL (dark blue-green) | as A4, Present from a worker thread |

All swap chains are B8G8R8A8_UNORM, DXGI_SCALING_STRETCH, DXGI_ALPHA_MODE_IGNORE, at the window's size in pixels, on
the direct queue that executes the clears. The log is `LocalState\PresentTest.log` (Device Portal, File explorer,
LocalAppData, `SSARecomp.PresentTest_<id>`).

**Report**: the colours seen, in order, and the log.
