# The Toy Box link: the launcher's Toy Box window and the running game

> **Purpose:** how does the launcher's Toy Box window (a window of its own, beside the game, on a second monitor) put figures on the running game's portal, and what keeps that link local, versioned and harmless? · **Status:** current (2026-11-07) · **Tags:** [Xbox] [Lux] [Alchemy] [SSA] [Giants] [SwapForce] [TrapTeam] [SuperChargers] [Imaginators]

The player's ask (0.9.4): the Toy Box in its own window, to swap Skylanders faster, and a real portal for one player with a
virtual figure for the other. The window is Arkeyan Launcher's (its own page, toybox_window.md in its docs: the launcher started again with
`--toybox <game>`, its Collection page alone, live); this page is the game's side and the protocol both speak. The mixed
portal is in [portal.md](portal.md#mixed-portal-a-real-portal-and-virtual-seats).

## The design
- **The game is the server, the Toy Box the authority.** The window sends what the player did (put this figure on that seat,
  take it off, swap, clear); the game runs it with the in-game Toy Box's own actions (`toybox::Model::RemotePut` and its
  siblings call `PutOn`, `TakeOff`, `SwapPlayers`, `ClearPortal`, with every check of those: locks, the seat's families, a
  full portal, a real toy) and sends its seats back. So the window, the in-game Toy Box (F6) and the wheel always show the
  same portal, and the game's toast says each change as it does for its own.
