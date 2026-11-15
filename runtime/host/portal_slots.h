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
// A replacement on an occupied slot is shown as a real portal shows a hand swap: one status report without the old
// toy, then the new one. Imaginators ignores a toy changed in place while the previous one's arrival still loads.
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
    // If one of them is occupied, every new figure of the batch arrives together, after the removal's report.
    bool Replace(const std::vector<Replacement>& replacements)
    {
        std::lock_guard lock(m_mutex);
        uint32_t used = 0;
        bool lift = false;
        for (const Replacement& replacement : replacements)
        {
            int slot = replacement.slot;
            if (slot < 0 || slot >= SlotCount || ((m_reserved | used) >> slot & 1) != 0)
                return false;
            used |= uint32_t(1) << slot;
            lift |= m_slots[slot].Held() != nullptr;
        }
        for (const Replacement& replacement : replacements)
        {
            Slot& slot = m_slots[replacement.slot];
            slot.changed |= !lift || slot.data.has_value(); // an empty slot stays unchanged until the arrival
            slot.path = replacement.data ? replacement.path : std::string();
            slot.data = lift ? std::nullopt : replacement.data;
            slot.pending = lift ? replacement.data : std::nullopt;
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
        std::optional<FigureData> data;    // what the title sees
        std::optional<FigureData> pending; // placed after the next report, which shows the slot empty
        std::string path;
        bool changed = false;

        // The figure the host has on this slot, arrived or not.
        const FigureData* Held() const { return data ? &*data : pending ? &*pending : nullptr; }
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
    auto free = [this](int i) { return m_slots[i].Held() == nullptr && (m_reserved >> i & 1) == 0; };
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
    if (slot < 0 || slot >= SlotCount || m_slots[slot].Held() == nullptr)
        return;

    m_slots[slot].changed |= m_slots[slot].data.has_value();
    m_slots[slot].data.reset();
    m_slots[slot].pending.reset();
    m_slots[slot].path.clear();
}

inline void PortalSlots::Renumber()
{
    std::lock_guard lock(m_mutex);
    int next = 0;
    for (int i = 0; i < SlotCount; i++)
    {
        Slot toy = std::move(m_slots[i]);
        m_slots[i] = {};
        if (toy.Held() == nullptr)
            continue;
        if (toy.pending)
            toy.data = std::exchange(toy.pending, std::nullopt);
        toy.changed = true;
        m_slots[next++] = std::move(toy);
    }
}

inline int PortalSlots::SlotOfSerial(const uint8_t* serial)
{
    std::lock_guard lock(m_mutex);
    for (int i = 0; i < SlotCount; i++)
    {
        const FigureData* held = m_slots[i].Held();
        if (held != nullptr && memcmp(held->data(), serial, 4) == 0)
            return i;
    }
    return -1;
}

inline std::string PortalSlots::SlotPath(int slot)
{
    std::lock_guard lock(m_mutex);
    return slot >= 0 && slot < SlotCount && m_slots[slot].Held() != nullptr ? m_slots[slot].path : std::string();
}

inline bool PortalSlots::Holds(int slot)
{
    std::lock_guard lock(m_mutex);
    return slot >= 0 && slot < SlotCount && m_slots[slot].Held() != nullptr;
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
        Slot& slot = m_slots[i];
        uint32_t bits = (slot.data ? 1 : 0) | (slot.changed ? 2 : 0);
        slot.changed = false;
        if (slot.pending)
        {
            slot.data = std::exchange(slot.pending, std::nullopt);
            slot.changed = true;
        }
        status |= bits << (i * 2);
    }
    return status;
}
