#pragma once

#include "host/figures/figure_data.h"
#include <cstring>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

// The virtual portal's slots. Status reports and block commands share this mutex with replacements, so a card's
// halves and a player swap become visible together. Kept independent of the device and file APIs for unit tests.
class PortalSlots
{
public:
    static constexpr int SlotCount = 16;

    struct Replacement
    {
        int slot;
        std::optional<FigureData> data; // empty: an actual removal
        std::string path;
    };

    // Exact slots, all or nothing. The caller has loaded and locked every new figure before replacing anything.
    bool Replace(const std::vector<Replacement>& replacements)
    {
        std::lock_guard lock(m_mutex);
        uint32_t used = 0;
        for (const Replacement& replacement : replacements)
        {
            int slot = replacement.slot;
            if (slot < 0 || slot >= SlotCount || ((m_reserved | used) >> slot & 1) != 0)
                return false;
            used |= uint32_t(1) << slot;
        }
        for (const Replacement& replacement : replacements)
        {
            Slot& slot = m_slots[replacement.slot];
            slot.data = replacement.data;
            slot.path = replacement.data ? replacement.path : std::string();
            slot.changed = true;
        }
        return true;
    }

    // Takes wanted if free, else the first free slot; -1 when full. An empty path keeps writes in memory.
    int Place(const FigureData& data, const std::string& path, int wanted = -1);
    void Remove(int slot);
    // A real portal's reset renumbers its toys from slot 0, each arriving again (the fake real portal's tests).
    void Renumber();
    // First four bytes of block 0: the tag serial. Returns -1 for none.
    int SlotOfSerial(const uint8_t* serial);
    std::string SlotPath(int slot);
    bool Holds(int slot);
    uint32_t TakeStatus();
    // Bit n: slot n belongs to a real toy of a mixed portal and cannot be placed or replaced here.
    void SetReserved(uint32_t slots)
    {
        std::lock_guard lock(m_mutex);
        m_reserved = slots;
    }

protected:
    struct Slot
    {
        std::optional<FigureData> data;
        std::string path;
        bool changed = false;
    };

    uint32_t StatusBits(); // under m_mutex: presence and change bits, clearing the change bits
    std::mutex m_mutex;
    Slot m_slots[SlotCount];
    uint32_t m_reserved = 0;
};

inline int PortalSlots::Place(const FigureData& data, const std::string& path, int wanted)
{
    std::lock_guard lock(m_mutex);

    // Prefer a slot whose last change was already reported. Replacements use Replace rather than remove/place.
    auto free = [this](int i) { return !m_slots[i].data && (m_reserved >> i & 1) == 0; };
    int slot = wanted >= 0 && wanted < SlotCount && free(wanted) ? wanted : -1;
    for (int i = 0; i < SlotCount && slot < 0; i++)
    {
        if (free(i) && !m_slots[i].changed)
            slot = i;
    }
    for (int i = 0; i < SlotCount && slot < 0; i++)
    {
        if (free(i))
            slot = i;
    }
    if (slot < 0)
        return -1;

    m_slots[slot].data = data;
    m_slots[slot].path = path;
    m_slots[slot].changed = true;
    return slot;
}

inline void PortalSlots::Remove(int slot)
{
    std::lock_guard lock(m_mutex);
    if (slot < 0 || slot >= SlotCount || !m_slots[slot].data)
        return;

    m_slots[slot].data.reset();
    m_slots[slot].path.clear();
    m_slots[slot].changed = true;
}

inline void PortalSlots::Renumber()
{
    std::lock_guard lock(m_mutex);
    int next = 0;
    for (int i = 0; i < SlotCount; i++)
    {
        Slot toy = std::move(m_slots[i]);
        m_slots[i] = {};
        if (!toy.data)
            continue;
        toy.changed = true;
        m_slots[next++] = std::move(toy);
    }
}

inline int PortalSlots::SlotOfSerial(const uint8_t* serial)
{
    std::lock_guard lock(m_mutex);
    for (int i = 0; i < SlotCount; i++)
    {
        if (m_slots[i].data && memcmp(m_slots[i].data->data(), serial, 4) == 0)
            return i;
    }
    return -1;
}

inline std::string PortalSlots::SlotPath(int slot)
{
    std::lock_guard lock(m_mutex);
    return slot >= 0 && slot < SlotCount && m_slots[slot].data ? m_slots[slot].path : std::string();
}

inline bool PortalSlots::Holds(int slot)
{
    std::lock_guard lock(m_mutex);
    return slot >= 0 && slot < SlotCount && m_slots[slot].data.has_value();
}

inline uint32_t PortalSlots::TakeStatus()
{
    std::lock_guard lock(m_mutex);
    return StatusBits();
}

// 'S' <LE32: 2 bits per slot, bit 0 = present, bit 1 = changed since the last report> <counter> <active>
inline uint32_t PortalSlots::StatusBits()
{
    uint32_t status = 0;
    for (int i = 0; i < SlotCount; i++)
    {
        uint32_t bits = (m_slots[i].data ? 1 : 0) | (m_slots[i].changed ? 2 : 0);
        m_slots[i].changed = false;
        status |= bits << (i * 2);
    }
    return status;
}
