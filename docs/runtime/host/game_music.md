# Each game's menu theme from the player's disc

> **Purpose:** where is each game's menu music, how is it decoded, and how does Arkeyan Launcher cache it? · **Status:** current (2026-11-01) · **Tags:** [Xbox] [Lux] [Alchemy] [SSA] [Giants] [SwapForce]

Arkeyan Launcher plays the visible game's theme softly. Nothing is shipped or downloaded: as with the
[pictures](game_art.md), the kit reads the player's own disc and writes
`<data>/<Game>/launcher-cache/menu_music.wav`. The launcher reads PCM only; it has no FFmpeg dependency.
Its speaker, fades and persistent preference are described in its `docs/launcher_ui.md` and the settings page of its docs.

## Extraction and packaging

`Arkeyan-Builder --iso <disc or copied data folder> --menu-music <file.wav> [--kit <folder>]` identifies the game
using the kit's `config/<id>/game.txt`, reads the `music` object in `games/<id>/art.json`, decodes XMA2 and
writes a 16-bit PCM WAV with a `smpl` loop. It prints `@done` on success; it neither builds nor starts a game.
The manifest contains names and identifiers only. All four manifests name a stereo menu theme.

The disc-check worker invokes this after extracting pictures. Missing themes, older kits and unavailable
sound devices leave the page silent with no speaker or error dialog. Failed extraction is remembered in
`menu_music.none` per kit build identity; a different build retries without requiring a version bump.
A valid cached theme survives a missing disc. `menu_music.kit` records the decoder generation and kit build; a later build refreshes it atomically and the player reloads the replacement. A failed refresh retains the previous playable file. Installs silence every page; after an install or game exit
(including a crash), only the visible game page resumes, provided the global speaker is on. A launch waits
for the launcher fade to finish before starting the process.

`tools/make_kit.sh` enables FFmpeg for Arkeyan-Builder and passes the same static `ARKEYAN_FFMPEG_ROOT` used by the
runtimes on Windows, macOS and Linux. No GPL/nonfree options are enabled; the existing configure-time LGPL
check also applies to Arkeyan-Builder. `ARKEYAN_FFMPEG=OFF` remains available for development; its extraction request
fails explicitly. There is no launcher or kit version bump.

Arkeyan-Builder now conveys FFmpeg in a shipped executable. Kits therefore include:

- The existing FFmpeg corresponding source, exact build script and LGPL text in `licenses/`.
- `relink/arkeyan-builder/`: Arkeyan-Builder objects and all link dependencies, including replaceable `libavcodec.a` and
  `libavutil.a`; `relink/arkeyan-builder.json`, a path-independent link recipe; and `arkeyan-builder.py` with instructions.
- The updated `THIRD_PARTY_NOTICES.md` and component table. The project source identified by `kit.txt` must
  accompany the release or remain available with it.

`kit_relink.py --pack` derives the recipe from Ninja's real link command, refusing unmapped absolute paths.
The relink bundle is included in `tools/kit_scan.sh`'s scan. No disc audio, archives or translated game code
are included. Relinking uses the matching platform compiler; see `relink/README.txt` inside the kit.

## Sources on the four discs

| Game | Manifest source | Decoded result |
|---|---|---|
| Spyro's Adventure | `audio/streaming.pck`, stream `0x0C45FC64` | Wwise XMA2, 44.1 kHz stereo, 62.8 s; container loop |
| Giants | `level/Title.arc`, `mu_title_main_loop.wav` | FMOD FSB5/XMA, 48 kHz stereo, 54.9 s; whole-track loop |
| Swap Force | `archives/inter_streamed_launch_bank.pak`, `INTER_Results_Screen` | FMOD FSB5/XMA, 48 kHz stereo, 64.5 s; whole-track loop |
| Trap Team | `level/title.arc`, `mu_title_main_loop.wav` | FMOD FSB5/XMA, 48 kHz stereo, 52.7 s; whole-track loop |
| SuperChargers | `archives/inter_streamed_bank.pak`, `INTER_Main_Theme.wav,0,48000,kDefaultFormat,40,1,0,1` (the entry names carry the encoding settings) | FMOD FSB5/XMA, 48 kHz stereo, 80.6 s ([its art.md](../../../games/superchargers/docs/art.md)) |

