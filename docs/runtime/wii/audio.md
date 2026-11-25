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

## On the host (`runtime/wii/audio.cpp`, `ax.{h,cpp}`, `timers.cpp`)
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
structure): the converter (8: 0 polyphase, 1 linear, 2 none), the state (16: 1 playing), the type (18: 1 a stream),
the mix (20), the volume envelope (106, its signed step for each sample at 108), the addressing (110: the loop flag,
the format, then the loop, end and current addresses, 32 bits each, in nibbles for ADPCM, samples for 16-bit PCM,
bytes for 8-bit), ADPCM's coefficients and state (126: the header in use at 160, the last two samples at 162 and 164),
the rate (166: 16.16 input samples for each output one, the position between two at 170), the loop's ADPCM state
(180). ADPCM is in 8-byte frames: a header byte (the predictor and the scale), a padding byte, 14 samples; each frame's
first two nibbles are its header.

For now the host plays nothing: each playing voice **advances** by one frame at its rate (a frame header read where one
begins; at its end the voice loops, to its loop's ADPCM state unless it is a stream, or stops), its volume ramp
advances, and the output buffer stays silent. The mix (decoding, the converter, the volumes, the aux buses, the
remotes) and the host's audio output come next (#82).

## How far it boots [V] (2026-11-24)
SuperChargers Racing: Wwise's start goes through (the audio interface's DMA, AX's task, its frames with 96 voices),
the engine starts its streaming threads, reads the disc through the DVD library (its file system table loaded by the
boot, [memory_boot.md](memory_boot.md)), and loads its game module (`gamelogic.rso`) into the heap; the call to the
module's prolog is the first to code that was not translated, where the boot stops (#82's next piece).
