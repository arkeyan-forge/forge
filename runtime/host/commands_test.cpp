// Checks the F3 menu's Commands page on a scratch launcher.ini (host/commands.h): an old layout file left to the
// launcher and edited the old way, a section seeded later upgraded again, a key written to the game's section over
// [input]'s, a key used twice (both kept and flagged), the pad jobs swapped so none is lost, the mouse's switch and
// buttons (one action per button), the Toy Box's keys, Reset back to the global layout (the Toy Box's keys kept), and
// the bindings the running game takes.
// usage: ArkeyanCommandsTest [<scratch dir>] (default: the system's temporary folder)
#include "host/commands.h"
#include "host/settings.h"
#include "host/toybox/keys.h"
#include "host/toybox/hit.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace
{
    int g_checks = 0;
    int g_failures = 0;

#define CHECK(condition)                                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        g_checks++;                                                                                                    \
        if (!(condition))                                                                                              \
        {                                                                                                              \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition);                                               \
            g_failures++;                                                                                              \
        }                                                                                                              \
    } while (0)

    constexpr commands::Row Rows[] = {
        { "left_up", "Move up", "Haut", nullptr },
        { "a", "Jump", "Sauter", "a" },
        { "x", "Attack 1", "Attaque 1", "x" },
        { "b", "Attack 2", "Attaque 2", "b" },
        { "rb", "Attack 2", "Attaque 2", "rb" },
    };
    constexpr size_t RowA = 1, RowX = 2, RowB = 3;
    constexpr size_t SlotToybox = std::size(Rows), SlotToyboxAdd = SlotToybox + 1;
    constexpr mouse::Default MouseDefaults[] = { { "x", mouse::Left }, { "b", mouse::Right } };

    std::string Value(const LauncherConfig& config, const std::string& section, const std::string& key)
    {
        auto entry = config.find(section);
        if (entry == config.end())
            return "<none>";
        auto value = entry->second.find(key);
        return value == entry->second.end() ? "<none>" : value->second;
    }

    size_t KeyIndex(const char* action)
    {
        for (size_t i = 0; i < std::size(g_keyActions); i++)
            if (std::string_view(action) == g_keyActions[i].name)
                return i;
        return 0;
    }

    // The bindings Read gives are the ones a start with that file gets (ParseSettings).
    bool SameAsParse(const LauncherConfig& config)
    {
        RuntimeSettings settings;
        std::vector<std::string> warnings;
        std::string error;
        if (!ParseSettings(
                config, "game", [](const char*) -> const char* { return nullptr; }, 0, nullptr, settings, warnings,
                error))
            return false;
        commands::Bindings read = commands::Read(config, "game");
        for (size_t i = 0; i < std::size(g_keyActions); i++)
            if (read.keys[i] != settings.keys[i])
                return false;
        for (int i = 0; i < PadButtonCount; i++)
            if (read.padMap.target[i] != settings.padMap.target[i])
                return false;
        for (size_t i = 0; i < mouse::ActionCount; i++)
            if (read.mouse.set[i] != settings.mouse.set[i] || read.mouse.stored[i] != settings.mouse.stored[i])
                return false;
        for (int i = 0; i < commands::ToyboxKeyCount; i++)
            if (read.toyboxKeys[i] != settings.toyboxKeys[i])
                return false;
        return read.mouse.enabled == settings.mouse.enabled && read.toyboxTab == settings.toyboxTab;
    }
}

