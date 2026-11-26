#include "juice_menu.h"
#include <algorithm>
#include <bit>

namespace juicemenu
{
    namespace
    {
        // The class metadata (igMetaObject): its name at +8; its fields' list at +0x14, the list's size in bytes in
        // the word before it. A field: its offset in the high half of +0x10, its name at +0x1C, a bit field's shift at
        // +0x28.
        constexpr uint32_t MetaName = 8, MetaFields = 0x14, FieldOffset = 0x10, FieldName = 0x1C, FieldShift = 0x28;
        constexpr uint32_t MaxFields = 256, MaxDepth = 16, MaxListItems = 256;
        constexpr float Canvas[2] = { 1280, 720 };

        bool IsGetMeta(uint32_t first, uint32_t second, uint32_t third, uint32_t& global)
        {
            // lis rN,high; lwz r3,low(rN); blr
            const uint32_t reg = (first >> 21) & 31;
            if (first >> 26 != 15 || ((first >> 16) & 31) != 0 || second >> 26 != 32 || ((second >> 21) & 31) != 3 ||
                ((second >> 16) & 31) != reg || third != 0x4E800020)
                return false;
            global = (first << 16) + uint32_t(int32_t(int16_t(second & 0xFFFF)));
            return true;
        }
    }

    uint32_t Reader::MetaOf(uint32_t object) const
    {
        if (object == 0 || (object & 3) != 0)
            return 0;
        const uint32_t vtable = m_memory.Word(object);
        const uint32_t getMeta = vtable != 0 && (vtable & 3) == 0 ? m_memory.Word(vtable) : 0;
        if (getMeta == 0 || (getMeta & 3) != 0)
            return 0;
        uint32_t global = 0;
        if (!IsGetMeta(m_memory.Word(getMeta), m_memory.Word(getMeta + 4), m_memory.Word(getMeta + 8), global))
            return 0;
        return m_memory.Word(global);
    }

    Reader::Field Reader::FieldOf(uint32_t meta, std::string_view name) const
    {
        const uint32_t data = m_memory.Word(meta + MetaFields);
        const uint32_t count = data != 0 ? m_memory.Word(data - 4) / 4 : 0;
        for (uint32_t i = 0; i < std::min(count, MaxFields); ++i)
        {
            const uint32_t field = m_memory.Word(data + 4 * i);
            if (field != 0 && m_memory.Text(m_memory.Word(field + FieldName)) == name)
                return { m_memory.Word(field + FieldOffset) >> 16, m_memory.Word(field + FieldShift), true };
        }
        return {};
    }

    float Reader::Float(uint32_t address) const { return std::bit_cast<float>(m_memory.Word(address)); }

    const Reader::ListLayout* Reader::List(uint32_t meta)
    {
        if (meta == 0 || m_memory.Text(m_memory.Word(meta + MetaName)) != "igJuiceList")
            return nullptr;
        auto it = m_lists.find(meta);
        if (it == m_lists.end())
            it = m_lists
                     .emplace(meta, ListLayout{ FieldOf(meta, "_items"), FieldOf(meta, "_visibleItems"),
                                        FieldOf(meta, "_focusItem"), FieldOf(meta, "_hasFocus"),
                                        FieldOf(meta, "_rootPlaceable"), FieldOf(meta, "_scrollPlaceable") })
                     .first;
        const ListLayout& layout = it->second;
        const bool complete = layout.items.found && layout.visibleItems.found && layout.focusItem.found &&
                              layout.hasFocus.found && layout.rootPlaceable.found && layout.scrollPlaceable.found;
        return complete ? &layout : nullptr;
    }

    const Reader::ArrayLayout* Reader::Array(uint32_t meta)
    {
        if (meta == 0)
            return nullptr;
        auto it = m_arrays.find(meta);
        if (it == m_arrays.end())
            it = m_arrays.emplace(meta, ArrayLayout{ FieldOf(meta, "_count"), FieldOf(meta, "_data") }).first;
        return it->second.count.found && it->second.data.found ? &it->second : nullptr;
    }

