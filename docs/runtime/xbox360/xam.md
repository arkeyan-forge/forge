# XAM services: config, profile, notifications, XMsg/XMP, achievements, network

> **Purpose:** what do our XAM services answer (XConfig, profile, notifications, XMsg/XMP, achievements, network)? · **Status:** current (2026-10-26) · **Tags:** [Xbox]

Code: `kernel/xconfig.cpp`, `xam_user.cpp`, `xam.cpp`, `xmp.cpp`, `xam_achievements.cpp`, `net.cpp`, `module.cpp`
(in `runtime/kernel/`). Class **[Xbox]** (the values are ours; the *need* is the title's).

**The rule of this file: a weak stub that returns 0 without writing its outputs is worse than a missing function.** Every
section below records an output whose absence cost hours.

## XConfig

`ExGetXConfigSetting(category, setting, buffer, size, required size out)` -> NTSTATUS. We emulate a **North-American retail
console on an HD output** (all values big-endian dwords unless noted):

| Category/setting | Meaning | We answer |
|---|---|---|
| 2 / 2 | AV region | `0x00001000` (NA). The title only tests PAL: `(v & 0xFF00) == 0x0300` [SSA] (`sub_822DF218`) |
| 2 / 3 | game region | `0x000000FF` |
| 3 / 9 | language | `XC_LANGUAGE_*` of the host language (1 English default, 2 ja, 3 de, 4 fr, 5 es, 6 it, 7 ko, 8 zh-tw, 9 pt, 10 zh-cn, 11 pl, 12 ru) |
| 3 / 10 | video flags | `0x00030000` (widescreen 720p) |
| 3 / 14 | country (1 byte) | 103 (US). **nl/sv/no/da/fi have no console language**: the title takes them from the country: 74 Netherlands, 90 Sweden, 75 Norway, 25 Denmark, 32 Finland |
| 3 / 1 | time zone bias (minutes) | host offset incl. daylight saving |
| 3 / 2, 3 | standard / daylight name | 4 chars (host `%Z`), not terminated |
| 3 / 4-7, 12 | transition dates, biases, retail flags | 0 (no transition: local time is right today) |

Unknown settings: log `[xconfig] setting c/s not available` once and return `STATUS_INVALID_PARAMETER_2` (secured/user
categories) or `_1`. `bufferSize < size` -> `STATUS_BUFFER_TOO_SMALL`. **Before this was real, the country came from
uninitialised stack** and sent the title down a wrong territory path.

Companions: `XGetAVPack` = **6** (VGA; the title warns about the video output unless the pack is 3, 4, 6 or 8),
`XGetGameRegion` 0xFF, `XGetLanguage`, **`XamGetSystemVersion` = 2.0.17559.0** encoded `(major<<28)|(minor<<24)|(build<<8)|qfe`.
A version of 0 sends the music-player check down its pre-2009 path reading unwritten outputs.

## Profile

The players' **offline** profiles the launcher passes (`SKY_PLAYER_*`, `SKY_PLAYER2_*`: the contract is
[profiles.md](../host/profiles.md)); with none, one profile on user 0: `XamUserGetSigninState` 1 (signed in locally),
`XamUserGetXUID` = `0xE000000000000001` (offline XUIDs live in the 0xE... range), `XamUserGetName` "Player",
`XamUserGetSigninInfo` -> `X_USER_SIGNIN_INFO` (`+0x00` u64 xuid, `+0x08` flags, `+0x0C` signin state, `+0x10` guest number,
`+0x14` sponsor, `+0x18` `char name[16]`, UTF-8), `XamUserCheckPrivilege` writes 1 (all allowed), `XamUserAreUsersFriends`
writes 0. User 1 is signed in only with a Player 2 profile; other user indices: `ERROR_NO_SUCH_USER` (0x525).
`XamReadTileToTexture` serves the profile's picture as the gamer tile; `XamContentGetCreator` names the asking user as
every save's creator.
Weak stubs: `XamUserReadProfileSettings`, `XamUserGetDeviceContext`.

