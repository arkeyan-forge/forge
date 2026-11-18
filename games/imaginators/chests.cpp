// Imaginite chests added from the launcher (launcher #152; games/imaginators/docs/research.md, "The chest pile";
// host/collection/chests.h for the two files). The launcher asks for chests of each kind for the slot played last
// (chests_add.txt); the next time that slot enters the Academy, each chest is added through the game's own in-game chest
// award, its type and variant swapped for the kind's in the two calls that make the chest (the loot roll and the id), so
// the game rolls the loot, adds the chest to the pile and saves it itself. chests.txt tells the launcher the slot and its
// pile each time a zone of a game is entered.
#include "chest_kinds.h"
#include "game.h"
#include "game_hooks.h"
#include "host/collection/chests.h"
#include "host/collection/progress.h"
#include <atomic>
#include <cstdio>
#include <mutex>
#include <ppc_config.h>
#include <ppc_context.h>
#include <string>

#ifdef GAME_ZONE_ENTERED
PPC_EXTERN_FUNC(GAME_FUNC_Chests_AwardInGame);

namespace
{
    using imaginators::ChestKind;
    using imaginators::ChestKinds;

    std::atomic<int> g_slot{ -1 };
    // The kind being added, on the game thread that adds it: the two hooks below change only that call's chest.
    thread_local const ChestKind* t_adding = nullptr;

    chests::Folder& Files()
    {
        static chests::Folder files(progress::Folder(g_gameInfo.id));
        return files;
    }

    uint32_t Pile(uint8_t* base)
    {
        const uint32_t manager = PPC_LOAD_U32(game::ChestManager);
        const uint32_t system = manager != 0 ? PPC_LOAD_U32(manager + game::ChestSystem) : 0;
        return system != 0 ? PPC_LOAD_U32(system + game::ChestPile) : 0;
    }

    int PileSize(uint8_t* base)
    {
        const uint32_t pile = Pile(base);
        return pile != 0 ? int32_t(PPC_LOAD_U32(pile + game::ListCount)) : 0;
    }

    // The chests not opened yet, by kind.
    chests::Status ReadPile(uint8_t* base, int slot)
    {
        chests::Status status{ slot, std::vector<int>(ChestKinds.size(), 0), 0 };
        const uint32_t pile = Pile(base);
        const int count = pile != 0 ? int32_t(PPC_LOAD_U32(pile + game::ListCount)) : 0;
        const uint32_t items = pile != 0 ? PPC_LOAD_U32(pile + game::ListItems) : 0;
        for (int i = 0; i < count && items != 0; i++)
        {
            const uint32_t chest = PPC_LOAD_U32(items + 4 * uint32_t(i));
            if (chest == 0 || PPC_LOAD_U8(chest + game::ChestOpened) != 0)
                continue;
            const int kind = imaginators::ChestKindOf(PPC_LOAD_U64(chest + game::ChestId));
            if (kind >= 0)
                status.pile[size_t(kind)]++;
            else
                status.other++;
        }
        return status;
    }

    // One chest of `kind` through the game's award; false when the game made none (no loot for it).
    bool AddChest(PPCContext& ctx, uint8_t* base, const ChestKind& kind)
    {
        const PPCContext saved = ctx;
        const int before = PileSize(base);
        ctx.r3.u64 = 0; // no popup
        ctx.f1.f64 = 0.0;
        ctx.f2.f64 = 0.0;
        t_adding = &kind;
        GAME_FUNC_Chests_AwardInGame(ctx, base);
        t_adding = nullptr;
        ctx = saved;
        return PileSize(base) == before + 1;
    }
}

PPC_FUNC_IMPL(GAME_IMP(Chests_LootRoll));
// (system, toy type, tier, group, bit 9, bit 10, bit 11) -> the loot of a new chest, 0 when there is none.
PPC_FUNC(GAME_HOOK(Chests_LootRoll))
{
    if (const ChestKind* kind = t_adding; kind != nullptr && kind->variant != 0)
    {
        const imaginators::LootRoll roll = imaginators::LootRollOf(*kind);
        ctx.r4.u64 = roll.type;
        ctx.r5.u64 = roll.tier;
        ctx.r6.u64 = roll.group;
        ctx.r7.u64 = roll.bit9;
        ctx.r8.u64 = roll.bit10;
        ctx.r9.u64 = roll.bit11;
    }
    GAME_IMP(Chests_LootRoll)(ctx, base);
}

PPC_FUNC_IMPL(GAME_IMP(Chests_ToyId));
// (toy type, variant, serial) -> the 64-bit toy id.
PPC_FUNC(GAME_HOOK(Chests_ToyId))
{
    if (const ChestKind* kind = t_adding; kind != nullptr && kind->variant != 0)
    {
        ctx.r3.u64 = kind->type;
        ctx.r4.u64 = kind->variant;
    }
    GAME_IMP(Chests_ToyId)(ctx, base);
}

PPC_FUNC_IMPL(GAME_IMP(Chests_SlotSet));
// (options storage, slot): a game was started or continued in that slot.
PPC_FUNC(GAME_HOOK(Chests_SlotSet))
{
    const int slot = int32_t(ctx.r4.u32);
    if (slot >= 0 && g_slot.exchange(slot) != slot)
        fprintf(stderr, "[chests] save slot %d\n", slot);
    GAME_IMP(Chests_SlotSet)(ctx, base);
}

void GAME_ZONE_ENTERED(PPCContext& ctx, uint8_t* base, std::string_view zone)
{
    const int slot = g_slot;
    // The menus hold no slot's pile.
    if (slot < 0 || game::InMenuLevel())
        return;
    static std::mutex mutex;
    std::lock_guard lock(mutex);
    if (zone == game::ChestZone)
    {
        if (const std::optional<chests::Request> request = Files().Claim(slot, imaginators::ChestKindNames))
        {
            const std::vector<int> plan = chests::Plan(*request, ReadPile(base, slot).Total());
            std::string added;
            int total = 0, refused = 0;
            for (size_t i = 0; i < ChestKinds.size(); i++)
            {
                int done = 0;
                for (int n = 0; n < plan[i]; n++)
                    done += AddChest(ctx, base, ChestKinds[i]) ? 1 : 0;
                refused += plan[i] - done;
                total += done;
                if (done > 0)
                    added += (added.empty() ? "" : " ") + std::string(ChestKinds[i].name) + "=" + std::to_string(done);
            }
            fprintf(stderr, "[chests] added %d (%s) to slot %d, %d asked, %d past the limit, %d refused by the game\n",
                total, added.c_str(), slot, request->Total(), request->Total() - (total + refused), refused);
        }
    }
    Files().WriteStatus(ReadPile(base, slot), imaginators::ChestKindNames);
}
#endif
