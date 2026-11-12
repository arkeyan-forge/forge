# The game's own zlib (and why we replace it on day one)

> **Purpose:** why the game's own zlib is replaced and how · **Status:** current (2026-10-26) · **Tags:** [Lux] [SSA]

Code: [`inflate.cpp`](../../../runtime/inflate.cpp). Class **[Lux]** (every Lux level/UI package is zlib), addresses **[SSA]**. Built with the system zlib when CMake finds it (macOS SDK, Linux), else with **miniz** (`ARKEYAN_INFLATE_MINIZ`: `thirdparty/XenosRecomp/thirdparty/smol-v/testing/external/miniz/miniz.c`, v1.15, public domain, whose `mz_stream` is zlib's field for field): **Windows now uses it** (2026-10-27;
**Xbox UWP keeps the translated inflate** until it is built with the same CMake path, untested). Adler-32 is verified by miniz too.

- The title statically links **zlib 1.2.x**. It calls only `inflateInit_` (`0x825F23B8`, a tail call to `inflateInit2_` `0x825F2298` with windowBits 15), `inflate` (`0x825F24F8`, calls `inflate_fast` `0x825F3C18`) and `inflateEnd` (`0x825F3B80`).
- Translated, it runs ~30 MB/s on the game thread: 86% of a level load. We override the three entry points (`PPC_FUNC(sub_825F2298)`, `sub_825F24F8`, `sub_825F3B80`).
- **Design**: one **host `z_stream` per guest `z_stream` address** (`g_streams`, mutex-guarded); the guest's `state` field (`+0x1C`) just stores the stream's own address to mark it open. Guest `z_stream` layout (32-bit big-endian): `+0x00 next_in`, `+0x04 avail_in`, `+0x08 total_in`,
  `+0x0C next_out`, `+0x10 avail_out`, `+0x14 total_out`, `+0x18 msg`, `+0x1C state`, `+0x2C data_type`, `+0x30 adler`, size `0x38` (`inflateInit2_` checks version[0] == '1' and stream_size == 0x38, else `Z_VERSION_ERROR`).
  Each `inflate` call translates the pointers (`Translate`), runs host `inflate`, and writes back `next_in/avail_in/total_in/next_out/avail_out/total_out/data_type/adler`. **Adler-32 is still verified** by zlib (every stream).
- A re-initialised stream without `inflateEnd` closes the old host stream first.
- **Finding it in a new binary**: the error strings `"incorrect header check"` (referenced from `inflate`) and `"invalid block type"`; or sigmatch by signature ([../toolchain/sigmatch.md](../../tools/toolchain/sigmatch.md)). Replace it the same day: it removes most of the load time.
- Windows, measured under Wine headless on an M1 Pro (Rosetta, `tools/windows/wine_smoke_test.sh` with `SKY_LEVEL=level_027`, the time between the first and the last log line of `level_027/mission.str`, 2 runs each): **0.75 s with the translated zlib, 0.45 s with miniz**; no fault, the same log otherwise. Wine's file log paces the reads, so a real PC should gain more; to be confirmed there.
- Numbers (macOS): mission.str (43 MB -> 85 MB) read in 0.3-0.5 s (was ~3.4 s); frontend assets (`screenmask.apk` -> frontend `mission.str`) 1.4 -> 0.5-0.65 s; load to the first frame with >1000 draws 4.5 -> 0.95-1.3 s.