## Notifications

`XamNotifyCreateListener(mask)` returns a **waitable handle** (a `KernelObject` never signalled): **Wwise's init requires it**.
`XNotifyGetNext(listener, id, id out, param out)` returns FALSE.

## XMsg and XMP

The title's statically linked XMP library talks to the system's music player app through `XMsgInProcessCall(app, message, buffer, length)`
and `XMsgStartIORequestEx(app, message, overlapped, buffer, length, ?)`. Layouts as in Xenia's XMP app (BSD, attribution in
`kernel/xmp.cpp`).

| App | Message | Our answer |
|---|---|---|
| 0xFA (XMP) | `0x00070009` GetStatus `{client, XMP_STATE*}` | `*state = 0` (idle) |
| 0xFA | `0x0007001A` SetPlaybackController | success |
| 0xFA | `0x0007001B` GetPlaybackController `{client, controller*, locked*}` | controller 0 (the **title**), locked 0 |
| 0xFB | `0x000B0008` WriteAchievements via `XMsgStartIORequest` | see below |
| other | | one-time `[xmsg] unhandled message`, success |

**Write only through non-null output pointers** (`WriteOutput` reads the pointer from the message buffer). **Why it matters
[Lux]**: Wwise mutes its background-music bus (`0x0DFBD437` at registration, `0x825A1F38` [SSA]) unless
`XMPTitleHasPlaybackControl()` (`0x824FC430` [SSA]: `XMPGetPlaybackController`, both outputs 0) says the title has control. With an
output-less stub the check read stack garbage: persistent mute ratio 0, title music at -96 dB while SFX played (D9).

## Achievements

> Status: 2026-10-28. Code: `kernel/xam_achievements.cpp`, `host/collection/achievements.{h,cpp}` (the list), `host/collection/progress.{h,cpp}`
> (what is kept). Tests: `ArkeyanProgressTest`, `ArkeyanAchievementsKernelTest`. [Xbox], all three games.

**The list comes from the player's own disc.** A title's `default.xex` holds its XDBF resource (the resource named after the title ID,
read through the decompressed image, like the title's name and icon in `host/xex_title.cpp`): an `XACH` block (id, name, unlocked and
locked description, image id, gamerscore, flags), the strings of each console language (`XSTR` blocks, UTF-8, English where a language lacks a
text) and one PNG per achievement (64x64). `achievements::Read` parses it with bounds checks (the file is the player's); nothing is in git.
The flags' low three bits are the type; `0x8` means "shown while locked" (a hidden achievement has it clear: none of the three discs has one).

| Game (disc) | Title ID | Achievements | Gamerscore | Ids |
|---|---|---|---|---|
| Spyro's Adventure | `415608AB` | 38 | 1000 | 1 to 38 |
| Giants | `415608DA` | 45 | 1000 | 200 to 244 |
| Swap Force (USA, Australia) | `415608E9` | 39 | 1000 | 7, 26, 28 to 64 |

(read from each disc by `ArkeyanProgressTest`; the ids are what `XUserWriteAchievements` sends). Each has all of its pictures and French names.

**An unlock is kept.** `XUserWriteAchievements` = `XMsgStartIORequest(0xFB, 0x000B0008, overlapped, {count, XUSER_ACHIEVEMENT* {user, id}})`.
The runtime logs `[achievement] user U unlocked achievement N (<name>)`, adds `<id> <unix time>` to `achievements.txt` in the game's data
folder (`SKY_SAVE_DIR` in test runs, else `<data>/<Game>/`; written atomically, the first time of an id stays, so a repeat is silent) and
shows a notice in the Toy Box's toast, in the text language: "Achievement unlocked: <name>" or "Succès débloqué : <name>"
(the name from the XDBF in the game's text language: en, fr, de, es, it, pt; other languages show English). The list is loaded when first needed
and logs `[achievement] N achievements, M unlocked (gamerscore X of 1000)`.

**The enumeration answers from both.** `XamUserCreateAchievementEnumerator(title, user, xuid, detail flags, start, count, buffer size out,
handle out)` builds `XACHIEVEMENT_DETAILS` (0x24 bytes: id, label*, description*, unachieved*, image id, gamerscore, FILETIME of the
unlock, flags) for the achievements from `start`, at most `count`. The buffer size it writes holds the array and the strings; the
strings (UTF-16 big endian, only those the detail flags ask for: `0x1` label, `0x2` description, `0x4` unachieved) are placed **after** the
array in the title's buffer, because a title walks the array with a 0x24 stride. `XamEnumerate` fills it (`XamEnumerator::Total` and
`Fill`, overridden by `AchievementEnumerator`) and returns `ERROR_NO_MORE_FILES` once the items are handed out. An unlocked entry has
`0x20000` (`XACHIEVEMENT_DETAILS_ACHIEVED`) in its flags and the unlock time as a FILETIME; the type and the shown-while-locked bits are the XDBF's. So the title's own
check ([SSA] `sub_8264ACF8`: "every other achievement is unlocked" for achievement 1) sees what the player really did.
The first version of this page records why an enumerator must exist at all: with a stub the handle was uninitialised and the title crashed
reading guest `0x00000008`.

Not done: other users (only index 0 and 0xFF), other titles' achievements (`XamUserCreateAchievementEnumerator` ignores the title ID), the
online flags, `XamUserGetAchievementImage`-style calls (no title uses them). Not seen in play yet: whether each game's own flow unlocks (a
run to the first level of SSA did not unlock anything; the calls are tested on a fake guest context).

