# Toy Box and figure management: UX design

> **Purpose:** how should the Toy Box and the figure screens work (screens, portal model, wheel, controls, the collection API they need), and what of it is built? · **Status:** current (2026-11-06, the 0.9.0 look) · **Tags:** [Xbox] [SSA] [Giants]

Built in the game's Toy Box and in Arkeyan Launcher's Collection page (the 2026-10-26 redesign, the UX work here and
in Arkeyan Launcher; awaiting the owner's validation from the screenshots). Section 14 says what was built,
verified and left. [V] = verified (the title's code, a run), [?] = to be checked. Wireframes: `private/ux/` (not in
git). The code and the controls as built: [figures.md](figures.md).

## 0. The model (decided 2026-10-25)

What the owner found unclear ("it is not very clear, in game or in the launcher"; "icons are missing for the adventure
packs and items"), and what the redesign answers:

| Problem (ranked, review of 2026-10-26) | Answer |
|---|---|
| 1. The same figure two or three times on one screen (On the portal, Recent, Favourites, then its element) | **The shelf**: every figure once, favourites first (a star), then recent; the portal shows what is on it |
| 2. Too many controls: Add, Manage, View, search, 7 to 9 section chips, "For: Player 1 · view: Smart"; in game a chip strip, a View menu (Y) with 6 rows | **One bar**: search and three menus (Element, Game, Type), `+ Add a figure`, the rest behind `...`; in game Y searches, no View menu |
| 3. The portal is vague: "Portal for the next session", a lower-case seat label, "Giants has no runtime yet" (false), a paragraph of help | **"Portal at the next start"** with the game's seats in a row, what each holds and its level; one line says what to do; Giants can be prepared |
| 4. The figure sheet: 11 buttons at one level (launcher), 10 lines (game); file name, "type 14" on show | **Three actions** (Put on the portal, Edit, Favourite), the rest behind `...` / More, the file and numbers behind **Details** |
| 5. Magic items and adventure packs drawn as initials ("AR", "DP") or a plain shape | **Pictures for every kind**: the disc's own, else the family's picture from the disc (SSA's potion, compass, paw), else the same mark drawn |
| 6. The two apps used different words and lines ("Lv 6" / "Level 6 · 2400 gold", "Choosing for" / "For:") | **The same card and words** on both sides (section 10) |

The model:
- **The shelf** is the launcher's Collection: browse, create, edit and back up there.
- **The portal** says who plays. In game, the Toy Box opens when the game asks for a figure, and a figure is placed or
  swapped in two steps at most (seat, figure; the wheel: hold, aim). In the launcher, the portal of the next start is
  prepared the same way (seat, figure).
- **The same cards, words and order** on both sides.
- **The level is shown for the running game**: "Level 10 (15)" in Spyro's Adventure for a figure Giants took to 15,
  "Level 15 (20)" in Giants for one Swap Force took to 20, "Level 20" in Swap Force (caps 10, 15, 20).
- **Co-op**: a Toy Box per player (Player 1, Player 2); today one pad reaches the game (section 4).

## 1. Who uses it, and when

| Moment | Who | Device | Must take |
|---|---|---|---|
| Before playing: set up the team | the player, often a parent | launcher (mouse) or Toy Box (pad) | seat, figure: 2 inputs per seat |
| During play: swap a Skylander | player 1 or 2, mid-level | pad (TV, Xbox) or keyboard | **2 inputs** (wheel), 3 with the Toy Box |
| The game asks for a figure | the player | pad | it opens by itself on that seat: 1 input (A) |
| Tidy the collection | the owner, rarely | launcher (mouse, keyboard); Toy Box on Xbox | clear, undoable |

**The F3 menu** (volume, FPS counter, the settings a player changes in play, Report a problem) is one more screen of this overlay,
opened by F3 or View + Start: [gamemenu.md](gamemenu.md).

Gamepad first: every screen works with the d-pad, A, B, X, Y and LB/RB; nothing needs a keyboard, the mouse or a
trigger.

## 2. The screens

```
Toy Box (game, 1280x720; 0.9.0: beside the rail)          Collection (Arkeyan Launcher)
┌ rail │ ON THE PORTAL  Choose a place, then a figure   COLLECTION  14 figures       [+ Add a figure] [...]
│ Toy  │ [Player 1][Player 2][Magic item][Adventure] [+ Add] PORTAL AT THE NEXT START [Spyro's Adventure v]  Click a seat...
│ Box  │                                             [Manage][Player 1][Player 2][Magic item][Adventure]
│ ...  │ FOR PLAYER 1 · 12 FIGURES      [Search....] [Filters] [Search........] [Element v] [Game v] [Type v]
│      │ [card][card][card][card][card][card]       (Choose a figure for Player 1 · Cancel)  while choosing
│ PM   │ ...                                          Not for this game (2) [card]... (Arkeyan Launcher only)
└      │ A Put on Player 1  X Details  Y Search  LB RB Place  B Back
```

**The 0.9.0 look** (2026-11-06; [ingame_ui.md](ingame_ui.md)): the Toy Box is the first page beside the F3 menu's rail
(Toy Box, Achievements, Settings, Controls, Performance; [gamemenu.md](gamemenu.md)), in Arkeyan Launcher's greys with the game's
colour as the accent; six columns of cards. Every behaviour, key and row below is unchanged; B goes back to the F3 panel when
the panel opened the Toy Box, and closes it otherwise (F6, View + Y, the game's prompt). The rail's rows are click targets;
the pad reaches the other pages through the panel (LB / RB stay the seats here).

- **The seats are on top** in both apps, one tile per seat of the game (catalogue `games[].seats`): the seat's name, the
  figure's picture, its name and level for this game; empty: a `+` and "Empty", "A: choose" / "Choose".
- **Below, the figures**: in game, those the game reads that go on the chosen seat; in the launcher, the whole shelf
  (the bar narrows it), or, while choosing for a seat, only what goes there and the game reads.
- **Two steps**: pick the seat (A, click), pick the figure (A, click). LB/RB moves to the previous/next seat. Dragging a
  card onto a seat also works in the launcher.
- **The figure sheet** (X, F2, right click; a click in the launcher when no seat is being chosen): picture, name, the
  level for this game and its bar, gold and play time, last played, where it sits; actions below.
- **Add a figure**: the catalogue of the game (all six in the launcher), one card per character, the versions in a side
  panel, "owned x2"; the launcher's category chips became the same Type menu as the bar. A toy with versions wears a
  "+3" badge (the count leaves out the toy's own "base" entry); the side panel is titled "Versions", starts with
  **Standard** (made under the id the toy is sold with when the catalogue has one: Jet-Vac `0x1000`, the Giants'
  LightCore ones `0x1206`, Swap Force's `0x2000`; a swapper's halves too) and never lists a `base` entry as a second row
  (the toy's own name again: 85 toys have one, 22 of them Giants'; `ArkeyanCollectionTest`); each row has the version's picture (the toy's own when it has none)
  and its kind ("Series", "Eon's Elite", an unknown kind shows as "Other"). On a shelf card and the figure sheet the
  variant's name is the figure's name and its kind is a tag. The game's Add screen has no search (it is browsed by
  section); the shelf's search (Y) already finds a variant by its name. Menus and rows that open or unfold something
  (More, Details) end with a **drawn chevron** (`Chevron` in `toybox/draw.cpp`: two lines, no font glyph).
  **Artworks** (2026-10-26): the figure sheet's big picture and the Versions column of Add a figure take
  `collection::BestPicturePath` (a variant's own artwork, else the toy's, else the round portrait): `ArtworkPicture` in
  `toybox/draw.cpp` fits the not-square artwork into its box. The shelf's small cards keep the round portraits (an
  artwork cropped to a circle at 80 px loses the figure's silhouette). Clicking a section chip in Add a figure now
  refreshes the Versions column (it showed the previous toy's).
  **Upgrades** (2026-10-26): the Edit sheet's "Upgrades" row (count, e.g. "4 of 11") opens its own screen
  (`Screen::Upgrades`, `DrawUpgrades`, `RunUpgrades`): the toy's upgrades by name from `collection::Upgrades`, the basic
  ones (with the Soul Gem and the Wow Pow under them) beside the two paths, the chosen path lit and the other greyed;
  A buys or gives back, All / None / OK below, the focused upgrade's description (one sentence) above them. The 16
  numbered boxes and their "not known yet" text are gone. The figure sheet shows a name once and no "Last played" for a
  figure the game never wrote.
  Texts follow the same short-text rules as the launcher (one short help line, no path in running text, none when the
  label says it); the Add screen has no help paragraph, the Edit sheet no backup note (the upgrades block waits for its
  own round).
- **Manage**: trash, backups, import and export, unknown toys, tidy (the launcher's `...` menu opens it on each).
- **The wheel** (game only): hold View (Tab), aim, release (section 5).

## 3. The shelf: order, filters, search

One order everywhere, `collection::Group(list, GroupBy::Smart, OrderBy::Recent, game)` (section `Kind::Shelf`):

1. **favourites** (a star on the card), newest use first;
2. then the figures **this game took**, newest first (`Figure::LastUsedIn`);
3. then the rest **by element** in the menus' order (Magic, Earth, Water, Air, Fire, Life, Undead, Tech, Light, Dark),
   then by category (items, adventure packs...); a character's variants next to it (Spyro, then Legendary Spyro).

**Every figure shows once**; what is on the portal is marked (P1, P2, "On" for the shared seats) and shown in its seat
tile, not repeated. After the shelf: **Unknown toys** (a type the catalogue lacks), then, **in Arkeyan Launcher only**, **Not for this game**
(titled and greyed), so a player who owns Giants figures does not think they are lost. **The in-game Toy Box hides what its game
cannot read** (a Swap Force figure in Giants; 2026-10-28, `Model::BuildList`): the figures stay in the collection and show in
Arkeyan Launcher, whose Filters row **Works in** keeps the toys one game reads. A figure the game does not tell apart shows as the game
sees it (a LightCore Warnado in Giants is "Warnado"; [collection.md](collection.md#what-each-game-reads-compatibility-2026-10-28)).
**Add a figure**: the plain toy is one row, named as the toy is (no "Standard" row), then only the versions that game tells apart.

Filters:
- **Toy Box**: the chosen seat decides the family (a player seat shows characters, Magic item shows items...); the bar
  below.
- **Launcher**: the same bar, below.
- The View menu (group by element, game, category, series, level, name; order; show) is gone from both screens: the
  shelf's order serves play, the bar serves browsing. `collection::GetView`/`SetView` and the other `GroupBy` values
  stay in the library (tested, stored in `collection.json`), unused by the screens.

### Search, filters and sort (2026-10-27, the user's four wishes and "lighter")

**One bar, the same words and order in both apps**: the search field first and wide, then one **Filters** button.
Nothing else sits on the line (the Element, Game and Type menus are gone from it).

| | Toy Box | Launcher |
|---|---|---|
| Field | a field with a magnifier and "Name, element, game, level..." / "Nom, élément, jeu, niveau..." (the pad's **Y** glyph, F3 on a keyboard, shown inside it); **typing on a keyboard goes straight into it**, Y or a click opens the on-screen keyboard; an x clears it | an ImGui field, same hint; Y, F3 or a click focus it |
| Filters | a button (**Start** on the pad, F4, or a click): "Filters · 2" when two are on | a button with the same label, opening a popup |
| The panel | four rows, **Element, Game, Type, Sort by**, then **Clear all**; Up/Down: row; Left/Right (or A, or a click on an arrow): the value, "All..." first; B or Start closes; a click outside closes | the same rows, each a drop-down; the panel closes on a click outside or B |
| Tags | under the bar, only when something is on: one small tag per filter with an x ("Fire x", "Swap Force x", "Sort: Name x"); then **Clear all** and the count at the right ("12 figures", "1 figure"; French "12 figurines", "0 figurine"); no "+ Add a figure" card among results; nothing found: "No figure matches" | the same, tags are focusable (A removes) |
| Pad | Start: the panel; B clears a typed search, else closes the Toy Box; tags have no pad focus (Clear all or the panel) | tags and Clear all are in the D-pad's reach (Up from the shelf: the bar) |

**Sort** (`collection::Sort`, in the panel as **Sort by**): **Recent** (the default and today's shelf: favourites first,
then what the game took last, then by element), **Name** (A to Z, accents ignored), **Level** (highest first, toys with
no level last), **Element** (the menus' order), **Game** (the catalogue's order, SSA first). The other four ignore
favourites; ties by name. Sort is not a filter, but it shows as a tag when it is not Recent, and Clear all resets it.
A swapper is one card in every order.

**The smart search** is `collection::SearchMatches(query, card, game)` (`search.h`, one function, unit-tested in
`ArkeyanCollectionTest`; the Toy Box and the launcher both call it). The query is words, **all** must match; case and accents
do not matter ("Étincelle" = "etincelle"). Both languages are always understood, whichever the app shows. A word matches:
- the inside of a name word: the figure's label, nickname, name, version name, edition or file name ("gill", "ill gr");
- an **element** ("fire", "feu", "lumiere", "mort vivant"), a **game** ("giants", "geants", "swap force", "ssa", "spyro"),
  a **toy type** ("item", "objet", "magic item", "objet magique", "adventure", "aventure", "pack", "mini", "sidekick",
  "trap", "piege", "trap master", "swapper", "top", "bottom"), a **version** ("legendary", "legendaire", "lightcore",
  "dark", "gold", "dore", "cristal"...), by the start of the word;
- **levels**: "level 10", "niveau 10", "lvl 10", "lv10" (a character at that level: the level the card shows in the game,
  "Level 10 (15)" counts as 10 in Spyro's Adventure, or its own, 15); "level" alone: any toy that has a level; a figure
  never played has none;
- **a typo** in a word of 5 letters or more (one letter added, missing, wrong or two swapped: "giatns", "lightcroe",
  "etincele"); shorter words must be right.
A swapper matches on either half (its top's name, the bottom's name, its toy types, both halves' versions); the card
stays one. "magic item" and "swap force" are read as the toy type and the game, not as the element Magic.

Checked: `ArkeyanCollectionTest` (`TestSearch`: ~150 EN/FR, accent, typo, level, swapper and sort cases),
`tools/regress/search_toybox.sh` (Swap Force through the real game: typing, the panel by pad, a tag's x, Clear all, with
screenshots EN/FR; the log's "[toybox] shelf: N found for ..." lines give each step's count) and Arkeyan Launcher's
`ArkeyanLauncherTest` (the shelf's search and sort through `CollectionModel::Shelf`; screenshots `47-` to `52-`).

## 4. The portal model

### Seats per game (catalogue `games[].seats`)

| Game | Seats |
|---|---|
| Spyro's Adventure | Player 1 · Player 2 · Magic item · Adventure |
| Giants | Player 1 · Player 2 · Item or adventure (Giants has no packs of its own; SSA's share the item seat) |
| Swap Force | Player 1 (a swapper, one card of two tags, or one figure) · Player 2 (idem) · Magic item · Adventure pack |
| Trap Team | Player 1 · Player 2 · Trap · Magic item · Adventure pack |
| SuperChargers | Player 1 + Vehicle 1 · Player 2 + Vehicle 2 · Magic item / trophy |
| Imaginators | Player 1 · Player 2 (Sensei, Imaginator crystal or any older figure) · Magic item · Adventure pack |

How many items or packs a game reads at once and how it numbers its players are to be checked per game [?]; the seats
only describe what the menus show. The emulated portal keeps its 16 hardware slots underneath.

### Swappers: one card, two tags [SwapForce] (built 2026-10-27)

A Swap Force swapper is a top and a bottom, two dumps linked in `collection.json` ([collection.md](collection.md),
"Swappers"). Everywhere it is **one card**: the shelf, the seat tile, the sheet, the wheel (a place holds the card), Add a figure (one card per swapper, "Boom Jet"; creating it makes both dumps,
linked), search (either half finds it).
- **The picture** is the game's swap screen: the top's artwork above, the bottom's below, meeting at the waist, the whole
  swapper, matched or mixed, each version with its own halves (`collection::SwapperPicture`: each half cut to its visible
  pixels, both at the same width, stacked; shared with Arkeyan Launcher, [game_art.md](game_art.md), "The rule for a toy's
  picture"). Drawn on cards, seats, the sheet (with its backing, as an artwork), the wheel and Add a figure (`draw.cpp`:
  `SwapperStack`, `CardPortrait`, `HeroPortrait`). A half with no such picture (the launcher has not read the disc yet) falls
  back to the top's round portrait with the bottom small at its corner (`BottomBadge`). The log says which
  (`[toybox] swapper picture Blast Zone (top) + Blast Zone (bottom): stacked`).
- **Put on / take off.** A on a swapper's card on a player seat puts **both halves** on the emulated portal, the top in
  one hardware slot and the bottom in the next; taking it off (Delete, the seat's options, picking another card, swapping
  players, Clear the portal) removes both. A seat is "taken" when the title read both tags. Both dumps are locked and
  touched. If the portal has no room for two, nothing is placed ("The portal is full").
- **Which slots.** `FreeSlot(seat, half)`: the seat's own slots if free, else by seat order so that the title, which
  reads players in slot order [?], finds player 1 below player 2 whichever was put on first: in a game that has swappers
  **P1 = slots 0-1, P2 = 2-3, the magic item 4, the adventure pack 5**, the bottom in the slot after the top; the other
  games keep one slot per seat (the seat's index). The first free slot is the fallback. **How the title reads two tags
  for one player** [V] (status.md, the swapper rows): it takes a top and a bottom on the portal as one character (a swapper with
  matching halves plays as that swapper, mixed halves as the mixed one; a lone half is not a character and a second
  character for the same player is "too many": `games/swapforce/docs/status.md`); which player a pair belongs to when two
  characters are on the portal is not read from the tags but from the order [?] (only player 1 was run with a swapper).
- **Pairing what is already on the portal.** At start (`portal.txt`, `SKY_PORTAL_PLACE`) or after F7, the Toy Box gives
  each tag a seat; a top whose bottom (its collection link) is on the portal too takes both tags into one seat, and a
  bottom waits for its top. A bottom with no top on the portal, or a half that is not linked, takes a seat of its own.
- **The Figure page** of a swapper (the top's page) shows both halves ("Top: ... · bottom: ...") and has **Change the
  bottom** (**Changer le bas**; on a lone bottom **Change the top** / **Changer le haut**; the hint says "mix halves" or
  "none yet"). It opens a list of every other dump of that half (one card each; the ones on a seat or in use are left
  out); A takes it: the card keeps its half and has the new one, and a swapper that lost the half it picked is given the old
  one. A card on the portal is lifted, changed and put back, so both tags are the new ones. Edit stats, Reset, Back up and
  Rename act on the top; Delete sends **both** halves to the trash ("hold A").
- Real portal: the seats show the tags as they come (a swapper's two tags on two seats, read-only); the pairing needs the
  collection's link, which a physical toy does not have.

### A trap's villain [TrapTeam] (built 2026-11-02)

A trap's Figure page has, under Put on / Take off, a **Villain** row (**Méchant**) and an **Evolved** switch (**Évolué**),
the in-game twin of Arkeyan Launcher's Edit ("villain list + Evolved switch", 0.7.1).
- **The choices** are the villains of the trap's element from the game's own table (`Game::villains`, in its order;
  a Kaos Trap offers Kaos): `toybox::TrapVillains` (`host/toybox/trap_villain.h`), the same list the page's villain strip
  shows. Every villain of the table has a quest, so every one has the switch; with no villain held it says "choose a
  villain first".
- **Keys.** On the Villain row Left / Right (the pad's d-pad and stick) step through the choices, round the ends; the
  row shows the choice and "A: put it in the trap", the strip rings it in gold. A puts it in; A with nothing new chosen
  shows the next one. On Evolved, A, Left or Right flips it and writes at once. **Mouse:** the row's two arrows step, a
  click on a villain's picture in the strip puts that villain in.
- **The write** is the shared library's (`collection::SetTrapVillain`: `figures::SelectTrapVillain`, then
  `WriteTrapStats`, a backup first). A villain chosen again keeps its own record, so its evolution
  (`toybox::KeptEvolved`); the other villains stay in the trap. The Toy Box refuses a dump in use by another process.
- **On the portal.** The game reads a trap when it arrives and keeps what it read, so a trap on the emulated portal is
  lifted and put back in its slot around the write (`Model::PutVillain`, as Edit stats does): the game reads the new
  villain, LT plays it. A real portal's trap cannot be lifted by the Toy Box: while a trap of that toy is on it, the row
  says **"Take the trap off the portal first"** and writes nothing, as Arkeyan Launcher's Edit needs the trap free.
- **Proof.** `ArkeyanToyboxTrapTest` (the choices of each of the 11 traps, all 46 villains once, the step, the write, the
  switch, a villain chosen again keeping its evolution). `tools/regress/trapteam_villain.sh [<run>] [en|fr]`: Gusto and a
  Water Trap holding the Gulper in the tutorial, LT plays the Gulper, the trap's page goes to Chill Bill, A, and LT
  plays Chill Bill (`\character\1019_*.arc` opens).

### Prepared in the launcher, live in the game

- The launcher prepares **the portal at the next start** of a game that can start with prepared seats (one with a
  runtime in the catalogue: SSA, Giants): `portal.txt` in the game's data folder (`SKY_SAVE_DIR` in tests), seat *i* in
  slot *i*. The selector next to "PORTAL AT THE NEXT START" lists those games only.
- While that game runs, the title says **"ON THE PORTAL NOW"**, the seats are read-only ("Spyro's Adventure is running:
  change them in its Toy Box") and the figures it holds read "Playing now".
- The runtime shows its own short messages through the same toast (`ToyboxNotice`, any thread): "30 FPS: 60 not held" once per
  run when `fps_limit=60` falls back to 30 ([video_vblank.md](../xbox360/video_vblank.md)). An achievement unlocked has its own
  card with the title's picture (`ToyboxAchievement`); the look and motion of both: [ingame_ui.md](ingame_ui.md).
- In game the seats are **live**: a change places the figure at once, a toast says it for 5 s ("P1: Gill Grunt → Bash").
- **The Toy Box window** (0.9.4): the launcher's Collection page in a window of its own while the game runs, its seats live
  and editable over the link to the game ([toybox_link.md](toybox_link.md)); the in-game Toy Box and the window show the same portal.

### Who plays

The emulated portal assigns seats by order, as a real portal does (first Skylander = player 1) [?] (lift player 1's
figure while player 2 plays, then place another). A swap is lift + place in the same hardware slot in one step
(`PortalPlaceFigureAt`), so the game never sees an empty portal. **Player 2** is a seat in both apps; in game it says
"needs a 2nd controller" while XAM user 1 has no device ([input.md](../xbox360/input.md); co-op design: [collection.md](collection.md#co-op-a-toy-box-per-player-design-the-code-has-one-player-today)).

### Remove or replace

- Game: on a seat, A chooses for it, X opens its options (take off, swap players, figure, clear the portal), Delete takes
  it off. Picking a figure for an occupied seat replaces it; picking the other player's figure swaps the players.
- Launcher: the X on a tile, Delete or the pad's X on a focused tile, the tile's right-click menu (Take it off, Choose
  another, Open its sheet), or dragging it off the tile onto the shelf. "Missing figure" for a seat whose dump is gone.

### Locks, a real portal

- A figure on a game's portal is locked (`<dump>.lock`, [collection.md](collection.md)); the launcher refuses to write it
  (Edit, Reset, Delete disabled; favourite and name stay editable); "Mark as not in use" (held) breaks a stale lock.
- A real portal: the seats show what is on it, read-only, from the title's traffic; seat options: **Save a copy to My
  Skylanders** [V] (the title reads 32 of 64 blocks, mask `88889FF88889FF8B`). A real portal with virtual seats (one
  player real, the other virtual): [portal.md](portal.md#mixed-portal-a-real-portal-and-virtual-seats) (0.9.4).

## 5. In-game quick swap: the wheel

Hold **View** (Back on a 360 pad; Tab on the keyboard) for 0.25 s; a short tap still reaches the game as View.

```
            [pin 1]
     [pin 8]        [pin 2]
  [pin 7]   (current)   [pin 3]
     [pin 6]        [pin 4]
            [pin 5]
```

- Eight stable places around the seat's figure: pinned ones keep their figure (muscle memory), empty ones fill from the
  seat's recent figures in this game and keep them. An empty place is a dashed ring, with no words.
- Each place shows the figure's picture, name and **the card's level line** ("Level 6"), or "playing now", or "P2: swap
  players".
- Aim with the stick, the d-pad, the arrows or 1-8; once aimed, the stick springing back keeps the aim. Release View/Tab,
  A or Enter swaps; B/Escape or never aiming cancels. Y/F3: the seat's full list; LB/RB: the other seats; X/F2 held
  0.6 s pins the current figure there.
- The wheel does not pause the game; the player's input is withheld from the game while it is open.
- **The player's own wheel** (2026-10-28). Why "I could not load Spyro": the wheel offered only the figures the game had *taken*
  (its recents), so a figure never played in that game was never on it, and a pin was three screens deep. Now a figure's page has
  **Add to the wheel** / **On the wheel** (the third action in the Toy Box; Arkeyan Launcher: the sheet's `...` menu, "On the wheel: <game>", and a
  fact row listing the games). It goes on the wheel of every player seat of the game that reads it (first empty place, else
  the first place the recents filled; a swapper's card is its top); removing it keeps the recents from bringing it back
  (`views.<game>.wheel_skipped.<seat>` in `collection.json`). The wheel shows the player's figures (pinned places, stored per game and
  seat as before, so older pins stay), then fills what is left from the recents. All eight places pinned: "The wheel is full". The old
  "Pin to the wheel…" (More) still picks the place. API: `OnWheel`, `SetOnWheel`, `GetWheelSkipped`
  ([collection_api.md](collection_api.md)). Check: `tools/regress/wheel_toybox.sh` (Giants: adds Spyro, holds Tab, aims 1, releases:
  "P1: Whirlwind → Spyro").

## 6. Collection management

| Task | Toy Box | Launcher |
|---|---|---|
| Create | Add a figure: character, version, name (optional), "Put it on Player 1" | Add a figure (six games, Type menu, search) |
| Edit | Figure → **Edit**: level, XP, gold, hero points, nickname, hat (Giants) | sheet → **Edit**: the name in the lists and the same stats, in two columns |
| Rename (the label) | Figure → More → Rename (Edit for items) | in Edit |
| Favourite | Figure → Favourite | sheet → Favourite |
| Duplicate, back up, restore | Figure → More | sheet → `...` (Restore a backup: a submenu with date, level, gold) |
| Reset to new, delete | Figure → More, hold A | sheet → `...` → a red bar with **Hold to reset / Hold to delete** |
| On the wheel | Figure → **Add to the wheel** (More → Pin to the wheel picks the place) | sheet → `...` → **On the wheel: <game>** |
| Trash, backups, import, export, unknown toys, tidy | Manage | `...` → each one |

Every write the menus make backs the dump up first (the newest 10 kept); Delete goes to the trash for 30 days; the 37
never-played starters older builds made are not the player's (tidied once, "Put them back").

**Empty states**: "My Skylanders is empty" (Add a figure, Import figure files, Start with Spyro, Gill Grunt, Trigger
Happy); a seat nothing goes on: the `+ Add a figure` card and "None yet: N exist for this game" (game), "Nothing for this
seat yet" with `+ Add a figure` (launcher); a search that finds nothing: "No figure matches" with "Clear the search and
the filters".

## 7. A picture for every kind of toy

`collection::PortraitPath(game, toy, variant, large)` ([game_art.md](game_art.md#the-rule-for-a-toys-picture)), the
same in both apps:

1. the toy's own picture from the disc of the game being viewed (its variant's if it has one: Giants' Series 2,
   LightCore, Legendary decos);
2. else from another game's disc (Jet-Vac on SSA's page shows Giants' portrait; SSA's Anvil Rain shows Giants' item art);
3. else **its family's picture** from the discs (catalogue `games[].kind_pictures`, SSA's `uigameimage.str`):
   `elementiconmagicitem` (a potion) for magic items, `elementiconlocation` (a compass) for adventure packs,
   `elementiconpet` (a paw) for minis and sidekicks [V: the SSA disc alone now gives every toy it reads a picture:
   `ArkeyanArtTest`];
4. else the **drawn icon**: the category's shape in the element's colour, with the variant's badge, and for those three
   families the same mark drawn (`collection::KindGlyph`: potion, compass, paw); characters get their initials in the
   launcher. Never an empty square.

| Kind | SSA disc only | With Giants' disc too | No disc |
|---|---|---|---|
| Characters, Legendary, Dark | 37 `toy_*_hud` / `*_vs` | Giants' portraits for its own and the variant decos | coloured shape, initials, badge |
| LightCore (Giants) | the character's, with the LightCore badge | `<name>_lightcore` | shape and badge |
| Magic items | 4 own (potion, swords, shield, boots), else the potion family icon | every item's own (`magicitem_*`) | the potion mark |
| Adventure packs | the compass family icon (no per-pack picture on SSA's disc) | every pack's own | the compass mark |
| Sidekicks (minis) | their counterpart's (Terrabite shows Terrafin: catalogue `names`); Giants' own four: the paw family icon | Giants' `*_sidekick` | the paw mark |

## 8. Controls

Same verbs everywhere: **A** choose, **B** back, **X** the figure (or a seat's options), **Y** search, **LB/RB** the other
seat (Edit stats: by 10), **View** hold = wheel, **View + Y** = Toy Box.

| Screen | Gamepad | Keyboard | Mouse |
|---|---|---|---|
| In game | hold View: wheel; View + Y: Toy Box | hold Tab: wheel; F6 (or tap Tab): Toy Box; F7: add the folder's next figure; F8: clear the portal (the three are editable, below) | — |
| Toy Box: seats (top row) | left/right: seat (it is chosen at once), then Add a figure and Manage (one above the other: up/down between them); A: choose for it; X: options; down: the bar; B: close | arrows, Enter, Delete: take off, Tab / Shift+Tab: next / previous row, Escape | click a seat; right click: options |
| Toy Box: bar (search, Filters) | left/right: the search field, Filters; A: the on-screen keyboard / the Filters panel; up: the seats; down: the figures; LB/RB: other seat; B: clear the search, else close | arrows, Enter, Tab, Escape; typing goes into the field from any row | click the field, Filters, a tag's x |
| Toy Box: figures | d-pad: move (left/right stay in the row; up/down keep the column); up from the first row: the bar; A: put on the seat; X: the figure; LB/RB: other seat; Y: search; Start: Filters; B: close | arrows, Enter, F2, Page Up/Down, F3, F4: Filters, Tab, type: search, Escape: clear / close | click: focus; double click: place; right click: the figure; wheel: scroll |
| Figure | up/down: action; A: do it; B: back (out of More first); LB/RB: previous/next figure | arrows, Enter, Escape | click |
| Wheel | either stick: aim; release or A: swap; B: cancel; Y: list; LB/RB: other seat; hold X: pin | arrows or 1-8; release Tab or Enter; Escape | point, click |
| Launcher Collection | d-pad (down from a seat to the shelf, up from its first row to the seats, the bar's search, Filters and tags on the way), A, B: stop choosing / clear the search, X: the sheet, Y: the search, LB/RB: choose for the other seat | Tab, arrows, Enter, Delete (seat), F2, F3, Page Up/Down, Escape | click, right click, drag a card onto a seat or a seat onto the shelf |

- **Mouse everywhere in the Toy Box and the F3 menu** (2026-11-02): every card, seat, button, row and field is a click target, and
  the one under the pointer gets a soft outline. The wheel scrolls the lists (Edit stats: steps; the F3 menu: moves the focus).
  Script checks: `SKY_TOYBOX_SCRIPT` `move=x/y`, `wheel=up|down` beside `click=x/y` (`tools/regress/f3mouse.sh`;
  [gamemenu.md](gamemenu.md)).

### Navigation (2026-11-02, the owner's Xbox-style spec)

- **Rows, top to bottom**: the seats (then Add a figure and Manage); the bar (the search field, Filters); the grid. Up and Down
  change row; each row **remembers its place** (`Model::column`, `barColumn`, `list.cursor`) and is entered there; a row not
  visited yet, or the grid rebuilt for another seat while the focus was above it, takes the thing **nearest under the focus**
  (the drawing records each row's x extents: `seatSpans`, `barSpans`, `gridSpans`). In the grid Left and Right stay in the row,
  Up and Down keep the column through a short last row (`Grid::keep`). LB/RB change seat from any row and never wrap round the
  end; Left/Right never wrap either. Tab / Shift+Tab: the next / previous row, round the end (the Toy Box's open key still
  closes it). The logic is `runtime/host/toybox/nav.h` (pure, checked by `ArkeyanToyboxLookTest`); `RunHome`, `RunBar`,
  `HomeStep` in `model.cpp` apply it.
- **One focus style** (0.9.0): a 2 px ring in the game's accent on the element's own edge (`FocusRing`), drawn after the grid's
  clip so the top row's ring is whole; the focused element takes the raised grey. The chosen seat is raised with its name in the
  accent; the pointer's hover is a thin grey line just outside the element.
- **The pointer moves the focus only while it moves**: moving onto a card, a button, the bar, a row of a list or a dialog's button
  focuses it (`Hit::focus`, `Pointer` in `draw.cpp`); a still pointer never takes the focus back from the keys, and moving within
  the same element does not either. The seats and Manage's sections are choices: the pointer only outlines them (a click chooses).
- **Buttons whole**: "+ Add" and "Manage" are as wide as their longer label in the game's language; the seats share the rest
  (five in Trap Team and SuperChargers: a smaller picture). On Add a figure the "+N" versions badge sits on the card's second line, so the name keeps the card's width.
- Proof: `ArkeyanToyboxLookTest` (the moves), `tools/regress/toybox_nav.sh` (the real game: 31 focus checks and 12 shots; its
  places follow the 0.9.0 layout, six columns); the before/after screens in English and French: `private/toybox-pass/`
  (`INDEX.md`, not in git); the 0.9.0 screens: `private/runs/overlay/` (not in git).
- **The Toy Box's keys are editable** (F3 > Commands, and Arkeyan Launcher's key page): `[input] key_toybox` (F6; while it is not set Tab
  also opens it, tap, and opens the wheel, hold; once set, that key does both), `key_toybox_add` (F7), `key_toybox_clear` (F8), every
  game (`ToyboxSetKeys`, `PortalSetHotkeys`).

## 9. Mapping to the collection API (`collection_api.md`)

Seats (`Game::seats`), ownership and the tidy (`UntouchedStarter`, `Tidy`), recents (`Touch`, `Recent`,
`LastUsedIn`), the shelf (`Group`, `Section::Kind::Shelf`), the card's level (`LevelText`, `HasProgress`), trash,
duplicate, reset, kept backups, import, export, the wheel's places (`GetWheel`/`SetWheel`), pictures (`PortraitPath`,
`GameKindPicturePath`, `KindGlyph`, `GeneratedIcon`), the portal at a glance (`PortalOverview`). The launcher's own
model adds `ShelfFilter` and `CollectionModel::Shelf` (the bar's filters and "choosing for a seat", tested by
`ArkeyanLauncherTest`).

Runtime only (not the library): seat ↔ hardware slot, "waiting for the game" (title reads), the wheel, the toast,
input routing, the on-screen keyboard.

## 10. The same card and words

| | Toy Box | Launcher |
|---|---|---|
| Card | picture, name, level line, star (favourite), seat tag (P1, P2, "On"), lock | the same, plus the variant badge's name |
| Name | the label, else the toy's name with the nickname the game shows: "Gill Grunt (Splash)" | the same |
| Level line | "Level 6", "Level 10 (15)", "New", "Unreadable"; nothing for items and packs | the same (`collection::LevelText`) |
| Seats | Player 1, Player 2, Magic item, Adventure (Giants: Item or adventure) | the same |
| Placing | "Put on Player 1"; "Choose for Player 1" | "Put on the portal"; "Choose a figure for Player 1" |
| A game has it | "in use by Arkeyan Launcher" (another program) | "Playing now" (the game's portal) |
| Main actions | Put on, Edit, Favourite; More…; Details | Put on the portal, Edit, Favourite; `...`; Details |

French in game: "Niveau 10 (15)", "Joueur 1", "Objet magique", "Aventure", "Choisir pour", "Plus…", "Détails".

**Languages (2026-10-26)** [V]: the game's Toy Box follows the game's text language (`SKY_LANGUAGE`, `[ssa] language`): every
text is `L(english, french)` in `runtime/host/toybox/` and the redesigned screens are fully French (screenshots checked);
another language shows English, and adding one would mean turning `L` into a table lookup. The launcher's Collection page
goes through `Tr` (Arkeyan Launcher's translations page), with the library's French asked for by a flag (`LevelText`,
`SeatTag`, `PortalOverview`). Toy and version names from the catalogue, Giants' hat names and a figure's own label stay as
the data has them.

## 11. What others do, and what we take

- **Skylanders games**: the toy on the portal is the choice; lifting one and placing another swaps at once. Taken: the
  swap is immediate and per player; the catalogue belongs in its own screen.
- **Switch Imaginators/SuperChargers**: owned characters picked in an in-game menu. Taken: the picker, sorted.
- **Character and weapon wheels** (LEGO games, shooters): hold, aim, release; stable places. Taken: the wheel and pins.
- **Disney Infinity base, LEGO Dimensions pad**: zones with a meaning. Taken: seats instead of numbered slots.
- **Nintendo's amiibo settings, Tagmo**: owner nickname, erase behind a confirmation; a library separate from the
  database of every toy. Taken: reset behind a hold, nickname separate from the label, the shelf vs Add a figure.
- **Dolphin and RPCS3 portal windows** (read only): numbered slots and file pickers, which do not work on a TV: left behind.

## 12. To verify in the game

1. Player numbers: who becomes player 1/2 on lift and place, in SSA (and later per game).
2. Two figures of the same character for both players.
3. How many magic items and adventure packs SSA reads at once; adventure packs outside the hub.
4. Whether sending Start before the Toy Box pauses safely, and resumes on close.
[V] already: a figure placed during a cutscene is read at once (0.1 s); the title reads 32 of 64 blocks of a real toy.

## 13. Open questions for the owner (2026-10-26)

The questions of this redesign, each with its recommendation: `private/ux/README.md` (not in git). The 2026-10-24 answers stand: a figure the player created counts as theirs; hold View (Tab) for the wheel,
which does not pause; one card per figure; a real portal is read-only with "Save a copy"; Player 2 designed in.

## 14. The build (2026-10-26): what was done, verified, left

| Part | Where |
|---|---|
| The shelf, the card's level, the family pictures and marks | `runtime/host/collection/collection.*` (`Group` Smart, `LevelText`, `HasProgress`), `icons.*` (`GameKindPicturePath`, `KindGlyph`), `catalogue.*` (`kind_pictures`), `resources/collection/catalogue.json` (SSA's family pictures; Giants' runtime), `art/game_art.cpp` (extracts them) |
| Toy Box logic | `runtime/host/toybox/model.*`: the top row (seats, Add a figure, Manage), the grid (one shelf section, untitled; the fold on its own row), LB/RB = other seat, Y = search (`OpenSearch`), the Figure page's main actions, More, Details |
| Toy Box look | `runtime/host/toybox/draw.cpp`: the seats row, `Card` (the shared card), the empty states, the wheel's level lines |
| Launcher | Arkeyan Launcher `docs/collection_ui.md` |

Verified:
- `ArkeyanCollectionTest` (the shelf: no figure twice, favourites then recent; `LevelText`; the marks), `ArkeyanArtTest` (the
  family tier; with the SSA disc every toy SSA reads has a picture), Arkeyan Launcher's `ArkeyanLauncherTest` (the bar's filters,
  choosing for a seat, Giants' seats, the marks).
- Screenshots of both screens in every state, by `SKY_TOYBOX_SCRIPT` and `SKY_LAUNCHER_TEST_COLLECTION`
  (`private/ux/`, not committed).

Left:
- **Player 2** needs a second controller (per-user input, XAM user 1, a second SDL pad, `ToyboxFilterGamepad` per pad).
- The launcher shows Player 2 as a seat; the game says "needs a 2nd controller".
- Not built: the "visiting Skylander" toast; drag and drop in the game; "changed elsewhere: keep mine / take theirs";
  pause on opening; USB import/export on the Xbox; unknown toys named take effect at the next start; locks record only
  the game id.
- Not run on the console (UWP build only).

## Out of health [Xbox] [SSA] [Giants] [SwapForce] (2026-10-28)

When a player's Skylander runs out of health the game asks that player for a figure again (it says the Skylander must be
swapped), so the Toy Box, which already opens by itself at the game's prompt (below), opens on that seat, and **the figure
that ran out shows a red cross** on its card and on its seat until it can play again, as the game lets it: **the next level**.
It stays pickable (the game decides what to do with it), the cross is only information; a toast says "Eruptor is out of
health: pick another one." / "Eruptor n'a plus de vie : choisis-en un autre." (5 s).
- **What marks a figure** (`FollowPrompt`, `Model::MarkOutOfHealth`, `host/toybox.cpp`): a prompt episode for player N starts
  (the tracker's `Appeared`, steady for 3 polls) and **either** the adapter says the Skylander died (`Prompt::defeated`:
  **Swap Force's CMagicMoment state 13 "Dead"**, whose reason 5 is PlayerDeath, [V] in the code and the public notes, not
  reached in play) **or** the seat holds a figure that the game had already taken (`SeatState::waiting` false): the game
  only asks for a seat that has a figure when that figure is no use to it. A figure placed a moment ago (the title has not
  read it yet) is never marked by the second rule. A swapper marks both halves. A real portal is not followed (no seats).
  Spyro's Adventure and Giants have no death state of their own to read: Spyro's Adventure's portal texts hold the same
  "Player N: Please put a Skylander on the Portal of Power" and Giants' portal script (`Portal Control`, every state and
  `NNN_PortalError_` key listed from memory on 2026-10-28) has no death state either, so they use the second rule [?]: the
  prompt a death shows was **not reached in play** (no way found to defeat a hero in these runs: the idle hero of Giants'
  `level_203_the_machine` and Chomp-Down, Swap Force's survival arenas fault in this build before any wave).
- **What clears them** (`game::LevelStarted`, `runtime/game.cpp`): a level started. Spyro's Adventure: the title opens
  `\levels\<name>\mission.str` (the front end's too) [V] (`[level] frontend starts`); Giants: `\level\<name>.arc`; Swap
  Force: the zone lookup (`engines/alchemy/zone.cpp`, once for each change of zone name, [?] a lookup of a neighbouring zone
  would clear early). Once per change of name, logged `[level] <name> starts`.
- **Only after the figure played in the level** (0.9.4, `toybox::InPlay`, `host/toybox/in_play.h`): the game also asks for a
  seat with its figure on it when an adventure or a level starts (seen: Swap Force asks for Player 1 right after a new
  adventure's save, Spyro marked by mistake). Either rule above marks a figure only once it has **played 20 s in a row in the
  current level**: taken by the title, its seat not asked for, not in one of the game's menu levels (`menuLevels`), no movie
  (`ultrawide::MoviePlaying`). A level start and a portal reset (`R`, `PortalResets`: the game sends it while it loads) start
  every seat over, so does a figure placed, swapped or put back. Otherwise the log says `player N's figure has not played in
  this level yet: not out of health`. A death in a level's first 20 s gets no cross (the game still asks). [V] Swap Force, a new
  game from the title with Eruptor on the portal: the tutorial's ask at its start logs that line, no cross
  (`ArkeyanToyboxInPlayTest` checks each case: seat filled before the level, level change, portal reset, menus and movies, another
  figure, two seats).
- **The test** (`tools/regress/out_of_health.sh <game> <disc>`, in `tools/test_all.sh`, [V] on the three games): the
  Toy Box's own script has two test aids, `prompt=1d` (the game asks player 1 for 3 s and says its Skylander died; `prompt=2`
  without the d) and `level` (a level starts): an Eruptor on Player 1, `level` at 30 s, `prompt=1d` at 32 s (too early: `has not
  played in this level yet`), `prompt=1d` again at 58 s (the Toy Box opens, the log says `player 1's figure is out of health`),
  a shot, `level` at 66 s, a shot: the first has the Toy Box's danger red (0xF85149) in quantity (the cross, about 1700 pixels),
  the second none.

## Opens by itself, and the overview (smart portal)

When the game asks for a figure (`[portal] toybox_auto_open`, on by default, emulated portal only) the Toy Box opens on
that player's seat and closes when the prompt goes away. Delete (or X, then A) takes a figure off a seat, Delete on a
card takes that figure off its seat, the Figure page has a "Take off" line. [portal.md](portal.md).

### Swapping without an empty portal (0.9.6, issue #40)

The wheel, the in-game Toy Box and the window's link `put` all use `Model::PutOn`; player swaps use `SwapPlayers`.
Previously these removed the old card before `PlaceCard` loaded and placed the new one, with a separate operation for
each swapper half. A game thread could read an empty portal between them, for as many reads as loading took. Its
adapter then reported a real figure prompt: after three stable polls, `FollowPrompt` opened the Toy Box. The
`has not played in this level yet: not out of health` check only prevented a health mark; it did not prevent opening.

`Model::ReplaceCards` now plans the affected slots together. `PortalReplaceFigures` loads and locks every replacement
before committing the whole batch through `PortalSlots::Replace`. The status and block commands use that same slot
mutex: they see the old card or the new card, including both swapper halves, never an intermediate empty portal.
Replacements retain presence and set the changed bit so the game rereads them. A single figure replacing a swapper
also removes the unused bottom in that commit. A full portal can replace occupied slots; invalid slots, real slots,
an unreadable replacement or a failed lock leave the old figures and seats intact. The same boundary covers player
swaps, changing a swapper half in the Toy Box, and the next-figure hotkey. This is shared host code for Lux and Alchemy,
including the virtual seats of a mixed portal. Prompt detection is unchanged: an explicit take-off or Clear portal
still allows the game to ask and the Toy Box to open.

Verification:
- `ArkeyanToyboxSwapTest` (registered with `sky_add_tests` and `tools/test_all.sh --quick`) uses only synthetic markers
  in memory: single and two-half replacements, player swaps, a full portal, rejected batches, concurrent status reads
  and an actual removal followed by a stable prompt. `ArkeyanToyboxInPlayTest` checks the existing health eligibility.
- With dependencies available: build those two targets and run
  `ctest --test-dir <build> -R 'ArkeyanToybox(Swap|InPlay)Test' --output-on-failure`.
- Play check still required: in each game, swap from the wheel, F6 Toy Box and separate Toy Box window, with Player 1
  alone and with Player 2 present; include swapper-to-swapper, single-to-swapper and swapper-to-single where supported.
  The log must show the replacement's `placed` and `P1: ... on slot ...` lines (both slots for a swapper), then
  `the game took ...`, without a new `prompt for player 1` / automatic `open` episode caused by the swap. A link `put`
  must finish with `put P1: done`. With portal traffic logging, the occupied player's presence bits must never all be
  zero during the swap; the replacement is reported present and changed. Finally take the figure off explicitly:
  the game's prompt and the Toy Box's automatic opening should still occur. No game or launcher was run for this fix.