    const Reader::ItemLayout* Reader::Item(uint32_t meta)
    {
        if (meta == 0)
            return nullptr;
        auto it = m_items.find(meta);
        if (it == m_items.end())
            it = m_items
                     .emplace(meta, ItemLayout{ FieldOf(meta, "_placeable"), FieldOf(meta, "_min"),
                                        FieldOf(meta, "_max"), FieldOf(meta, "_visible") })
                     .first;
        const ItemLayout& layout = it->second;
        return layout.placeable.found && layout.min.found && layout.max.found && layout.visible.found ? &layout
                                                                                                      : nullptr;
    }

    const Reader::PlaceableLayout* Reader::Placeable(uint32_t meta)
    {
        if (meta == 0)
            return nullptr;
        auto it = m_placeables.find(meta);
        if (it == m_placeables.end())
            it = m_placeables
                     .emplace(meta, PlaceableLayout{ FieldOf(meta, "_parent"), FieldOf(meta, "_position"),
                                        FieldOf(meta, "_pivot"), FieldOf(meta, "_scale"), FieldOf(meta, "_extent"),
                                        FieldOf(meta, "_alpha"), FieldOf(meta, "_finalHidden") })
                     .first;
        const PlaceableLayout& layout = it->second;
        const bool complete = layout.parent.found && layout.position.found && layout.pivot.found &&
                              layout.scale.found && layout.extent.found && layout.alpha.found &&
                              layout.finalHidden.found && layout.finalHidden.shift < 32;
        return complete ? &layout : nullptr;
    }

    // An igObjectList: its count, and its data (a memory reference: its size, then the pointer).
    uint32_t Reader::Count(uint32_t array, uint32_t& data)
    {
        data = 0;
        const ArrayLayout* layout = Array(MetaOf(array));
        if (layout == nullptr)
            return 0;
        const uint32_t count = m_memory.Word(array + layout->count.offset);
        data = m_memory.Word(array + layout->data.offset + 4);
        return count <= MaxListItems && data != 0 ? count : 0;
    }

    // No placeable from this one up is hidden by the engine or transparent.
    bool Reader::Shown(uint32_t placeable)
    {
        float alpha = 1;
        for (uint32_t depth = 0; placeable != 0 && depth < MaxDepth; ++depth)
        {
            const PlaceableLayout* layout = Placeable(MetaOf(placeable));
            if (layout == nullptr)
                return false;
            const uint32_t bits = m_memory.Word(placeable + layout->finalHidden.offset);
            if ((bits >> layout->finalHidden.shift) & 1)
                return false;
            alpha *= Float(placeable + layout->alpha.offset);
            placeable = m_memory.Word(placeable + layout->parent.offset);
        }
        return placeable == 0 && alpha > 0.01f;
    }

    // Each placeable maps its own space to its parent's: its pivot (a fraction of its extent) sits at its position,
    // scaled about it.
    void Reader::ToCanvas(uint32_t placeable, float& x, float& y)
    {
        for (uint32_t depth = 0; placeable != 0 && depth < MaxDepth; ++depth)
        {
            const PlaceableLayout* layout = Placeable(MetaOf(placeable));
            if (layout == nullptr)
                return;
            const uint32_t position = placeable + layout->position.offset, pivot = placeable + layout->pivot.offset,
                           scale = placeable + layout->scale.offset, extent = placeable + layout->extent.offset;
            x = Float(position) + (x - Float(pivot) * Float(extent)) * Float(scale);
            y = Float(position + 4) + (y - Float(pivot + 4) * Float(extent + 4)) * Float(scale + 4);
            placeable = m_memory.Word(placeable + layout->parent.offset);
        }
    }