- **Transport** (`host/toybox_socket.*`): TCP on `127.0.0.1` only, a port the system chooses. Nothing outside the computer
  can connect (the listener is bound to the loopback, and a peer that is not 127.x is closed at once). On the same computer,
  only a program that can read the game's link file has the token: `toybox.link` in the game's data folder (beside
  `portal.txt`: `SKY_SAVE_DIR`, else the player profile's folder, `PortalSessionFolder`), mode 0600, written at start and
  removed at exit. A socket file or a named pipe would differ per system; loopback TCP is one code for macOS, Windows and
  Linux (Winsock: `ws2_32`). A listener on the loopback alone should raise no firewall prompt [?]: the tester checks it on
  Windows and macOS with the firewall on.
- **One window at a time**: a second connection is answered `refused busy` and closed. A connection that does not say hello
  within 5 s is dropped, so a stray client cannot hold the place.
- **A game without the launcher works as today**: nothing waits for the window; with no window connected the per-frame cost
  is one atomic read (`toyboxremote::Connected`). `SKY_TOYBOX_LINK=0` turns the link off.

## The messages (`host/toybox_link.h`, version 1)
One line per message, tab-separated fields, `\n` at the end, at most 4096 bytes (a longer line closes the link). A field with
a tab or a line break is never sent.

| Window → game | Game → window |
|---|---|
| `hello <version> <token> <client>` (first) | `hello <version> <game id>`, or `refused version <its version>` / `refused token` / `refused busy`, then it closes |
| `put <seat> <path>` | `seat <i> empty` · `seat <i> virtual <path> <bottom path> <taken 0/1>` · `seat <i> real <toy type> <variant> <known 0/1>` |
| `off <seat>` · `swap` · `clear` | `sync <seat count>`: the seats sent since the last sync are the portal now |
| | `done <verb> <seat> ok` or `done <verb> <seat> <refusal>`: `real`, `unknown`, `busy`, `cannot_read`, `already`, `full`, `no_portal` |
| | `bye`: the game is closing |

Seats are the catalogue's (`games[].seats`), the same index on both sides. The game sends every seat after a hello, after
each command, and whenever its seats change (looked at 5 times a second while a window is connected). A message the game
does not know is ignored and logged once (a newer window). **Versions:** both sides speak `toyboxlink::Version`; another one
is refused with the game's number, and the window says which side to update ("The game is newer: update the launcher", "The
game is older: update it"); a game from before the link writes no link file, and the window says the same after 15 s.

## Code
- `runtime/host/toybox_link.{h,cpp}` (the messages, the link file) and `toybox_socket.{h,cpp}` (the transport) are shared
  word for word with Arkeyan Launcher (`src/shared/`, its `tools/sync_collection.sh`).
- `runtime/host/toybox_remote.{h,cpp}`: the game's end (a thread: listen, greet, queue the commands, send the lines).
- `runtime/host/toybox.cpp` (`ServeWindow`, in `ToyboxUpdate`): runs the commands on the main thread and sends the seats.
- `runtime/host/toybox/model.cpp` (`Remote*`): the commands as the Toy Box's own actions; `PutOn` records why it refused.
- Log: `[toybox-link] listening`, `a window connected`, `put P1: done`, `the window left`; the Toy Box's own lines say what
  the game did (`[toybox] P1: Spyro on slot 0`, `[toybox] P1: the game took Spyro`).

## How to verify
- `ArkeyanToyboxLinkTest` (in `tools/test_all.sh --quick`): the messages both ways, lines split and too long, the link file and
  its mode, then the game's end on the loopback against a fake window: another version, a wrong token, a second window, the
  commands in order, the seats out, a window leaving and another coming, a flood, `bye` and the file removed. [V]
- Arkeyan Launcher's `ArkeyanLauncherTest` (`SKY_LAUNCHER_TEST_TOYBOX_ONLY=1` for this part): its client against a fake game. [V]
- `LAUNCHER=<Arkeyan Launcher> tools/regress/toybox_window.sh [<game> ...]` (in `tools/test_all.sh` with `LAUNCHER` set):
  per game the launcher plays it with "Open with the game" ticked, its window puts a figure on Player 1 and Player 2, swaps
  them, takes one off and fills the game's own seat (Swap Force a swapper, Trap Team a trap, SuperChargers a vehicle), and
  the game's log must show each change and the game reading each figure; the window closes with the game. `MIXED=1` adds the
  mixed portal on Giants. Results of the 0.9.4 runs: see "Status".
- By hand: the Play page's On the portal card, "Toy Box window" while the game runs; move the window to another monitor,
  close it, open it again (it comes back where it was); put, swap and take off figures; the in-game Toy Box (F6) shows the
  same seats.

## Status (2026-11-07)
`LAUNCHER=... MIXED=1 tools/regress/toybox_window.sh` on macOS, the launcher playing each game with "Open with the game"
(the swap counts as seen when the game reads the new Player 1 after it; `CHECK_ONLY=1` checks the runs again):

| Game | Window opens with the game, P1 and P2 put on, swapped, taken off, read by the game, window closes | Its own seat |
|---|---|---|
| Spyro's Adventure | 13 of 13 [V] | |
| Giants | 13 of 13 [V] | |
| Swap Force | 13 of 13 [V]; after the swap the game reads Player 1 again (16 s: it was at a prompt) and had not read the swapper on Player 2 when it was taken off 16 s later: with one controller it reads Player 2 when it needs it [?] | the swapper is one card, two tags [V] |
| Trap Team | 13 of 13 [V] | the trap seat, read by the game [V] |
| SuperChargers | 13 of 13 [V] (steps 16 s apart: at 8 s the swapped Player 2 was taken off before the game read it) | the vehicle and the SuperCharger put on from the window before the intro read them [V] |
| Imaginators | 13 of 13 [V] | |
| Mixed portal | Giants: a virtual Player 1, then a real Player 2, 7 of 7 [V]; Spyro's Adventure: a real Player 1, then a virtual Player 2, 5 of 5 [V] | |

The pictures (`<run>/<game>/beside`) show the window beside the game at each step. With one controller the games say "too many
Skylanders" when Player 2's figure arrives (Player 2 needs a second controller: [toybox_ux.md](toybox_ux.md#who-plays)); they read
the figure all the same. Windows and Linux: the same code (Winsock / BSD sockets), which compiles for Windows (mingw-w64), not
run on a machine [?]. A real portal with the mixed portal: not run (no hardware here) [?]; what the tester checks is in
[portal.md](portal.md#mixed-portal-a-real-portal-and-virtual-seats).
