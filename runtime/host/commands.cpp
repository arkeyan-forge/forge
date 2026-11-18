#include "host/commands.h"
#include "host/settings.h"
#include <SDL3/SDL.h>
#include <cstdio>

namespace
{
    std::string KeyLine(const char* action) { return std::string("key_") + action; }
    std::string PadLine(int button) { return std::string("pad_") + g_padButtons[button]; }

    const std::string* Find(const LauncherConfig& config, const std::string& section, const std::string& key)
    {
        auto entry = config.find(section);
        if (entry == config.end())
            return nullptr;
        auto value = entry->second.find(key);
        return value == entry->second.end() || value->second.empty() ? nullptr : &value->second;
    }

    const char* DefaultKey(std::string_view action)
    {
        for (const KeyAction& entry : g_keyActions)
            if (action == entry.name)
                return entry.defaultKey;
        return "";
    }

    bool SameKey(const std::string& a, const std::string& b)
    {
        return SDL_GetScancodeFromName(a.c_str()) == SDL_GetScancodeFromName(b.c_str());
    }

    const std::string* FindLine(const LauncherConfig& config, std::string_view game, const std::string& key)
    {
        if (const std::string* value = Find(config, commands::Section(game), key))
            return value;
        return Find(config, "input", key);
    }

    // The value a game inherits for a key_ line: [input]'s, else the default.
    std::string GlobalKey(const LauncherConfig& config, const char* action)
    {
        if (const std::string* value = Find(config, "input", KeyLine(action)))
            return *value;
        return DefaultKey(action);
    }

    int GlobalPad(const LauncherConfig& config, int button)
    {
        const std::string* value = Find(config, "input", PadLine(button));
        if (value == nullptr)
            return button;
        if (*value == "none")
            return -1;
        const int target = PadButtonIndex(*value);
        return target < 0 ? button : target;
    }

    bool On(const std::string& value) { return value == "1" || value == "true" || value == "on" || value == "yes"; }

    void Erase(LauncherConfig& config, const std::string& section, const std::string& key)
    {
        if (auto entry = config.find(section); entry != config.end())
            entry->second.erase(key);
    }

    // The game's pad line; none when the button does what the global layout gives it.
    void StorePad(LauncherConfig& config, std::string_view game, int button, int target)
    {
        const std::string section = commands::Section(game);
        if (target == GlobalPad(config, button))
            Erase(config, section, PadLine(button));
        else
            config[section][PadLine(button)] = target < 0 ? "none" : g_padButtons[target];
    }

    // A section an older launcher seeded (its copy of [input]; key lines it lacked were the defaults) becomes its own
    // lines over the global layout `shared`.
    void MigrateSeeded(std::map<std::string, std::string>& section, const std::map<std::string, std::string>& shared)
    {
        auto sharedValue = [&](const std::string& key) -> const std::string* {
            auto value = shared.find(key);
            return value == shared.end() || value->second.empty() ? nullptr : &value->second;
        };
        for (const KeyAction& entry : g_keyActions)
        {
            const std::string line = KeyLine(entry.name);
            if (!commands::Shared(entry.name))
                continue;
            auto own = section.find(line);
            const bool has = own != section.end() && !own->second.empty();
            const std::string* global = sharedValue(line);
            const std::string globalKey = global != nullptr ? *global : entry.defaultKey;
            if (has && SameKey(own->second, globalKey))
                section.erase(own);
            else if (!has && !SameKey(entry.defaultKey, globalKey))
                section[line] = entry.defaultKey;
        }
        for (auto line = section.begin(); line != section.end();)
        {
            const std::string& key = line->first;
            const std::string* global = sharedValue(key);
            bool drop = false;
            if (key.starts_with("pad_") && toybox::pad::Line(key) < 0)
                drop = global != nullptr ? *global == line->second : line->second == key.substr(4);
            else if (key == "rumble_p1" || key == "rumble_p2")
                drop = line->second == (global != nullptr ? *global : "normal");
            else if (key == "mouse")
                drop = On(line->second) == (global != nullptr && On(*global));
            line = drop ? section.erase(line) : std::next(line);
        }
        section.erase("seeded");
    }

    constexpr const char* ToyboxDefaults[commands::ToyboxKeyCount] = { "F6", "F7", "F8" };

    size_t ActionIndex(std::string_view action)
    {
        for (size_t i = 0; i < std::size(g_keyActions); i++)
            if (action == g_keyActions[i].name)
                return i;
        return std::size(g_keyActions);
    }

