# Audio: XAudio2 render driver, XMA, host output

> **Purpose:** how does the runtime play the title's audio: the XAudio2 render driver, the XMA hardware model, the decoder and the host output? · **Status:** current (2026-11-07) · **Tags:** [Xbox] [XDK] [Lux] [Alchemy] [SSA] [Giants] [SwapForce] [TrapTeam]

Code: [`kernel/audio.cpp`](../../../runtime/kernel/audio.cpp) (XAudio render driver),
[`kernel/xma.cpp`](../../../runtime/kernel/xma.cpp) (hardware register model, hooks),
[`apu/xma_context.*`](../../../runtime/apu/xma_context.cpp) (decoder context),
[`apu/xma_decoder_ffmpeg.cpp`](../../../runtime/apu/xma_decoder_ffmpeg.cpp),
[`host/audio.cpp`](../../../runtime/host/audio.cpp) (SDL3 out), [`host/audio_xaudio2.cpp`](../../../runtime/host/audio_xaudio2.cpp) (UWP out).
Class: render driver and XMA register model **[Xbox]**; engine addresses **[SSA]**; Wwise needs **[Lux]**.

## Layers

```
Wwise (Lux)  --voices-->  XAudio2 (XDK) --render driver client--> kernel/audio.cpp (driver thread)
   XMA voices: hardware contexts (kernel/xma.cpp + apu/)            |  6 planar channels x 256 floats
                                                                    v
                                  host/audio.cpp: stereo fold or 5.1 (host/speakers.h), SDL3 stream F32 48 kHz   (UWP: XAudio2 2.9)
```

## XAudio render driver (`XAudioRegisterRenderDriverClient`)

- **A render-driver client = `{callback, context}`** read from the pointer in r3. The driver calls each active client **once per
  audio frame = 256 samples at 48 kHz (5.333 ms)**, passing r3 = **a pointer to a cell holding the client's context** (not the
  context itself; the cell is a 4-byte `SystemAlloc`). The client answers with `XAudioSubmitRenderDriverFrame(driver, samples)` **from inside
  its callback**. Handle written to r4: `0x41550000 | index`. Up to 8 clients (`E_OUTOFMEMORY` 0x8007000E beyond).
- **One driver thread** (`StartHostDrivenThread(DriverThread, cpu 0)`): every period it zeroes a mix buffer, calls every client
  in turn (each submit adds into the mix: **6 planar channels of 256 big-endian floats**: FL, FR, C, LFE, BL, BR), then
  `HostAudioSubmit(mix)`. The **first frame is requested one period after registration** (the client needs time to initialise).
  It keeps its own clock, **one frame every 5.333 ms, never a burst**; with a host device open, the queue level trims the
  period (1/64 per frame away from **6 queued frames ~32 ms**). Bursts break titles that read XAudio2's `SamplesPlayed` as
  the cursor of a looping buffer: **[Giants]** mixes into a looping 1024-sample voice and uses `SamplesPlayed mod 1024`;
  refilling the device queue 4 frames at a time froze that cursor and the loop replayed (75.8% repeated output).
- `XAudioUnregisterRenderDriverClient` takes the client's **recursive** mutex, so a callback in progress finishes first (the
  console guarantees no callback outlives it; a client may unregister from its own callback).
- `XAudioGetSpeakerConfig` -> `0x00010001` in 5.1, `0x00000001` in stereo (below); `XAudioGetVoiceCategoryVolume` -> 1.0f;
  `XAudioGetDuckerLevel` -> 0.
- Host mix: stereo L = FL + 0.7071 C + 0.7071 BL; R = FR + 0.7071 C + 0.7071 BR, clamped; LFE dropped. 5.1: below.
  `SKY_AUDIO_DUMP=<path>` writes the output, float32 48 kHz interleaved (2 channels, or 6 in 5.1).
- **[SSA]** XAudio2 clients: two share callback `0x82523060` (a wrapper that dereferences the context cell) and buffer
  `0x82B74000`; `XAudio2_RenderDriverCallback` `0x82522E80`.

## 5.1: the speakers setting

`[<game>.audio] speakers = auto | stereo | 5.1` (or `[audio]`; env `SKY_SPEAKERS`; default auto), the F3 menu's **Speakers**
row ([gamemenu.md](../host/gamemenu.md)); it applies at the next start. Code: [`host/speakers.h`](../../../runtime/host/speakers.h)
(the rule, the frame functions), `host/audio.cpp` / `host/audio_xaudio2.cpp` (the output), `kernel/audio.cpp` (what the title is told).

