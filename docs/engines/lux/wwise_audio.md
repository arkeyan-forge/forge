# Wwise, XAudio2 and XMP as the Lux games use them

> **Purpose:** what Wwise, XAudio2 and XMP need from the runtime in SSA · **Status:** current (2026-10-26) · **Tags:** [Lux] [XDK] [SSA]

Class **[Lux]** requirements, **[SSA]** addresses. Platform side: [../xbox360/audio_xma_xaudio.md](../../runtime/xbox360/audio_xma_xaudio.md), [../xbox360/xam.md](../../runtime/xbox360/xam.md).

- **Init** `Game_InitSound` (`0x82865448`): MemoryMgr, StreamMgr, low-level IO, then `AK::SoundEngine::Init` (`0x825852C0`). Needs: a **`XamNotifyCreateListener` handle**, **user APCs** (`ReadFileEx` completions in alertable waits) for bank loading.
- **Banks**: `audio\init.bnk`, `global.bnk`, `audio\<lang>\voice.bnk`, `audio\<lang>\levels\*.bnk`; streamed media in `audio\streaming.pck` with per-language sub-tables (the current language of every file package is set through `sub_82156A00`).
  Voice banks differ per language (BKHD language id).
- **The XMP mute trap (D9)**: Wwise mutes its **background-music bus** (`0x0DFBD437`) at registration (`0x825A1F38`) unless **`XMPTitleHasPlaybackControl`** (`0x824FC430` -> `XMsgInProcessCall(0xFA, 0x0007001B)`) says the title has control. With an output-less stub
  the check read stack garbage -> persistent mute ratio 0 -> **title music at -96 dB while SFX played**. Traced through Wwise: mute map of `CAkPBI` at `+24/+28` read by `0x825868B0`, audible flag `+865` in `0x825A7E90`. Also `XamGetSystemVersion` must be non-zero.
- **XMA**: two engines (XAudio2's and Wwise's); Wwise uses **hardware XMA loops** for looping in-memory sounds (`XMASetLoopData` `0x82617390`: loop subframe end -> `dword1` bits 12-13, skip -> bits 17-19). Wwise **drops a voice on XMA error 4** (a frame split across input buffers must wait instead of erroring).
- **XAudio2 clients**: two share callback `0x82523060` and buffer `0x82B74000`; render-driver callback `0x82522E80`.
- **Language of voices**: `sub_827FD4D8` -> `sub_82864B90(code)` sets Wwise's language folder (`\audio\<code>\voice.bnk`, levels' banks) and every file package's language. Hooked to choose the voice language separately ([languages.md](languages.md)). Streamed voices in `streaming.pck`: 2055 per language (a traced level run hit 5 English streams, 0 French with the default).
- A throttle: `sub_827F3378` (from `sub_827F6850`) counts down 1/60 per frame from 1/20 to throttle `.bnk` loading, so at 60 fps banks are queued twice as often (harmless).
- Debug: `SKY_AUDIO_DUMP=<path>` writes the float32 48 kHz output without speakers (2 channels, 6 in 5.1).
- **5.1** [SSA] [V]: Wwise mixes 5.1 (the 6-channel master voice) but puts nothing on the centre and the LFE in the scenes measured: front and back pairs only ([audio_xma_xaudio.md](../../runtime/xbox360/audio_xma_xaudio.md#51-the-speakers-setting)).
