#include "menu_pointer.h"
#include <algorithm>
#include <cmath>

namespace alchemymenu
{
    namespace
    {
        constexpr float Still = 0.5f; // canvas pixels: less is a pointer at rest
        constexpr uint16_t DpadUp = 0x0001, DpadDown = 0x0002, DpadLeft = 0x0004, DpadRight = 0x0008, A = 0x1000;

        const Item* Find(std::span<const Item> items, uint32_t id)
        {
            for (const Item& item : items)
                if (item.id == id)
                    return &item;
            return nullptr;
        }

        // The d-pad press from one item towards another: along the larger distance between their centres, or the
        // other axis.
        uint16_t Towards(const Item& from, const Item& to, bool otherAxis)
        {
            const float dx = (to.left + to.width / 2) - (from.left + from.width / 2);
            const float dy = (to.top + to.height / 2) - (from.top + from.height / 2);
            const bool horizontal = (std::fabs(dx) >= std::fabs(dy)) != otherAxis;
            if (horizontal)
                return dx == 0 ? 0 : dx > 0 ? DpadRight : DpadLeft;
            return dy == 0 ? 0 : dy > 0 ? DpadDown : DpadUp;
        }
    }

    uint32_t Hit(std::span<const Item> items, float x, float y)
    {
        // Boxes may overlap a little (a focused item grows): the one whose centre is nearest, for its size, wins.
        uint32_t best = 0;
        float bestDistance = 0;
        for (const Item& item : items)
        {
            if (item.width <= 0 || item.height <= 0)
                continue;
            const float distance = std::max(std::fabs(x - item.left - item.width / 2) / (item.width / 2),
                std::fabs(y - item.top - item.height / 2) / (item.height / 2));
            if (distance <= 1 && (best == 0 || distance < bestDistance))
            {
                best = item.id;
                bestDistance = distance;
            }
        }
        return best;
    }

    void Driver::Drop()
    {
        m_target = 0;
        m_accept = false;
        m_pressedFrom = 0;
        m_otherAxis = false;
        m_steps = 0;
    }

    uint16_t Driver::Step(const Pointer& pointer, const Page* page, int64_t now)
    {
        if (!pointer.active)
        {
            m_known = false;
            Drop();
            return 0;
        }
        const uint32_t handle = page != nullptr ? page->handle : 0;
        if (handle != m_page)
        {
            m_page = handle;
            Drop();
        }

        const bool moved = m_known && (std::fabs(pointer.x - m_x) >= Still || std::fabs(pointer.y - m_y) >= Still);
        m_known = true;
        m_x = pointer.x;
        m_y = pointer.y;

        uint16_t press = 0;
        if (pointer.inside)
        {
            const uint32_t under = page != nullptr ? Hit(page->items, pointer.x, pointer.y) : 0;
            if (pointer.clicks > 0 && page != nullptr && page->turns)
            {
                Drop();
                constexpr float Third = 1280.0f / 3;
                press = pointer.x < Third ? DpadLeft : pointer.x >= 2 * Third ? DpadRight : A;
            }
            else if (pointer.clicks > 0 && (page == nullptr || page->items.size() <= 1))
            {
                // No page (a title screen), a notice or a single button: a click anywhere is its A.
                Drop();
                press = A;
            }
            else if (pointer.clicks > 0 && under != 0 && page->focused != 0)
            {
                m_target = under;
                m_accept = true;
                m_steps = 0;
            }
            else if (moved && under != 0 && !page->scrolls && page->focused != 0 && under != m_target)
            {
                m_target = under;
                m_accept = false;
                m_steps = 0;
            }
        }
        if (now < m_next)
            return 0;
        if (press == 0 && m_target != 0 && page != nullptr)
        {
            const Item* from = Find(page->items, page->focused);
            const Item* to = Find(page->items, m_target);
            if (page->focused == m_target)
            {
                press = m_accept ? A : 0;
                Drop();
            }
            else if (from == nullptr || to == nullptr || m_steps > int(page->items.size()) * 2)
                Drop();
            else
            {
                // A press that left the focus where it was: the other axis once, then the item cannot be reached.
                if (m_steps > 0 && page->focused == m_pressedFrom)
                {
                    if (m_otherAxis)
                    {
                        Drop();
                        return 0;
                    }
                    m_otherAxis = true;
                }
                else
                    m_otherAxis = false;
                press = Towards(*from, *to, m_otherAxis);
                m_pressedFrom = page->focused;
                m_steps++;
                if (press == 0)
                    Drop();
            }
        }
        if (press != 0)
            m_next = now + PressGap;
        return press;
    }
}
