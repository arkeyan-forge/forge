# The Wii host's audio: the audio interface, the DSP and AX

> **Purpose:** how does the host answer the Wii's audio hardware, and what does it do with AX's command lists? · **Status:** current (2026-11-24) · **Tags:** [V]

## How AX drives the hardware
AX, the SDK's audio library, keeps the voices on the CPU and has the DSP mix them, one **frame** of 3 ms at a time (96
samples at 32 kHz), from the game's own code (`__AXOutInit`, `__AXOutNewFrame`, `__AXNextFrame`, `__AXSyncPBs`):
- the **audio interface** (AI) plays a 384-byte buffer (96 16-bit stereo samples) by DMA and interrupts at its end;
  its callback, `__AXOutAiCallback`, starts the next frame when the DSP has finished the last one (else it asks the
  DSP's task to yield), and sets the next buffer (`AIInitDMA`);
- a frame: AX copies the voices' changes into the DSP's parameter blocks, sends the DSP two mails (`0xBABE0080`, then
  the command list's address), runs the aux buses' and the game's callbacks, and builds the next command list;
- the DSP runs the list, then yields its task; the task's resume callback, `__AXDSPResumeCallback`, marks the frame
  done (or starts the next at once, when the interface asked meanwhile).

## On the host (`runtime/wii/audio.cpp`, `audio_out.{h,cpp}`, `ax.{h,cpp}`, `ax_mix.{h,cpp}`, `timers.cpp`)
- `AIInit` is skipped (it times the interface's sample counter); `AIRegisterDMACallback`, `AIInitDMA` (kept, and its
  registers written too), `AIStartDMA`, `AIStopDMA` are the host's; `AIGetDMABytesLeft` is 0. From `AIStartDMA` on,
  the host's timers post the interface's interrupt every buffer (3 ms), which calls the DMA callback as an event
  ([threads.md](threads.md)).
- `DSPInit` is skipped; `DSPAddTask` starts the task at once: AX waits for its start callback in a loop with no safe
  point. `DSPSendMailToDSP`: after `0xBABE....`, the next mail is a command list's address, which the host runs
  (`wii::ax::RunCommandList`), then it posts the task's resume. `DSPCheckMailToDSP` is 0 (every mail taken at once),
  `DSPAssertTask` does nothing.

**The command list** (16-bit words: a command, then its arguments; an address takes two): 0 the studio, 1-3 the
output mode's buffer, 4 the voices (the first parameter block), 5-7 the aux buses A-C (a volume, the buffer the DSP
fills, the one the CPU returns), 8-9 A and B in Dolby Pro Logic II (a volume and six buffers), 10 the compressor, 11-12
the final mix (a volume, the mode's buffer, the output buffer), 13 the four remotes' speakers, 14 the end. Addresses
are physical: the host reads them at MEM1's or MEM2's cached address.

**A voice's parameter block** (320 bytes, linked by the address at its start; the CPU's own copy is at 40 in AX's voice
structure): the converter (8: 0 polyphase, 1 linear, 2 none), the mixer control (12: 32 bits), the state (16: 1 playing),
the type (18: 1 a stream), the mix (20), the volume envelope (106, its signed step for each sample at 108),
the addressing (110: the loop flag, the format, then the loop, end and current addresses, 32 bits each, in nibbles
for ADPCM, samples for 16-bit PCM, bytes for 8-bit), ADPCM's coefficients and state (126: the header in use at 160,
the last two samples at 162 and 164),
the rate (166: 16.16 input samples for each output one, the position between two at 170), the loop's ADPCM state
(180). ADPCM is in 8-byte frames: one header byte (the predictor and the scale), then 14 sample nibbles; each frame's
first two nibbles are its header.

The mix occupies bytes 20..67: twelve pairs of a big-endian unsigned 16-bit gain and a signed 16-bit step, in this
order: main L, main R, aux A L, aux A R, aux B L, aux B R, aux C L, aux C R, main S, aux A S, aux B S, aux C S.
Gain `0x8000` is unity. The mixer control's bits are:

| Group | L hint | R hint | S hint | L/R ramp | S ramp |
|---|---|---|---|---|---|
| Main | `0x1` | `0x2` | `0x8` | `0x4` | `0x10` |
| Aux A | `0x10000` | `0x20000` | `0x80000` | `0x40000` | `0x100000` |
| Aux B | `0x200000` | `0x400000` | `0x1000000` | `0x800000` | `0x2000000` |
| Aux C | `0x4000000` | `0x8000000` | `0x20000000` | `0x10000000` | `0x40000000` |