**Play time** (`progress::PlaySession`, started in `main.cpp` next to the save marker): `playtime.txt` in the same folder
(`total=<seconds>`, `last_end=<unix time>`, `sessions=<count>`), written when the game starts, at most once a minute from the main loop,
and at exit; a killed game loses less than a minute. It counts when the game starts from its icon or a shortcut too, as the runtime
counts it. (The Xbox UWP build does not run this loop: its entry point, `host/uwp/main.cpp`, does not call the session.)

Arkeyan Launcher reads the same files and the list (its disc check caches the list and the pictures, `achievements::WriteCache`): the Achievements page
and the play time on the game page, [its docs](../../release/launcher/README.md).

## Network

Offline, as if no cable: `XNetStartup/Cleanup` 0; `WSAStartup` writes version `0x0202`; sockets are handles that accept
`setsockopt`, `ioctlsocket(FIONBIO)` (non-blocking flag), `bind`; `connect` -> `WSAENETUNREACH` (10051); `send/sendto` ->
`WSAENETDOWN` (10050); `recv/recvfrom` -> `WSAEWOULDBLOCK` (10035) if non-blocking else `WSAENETDOWN`; **`XNetGetTitleXnAddr` zero-fills
the 36-byte XNADDR and returns `XNET_GET_XNADDR_NONE` (1)**. `XNetRandom` gives host random bytes. Every `NetDll_*` takes the
caller type first (`XNCALLER_TITLE`). Session code treats NONE as "no network" [SSA] (`sub_8254A4F8`).

**[SSA] gotcha D25: "STATE_GAME_FINALCHECKS lasts 22 s"** disappeared once `XNetGetTitleXnAddr` stopped staying "pending forever" and
XConfig/`XamGetSystemVersion` outputs were written (commit 4ee1adc). If a startup state hangs in another game, suspect an
async XAM/XNet call left pending or a stub with unwritten outputs first.

## Module and loader exports

See [loader_xex.md](loader_xex.md) (`XexGetModuleHandle`, `XexLoadImage`, `XexGetProcedureAddress`, `RtlImageXexHeaderField`).
Imports linked but never needed (weak stubs): `XamShowSigninUI`, `XamShowFriendsUI`, `XamShowMessagesUI`, `XamShowKeyboardUI`,
`XamShowMessageBoxUI(Ex)`, `XamShowDirtyDiscErrorUI`, `XamLoaderLaunchTitle/TerminateTitle`, `XamVoice*`, `XamSession*`.