Trap Team uses Alchemy's IGA v11 archives and FMOD FSB5 banks with XMA audio. The common
[archive reader](game_art.md) reads these directly. Its title archive also contains a PvP loop and a short
front bumper; the manifest selects the main loop explicitly. This is strong disc-name evidence; listening
on a running game remains the final perceptual check. No game was started during this work.

## Swap Force: the menu event and its sample

The earlier investigation rejected `INTER_Results_Screen` because of its name and an unmatched runtime
recording. The disc supplies stronger evidence than that name:

1. The `mainmenubackground.mua` scene in `archives/mainmenubackground.pak` sets its traversal stream to
   `INTER_Streamed_Launch_bank.INTER_Menu_Screen`.
2. The launch sound bank in `archives/permanent.pak`, `sounds/banks/inter_streamed_launch_bank.igz`, contains
   that `CSound` event. Its sub-sound list points to the `CSubSound` naming `INTER_Results_Screen.wav`.
3. That sample is the stereo entry `INTER_Results_Screen` in the streamed launch bank. The results-screen
   event shares the same sub-sound. The adjacent alternate sample has a different sub-sound and is not selected.

The menu therefore uses a stereo sample, despite many gameplay cues having adaptive stems. Identification
is high-confidence from the scene and object links, with no listening claim. To settle the earlier recording
mismatch, record 20–30 seconds after the title's menu painting appears, without navigation or voices, with
game music on and effects low. Compare it with the decoded stereo theme, allowing a different start offset
and volume. The old frame-traversal bug also makes the earlier decoded comparison unreliable. A title/press-start capture before that painting may exercise a different cue.

The common streamed bank contains 161 samples: 59 stereo, 10 four-channel, 27 six-channel, 17 eight-channel,
and 48 ten-channel samples. The 102 non-stereo samples are adaptive stereo stems, not all ten-channel.
`music::Decode` now handles these too. Each stream starts in one of the first packets; its next packet is
`current + skip count + 1`. Unclaimed packets must be zero padding. Demultiplexing preserves split frames
across packets; each stereo decoder is reset independently. No arbitrary adaptive mix is substituted for
the menu event.

## Frame traversal and validation

The Wwise reader preserves the stream's length and loop; the FMOD reader uses its sample header and any
loop extension. Both feed the runtime's LGPL `XmaFrameDecoder` ([audio](../xbox360/audio_xma_xaudio.md)).
XMA2 packet headers determine how many frames start in each packet. Frame footers alone undercount these
discs: the paused implementation wrote roughly half a theme followed by zero padding. Extraction now
uses the packet counts and rejects a stream that falls more than one codec frame short. The priming frame
is omitted and the output is trimmed to the container length.

`ArkeyanBuilderMusicTest` checks synthetic Wwise files in both byte orders, package lookup, WAV loops, FMOD
channel extensions, packet counts, stream chains, padding and malformed input. With `SSA_ISO`, `GIANTS_ISO`,
`SWAPFORCE_ISO` and `TRAPTEAM_ISO` it reads all four discs, decodes the selected themes and checks length,
overall level, clipping fraction and non-silent tails. It also decodes all 102 adaptive samples of Swap Force’s common streamed bank, checking their lengths and levels. This catches the old half-silent extraction.
`ArkeyanBuilderTest` checks the existing kit/build contract. Use scratch `HOME`, `TMPDIR` and data roots,
with `SDL_AUDIO_DRIVER=dummy`; no test starts a real game or uses the speakers.
