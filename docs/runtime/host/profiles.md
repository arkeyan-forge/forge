# Player profiles: what a game shows of who plays

> **Purpose:** the contract between the launcher and a game for the player's name, picture and XUID (`SKY_PLAYER_*`, `SKY_PLAYER2_*`), what the runtime does with them, and where each game shows them · **Status:** current (2026-11-05) · **Tags:** [Xbox] [Alchemy] [Lux] [SSA] [Giants] [SwapForce] [TrapTeam] [SuperChargers] [Imaginators]

The launcher's profiles (0.9.0) each have a pseudo, a square picture, a colour and a stable id, and their own Toy Box,
saves, keys and achievements: the launcher already points a game at them through `SKY_SAVE_DIR`, `SKY_COLLECTION` and
`SKY_SETTINGS`. The variables below make the game show the profile as the console's signed-in gamer profile.

Code: `runtime/host/profiles.{h,cpp}` (the variables, the names the fonts draw, the picture tile),
`runtime/kernel/xam_user.cpp` (the XAM calls that serve them), `XamContentGetCreator` in `runtime/kernel/xam_content.cpp`,
`game::Adapter::nameAccents` (`runtime/game.h`, set in `games/superchargers/adapter.cpp`). Test: `ArkeyanProfilesTest`.

## The contract

| Variable | Value | Unset |
|---|---|---|
| `SKY_PLAYER_NAME` | the first pad's profile name, UTF-8 (the pseudo, 1-15 characters) | `Player` |
| `SKY_PLAYER_PICTURE` | a PNG file's path (UTF-8; any size, square or not) | no picture: the game's own empty picture |
| `SKY_PLAYER_XUID` | 16 hex digits, not all zero | `E000000000000001` |
| `SKY_PLAYER2_NAME` | the second pad's profile name: **set and not empty, Player 2 is signed in** | user 1 not signed in |
| `SKY_PLAYER2_PICTURE` | as `SKY_PLAYER_PICTURE`, for Player 2 | no picture |
| `SKY_PLAYER2_XUID` | as `SKY_PLAYER_XUID`, for Player 2 | `E000000000000002` |

With none of them set a game sees exactly what it saw before: one offline profile "Player", XUID `E000000000000001`, on
the first pad, the second pad not signed in, no gamer picture. A bad XUID (not 16 hex digits, or zero) is the default one.
As every `SKY_` name, the older `SSA_` spelling is read too (`runtime/host/env.h`). The log names what was read without the
name or the path (problem reports carry the log): `[profile] user 0 signed in: a name of 13 bytes, xuid E000000000000001, a picture`.

**What the launcher passes.** For the first profile (the one the old saves belong to), `SKY_PLAYER_XUID` unset or
`E000000000000001`: every existing save keeps opening. For any other profile, an XUID derived from its stable id, the same
at every launch, in the offline range (`E0` then 14 hex digits), different from the other profiles'. The picture as a PNG
file (the profile's own square picture; the game scales it). For Player 2: its name, picture and XUID when Player 2 is
another profile; nothing for a guest.

**Saves.** A save folder (`SKY_SAVE_DIR`) belongs to one profile: `XamContentGetCreator` answers that the asking user made
every save in it, with that user's XUID. A save copied by hand from one profile's folder to another's is still read, as far
as the XAM side goes; a game that stores the XUID inside its own save file may still refuse it.

## Names

The games read the profile name as **UTF-8** (SuperChargers drew "Éloïse" from UTF-8, and drew boxes for Latin-1 bytes).
`profiles::GameName` keeps Latin letters, digits, spaces and the French accented letters (`àâäæçèéêëîïôöùûüÿ` and their
capitals; "œ" becomes "oe", "Ÿ" "Y"); anything else (an emoji, another script, punctuation) becomes a space, spaces are
never doubled nor at the ends, and a name with nothing left is "Player" ("Player 2" for the second pad). At most 15
characters, and at most 15 bytes (the profile's `char name[16]`): when the accents would not fit, the whole name loses
them ("Amélie Poulains" is "Amelie Poulains"). A guest buffer shorter than the name gets whole characters only.

A game whose name font lacks some accents lists the ones it has in `game::Adapter::nameAccents`; the others become plain
letters. Only SuperChargers needs it:

| Game | Accents its name font draws (frames `private/profiles/frames/*_accents*.png`) |
|---|---|
| SuperChargers | every capital; in lower case only `æéêö` (the other lower-case ones drew an empty box) |
| Imaginators, Swap Force | all of them |

## The gamer picture

`XamReadTileToTexture(type, title id, tile id, user index, buffer, pitch, height, overlapped)`. The games ask for the large
gamer tile of user 0 (type 2, a 64 x 64 texture, pitch 256, height 64). For a signed-in user and the gamer tile types 2 to 5
(large and small, and the "local" pair), the runtime decodes the profile's PNG once (stb_image), takes its largest centred
square, averages it down (or samples it up) to `height` x `height` (narrower if the pitch is), and writes **big-endian
A8R8G8B8**: the bytes A, R, G, B, rows `pitch` apart, linear. The format is proven in a frame: a picture of red, green, blue
and white quadrants with a yellow dot shows the right colours the right way up in SuperChargers, Imaginators and Swap Force
(`private/profiles/frames/*_name_picture.png`). Transparent pixels stay transparent. No picture, a file that is not an image,
or any other tile (type 9, a tile by key): the texture is cleared and the call fails with ERROR_NOT_FOUND, as before, and
the game draws its own empty picture.

## Where each game shows it

Runs with a name, a picture and a Player 2 profile through each game's usual route (private/runs/prof-*):

| Game | Name and picture | When |
|---|---|---|
| SuperChargers | top left of the front-end menus: the picture in a round frame, the name beside it (Player 1 only) | from the play-style screen (~36 s into a new game) |
| Imaginators | top left of the save slot and difficulty screens, square picture, name beside it (Player 1 only) | after the licence (~40 s) |
| Swap Force | top left of "Choose a savegame": the picture in a rounded frame, the name and "Skylanders: N" (Player 1 only) | the slot screen (~30 s) |
| Trap Team, Giants, Spyro's Adventure | not shown on the route from the title into the first level | SSA reads the sign-in info at boot; none of them asks for the name (`XamUserGetName`) or a tile there |

**Player 2 signed in changes no game's flow** on those routes: the same screens, levels and saves with and without
`SKY_PLAYER2_NAME` (SuperChargers' and Imaginators' new game and continue, `tools/regress/superchargers_lobby.sh` and
`imaginators_hub.sh` with the variables; Swap Force, Trap Team, Giants and SSA to their first level). No game asked for user
1's name or picture there; they are served the same way when a game does.

## The profile's own files
A profile other than the first also gets `SKY_PROFILE_DIR` (its folder) and `SKY_COLLECTION` (its Toy Box) from the launcher.
`datadir::PlayerGame(id)` is then `<SKY_PROFILE_DIR>/<Game>`: the game's saves (`savebackup::Root`), achievements and play
time (`progress::Folder`) and portal seats (`portal.txt`) live there; the built game, its caches and its log stay in
`datadir::Game(id)`. Unset (the first profile, a test run): `PlayerGame` is `Game`. `ArkeyanDatadirTest` checks both.