- **Auto**: 5.1 when the default output device reports 6 channels or more (`SDL_GetAudioDeviceFormat`; XAudio2: the mastering
  voice's input channels), else stereo; no device (headless): stereo. The log says it: `[audio] speakers auto: 5.1 (device: 8 channels)`.
- **What the games mix** [V] (all six): their XAudio2 always creates a **6-channel, 48 kHz master voice with the mask `0x3F`**
  (FL FR FC LFE BL BR; one call per title, `li r5,6` / `li r9,63`), and its render client hands the driver that frame. The only
  reader of `XAudioGetSpeakerConfig` (one call site per title, at the client's registration) tests **bit 31 alone** (it picks
  `0x200` or `0x400` for a field of the client); SSA run with bit 31 set: the same levels. So the word changes nothing in their
  mix: the runtime reports `0x00010001` (Dolby Digital on the digital output, its old value) in 5.1 and `0x00000001` in stereo,
  for a title that would ask (the bit meanings are inferred, not documented [?]). `XGetAVPack` (6, HD) and the XConfig
  audio setting are not involved (no game asks for setting 3/11).
- **Channel order**: the guest's FL FR C LFE BL BR is SDL 3's 6-channel order ("FL, FR, FC, LFE, BL, BR") and XAudio2's
  (`SPEAKER_FRONT_LEFT` ... `SPEAKER_BACK_RIGHT`, mask order): the frame is interleaved as it is, nothing swapped
  (`ArkeyanSpeakersTest` puts a tone on each channel alone and finds it alone at its place).
- **The LFE is band-limited to 120 Hz** (4th-order Butterworth, two biquads, `speakers::LfeLowPass`), as the console's Dolby
  Digital encoder does: the Alchemy games send it full-band effects (Giants: about 90 % of its power above 120 Hz before the
  filter, 8 % after), which a PCM output would give the subwoofer as they are. Gain: 1.000 at 40 Hz, 0.707 at 120 Hz, 0.0002 at 1 kHz.
- **The portal speaker's voice** (Trap Team, [portal.md](../host/portal.md)) goes to the front pair at the stereo level.
- **Stereo is unchanged, bit for bit**: the fold is the same code (now `speakers::FoldToStereo`); `ArkeyanSpeakersTest` compares it
  with a verbatim copy of the 0.9.3 loop on 4000 frames (random, clipping, denormal and signed-zero samples, volumes 0-100): equal
  bytes. The device stream stays F32 stereo at 48 kHz; only 5.1 opens it at 6 channels (SDL converts if the device has another
  layout, e.g. 5.1 forced on a stereo device: SDL's own downmix).
- **Per game** [V] (2026-11-07, `tools/regress/surround.sh <game> <disc>`: SKY_SPEAKERS=5.1, the disk driver at 6 channels, a
  level with music, voices and effects; RMS in dBFS over the run, after the first second of sound; LFE: share of its power below 120 Hz):

  | Game | Route (scene) | FL | FR | C | LFE | BL | BR | LFE below 120 Hz |
  |---|---|---|---|---|---|---|---|---|
  | SSA | level_027 and its menus, 90 s | -24.5 | -24.6 | -119.4 | -121.1 | -40.4 | -40.2 | (empty) |
  | Giants | title to Chapter 1's tutorial, 110 s | -32.2 | -32.5 | -23.8 | -37.6 | -32.6 | -32.8 | 92 % |
  | Swap Force | title to the tutorial level, 100 s | -18.2 | -17.3 | -22.7 | -31.4 | -28.1 | -28.0 | 92 % |
  | Trap Team | title to the tutorial, 110 s | -32.5 | -33.0 | -24.4 | -42.0 | -33.2 | -33.4 | 84 % |
  | SuperChargers | new game, academy, 150 s | -23.0 | -23.0 | -25.5 | -44.6 | -33.8 | -33.6 | 96 % |
  | Imaginators | licence, title, level_508, 200 s | -27.4 | -26.8 | -37.2 | -60.5 | -47.2 | -47.1 | 98 % |

  The five Alchemy games put voices on the centre (Swap Force's and SuperChargers' centre: about 1 % of its power below
  120 Hz) and their own bass and effects on the LFE; no channel is a copy of another (|correlation| under 0.995; Giants' and
  Trap Team's music feeds the back pair like the front, 0.93 to 0.98). Each game's auto run (40 s, the disk driver's 2-channel
  device) logged stereo and `0x00000001`.

  SSA's Wwise mix is 5.1 (the back pair carries its own signal, correlation 0.1 with the front) but leaves the **centre and
  the LFE empty** in the scenes measured (a level, its menus; the start-up movies are stereo Bink): [?] Wwise's 3D panning does
  not use the centre unless a sound sets a centre share, and only authored sends reach the LFE. That is the game's own mix,
  as the console would encode it; the runtime does not invent a centre or a sub channel (no upmix).
- **How to verify** without speakers: the command above (exit 0: PASS), or any run with `SDL_AUDIO_DRIVER=disk
  SDL_AUDIO_CHANNELS=6 SDL_AUDIO_DISK_OUTPUT_FILE=<file> SKY_SPEAKERS=5.1`, then `python3 -I tools/regress/surround_rms.py <file> 6`.
  **With a real 5.1 system** [?]: the log's `[audio] speakers auto: 5.1 (device: N channels)` (auto must find the device's
  channels: 6 or 8 on HDMI or a 5.1 card; macOS: the device's layout in Audio MIDI Setup), then in a Swap Force or SuperChargers
  level: voices from the centre, music and ambience around (back pair), explosions and engines in the subwoofer, the left
  speakers on the left (a figure walking left of the camera). F3, Speakers: Stereo, restart: everything back to the two front speakers.

## The output device (2026-11-10)
A Traptanium portal with a speaker can be an audio device: plugged in, the Xbox One portal becomes one on Windows, which
makes it the default output, so the game's sound went into the portal. The game never plays on a portal's output
(`host/audio_device.h`, `ArkeyanAudioDeviceTest` on fake device names) [V]: it opens a device by its id, never SDL's
default (SDL moves a stream opened on the default to the new default by itself): the system's default unless its name says
"Portal", "Skylander", "Spyro" or "Traptanium", or (Windows) it sits on a portal's USB device
(`windows_devices::PortalAudioOutputNames`); else the first output that is none of these; none at all: no sound. The main
loop looks again on each SDL audio device event and once a second at the default's name (no allocation): a new default is
followed unless it is a portal's. The log names both: `[audio] output: "Speakers (Realtek(R) Audio)" (the default is
"Speakers (Skylanders Portal)")`, `[audio] skipped "Speakers (Skylanders Portal)": a portal's speaker, the portal's
business`, `[audio] skipped "<portal>", the new default: a portal's speaker; still "<output>"` when the system makes a
portal the default, and `[audio] output now: ...` on a change. The launcher's music does the same (`gamemusic::ChooseOutput`).
To verify on Windows [?]: plug the Xbox One portal in while the game runs; the sound stays on the speakers and the log
has the two lines.

## XMA hardware model

The Xenon's XMA decoder is driven by **64-byte `XMA_CONTEXT_DATA` blocks in one array in physical memory** and **little-endian
registers at `0x7FEA0000 + 4*index`** (register names from Xenia). Those registers are plain guest memory here, so the
title's **register writer functions are hooked**: run the original under one recursive mutex, then read the Kick/Lock/Clear bit
words it wrote, zero them and act.

**A writer stores the register once per context of its voice** (`1 << (index & 31)` to word `index >> 5`, a plain store, not
an OR) [Xbox] [XDK] [V]. On the console every store reaches the decoder; read after the writer returns, only the last store
to a word is left. So each hook also reads the voice it was given (r3: context count at +0, records at +8, see the record
below) and adds a **kick for every context of the voice** (VoiceKick) and a **clear for every context the writer created**
(VoiceClear: the record's +64 was 0 before). Without it, a voice of several contexts in one word (Swap Force's 8-channel
tutorial stream: 4 stereo contexts) had only its last context decoded and the Wwise voice service spun forever on the
others ([Swap Force boot.md](../../../games/swapforce/docs/boot.md), "XMA"). Locks need nothing.

| Register | Offset | Role |
|---|---|---|
| ContextArrayAddress | `0x1800` | physical address of the context array, **little-endian, must be set before the engines' init reads it** (read once: engine 1 `0x82503F40` -> global `0x82B73AB0`; engine 2 `0x82616A80` -> `0x82B7FA70`) |
| CurrentContextIndex | `0x1818` | read by `0x82616DA8` and compared with `index ^ 0x200`; 0 is fine |
| Kick | `0x1940` | one bit per context (`+4*(i>>5)`, bit `i & 31`): decode now |
| Lock | `0x1A40` | same layout: "leave this context alone" (decoding is synchronous so nothing to wait for) |
| Clear | `0x1A80` | same layout: reset the context |
| (0x1804) | | written 2/3 around locks, ignored |

- Array: **320 contexts** x 64 bytes, allocated with `PhysicalAlloc(..., 0x10000, 0xA0000000)` (the 64 KB window) so the title's init
  code can read the physical address back (`g_contexts & 0x1FFFFFFF` written to the register). Context index =
  `(MmGetPhysicalAddress(ctx) - ContextArrayAddress) >> 6`.
- `XMACreateContext(XMA_CONTEXT_DATA** out)`: first free index, zeroed, `Reset()`; `XMAReleaseContext`. XMA buffers must come from
  `MmAllocatePhysicalMemoryEx` (`PhysicalToGuest` returns 0 otherwise: `[xma] input buffer 0x... is not in physical memory`).
- **[SSA] two copies of the XMA voice code** (XAudio2 and Wwise each link one): **engine 1** `0x825xxxxx` (XAudio2's XMA voices;
  maps contexts through `MmMapIoSpace(2, physOfContext, 0x40, 0x404)` and writes through the result) and **engine 2**
  `0x826xxxxx` (Wwise; all XMA heard). Hooked writers:

  | Engine | Create/Clear | Lock | Kick |
  |---|---|---|---|
  | 1 | `0x82503FA0` | `0x825047C8` (**enters critical section `0x82A3CC30` first**: our hook takes it before our mutex to keep one lock order, else deadlock) | `0x82504938` |
  | 2 | `0x82616B50` | `0x82616D10` | `0x82616EF8` |
  | (0x1804 only) | `0x82617578` | | |

  Engine 2 voice record (96 bytes): `[0..63]` local `XMA_CONTEXT_DATA` copy (copied to the hardware context on kick, copied back
  by `0x82616DA8` after lock), `+64` hardware context pointer, `+68` output buffer, `+72` work buffer, `+80` u16 hardware index,
  `+84/+88` input buffer 0/1 (zeroed when consumed), `+92` `MmMapIoSpace` result (engine 1).

### `XMA_CONTEXT_DATA` (16 big-endian dwords; Xenia layout, BSD; checked against the title's `XMASetLoopData` `0x82617390`)

| Dword | Fields (from the low bit) |
|---|---|
| 0 | input buffer 0 packet count (12, 2048-byte packets), loop count (8; 255 = forever), buffer 0 valid, buffer 1 valid, output block count (5, 256-byte blocks), output write offset (5) |
| 1 | input buffer 1 packet count (12), **loop_subframe_end bits 12-13**, **loop_subframe_skip bits 17-19**, subframe decode count (4), output padding (3), sample rate (2: 24/32/44.1/48 kHz), stereo, ?, output buffer valid |
| 2 | input read offset (26, **in bits**), error status (5), error set |
| 3 | loop start (26 bits), parser error status, parser error set |
| 4 | loop end (26 bits), packet metadata (5), current buffer (1) |
| 5-8 | physical addresses: input buffer 0, input buffer 1, output buffer, work buffer |
| 9 | output read offset (5 blocks), stop-when-done, interrupt-when-done |

First context seen: dword0 `0x07800000`, dword1 `0xB0800000` (stereo 44.1 kHz), output phys `0x046CD280` (virtual `0xE46CD280`).

### XMA2 packets and frames

- Packet = 2048 bytes: header **frame count (6 bits), bit offset of the first frame starting in the packet (15 bits, from the end of
  the header), metadata (3 bits, 1 = XMA2), packets of other streams to skip after this one (8 bits)**. Frames have a 15-bit
  length prefix and carry **512 samples per channel**; a trailing bit says whether another frame follows in the packet.
- Decoder (`XmaContext::Kick`, synchronous): parse frame -> decode -> write **big-endian 16-bit interleaved** samples into 256-byte
  blocks of the `[read, write)` output ring; a full ring clears `output_buffer_valid`; a consumed input buffer clears its valid bit and toggles
  `current_buffer`. Behaviours the **title needs** (differences from Xenia):
  1. the loop end frame plays **up to its end subframe**, then playback jumps to the loop start frame;
  2. **a frame split across input buffers waits for the next buffer** instead of setting error 4 (**Wwise drops the voice on error 4**, D34);
  3. a frame the codec cannot output still takes its slot as 512 samples of silence;
  4. a starved context keeps the output it already wrote.

### FFmpeg shortcut (licence-critical)

FFmpeg's `xma1/xma2` decoders take whole packets and **hold 4096 samples back until end of stream** (D33): input consumption ran
ahead of output and hardware loops could not be honoured. Instead each XMA frame is wrapped in a **WMA Pro packet of its own**
(4-bit sequence, 2 unused bits, 15-bit "previous split length" = 0, then the frame) for the plain **`wmapro`** decoder configured with
XMA's parameters: `block_align 4095`, 512-sample frames, **decode flags `0x10D6`**, extradata bits-per-sample 16, channel mask 3
(stereo) / 4 (mono). The first frame of a stream is silence. Accuracy vs FFmpeg's own xma2: 75 dB (title music), 78 dB (menu).
Output: planar float -> interleaved, saturated, rounded to 16 bit.

- Build: `tools/ffmpeg/build.sh <host|windows-x64|uwp-x64>`: **static LGPL-2.1+ FFmpeg 8.1.2** (SHA-256 pinned), only `wmapro`;
  configure **refuses** a GPL or non-free FFmpeg (probe `avcodec_license()` or library strings when cross-compiling).
  Homebrew's FFmpeg is GPL: linking it statically would make the binary GPL (D36). `-DARKEYAN_FFMPEG=OFF` builds the null decoder
  (`apu/xma_decoder_null.cpp`: contexts stay silent).
- Cost: 2-9 us per writer call; process CPU ~350-420% on an M1 Pro with or without XMA.
- Untested [?]: engine 1 (XAudio2 XMA voices), multi-stream (> 2 channels) contexts. Hardware loops in levels: audited, see below.

### Hardware loops in levels (2026-10-28) [SSA] [V]

Audit with `SKY_XMA_TRACE=1` (test switch, [debugging.md](../../porting/debugging.md)), `tools/regress/ssa_level_play.sh` at `level_027` (isolated, ~100 s):
- **SSA's Wwise uses hardware loops all the time**: 8-16 contexts armed with `loop_count 255` (forever) per run, hundreds of wraps in 90 s
  (one voice wraps every ~1.4 s). Every loop observed starts at **bit 32, the first frame of its stream**, with `loop_subframe_skip 3`
  and `loop_subframe_end` 0, 1 or 3 (loop ends 41378 to 647473 bits).
- **The first frame of a stream is dropped, and so is every loop-start frame.** FFmpeg's WMA Pro decoder applies the frame header's
  trim fields (`trim_start`, `trim_end`; for WMA Pro only, FFmpeg's own XMA decoders ignore them): at a wrap it returns no frame at all
  (168 of 169 loop-start frames), so the 512 samples of the frame are silence. Played from subframe 3 (skip 3) that is **128 silent
  samples (about 3 ms) after each wrap**, and a hard step where the loop end is loud (one voice: 13331 to 0 each wrap).
  Rebuilding FFmpeg with the two trim conditions disabled (a local experiment, not kept) makes the same frames decode:
  subframe RMS of the loop-start frame `6453 3300 64 14`, `8 0 0 3`, `0 0 0 15`, `0 0 0 7`, `14 0 0 15`, `0 0 0 31`. The decoder state is
  not flushed at a wrap, so the first subframes there are the end frame's tail (the 6453 and 3300), which the skip hides.
- **Audible effect: none worth a change.** The part that plays (subframe 3) has an RMS of 3 to 31 on every loop, against a loop end
  of 16 to 14235: what the hardware would play there (if it ignores the trim like FFmpeg's XMA decoders) is within 36 dB or more under the end's level
  and at most -53 dB re the loudest one (31 against 14235 and 949), as good as silent, and the loud loop ends
  drop to that near-silence either way (these loops are repeating sound effects, not seamless beds). The
  "192 samples of start padding lost" of another port is therefore a real property of the decoder here (the whole frame is dropped)
  but not an audible defect in SSA. **Not changed**; a trim-free decoder would need a patched FFmpeg (a modified LGPL build) and a
  reference capture of the console to prove it better, neither of which exists.
- **A previous sound's tail on a reused context: not possible here.** `XMACreateContext` and `XMAReleaseContext` zero the context and
  reset the decoder (`XmaContext::Reset`: decoder flush with its overlap state, the pending frame, the loop state), and `Clear` does
  the same. Traces: `private/ssa100/xma/` (not in git).

## Wwise requirements [Lux]

`Game_InitSound` (`0x82865448`) -> MemoryMgr, StreamMgr, low-level IO, then `AK::SoundEngine::Init` (`0x825852C0`). It needs:
a `XamNotifyCreateListener` handle, **user APCs** for bank loading ([threads_sync.md](threads_sync.md)), the XMP "title has control"
answer ([xam.md](xam.md)), XMA loops (`XMASetLoopData` `0x82617390`). Details: [lux/wwise_audio.md](../../engines/lux/wwise_audio.md).