    bool Reader::Page(uint32_t list, Menu& menu)
    {
        const ListLayout* layout = List(MetaOf(list));
        if (layout == nullptr)
            return false;
        const uint32_t hasFocus = list + layout->hasFocus.offset;
        if (((m_memory.Word(hasFocus & ~3u) >> (24 - 8 * (hasFocus & 3))) & 0xFF) == 0)
            return false;
        uint32_t data = 0, visibleData = 0;
        const uint32_t count = Count(m_memory.Word(list + layout->items.offset), data);
        if (count == 0 || !Shown(m_memory.Word(list + layout->rootPlaceable.offset)))
            return false;
        const uint32_t visible = Count(m_memory.Word(list + layout->visibleItems.offset), visibleData);
        const uint32_t scroll = m_memory.Word(list + layout->scrollPlaceable.offset);

        size_t boxes = 0;
        for (uint32_t i = 0; i < count && boxes < MaxItems; ++i)
        {
            const uint32_t item = m_memory.Word(data + 4 * i);
            const ItemLayout* itemLayout = Item(MetaOf(item));
            if (itemLayout == nullptr)
                continue;
            const uint32_t shown = item + itemLayout->visible.offset;
            if (((m_memory.Word(shown & ~3u) >> (24 - 8 * (shown & 3))) & 0xFF) == 0)
                continue;
            float left = Float(item + itemLayout->min.offset), top = Float(item + itemLayout->min.offset + 4);
            float right = Float(item + itemLayout->max.offset), bottom = Float(item + itemLayout->max.offset + 4);
            ToCanvas(scroll, left, top);
            ToCanvas(scroll, right, bottom);
            alchemymenu::Item& box = menu.items[boxes];
            box.id = item;
            box.left = std::min(left, right);
            box.top = std::min(top, bottom);
            box.width = std::max(left, right) - box.left;
            box.height = std::max(top, bottom) - box.top;
            // Items scrolled off the canvas are not targets.
            if (box.width > 0 && box.height > 0 && box.left + box.width > 0 && box.left < Canvas[0] &&
                box.top + box.height > 0 && box.top < Canvas[1])
                boxes++;
        }
        menu.list = list;
        menu.page.handle = list;
        menu.page.focused = m_memory.Word(list + layout->focusItem.offset);
        menu.page.scrolls = visible < count;
        menu.page.items = std::span<const alchemymenu::Item>(menu.items.data(), boxes);
        return true;
    }

    std::vector<uint32_t> Reader::ListVtables(uint32_t begin, uint32_t end) const
    {
        std::vector<uint32_t> getMetas;
        for (uint32_t address = begin; address + 12 <= end && address >= begin; address += 4)
        {
            uint32_t global = 0;
            if (!IsGetMeta(m_memory.Word(address), m_memory.Word(address + 4), m_memory.Word(address + 8), global))
                continue;
            const uint32_t meta = m_memory.Word(global);
            if (meta != 0 && m_memory.Text(m_memory.Word(meta + MetaName)) == "igJuiceList")
                getMetas.push_back(address);
        }
        std::vector<uint32_t> vtables;
        if (getMetas.empty())
            return vtables;
        for (uint32_t address = begin; address + 4 <= end && address >= begin; address += 4)
            if (std::find(getMetas.begin(), getMetas.end(), m_memory.Word(address)) != getMetas.end())
                vtables.push_back(address);
        return vtables;
    }

    bool Reader::Read(std::span<const uint32_t> lists, Menu& menu)
    {
        std::vector<uint32_t> shown;
        uint32_t newest = 0;
        for (const uint32_t list : lists)
        {
            Menu candidate;
            if (!Page(list, candidate))
                continue;
            shown.push_back(list);
            if (std::find(m_shown.begin(), m_shown.end(), list) == m_shown.end())
                newest = list;
        }
        m_shown = shown;
        if (newest != 0)
            m_current = newest;
        else if (std::find(shown.begin(), shown.end(), m_current) == shown.end())
            m_current = shown.empty() ? 0 : shown.front();
        return m_current != 0 && Page(m_current, menu);
    }
}