int main(int argc, char** argv)
{
    const std::string section = "game.input";
    // The exact key classification used by ToyboxHandleEvent, both at startup and after a live rebind.
    {
        toybox::Keys keys;
        CHECK(keys.Open(SDL_SCANCODE_F6) && keys.Hold(SDL_SCANCODE_TAB));
        CHECK(!keys.Hold(SDL_SCANCODE_F6) && !keys.Open(SDL_SCANCODE_TAB));
        CHECK(keys.add == SDL_SCANCODE_F7 && keys.clear == SDL_SCANCODE_F8);
        LauncherConfig config;
        config["input"] = { { "key_toybox", "O" }, { "key_toybox_add", "P" }, { "key_toybox_clear", "U" } };
        RuntimeSettings settings;
        std::vector<std::string> warnings;
        std::string error;
        CHECK(ParseSettings(
            config, "game", [](const char*) -> const char* { return nullptr; }, 0, nullptr, settings, warnings, error));
        keys.Set(settings.toyboxKeys, settings.toyboxTab);
        CHECK(keys.Hold(SDL_SCANCODE_O) && !keys.Open(SDL_SCANCODE_O)); // tap/hold uses this key now
        CHECK(!keys.Hold(SDL_SCANCODE_TAB) && !keys.Open(SDL_SCANCODE_F6));
        CHECK(keys.add == SDL_SCANCODE_P && keys.clear == SDL_SCANCODE_U);
        config["input"]["key_toybox"] = "Tab";
        auto bindings = commands::Read(config, "game");
        keys.Set(bindings.toyboxKeys, bindings.toyboxTab);
        CHECK(keys.Hold(SDL_SCANCODE_TAB) && !keys.Open(SDL_SCANCODE_TAB));
        commands::Reset(config, "game", Rows); // a global setting: the game's reset leaves it
        bindings = commands::Read(config, "game");
        CHECK(bindings.toyboxKeys[0] == "Tab" && bindings.toyboxKeys[1] == "P" && !bindings.toyboxTab);
        for (const char* line : commands::ToyboxLines)
            config["input"].erase(line);
        bindings = commands::Read(config, "game");
        keys.Set(bindings.toyboxKeys, bindings.toyboxTab);
        CHECK(keys.Hold(SDL_SCANCODE_TAB) && keys.Open(SDL_SCANCODE_F6));
        CHECK(keys.add == SDL_SCANCODE_F7 && keys.clear == SDL_SCANCODE_F8);
    }
    // Both clicks and hover use the topmost drawn hit, with half-open edges and blocking backdrops.
    {
        struct Point
        {
            float x, y;
        };
        struct Hit
        {
            Point min, max;
        };
        const Hit hits[] = { { { 0, 0 }, { 20, 20 } }, { { 10, 10 }, { 30, 30 } } };
        CHECK(toybox::HitAt(std::span<const Hit>(hits), 0, 0) == 0);
        CHECK(toybox::HitAt(std::span<const Hit>(hits), 15, 15) == 1);
        CHECK(toybox::HitAt(std::span<const Hit>(hits), 20, 5) == -1);
        CHECK(toybox::HitAt(std::span<const Hit>(hits), 30, 30) == -1);
        CHECK(toybox::HitAt(std::span<const Hit>{}, 0, 0) == -1);
    }
    // A key on any right-stick row names its direction, never the page's abbreviated "Same"; both rows keep the key.
    {
        constexpr commands::Row rows[] = {
            { "b", "Special Power / Back", "Pouvoir spécial / Retour", "b" },
            { "right_up", "Shake / Insert / Swing", "Agiter / Insérer / Balancer", nullptr },
            { "right_down", "Same", "Idem", nullptr },
            { "right_left", "Same", "Idem", nullptr },
            { "right_right", "Same", "Idem", nullptr },
        };
        const char* keys[] = { "I", "K", "J", "L" };
        const char* english[] = { "R stick up", "R stick down", "R stick left", "R stick right" };
        const char* french[] = { "stick droit haut", "stick droit bas", "stick droit gauche", "stick droit droite" };
        for (size_t i = 0; i < std::size(keys); i++)
        {
            LauncherConfig config;
            int other = commands::SetKey(config, "game", rows, 0, keys[i]);
            CHECK(other == int(i + 1));
            CHECK(commands::SlotKey(config, "game", rows, size_t(other)) == keys[i]); // not swapped
            CHECK(commands::SlotConflict(config, "game", rows, 0) &&
                  commands::SlotConflict(config, "game", rows, size_t(other)));
            CHECK(commands::SlotLabel(rows, size_t(other), false) == english[i]);
            CHECK(commands::SlotLabel(rows, size_t(other), true) == french[i]);
            CHECK(commands::SetKey(config, "game", rows, 0, keys[i]) == int(i + 1)); // still shared
            CHECK(commands::SetKey(config, "game", rows, 0, "T") == -1);
            CHECK(!commands::SlotConflict(config, "game", rows, 0) &&
                  !commands::SlotConflict(config, "game", rows, size_t(other)));
            // From a right-stick row, the other game action is named as well.
            config.clear();
            other = commands::SetKey(config, "game", rows, i + 1, "X");
            CHECK(other == 0);
            CHECK(commands::SlotKey(config, "game", rows, 0) == "X");
            CHECK(commands::SlotLabel(rows, size_t(other), false) == "Special Power / Back");
        }
        CHECK(commands::SlotLabel(rows, 0, false) == "Special Power / Back");
        CHECK(commands::SlotLabel(rows, std::size(rows), false) == "Open the Toy Box");
        CHECK(commands::SlotLabel(rows, std::size(rows) + 3, false).empty());
        LauncherConfig config;
        CHECK(commands::SetKey(config, "game", rows, 2, "T") == -1);
    }
    // Back is shared like the moves and Pause: a section an older launcher seeded after the upgrade is done again, with
    // [input]'s key_back as the global one, and Reset gives the shared rows the global keys and the game's own rows
    // their defaults.
    {
        constexpr commands::Row rows[] = {
            { "a", "Jump", "Sauter", "a" },
            { "back", "Stats", "Stats", "back" },
        };
        CHECK(commands::Shared("back") && !commands::Shared("a"));
        LauncherConfig config;
        config["input"] = { { "global", "1" }, { "key_back", "Delete" }, { "key_a", "Z" } };
        config["game.input"] = { { "seeded", "1" }, { "key_back", "Delete" }, { "pad_back", "back" } };
        config["other.input"] = { { "seeded", "1" }, { "key_a", "M" } };
        config["third.input"] = { { "mouse", "1" } };
        CHECK(commands::Migrate(config));
        CHECK(Value(config, "input", "key_back") == "Delete" && Value(config, "input", "key_a") == "<none>");
        CHECK(
            Value(config, "game.input", "key_back") == "<none>" && Value(config, "game.input", "pad_back") == "<none>");
        CHECK(Value(config, "other.input", "key_back") == "Backspace"); // it played with the default
        CHECK(
            Value(config, "third.input", "key_back") == "<none>" && Value(config, "third.input", "key_a") == "<none>");
        CHECK(Value(config, "third.input", "mouse") == "1");
        CHECK(commands::Key(config, "game", rows[1]) == "Delete" &&
              commands::Key(config, "other", rows[1]) == "Backspace");
        CHECK(commands::Key(config, "third", rows[1]) == "Delete");

        CHECK(commands::SetKey(config, "game", rows, 0, "G") == -1);
        CHECK(commands::SetKey(config, "game", rows, 1, "Tab") == -1);
        CHECK(
            commands::SetPad(config, "game", rows, PadButtonIndex("a"), PadButtonIndex("back")) == PadButtonIndex("a"));
        commands::Reset(config, "game", rows);
        CHECK(commands::Key(config, "game", rows[0]) == "Space" && commands::Key(config, "game", rows[1]) == "Delete");
        CHECK(commands::PadTarget(config, "game", PadButtonIndex("back")) == PadButtonIndex("back"));
        CHECK(commands::PadTarget(config, "game", PadButtonIndex("a")) == PadButtonIndex("a"));
        CHECK(Value(config, "input", "key_back") == "Delete");
    }
    // A file still in the old layout (no [input] global=1): PLAY copied the last game's keys into [input], and games
    // without a section read them. Only the launcher, which knows every game, upgrades it; the runtime reads it the old
    // way and F3 changes the running game's section only, so another game keeps [input]'s keys.
    {
        LauncherConfig config;
        config["input"] = { { "device", "auto" }, { "key_x", "F" }, { "key_left_up", "Up" }, { "mouse", "1" },
            { "mouse_a", "Mouse Left" } };
        config["third.input"] = { { "seeded", "1" }, { "key_left_up", "W" } };
        const LauncherConfig before = config;
        CHECK(!commands::Migrate(config));
        CHECK(config == before);
        CHECK(commands::Key(config, "game", Rows[RowX]) == "F" && commands::Key(config, "game", Rows[0]) == "Up");
        CHECK(commands::Read(config, "game").keys[KeyIndex("x")] == "F" && commands::MouseOn(config, "game"));
        CHECK(SameAsParse(config));
        CHECK(commands::SetKey(config, "game", Rows, RowB, "J") == -1);
        CHECK(commands::SetKey(config, "game", Rows, RowX, "G") == -1);
        CHECK(Value(config, section, "key_b") == "J" && Value(config, section, "key_x") == "G");
        CHECK(commands::Read(config, "game").keys[KeyIndex("x")] == "G");
        CHECK(commands::SetKey(config, "game", Rows, RowX, "f") == -1); // [input]'s key: inherited again
        CHECK(Value(config, section, "key_x") == "<none>" && commands::Key(config, "game", Rows[RowX]) == "F");
        CHECK(commands::SetPad(config, "game", Rows, PadButtonIndex("a"), PadButtonIndex("b")) == PadButtonIndex("a"));
        CHECK(commands::PadTarget(config, "game", PadButtonIndex("b")) ==
              commands::Read(config, "game").padMap.target[PadButtonIndex("b")]);
        CHECK(SameAsParse(config));
        CHECK(Value(config, "input", "key_x") == "F" && Value(config, "input", "global") == "<none>");
        CHECK(Value(config, "input", "mouse") == "1" && Value(config, "input", "mouse_a") == "Mouse Left");
        CHECK(commands::Read(config, "other").keys[KeyIndex("x")] == "F" && commands::MouseOn(config, "other"));
        CHECK(Value(config, "third.input", "seeded") == "1"); // left to the launcher
        // Reset: the game's own rows their defaults, the shared rows [input]'s keys; the other games keep [input]'s.
        commands::Reset(config, "game", Rows);
        CHECK(commands::Key(config, "game", Rows[RowX]) == "C" && commands::Key(config, "game", Rows[0]) == "Up");
        CHECK(commands::Key(config, "game", Rows[RowB]) == "X" && !commands::MouseOn(config, "game"));
        CHECK(commands::PadTarget(config, "game", PadButtonIndex("b")) == PadButtonIndex("b"));
        CHECK(commands::Read(config, "game").keys[KeyIndex("x")] == "C");
        CHECK(SameAsParse(config));
        CHECK(Value(config, "input", "key_x") == "F" && commands::Key(config, "other", Rows[RowX]) == "F");
        CHECK(commands::MouseOn(config, "other"));
        CHECK(!commands::Migrate(config));
    }

    namespace fs = std::filesystem;
    fs::path scratch = argc > 1 ? fs::path(argv[1]) : fs::temp_directory_path() / "ArkeyanCommandsTest";
    fs::create_directories(scratch);
    const std::string path = (scratch / "launcher.ini").string();
    {
        // The file as the launcher's upgrade leaves it (its MigrateInput on an older launcher's file): [input] holds
        // the shared keys and global=1; each game's section its own lines.
        std::ofstream file(path, std::ios::binary);
        file << "[input]\ndevice=auto\nglobal=1\nkey_left_up=Up\nkey_start=P\n"
                "[game.input]\nkey_x=M\nkey_b=K\nkey_start=Return\n"
                "[other.input]\nkey_a=Z\nkey_lb=Q\nmouse=1\n[third.input]\nkey_left_up=W\nkey_start=Return\n"
                "[game.video]\nfps_limit=60\n";
    }

    // Upgraded: the runtime changes nothing; [input]'s shared keys reach every game, a game's own lines win.
    {
        LauncherConfig config = LoadLauncherConfig(path);
        const LauncherConfig loaded = config;
        const std::string before[] = { "Up", "Space", "M", "K", "E" };
        CHECK(!commands::Migrate(config));
        CHECK(config == loaded);
        for (size_t i = 0; i < std::size(Rows); i++)
            CHECK(commands::Key(config, "game", Rows[i]) == before[i]);
        CHECK(commands::Read(config, "game").keys[KeyIndex("start")] == "Return");
        CHECK(commands::Read(config, "other").keys[KeyIndex("start")] == "P");
        CHECK(SameAsParse(config));
        // An older launcher then seeds the game again and puts its keys in [input]: done again for that section.
        LauncherConfig older = config;
        older[section]["seeded"] = "1";
        older[section]["key_left_up"] = "Up";
        older["input"]["key_a"] = "Space";
        CHECK(commands::Migrate(older));
        CHECK(Value(older, section, "key_left_up") == "<none>" && Value(older, section, "seeded") == "<none>");
        CHECK(Value(older, "input", "key_a") == "<none>" && Value(older, section, "key_x") == "M");
        CHECK(Value(older, "other.input", "key_a") == "Z" && Value(older, "input", "global") == "1");
        CHECK(!commands::Migrate(older)); // once
        CHECK(SaveLauncherConfig(path, config));
    }

    // A key: the game's line; [input] is not touched; the global key again removes the line.
    {
        LauncherConfig config = LoadLauncherConfig(path);
        CHECK(commands::SetKey(config, "game", Rows, RowB, "J") == -1);
        CHECK(Value(config, section, "key_b") == "J" && Value(config, "input", "key_b") == "<none>");
        CHECK(commands::SetKey(config, "game", Rows, 0, "T") == -1); // Move up, a shared action: the game's own now
        CHECK(Value(config, section, "key_left_up") == "T" && Value(config, "input", "key_left_up") == "Up");
        CHECK(commands::SetKey(config, "game", Rows, 0, "up") == -1); // the global key: inherited again
        CHECK(Value(config, section, "key_left_up") == "<none>" && commands::Key(config, "game", Rows[0]) == "Up");
        CHECK(commands::SetKey(config, "game", Rows, RowA, "Z") == -1);
        CHECK(SaveLauncherConfig(path, config));
    }
    {
        LauncherConfig config = LoadLauncherConfig(path);
        CHECK(Value(config, section, "key_b") == "J");
        CHECK(Value(config, section, "key_a") == "Z");
        CHECK(Value(config, "input", "device") == "auto");
        CHECK(Value(config, "game.video", "fps_limit") == "60"); // the other sections stay
        std::ifstream file(path, std::ios::binary);
        std::stringstream text;
        text << file.rdbuf();
        CHECK(text.str().find("[game.input]") != std::string::npos);
        CHECK(text.str().find("key_b=J") != std::string::npos);
        CHECK(SameAsParse(config));
        // A global key the game does not change reaches it.
        config["input"]["key_start"] = "Tab";
        CHECK(commands::Read(config, "game").keys[KeyIndex("start")] == "Return"); // its own line
        config[section].erase("key_start");
        CHECK(commands::Read(config, "game").keys[KeyIndex("start")] == "Tab");
        CHECK(SameAsParse(config));
    }

    // A key another row has: both rows keep it, both flagged; another key clears the flags.
    {
        LauncherConfig config = LoadLauncherConfig(path);
        CHECK(commands::SetKey(config, "game", Rows, RowX, "z") == int(RowA)); // the same key, whatever its case
        CHECK(Value(config, section, "key_x") == "z");
        CHECK(Value(config, section, "key_a") == "Z"); // unchanged
        CHECK(commands::SlotConflict(config, "game", Rows, RowX) && commands::SlotConflict(config, "game", Rows, RowA));
        CHECK(!commands::SlotConflict(config, "game", Rows, RowB));
        CHECK(SameAsParse(config));
        CHECK(commands::SetKey(config, "game", Rows, RowX, "C") == -1);
        CHECK(!commands::SlotConflict(config, "game", Rows, RowA));
        CHECK(commands::SetKey(config, "game", Rows, RowA, "C") == int(RowX));
        CHECK(Value(config, section, "key_a") == "C" && Value(config, section, "key_x") == "<none>"); // C: its default
        CHECK(commands::Key(config, "game", Rows[RowX]) == "C");
        CHECK(SaveLauncherConfig(path, config));
    }

    // Pad: the physical B does A's job; A, which did it, takes B's old job.
    {
        LauncherConfig config = LoadLauncherConfig(path);
        const int a = PadButtonIndex("a"), b = PadButtonIndex("b"), rb = PadButtonIndex("rb");
        CHECK(commands::SetPad(config, "game", Rows, a, b) == a);
        CHECK(Value(config, section, "pad_b") == "a");
        CHECK(Value(config, section, "pad_a") == "b");
        CHECK(commands::PadTarget(config, "game", b) == a);
        CHECK(commands::PadTarget(config, "game", a) == b);
        CHECK(SameAsParse(config));
        // The physical RB does B's job (which A does now): A takes RB's.
        CHECK(commands::SetPad(config, "game", Rows, b, rb) == a);
        CHECK(Value(config, section, "pad_rb") == "b");
        CHECK(Value(config, section, "pad_a") == "rb");
        // Back: A does its own job again, so its line goes; B did it and takes A's last one (RB's).
        CHECK(commands::SetPad(config, "game", Rows, a, a) == b);
        CHECK(Value(config, section, "pad_a") == "<none>");
        CHECK(Value(config, section, "pad_b") == "rb");
        // The same job again changes nothing.
        CHECK(commands::SetPad(config, "game", Rows, a, a) == -1);
        // A "none" from the launcher: that button does nothing; giving it a job leaves the old holder without one.
        config[section]["pad_y"] = "none";
        CHECK(commands::PadTarget(config, "game", PadButtonIndex("y")) == -1);
        CHECK(SameAsParse(config));
        CHECK(SaveLauncherConfig(path, config));
    }

    // The mouse: off by default; on, the game's defaults; a button moves to another action, which loses it.
    {
        LauncherConfig config = LoadLauncherConfig(path);
        mouse::Button now[mouse::ActionCount];
        CHECK(!commands::MouseOn(config, "game"));
        mouse::Effective(commands::MouseLines(config, "game"), MouseDefaults, now);
        CHECK(!mouse::AnyMapped(now));
        commands::SetMouseOn(config, "game", Rows, true);
        CHECK(Value(config, section, "mouse") == "1");
        mouse::Effective(commands::MouseLines(config, "game"), MouseDefaults, now);
        CHECK(now[KeyIndex("x")] == mouse::Left && now[KeyIndex("b")] == mouse::Right);
        // The left button for A: X (its default) loses it.
        CHECK(
            commands::SetMouse(config, "game", Rows, MouseDefaults, KeyIndex("a"), mouse::Left) == int(KeyIndex("x")));
        CHECK(Value(config, section, "mouse_a") == "Mouse Left");
        CHECK(Value(config, section, "mouse_x") == "none");
        mouse::Effective(commands::MouseLines(config, "game"), MouseDefaults, now);
        CHECK(now[KeyIndex("a")] == mouse::Left && now[KeyIndex("x")] == mouse::None &&
              now[KeyIndex("b")] == mouse::Right);
        // The wheel for a stick direction; nobody had it.
        CHECK(commands::SetMouse(config, "game", Rows, MouseDefaults, KeyIndex("left_up"), mouse::WheelUp) == -1);
        CHECK(Value(config, section, "mouse_left_up") == "Wheel Up");
        CHECK(SameAsParse(config));
        // Off again: the lines stay, nothing is mapped.
        commands::SetMouseOn(config, "game", Rows, false);
        mouse::Effective(commands::MouseLines(config, "game"), MouseDefaults, now);
        CHECK(!mouse::AnyMapped(now));
        CHECK(Value(config, section, "mouse_a") == "Mouse Left");
        commands::SetMouseOn(config, "game", Rows, true);
        CHECK(SaveLauncherConfig(path, config));
    }

    // The Toy Box's keys: F6 (Tab too), F7, F8 until set; in [input]; flagged against each other and the game's rows.
    {
        LauncherConfig config = LoadLauncherConfig(path);
        CHECK(commands::ToyboxKey(config, 0) == "F6" && commands::ToyboxKey(config, 2) == "F8");
        CHECK(commands::ToyboxTab(config));
        CHECK(commands::SetKey(config, "game", Rows, SlotToyboxAdd, "G") == -1);
        CHECK(Value(config, "input", "key_toybox_add") == "G");
        CHECK(commands::ToyboxTab(config)); // only key_toybox takes Tab's place
        // Two Toy Box keys on one key: both kept, both flagged.
        CHECK(commands::SetKey(config, "game", Rows, SlotToyboxAdd, "F8") == int(SlotToybox + 2));
        CHECK(commands::SlotConflict(config, "game", Rows, SlotToyboxAdd) &&
              commands::SlotConflict(config, "game", Rows, SlotToybox + 2));
        CHECK(commands::SetKey(config, "game", Rows, SlotToyboxAdd, "G") == -1);
        // A row takes the Toy Box's key: both keep it, both flagged.
        const std::string xKey = commands::Key(config, "game", Rows[RowX]);
        CHECK(commands::SetKey(config, "game", Rows, SlotToybox, xKey) >= 0);
        CHECK(Value(config, "input", "key_toybox") == xKey);
        CHECK(commands::Key(config, "game", Rows[RowX]) == xKey);
        CHECK(commands::SlotConflict(config, "game", Rows, RowX) &&
              commands::SlotConflict(config, "game", Rows, SlotToybox));
        CHECK(!commands::ToyboxTab(config));
        CHECK(SameAsParse(config));
        commands::Bindings bindings = commands::Read(config, "game");
        CHECK(bindings.toyboxKeys[0] == xKey && bindings.toyboxKeys[1] == "G" && bindings.toyboxKeys[2] == "F8");
        CHECK(!bindings.toyboxTab);
        CHECK(SaveLauncherConfig(path, config));
    }

    // Reset: back to the global layout. The game's key_, pad_ and mouse_ lines and its mouse switch go; [input] keeps
    // its keys, the Toy Box's too (all games').
    {
        LauncherConfig config = LoadLauncherConfig(path);
        commands::Reset(config, "game", Rows);
        CHECK(SaveLauncherConfig(path, config));
        config = LoadLauncherConfig(path);
        for (const auto& [key, value] : config[section])
            CHECK(!key.starts_with("key_") && !key.starts_with("pad_") && !key.starts_with("mouse"));
        CHECK(Value(config, "input", "key_toybox") == "C" && Value(config, "input", "key_toybox_add") == "G");
        CHECK(!commands::ToyboxTab(config) && !commands::MouseOn(config, "game"));
        CHECK(Value(config, "input", "key_left_up") == "Up" && commands::Key(config, "game", Rows[0]) == "Up");
        CHECK(commands::Key(config, "game", Rows[RowB]) == "X");
        CHECK(SameAsParse(config));
        commands::Bindings bindings = commands::Read(config, "game");
        CHECK(bindings.keys[KeyIndex("a")] == "Space");
        CHECK(bindings.padMap.Identity());
    }

    // The running game takes what the page saved, once.
    {
        RuntimeSettings settings;
        CHECK(!commands::Take(settings));
        LauncherConfig config = LoadLauncherConfig(path);
        commands::SetKey(config, "game", Rows, RowB, "K");
        commands::SetPad(config, "game", Rows, PadButtonIndex("a"), PadButtonIndex("b"));
        commands::Publish(commands::Read(config, "game"));
        CHECK(commands::Take(settings));
        CHECK(settings.keys[KeyIndex("b")] == "K");
        CHECK(settings.padMap.target[PadButtonIndex("b")] == PadButtonIndex("a"));
        CHECK(!settings.mouse.enabled); // Reset took the game's switch: [input] has none
        CHECK(settings.toyboxKeys[0] == "C" && !settings.toyboxTab); // Reset left the Toy Box's keys
        CHECK(!commands::Take(settings));
        commands::SetMouse(config, "game", Rows, MouseDefaults, KeyIndex("y"), mouse::X1);
        commands::SetKey(config, "game", Rows, SlotToybox, "T");
        commands::Publish(commands::Read(config, "game"));
        CHECK(commands::Take(settings));
        CHECK(settings.mouse.set[KeyIndex("y")] && settings.mouse.stored[KeyIndex("y")] == mouse::X1);
        CHECK(settings.toyboxKeys[0] == "T" && !settings.toyboxTab);
    }

    std::error_code ignored;
    fs::remove(path, ignored);
    printf("%d checks, %d failures\n", g_checks, g_failures);
    printf(g_failures == 0 ? "PASS\n" : "FAIL\n");
    return g_failures == 0 ? 0 : 1;
}
