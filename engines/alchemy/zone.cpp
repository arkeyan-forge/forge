// SKY_LEVEL, the Alchemy engine's side (Swap Force): a new game starts in another zone (docs/porting/debugging.md).
//
// Swap Force names its levels ("zones") "<area>\<map>\<map>" (the folders of maps\ in zoneinfos.pak, e.g.
// tutorial\tutorial_a\tutorial_a, barrens\brn1\brn1) and finds a zone's index by name (Zone_IndexByName). A file
// redirect, as SSA and Giants have (kernel/files.cpp RedirectLevel), does not work here: a level's archive holds its
// package under the map's own path (brn1.pak: packages\generated\maps\barrens\brn1\brn1.pkgb), so the tutorial's name
// would be looked up in it and not found. When the game looks the first zone (game::FirstZone) up, its copy of the
// name becomes SKY_LEVEL's zone instead: the new game then loads that zone's own info, archives and package.
// That copy is a string of the engine's name table (game::NameTable), its last one at that moment: a longer name (most
// zones: multiplayer\survival\survival_wildwoods_alt01\survival_wildwoods_alt01 is 72 characters) is written in its
// place and the table's end moved past it, as if the game had stored that name; a shorter one fits in place.
// A survival arena (multiplayer\survival\...) also needs the mode its menu sets (Arena Modes > Solo Survival), else it
// faults at its first wave: it is stored as the next zone's mode, which the game applies itself when it enters the zone.
#include "game.h"
#include "game_hooks.h"
#include "memory.h"
#include "zone_name.h"
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <ppc_config.h>
#include <ppc_context.h>
#include "host/env.h"

#ifdef GAME_HOOK_Zone_IndexByName
namespace
{
    // SKY_LEVEL as a zone name (alchemy::BackslashZone); empty when unset.
    const std::string& LevelZoneName()
    {
        static const std::string name = [] {
            const char* value = skyenv::Get("SKY_LEVEL");
            if (value == nullptr || *value == '\0')
                return std::string();
            std::optional<std::string> zone = alchemy::BackslashZone(value);
            if (!zone)
            {
                fprintf(stderr,
                    "[level] SKY_LEVEL=%s ignored: a zone is <area>/<map> (e.g. barrens/brn1) or a zone name "
                    "of zoneinfos.pak (e.g. multiplayer/pvp/pvp_sandcastle/pvp_sandcastle)\n",
                    value);
                return std::string();
            }
            fprintf(stderr, "[level] a new game starts in zone %s (SKY_LEVEL)\n", zone->c_str());
            return *zone;
        }();
        return name;
    }

    // The zone whose name becomes SKY_LEVEL's: SKY_LEVEL_FROM (a Battle Arena or Ring Out arena the menus start: the
    // game's own two-player round, in the arena SKY_LEVEL names; a trailing slash: any zone under it), else the new
    // game's first zone.
    const std::string& FromZoneName()
    {
        static const std::string name = [] {
            const char* value = skyenv::Get("SKY_LEVEL_FROM");
            std::optional<std::string> zone = value != nullptr ? alchemy::BackslashZone(value) : std::nullopt;
            return zone ? *zone : std::string(game::FirstZone);
        }();
        return name;
    }
}

PPC_FUNC_IMPL(GAME_IMP(Zone_IndexByName));
// (zone list, const char* zone name) -> the zone's index, -1 if there is none.
PPC_FUNC(GAME_HOOK(Zone_IndexByName))
{
    // The name is the game's own copy of the zone it is about to enter (the archive names are made from it later), so
    // it is rewritten in place. A longer name needs the copy to be the name table's last string: the table's end moves.
    const std::string& level = LevelZoneName();
    if (!level.empty() && ctx.r4.u32 != 0)
    {
        auto* name = static_cast<char*>(g_memory.Translate(ctx.r4.u32));
        const std::string& from = FromZoneName();
        const std::string_view asked(name, strnlen(name, 160));
        // A SKY_LEVEL_FROM that ends with a backslash takes any zone under it (multiplayer\ringout\: whichever arena the
        // menu starts first).
        if (from.back() == '\\' ? asked.starts_with(from) : asked == from)
        {
            constexpr uint32_t endField = game::NameTable + 0x20004, strings = game::NameTable + 0x20008;
            uint32_t offset = ctx.r4.u32 - strings, end = g_memory.Read<uint32_t>(endField);
            bool last = ctx.r4.u32 >= strings && end == offset + asked.size() + 1;
            if (level.size() <= asked.size() || last)
            {
                memcpy(name, level.c_str(), level.size() + 1);
                if (last && level.size() > asked.size())
                    g_memory.Write<uint32_t>(endField, offset + uint32_t(level.size()) + 1);
                if (game::GameInstance != 0 && level.starts_with(game::SurvivalZones))
                {
                    constexpr uint32_t next = game::GameInstance + game::NextGameModeOffset;
                    fprintf(stderr,
                        "[level] the next zone's game mode %u -> %u (a survival arena, as Arena Modes > Solo "
                        "Survival)\n",
                        g_memory.Read<uint32_t>(next), game::GameModeSoloSurvival);
                    g_memory.Write<uint32_t>(next, game::GameModeSoloSurvival);
                }
            }
            else
                fprintf(stderr,
                    "[level] SKY_LEVEL ignored: the first zone's name is not the name table's last string "
                    "(end 0x%X, name at 0x%X), so %s does not fit\n",
                    end, offset, level.c_str());
        }
    }
    // The zone the game enters: a level started (the Toy Box's figures that ran out of health can play again).
    if (ctx.r4.u32 != 0)
    {
        const char* entered = static_cast<const char*>(g_memory.Translate(ctx.r4.u32));
        game::LevelStarted(std::string_view(entered, strnlen(entered, 160)));
    }
    GAME_IMP(Zone_IndexByName)(ctx, base);
}
#endif