    void StoreSlot(LauncherConfig& config, std::string_view game, std::span<const commands::Row> rows, size_t slot,
        const std::string& key)
    {
        if (slot >= rows.size())
        {
            config["input"][commands::ToyboxLines[slot - rows.size()]] = key;
            return;
        }
        const std::string line = KeyLine(rows[slot].action);
        if (SameKey(key, GlobalKey(config, rows[slot].action)))
            Erase(config, commands::Section(game), line);
        else
            config[commands::Section(game)][line] = key;
    }

    commands::Bindings g_published;
    bool g_pending;
}

std::string commands::Section(std::string_view game) { return std::string(game) + ".input"; }

bool commands::Shared(std::string_view action)
{
    for (const char* shared : SharedActions)
        if (action == shared)
            return true;
    return false;
}

bool commands::Migrate(LauncherConfig& config)
{
    // The first upgrade needs every game's section (a game without one reads [input]'s keys), which only the launcher
    // knows: until then the file stays as it is and is read the old way.
    if (!Global(config))
        return false;
    const std::map<std::string, std::string> shared = config["input"];
    bool changed = false;
    for (auto& [name, section] : config)
        if (name.size() > 6 && name.ends_with(".input") && section.count("seeded") != 0)
        {
            MigrateSeeded(section, shared);
            changed = true;
        }
    if (!changed)
        return false;
    // The other actions are each game's own now: an older version's PLAY may have put them back.
    for (const KeyAction& entry : g_keyActions)
        if (!Shared(entry.name))
            Erase(config, "input", KeyLine(entry.name));
    for (auto section = config.begin(); section != config.end();)
        section =
            section->first.ends_with(".input") && section->second.empty() ? config.erase(section) : std::next(section);
    return true;
}

bool commands::Global(const LauncherConfig& config) { return Find(config, "input", "global") != nullptr; }

std::string commands::Key(const LauncherConfig& config, std::string_view game, const Row& row)
{
    if (const std::string* value = FindLine(config, game, KeyLine(row.action)))
        return *value;
    return DefaultKey(row.action);
}

int commands::PadTarget(const LauncherConfig& config, std::string_view game, int button)
{
    const std::string* value = FindLine(config, game, PadLine(button));
    if (value == nullptr)
        return button;
    if (*value == "none")
        return -1;
    int target = PadButtonIndex(*value);
    return target < 0 ? button : target;
}

std::string commands::ToyboxKey(const LauncherConfig& config, int which)
{
    if (const std::string* value = Find(config, "input", ToyboxLines[which]))
        return *value;
    return ToyboxDefaults[which];
}

bool commands::ToyboxTab(const LauncherConfig& config) { return Find(config, "input", ToyboxLines[0]) == nullptr; }

int commands::PadConflict(const LauncherConfig& config, std::string_view game, std::span<const Row> rows, int action)
{
    const auto binding = Read(config, game).toyboxPad.buttons[size_t(action)];
    for (int button : { binding.first, binding.second })
    {
        if (button < 0 || (button == binding.first && button == 9))
            continue; // View is the default wheel/first button, never a warning.
        if (binding.first == 9 && button == binding.second &&
            ((action == toybox::pad::Toybox && button == 3) || (action == toybox::pad::Menu && button == 8)))
            continue;
        // Every button reaches the game (menus read the D-pad and the stick clicks too), so any job counts, named or not.
        const int target = PadTarget(config, game, button);
        if (target < 0)
            continue;
        for (size_t row = 0; row < rows.size(); ++row)
            if (rows[row].pad != nullptr && PadButtonIndex(rows[row].pad) == target)
                return int(row);
        return int(rows.size());
    }
    return -1;
}

std::string commands::SlotKey(
    const LauncherConfig& config, std::string_view game, std::span<const Row> rows, size_t slot)
{
    return slot < rows.size() ? Key(config, game, rows[slot]) : ToyboxKey(config, int(slot - rows.size()));
}

bool commands::SlotConflict(const LauncherConfig& config, std::string_view game, std::span<const Row> rows, size_t slot)
{
    const std::string key = SlotKey(config, game, rows, slot);
    for (size_t i = 0; i < rows.size() + ToyboxKeyCount; i++)
        if (i != slot && SameKey(SlotKey(config, game, rows, i), key))
            return true;
    return false;
}

int commands::SetKey(
    LauncherConfig& config, std::string_view game, std::span<const Row> rows, size_t slot, const std::string& key)
{
    StoreSlot(config, game, rows, slot, key);
    for (size_t i = 0; i < rows.size() + ToyboxKeyCount; i++)
        if (i != slot && SameKey(SlotKey(config, game, rows, i), key))
            return int(i);
    return -1;
}

