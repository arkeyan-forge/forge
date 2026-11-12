// SKY_LEVEL=<map> (or a full zone path, zone_name.h) for the games that ask for their next zone by name (SuperChargers, Imaginators; docs/porting/debugging.md):
// a new game plays its opening, then enters the zone <map>/<map> (Imaginators': 2016/<map>/<map>) instead of the one it asks for first after it
// (game::ZoneAfterIntro: SuperChargers' intro1/intro1 after the rift tunnel ride; games/superchargers/docs/boot.md "SKY_LEVEL").
//
// Zones are named "<area>/<map>" (academy/academy) or longer paths (ui/mainmenubackground/mainmenubackground). The game asks
// for its next zone by name (game::ZoneRequest), then enters it (game::GAME_HOOK_Zone_Enter): the zone's lookup, its archive
// \archives\<map>.pak and its package packages\generated\maps\<map>\<map>_pkg are all made from that name, so the request
// itself is renamed. Its text is interned in the engine's string pool (a reference count before the text), sized to the old
// name, so it is not written over: the request is pointed at a text of our own (a guest page) with the pointer's bit 0 set,
// the engine's mark of a borrowed text (sub_82450088), which the pool neither counts nor frees and which a copy interns
// (sub_824AD560). Only the lookup that enters a zone (game::ZoneEnterReturn) is a level start: another caller looks up the
// zone being left.
#include "game.h"
#include "game_hooks.h"
#include "host/env.h"
#include "memory.h"
#include "zone_name.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <ppc_config.h>
#include <ppc_context.h>

#if defined(GAME_HOOK_Zone_Find) && defined(GAME_HOOK_Zone_Enter)
namespace
{
    // SKY_LEVEL's zone: <game::ZonePrefix><map>/<map>, or the full zone path given; lower case; empty when unset.
    const std::string& LevelZoneName()
    {
        static const std::string name = [] {
            const char* value = skyenv::Get("SKY_LEVEL");
            if (value == nullptr || *value == '\0')
                return std::string();
            std::optional<std::string> named = alchemy::LevelZone(value, game::ZonePrefix);
            if (!named)
            {
                fprintf(stderr,
                    "[level] SKY_LEVEL=%s ignored: give the map's name (academy, sky1) or the zone's path\n", value);
                return std::string();
            }
            const std::string& zone = *named;
            fprintf(stderr, "[level] after the intro, the game enters zone %s (SKY_LEVEL)\n", zone.c_str());
            return zone;
        }();
        return name;
    }
}

PPC_FUNC_IMPL(GAME_IMP(Zone_Enter));
// (enter object): enters the zone game::ZoneRequest names.
PPC_FUNC(GAME_HOOK(Zone_Enter))
{
    const std::string& level = LevelZoneName();
    uint32_t request = g_memory.Read<uint32_t>(game::ZoneRequest);
    uint32_t text = request != 0 ? g_memory.Read<uint32_t>(request + game::ZoneRequestName) : 0;
    if (!level.empty() && text != 0)
    {
        std::string_view asked(static_cast<const char*>(g_memory.Translate(text & ~1u))); // bit 0: a borrowed text
        static const uint32_t page = g_virtualHeap4K.Alloc(4096);
        // SKY_LEVEL_FROM=<zone>: the request renamed is that zone's (academy/academy: a continued save), not the one
        // after the intro, so a test reaches its zone without the new game's opening.
        static const std::string from = [] {
            const char* value = skyenv::Get("SKY_LEVEL_FROM");
            return std::string(value != nullptr && *value != '\0' ? std::string_view(value) : game::ZoneAfterIntro);
        }();
        if (asked == from && page != 0)
        {
            memcpy(g_memory.Translate(page), level.c_str(), level.size() + 1);
            g_memory.Write<uint32_t>(request + game::ZoneRequestName, page | 1); // a borrowed text
            fprintf(stderr, "[level] the zone asked for, %.*s, is now %s (SKY_LEVEL)\n", int(asked.size()),
                asked.data(), level.c_str());
        }
    }
    GAME_IMP(Zone_Enter)(ctx, base);
}

PPC_FUNC_IMPL(GAME_IMP(Zone_Find));
// (zone list, const char* zone name) -> the zone, 0 if there is none.
PPC_FUNC(GAME_HOOK(Zone_Find))
{
    if (uint32_t(ctx.lr) == game::ZoneEnterReturn && ctx.r4.u32 != 0)
        game::LevelStarted(static_cast<const char*>(g_memory.Translate(ctx.r4.u32)));
    GAME_IMP(Zone_Find)(ctx, base);
}
#endif
