#pragma once

#include <array>
#include <cstdint>
#include <string_view>

// The chests of Imaginators' pile (games/imaginators/docs/research.md, "The chest pile"). Each chest in the pile is a
// toy id (64 bits: bit 63 set, the toy type at bits 48-62, the variant at 32-47, a serial below) and its loot. Three toy
// types: 236 the in-game chest, 237 a purchased one, 235 a Mystery Chest toy from the portal. A variant is 0x5000 with
// the tier in its low byte; the game's loot roll takes (type, low byte, bits 12-15, bit 9, bit 10, bit 11).
namespace imaginators
{
    struct ChestKind
    {
        std::string_view name; // the key in chests.txt and chests_add.txt
        uint16_t type, variant;
    };

    // In the launcher's order. The in-game chest's variant is 0: its award picks the loot itself.
    constexpr std::array<ChestKind, 9> ChestKinds = { { { "imaginite", 236, 0 }, { "copper", 237, 0x5004 },
        { "bronze", 237, 0x5007 }, { "silver", 237, 0x5008 }, { "gold", 237, 0x5009 }, { "platinum", 237, 0x5006 },
        { "toy_bronze", 235, 0x5001 }, { "toy_silver", 235, 0x5002 }, { "toy_gold", 235, 0x5003 } } };

    constexpr std::array<std::string_view, ChestKinds.size()> ChestKindNames = [] {
        std::array<std::string_view, ChestKinds.size()> names{};
        for (size_t i = 0; i < ChestKinds.size(); i++)
            names[i] = ChestKinds[i].name;
        return names;
    }();

    struct LootRoll
    {
        uint32_t type, tier, group, bit9, bit10, bit11;
    };
    // The loot roll's arguments for a chest of this kind, as the game's purchased-chest path derives them.
    constexpr LootRoll LootRollOf(const ChestKind& kind)
    {
        const uint32_t v = kind.variant;
        return { kind.type, v & 0xFF, (v >> 12) & 0xF, (v >> 9) & 1, (v >> 10) & 1, (v >> 11) & 1 };
    }

    // The kind of a chest in the pile from its toy id; -1 for any other.
    constexpr int ChestKindOf(uint64_t id)
    {
        const uint32_t type = uint32_t(id >> 48) & 0x7FFF, tier = uint32_t(id >> 32) & 0xFF;
        for (size_t i = 0; i < ChestKinds.size(); i++)
        {
            const ChestKind& kind = ChestKinds[i];
            // In-game chests all count as one kind, whatever their variant.
            if (kind.type == type && (kind.variant == 0 || (kind.variant & 0xFF) == tier))
                return int(i);
        }
        return -1;
    }
}
