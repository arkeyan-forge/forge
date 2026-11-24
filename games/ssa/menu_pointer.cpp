#include "menu_pointer.h"
#include <algorithm>
#include <cmath>

namespace ssamenu
{
    namespace
    {
        constexpr int AcceptWait =
            15; // frames a click waits for the game to move the focus (a greyed item never takes it)
        constexpr float Still = 0.5f;  // canvas pixels: less is a pointer at rest

        struct Found
        {
            const Page* page = nullptr;
            uint32_t item = 0;
        };

        // A page with a single item (the title's Press START) or none (a notice's OK) takes a click anywhere.
        Found Under(std::span<const Page> pages, float x, float y)
        {
            for (const Page& page : pages)
                if (uint32_t item = Hit(page.items, x, y))
                    return { &page, item };
            for (const Page& page : pages)
                if (page.items.size() <= 1)
                    return { &page, page.items.empty() ? page.focused : page.items[0].id };
            return {};
        }

        const Page* Find(std::span<const Page> pages, uint32_t handle)
        {
            for (const Page& page : pages)
                if (page.handle == handle)
                    return &page;
            return nullptr;
        }
    }

    void ToUnits(float canvasX, float canvasY, float& x, float& y)
    {
        x = (canvasX - 640.0f) / CanvasPerUnit;
        y = (360.0f - canvasY) / CanvasPerUnit;
    }

    uint32_t Hit(std::span<const Item> items, float x, float y)
    {
        // Boxes of neighbours overlap a little (a model's bounds take its glow): the one whose centre is nearest, for its
        // size, wins.
        uint32_t best = 0;
        float bestDistance = 0;
        for (const Item& item : items)
        {
            const float distance =
                std::max(std::fabs(x - item.x) / item.halfWidth, std::fabs(y - item.y) / item.halfHeight);
            if (distance <= 1 && (best == 0 || distance < bestDistance))
            {
                best = item.id;
                bestDistance = distance;
            }
        }
        return best;
    }

    Action Driver::Step(const Pointer& pointer, std::span<const Page> pages)
    {
        Action action;
        if (!pointer.active)
        {
            m_known = false;
            m_acceptPage = m_acceptItem = 0;
            return action;
        }

        // A click waiting for the focus: Accept once the game has moved it there, dropped if it never does. While it
        // moves, the page may hold no focus for a frame.
        if (m_acceptItem != 0)
        {
            const Page* page = Find(pages, m_acceptPage);
            if (page != nullptr && page->focused == m_acceptItem && !page->moving)
            {
                action.page = page->handle;
                action.accept = true;
                m_acceptPage = m_acceptItem = 0;
                return action;
            }
            if (++m_acceptFrames > AcceptWait)
                m_acceptPage = m_acceptItem = 0;
        }

        const bool moved = m_known && (std::fabs(pointer.x - m_x) >= Still || std::fabs(pointer.y - m_y) >= Still);
        m_known = true;
        m_x = pointer.x;
        m_y = pointer.y;
        if (!pointer.inside || pages.empty())
            return action;
        float x = 0, y = 0;
        ToUnits(pointer.x, pointer.y, x, y);
        const Found found = Under(pages, x, y);
        if (found.page == nullptr || (!moved && pointer.clicks == 0) || found.page->moving)
            return action;

        action.page = found.page->handle;
        if (found.item != found.page->focused)
            action.focus = found.item;
        if (pointer.clicks == 0)
            return action;
        if (action.focus == 0)
            action.accept = true;
        else
        {
            m_acceptPage = found.page->handle;
            m_acceptItem = found.item;
            m_acceptFrames = 0;
        }
        return action;
    }
}
