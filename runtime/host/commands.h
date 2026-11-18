#pragma once

// The F3 menu's Commands page on launcher.ini (docs/runtime/host/gamemenu.md, "Commands"): a game's actions with their
// keyboard key, pad button and mouse button, stored as Arkeyan Launcher stores them (its launcher/game_keys.cpp). [input]
// is the layout every game inherits (key_<action>=<SDL scancode name>, pad_<button>=<button>|none, mouse=0|1,
// mouse_<action>=<mouse name>|none, rumble_p1/p2) and the Toy Box's keys (key_toybox, key_toybox_add, key_toybox_clear)
// and physical pad shortcuts (pad_wheel, pad_toybox, pad_menu; shared only, never remapped);
// [<game>.input] holds the game's own lines, which win. The page itself is host/toybox/menu.cpp.
#include "host/launcher.h"
#include "host/mouse.h"
#include "host/pad_map.h"
#include "host/toybox/pad_bindings.h"
#include <span>
#include <string>
#include <string_view>

struct RuntimeSettings;

namespace commands
{
    // One action of a game (game::Adapter::inputRows), in Arkeyan Launcher's order and words.
    struct Row
    {
        const char* action; // key_<action>: one of g_keyActions ("a", "left_up")
        const char* label;  // English, as Arkeyan Launcher words it
        const char* french; // its French (Arkeyan Launcher's lang/fr.txt)
        const char*
            pad; // the pad button whose job it is (g_padButtons: "a", "lb"); nullptr: a stick direction (no pad cell)
    };

    std::string Section(std::string_view game); // "ssa.input"

    // The actions every game names the same way (the launcher's "All games" keys: the moves, Back, Pause): [input] holds
    // them for all games. Every other action is a game's own, in its [<game>.input].
    inline constexpr const char* SharedActions[] = { "left_up", "left_down", "left_left", "left_right", "back",
        "start" };
    bool Shared(std::string_view action);

    // True once Arkeyan Launcher's MigrateInput upgraded the file to the global layout ([input] global=1). Before it,
    // PLAY copied a game's keys into [input], so [input] held the last game played, and a game without its own line
    // reads [input]'s: the runtime reads and edits such a file that way and leaves the upgrade to the launcher, the
    // only one that knows every game.
    bool Global(const LauncherConfig& config);
    // On an upgraded file, a section an older launcher seeded again (marked seeded) is upgraded again: its lines equal
    // to the global layout go, [input] loses the keys of the actions that are not shared. Nobody's keys change. An old
    // layout file is left as it is. Returns true when the file changed.
    bool Migrate(LauncherConfig& config);

    // A row's key: the game's line, else [input]'s, else the default (g_keyActions).
    std::string Key(const LauncherConfig& config, std::string_view game, const Row& row);
    // The pad button whose job `button` does: the game's line, else [input]'s, else its own; -1 for none.
    int PadTarget(const LauncherConfig& config, std::string_view game, int button);

    // The Toy Box's keys (all games): open it, add the next figure, clear the portal; F6, F7, F8 when not set. While
    // key_toybox is not set Tab opens it too (ToyboxTab).
    inline constexpr int ToyboxKeyCount = 3;
    inline constexpr const char* ToyboxLines[ToyboxKeyCount] = { "key_toybox", "key_toybox_add", "key_toybox_clear" };
    std::string ToyboxKey(const LauncherConfig& config, int which);
    bool ToyboxTab(const LauncherConfig& config);
    // A shortcut button that does a job in the game: the row of that job, rows.size() for a job no row names (the D-pad),
    // -1 for none. The defaults (View, and Y and Start after View) are never flagged.
    int PadConflict(const LauncherConfig& config, std::string_view game, std::span<const Row> rows, int action);

    // A key slot of the page: [0, rows.size()) the game's rows, then the Toy Box's three keys.
    std::string SlotKey(const LauncherConfig& config, std::string_view game, std::span<const Row> rows, size_t slot);
    // True when another slot of the page has the slot's key (both do something on that key).
    bool SlotConflict(const LauncherConfig& config, std::string_view game, std::span<const Row> rows, size_t slot);
    // A conflict notice names a direction even when the page abbreviates its action as "Same".
    std::string SlotLabel(std::span<const Row> rows, size_t slot, bool french);

    // Each change goes in the game's section (the Toy Box's keys and physical pad shortcuts in [input]); a line equal to [input]'s value goes, so the
    // game follows the global layout again.
    // `key` for a slot; a slot that has it too (a row of the game or a Toy Box key) keeps it: both are flagged
    // (SlotConflict). Returns the first such slot, -1: none.
    int SetKey(
        LauncherConfig& config, std::string_view game, std::span<const Row> rows, size_t slot, const std::string& key);
    // The physical `button` does the job `job` (g_padButtons' indexes); the button that did `job` takes `button`'s old
    // job, so no job is lost. Returns that button, -1: none.
    int SetPad(LauncherConfig& config, std::string_view game, std::span<const Row> rows, int job, int button);
    // The mouse's switch (the game's, else [input]'s, off by default) and the button each action is on ([input]'s lines,
    // then the game's; mouse::Effective: None everywhere when off).
    bool MouseOn(const LauncherConfig& config, std::string_view game);
    void SetMouseOn(LauncherConfig& config, std::string_view game, std::span<const Row> rows, bool on);
    mouse::Bindings MouseLines(const LauncherConfig& config, std::string_view game);
    // `button` for g_keyActions[action] (one action per button: the action that had it gets none). Returns that action
    // (an index of g_keyActions), -1: none.
    int SetMouse(LauncherConfig& config, std::string_view game, std::span<const Row> rows,
        std::span<const mouse::Default> defaults, size_t action, mouse::Button button);

    // "Reset to defaults" for the game: its key_, pad_, mouse_ lines and its mouse switch go (the Toy Box's keys, shared
    // by every game, stay, as do the shared physical pad shortcuts); a shared row then has the global key, a game's own row its default. On an old layout file the
    // game gets lines with its defaults where [input]'s keys or mouse switch differ, since the other games read them.
    void Reset(LauncherConfig& config, std::string_view game, std::span<const Row> rows);

    // The keys, pad map, mouse, Toy Box keys and shared physical pad shortcuts a start with `config` uses (as ParseSettings reads them: [input], then
    // [<game>.input]).
    struct Bindings
    {
        std::string keys[22]; // per g_keyActions entry
        PadMap padMap;
        mouse::Bindings mouse;
        std::string toyboxKeys[ToyboxKeyCount] = { "F6", "F7", "F8" };
        bool toyboxTab = true;
        toybox::pad::Bindings toyboxPad;
    };
    Bindings Read(const LauncherConfig& config, std::string_view game);

    // The running game: the page hands over the bindings it saved; the main loop takes them once (true) and rebuilds
    // its input from `settings`.
    void Publish(const Bindings& bindings);
    bool Take(RuntimeSettings& settings);
}
