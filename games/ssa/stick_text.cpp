// SSA: the hints that name the stick show the keys on the keyboard (docs: docs/runtime/xbox360/input.md, "Glyphs").
// Every other button in SSA's text is &ICON(0,ICON_<name>) markup, drawn from a texture glyphs.cpp redraws for the
// device; two gameplay hints spell the stick instead, in all ten languages: level_008's boat (left or right) and
// level_025's War Machine (aim). On the keyboard style, when Text_Lookup asks for one of them, Text_Compile gets the
// text with the language's noun for the stick replaced by the blue arrows' icons (ICON_RL, plus ICON_UD for aiming),
// which the keyboard style draws as the left stick's keys (arrow_rl, arrow_ud in adapter.cpp). Only those nouns are
// stored here, no game text. The text is made when the hint is looked up: a device switch shows from the next one.
#include "game_hooks.h"
#include "host/glyphs.h"
#include "memory.h"
#include <cstdio>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <ppc_config.h>
#include <ppc_context.h>

namespace
{
    constexpr uint32_t BoatHint = 0xA0DDCEC4; // level_008: steer the ship with the stick, left or right
    constexpr uint32_t AimHint = 0x7A811D43;  // level_025: aim the War Machine with the stick

    // The stick as the ten languages name it in those hints, with its article where it has one (the longest first: a
    // shorter one is part of a longer one).
    constexpr std::string_view StickNouns[] = {
        "the Directional Stick",
        "le stick directionnel",
        "la levetta direzionale",
        "la palanca de direcci\xC3\xB3n",
        "la palanca de control",
        "de richtingsstick",
        "das Steuerkreuz",
        "retningspinden",
        "ohjaussauvalla",
        "venstre spak",
        "styrspaken",
        "the stick",
        "le stick",
        "den Stick",
        "la levetta",
        "de stick",
        "pinden",
        "sauvaa",
        "sauva",
        "spaken",
    };

    thread_local uint32_t t_key = 0; // the key Text_Lookup is looking up

    std::string WithKeys(std::string text, std::string_view icons)
    {
        for (std::string_view noun : StickNouns)
            for (size_t at = text.find(noun); at != std::string::npos; at = text.find(noun, at + icons.size()))
                text.replace(at, noun.size(), icons);
        return text;
    }

    // The guest copy of each rewritten text (Text_Compile copies it; made once and kept).
    uint32_t GuestText(const std::string& text)
    {
        static std::mutex mutex;
        static std::map<std::string, uint32_t> texts;
        std::lock_guard lock(mutex);
        auto [it, added] = texts.try_emplace(text, 0);
        if (added)
        {
            it->second = g_memory.SystemAlloc(uint32_t(text.size() + 1));
            std::memcpy(g_memory.Translate(it->second), text.c_str(), text.size() + 1);
        }
        return it->second;
    }
}

#if defined(GAME_HOOK_Text_Lookup) && defined(GAME_HOOK_Text_Compile)
PPC_FUNC_IMPL(GAME_IMP(Text_Lookup));
PPC_FUNC(GAME_HOOK(Text_Lookup))
{
    uint32_t outer = t_key;
    t_key = ctx.r5.u32;
    GAME_IMP(Text_Lookup)(ctx, base);
    t_key = outer;
}

PPC_FUNC_IMPL(GAME_IMP(Text_Compile));
PPC_FUNC(GAME_HOOK(Text_Compile))
{
    uint32_t key = std::exchange(t_key, 0); // only the lookup's own call, not a nested $NAME$ one
    if ((key == BoatHint || key == AimHint) && ctx.r3.u32 != 0 && glyphs::Current() == glyphs::Style::Keyboard)
    {
        std::string text = static_cast<const char*>(g_memory.Translate(ctx.r3.u32));
        std::string keys = WithKeys(text, key == BoatHint ? "&ICON(0,ICON_RL)" : "&ICON(0,ICON_RL)&ICON(0,ICON_UD)");
        if (keys != text)
        {
            fprintf(stderr, "[glyphs] hint %08X: the stick shown as its keys\n", key);
            ctx.r3.u64 = GuestText(keys);
        }
    }
    GAME_IMP(Text_Compile)(ctx, base);
}
#endif