std::string commands::SlotLabel(std::span<const Row> rows, size_t slot, bool french)
{
    if (slot < rows.size())
    {
        const Row& row = rows[slot];
        const std::string_view action = row.action;
        static constexpr const char* directions[] = { "right_up", "right_down", "right_left", "right_right" };
        static constexpr const char* english[] = { "R stick up", "R stick down", "R stick left", "R stick right" };
        static constexpr const char* translated[] = { "stick droit haut", "stick droit bas", "stick droit gauche",
            "stick droit droite" };
        for (size_t i = 0; i < std::size(directions); i++)
            if (action == directions[i])
                return french ? translated[i] : english[i];
        return french ? row.french : row.label;
    }
    static constexpr const char* english[] = { "Open the Toy Box", "Add a figure", "Clear the figures" };
    static constexpr const char* translated[] = { "Ouvrir la Toy Box", "Ajouter une figurine",
        "Retirer les figurines" };
    slot -= rows.size();
    return slot < ToyboxKeyCount ? (french ? translated[slot] : english[slot]) : "";
}

bool commands::MouseOn(const LauncherConfig& config, std::string_view game)
{
    const std::string* value = FindLine(config, game, "mouse");
    return value != nullptr && On(*value);
}

void commands::SetMouseOn(LauncherConfig& config, std::string_view game, std::span<const Row>, bool on)
{
    const std::string* global = Find(config, "input", "mouse");
    if (on == (global != nullptr && On(*global)))
        Erase(config, Section(game), "mouse");
    else
        config[Section(game)]["mouse"] = on ? "1" : "0";
}

mouse::Bindings commands::MouseLines(const LauncherConfig& config, std::string_view game)
{
    mouse::Bindings bindings;
    bindings.enabled = MouseOn(config, game);
    for (const std::string& name : { std::string("input"), Section(game) })
    {
        auto section = config.find(name);
        if (section == config.end())
            continue;
        for (const auto& [key, value] : section->second)
        {
            mouse::Button button;
            size_t action =
                key.starts_with("mouse_") ? ActionIndex(std::string_view(key).substr(6)) : std::size(g_keyActions);
            if (action < std::size(g_keyActions) && mouse::FromName(value, button))
            {
                bindings.stored[action] = button;
                bindings.set[action] = true;
            }
        }
    }
    return bindings;
}

int commands::SetMouse(LauncherConfig& config, std::string_view game, std::span<const Row>,
    std::span<const mouse::Default> defaults, size_t action, mouse::Button button)
{
    mouse::Button now[mouse::ActionCount];
    mouse::Effective(MouseLines(config, game), defaults, now);
    int other = -1;
    for (size_t i = 0; i < mouse::ActionCount && other < 0; i++)
        if (i != action && button != mouse::None && now[i] == button)
            other = int(i);
    const std::string section = Section(game);
    config[section][std::string("mouse_") + g_keyActions[action].name] =
        button == mouse::None ? "none" : mouse::Name(button);
    if (other >= 0)
        config[section][std::string("mouse_") + g_keyActions[other].name] = "none";
    return other;
}

int commands::SetPad(LauncherConfig& config, std::string_view game, std::span<const Row>, int job, int button)
{
    const int old = PadTarget(config, game, button);
    int other = -1;
    if (old != job)
    {
        for (int b = 0; b < PadButtonCount && other < 0; b++)
            if (b != button && PadTarget(config, game, b) == job)
                other = b;
        StorePad(config, game, button, job);
        if (other >= 0)
            StorePad(config, game, other, old);
    }
    return other;
}

void commands::Reset(LauncherConfig& config, std::string_view game, std::span<const Row>)
{
    std::map<std::string, std::string>& section = config[Section(game)];
    for (const KeyAction& entry : g_keyActions)
        section.erase(KeyLine(entry.name));
    for (int button = 0; button < PadButtonCount; button++)
        section.erase(PadLine(button));
    for (const KeyAction& entry : g_keyActions)
        section.erase(std::string("mouse_") + entry.name);
    section.erase("mouse");
    if (Global(config))
        return;
    // Old layout: [input] still holds the last played game's keys and mouse, which the other games read.
    for (const KeyAction& entry : g_keyActions)
        if (!Shared(entry.name))
            if (const std::string* value = Find(config, "input", KeyLine(entry.name));
                value != nullptr && !SameKey(*value, entry.defaultKey))
                section[KeyLine(entry.name)] = entry.defaultKey;
    if (const std::string* value = Find(config, "input", "mouse"); value != nullptr && On(*value))
        section["mouse"] = "0";
}