The SDK sets channel hints from non-zero gains; the host mixes every gain regardless of those hints. Only the
group's ramp bit enables its steps, added once per output sample; without it the effective step is zero. DSP
write-back updates the gains after the frame's ramp and preserves the PB's original steps and mixer control.

**Voice rendering** (`wii::ax::RenderVoice`) works on a plain voice state: 96 output samples, decoding signed
PCM8, big-endian PCM16 or ADPCM with eight predictor pairs, rounding and saturation. Reads use cached MEM1/MEM2
addresses and stop on unmapped memory. The end address is inclusive; a voice then stops or loops, restoring its
saved ADPCM state unless it is a stream. Envelope and all twelve gain ramps clamp to 0..65535, with 32768 as unity,
and accumulate into the main L/R/S and aux A/B/C L/R/S buses. `FinalMix` folds surround into left and right at
-3 dB (23170/32768), applies the final command's volume (`0x8000` unity) before saturation, and writes 384 bytes
of interleaved big-endian stereo. Neither function allocates.

The converter consumes the integer carry of a 16.16 phase accumulator each output sample. Mode 2 ignores the ratio
and consumes one input. Mode 1 linearly interpolates the last two inputs; mode 0 uses a causal four-tap Lagrange cubic
with 128 phases, Q15 weights computed at compile time and summing exactly to unity. Linear and cubic add one and two
input samples of delay respectively at unity ratio. Four past inputs (PB offsets 172..178, oldest first) and the
fraction carry between frames. This is an interpolation filter, without a separate anti-alias low-pass for downsampling.

`ReadVoice`/`WriteVoice` transfer the parameter block's state, including the coefficients at 126 and the twelve mix
gains. `AdvanceVoice` renders into the frame's shared bus and writes the voice back with the histories it decoded.
Envelope and enabled gain ramps advance across all 96 output slots, even when the voice ends early; a voice stopped
at entry is left alone.

`RunCommandList` clears one bus per frame and accumulates all playing voices from command 4. Commands 11 and 12
read their volume from argument word 0 and the output address from words 3..4, then write the final mix only when
the full 384-byte destination is in memory. The 4608-byte bus uses reusable thread-local storage, keeping it off
the guest threads' small stacks, with no per-frame allocation; synchronous list execution keeps frames separate.

Aux A/B/C mixes accumulate and their gains advance, but their buffers are not transferred to the CPU yet and
their samples are left out of the final output. Still to connect: the mode's input buffers, aux bus callback
transfers and effects, the remotes' speakers.

**The host's sound** (`runtime/wii/audio_out.{h,cpp}`): at each audio interface interrupt, the buffer the DMA just
played (the one `AIInitDMA` had set when that DMA began) goes to an SDL audio stream, 16-bit stereo at 32 kHz, its
samples turned to the host's byte order. The device opens at the first `AIStartDMA`; with none (no audio driver) the
game runs silent. Past 100 ms queued (the host's timers ahead of the device) a buffer is dropped rather than letting
the delay grow. SDL's signal handlers are off: they would turn SIGINT and SIGTERM into a quit event nothing reads.
`ArkeyanWiiAudioOutTest` (SDL's dummy driver): the byte order, the queue's bound, a buffer larger than one piece.

Synthetic tests (no game data or game run):
```sh
ninja -C build-tests -j4 ArkeyanAxMixTest ArkeyanWiiTest
./build-tests/ArkeyanRuntime/ArkeyanAxMixTest
./build-tests/ArkeyanRuntime/ArkeyanWiiTest .scratch/wii
```
`ArkeyanAxMixTest` checks an encoded sine, known ADPCM samples, PCM formats, loops, bounds, all converters at ratios
0.5/1/2/1.37, phase and history continuity, ramps, buses, and final byte order/clipping. Command-list tests use
synthetic PBs to check the twelve gains and ramp bits, stereo/surround output, final volume, silence, multiple
voices and clipping, stopped voices, frame clearing, MEM2 output and rejected destinations. It also reports the time
for 96 ADPCM voices over 1000 frames and requires less than one second in Release.

## The first sound [V] (2026-11-26)
SuperChargers Racing, with remote 1's scripted presses past the strap screen: the copyright screen, the language
menu, the Activision movie and the title screen play through the host's device (2 voices at a time in the menus);
the aux buses' effects (reverb) are still missing.

## How far it boots [V] (2026-11-24)
SuperChargers Racing: Wwise's start goes through (the audio interface's DMA, AX's task, its frames with 96 voices),
the engine starts its streaming threads, reads the disc through the DVD library (its file system table loaded by the
boot, [memory_boot.md](memory_boot.md)), and loads its game module (`gamelogic.rso`) into the heap; the call to the
module's prolog is the first to code that was not translated, where the boot stops (#82's next piece).