commands::Bindings commands::Read(const LauncherConfig& config, std::string_view game)
{
    Bindings bindings;
    for (size_t i = 0; i < std::size(g_keyActions); i++)
        bindings.keys[i] = g_keyActions[i].defaultKey;
    for (const std::string& name : { std::string("input"), Section(game) })
    {
        auto section = config.find(name);
        if (section == config.end())
            continue;
        for (const auto& [key, value] : section->second)
        {
            if (key.starts_with("key_") && SDL_GetScancodeFromName(value.c_str()) != SDL_SCANCODE_UNKNOWN)
            {
                for (size_t i = 0; i < std::size(g_keyActions); i++)
                    if (key.compare(4, std::string::npos, g_keyActions[i].name) == 0)
                        bindings.keys[i] = value;
            }
            else if (key.starts_with("pad_") && toybox::pad::Line(key) < 0)
            {
                const int from = PadButtonIndex(std::string_view(key).substr(4)), to = PadButtonIndex(value);
                if (from >= 0 && (to >= 0 || value == "none"))
                    bindings.padMap.target[from] = int8_t(to);
            }
        }
    }
    for (int action = 0; action < toybox::pad::Count; ++action)
        if (const std::string* value = Find(config, "input", toybox::pad::Lines[action]))
            bindings.toyboxPad.buttons[size_t(action)] = toybox::pad::Parse(action, *value);
    bindings.mouse = MouseLines(config, game);
    for (int i = 0; i < ToyboxKeyCount; i++)
        if (const std::string* value = Find(config, "input", ToyboxLines[i]);
            value != nullptr && SDL_GetScancodeFromName(value->c_str()) != SDL_SCANCODE_UNKNOWN)
        {
            bindings.toyboxKeys[i] = *value;
            bindings.toyboxTab = bindings.toyboxTab && i != 0;
        }
    return bindings;
}

void commands::Publish(const Bindings& bindings)
{
    g_published = bindings;
    g_pending = true;
}

bool commands::Take(RuntimeSettings& settings)
{
    if (!g_pending)
        return false;
    g_pending = false;
    std::string changed;
    for (size_t i = 0; i < std::size(g_keyActions); i++)
    {
        if (settings.keys[i] != g_published.keys[i])
            changed +=
                std::string(changed.empty() ? "" : ", ") + "key_" + g_keyActions[i].name + "=" + g_published.keys[i];
        settings.keys[i] = g_published.keys[i];
    }
    for (int i = 0; i < PadButtonCount; i++)
    {
        int8_t target = g_published.padMap.target[i];
        if (settings.padMap.target[i] != target)
            changed += std::string(changed.empty() ? "" : ", ") + "pad_" + g_padButtons[i] + "=" +
                       (target < 0 ? "none" : g_padButtons[target]);
    }
    settings.padMap = g_published.padMap;
    if (settings.mouse.enabled != g_published.mouse.enabled)
        changed += std::string(changed.empty() ? "" : ", ") + "mouse=" + (g_published.mouse.enabled ? "1" : "0");
    for (size_t i = 0; i < mouse::ActionCount; i++)
        if (g_published.mouse.set[i] &&
            (!settings.mouse.set[i] || settings.mouse.stored[i] != g_published.mouse.stored[i]))
            changed += std::string(changed.empty() ? "" : ", ") + "mouse_" + g_keyActions[i].name + "=" +
                       (g_published.mouse.stored[i] == mouse::None ? "none" : mouse::Name(g_published.mouse.stored[i]));
    settings.mouse = g_published.mouse;
    for (int i = 0; i < ToyboxKeyCount; i++)
    {
        if (settings.toyboxKeys[i] != g_published.toyboxKeys[i])
            changed += std::string(changed.empty() ? "" : ", ") + ToyboxLines[i] + "=" + g_published.toyboxKeys[i];
        settings.toyboxKeys[i] = g_published.toyboxKeys[i];
    }
    settings.toyboxTab = g_published.toyboxTab;
    for (int action = 0; action < toybox::pad::Count; ++action)
        if (settings.toyboxPad.buttons[size_t(action)] != g_published.toyboxPad.buttons[size_t(action)])
            changed += std::string(changed.empty() ? "" : ", ") + toybox::pad::Lines[action] + "=" +
                       toybox::pad::Format(g_published.toyboxPad.buttons[size_t(action)]);
    settings.toyboxPad = g_published.toyboxPad;
    fprintf(stderr, "[input] bindings in use now: %s\n", changed.empty() ? "unchanged" : changed.c_str());
    return true;
}
