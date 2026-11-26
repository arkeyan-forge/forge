#include "host/toybox/add_list.h"
#include "host/collection/collection.h"
#include <algorithm>

namespace toybox
{
    using namespace collection;

    std::vector<AddGroup> AddGroups(const std::string& game, const Seat* fits)
    {
        const Catalogue& catalogue = Catalogue::Get();
        std::vector<const Toy*> toys = catalogue.Accepted(game);
        // A racing villain or its vehicle is no toy: the game plays it from a trophy (Toy::FromTrophy).
        std::erase_if(toys, [](const Toy* toy) { return toy->FromTrophy(); });
        // A swapper is added as one card (its top): the bottom of the same swapper is made with it.
        auto madeWithTop = [&](const Toy& toy) {
            return toy.category == Category::SwapBottom && catalogue.Accepts(game, MatchingHalf(toy.type));
        };
        if (fits != nullptr)
            std::erase_if(toys, [fits](const Toy* toy) { return !fits->Takes(*toy); });
        std::vector<AddGroup> groups;
        // None last: a character outside the ten elements (Imaginators' Kaos) has a group of its own.
        static const Element Elements[] = { Element::Magic, Element::Earth, Element::Water, Element::Air, Element::Fire,
            Element::Life, Element::Undead, Element::Tech, Element::Light, Element::Dark, Element::None };
        for (Element e : Elements)
        {
            AddGroup group{ Section::Kind::Element, int(e), {} };
            for (const Toy* toy : toys)
            {
                if (toy->element == e && toy->category != Category::Mini && toy->category <= Category::Sensei &&
                    !madeWithTop(*toy))
                    group.toys.push_back(toy);
            }
            groups.push_back(group);
        }
        for (Category c : { Category::Mini, Category::Trap, Category::Vehicle, Category::CreationCrystal,
                 Category::MagicItem, Category::Adventure, Category::Trophy })
        {
            AddGroup group{ Section::Kind::Category, int(c), {} };
            for (const Toy* toy : toys)
            {
                if (toy->category == c)
                    group.toys.push_back(toy);
            }
            groups.push_back(group);
        }
        return groups;
    }

    Grid AddGrid(const std::vector<AddGroup>& groups, int columns, bool showAll, std::vector<const Toy*>& toys)
    {
        Grid grid;
        toys.clear();
        for (const AddGroup& group : groups)
        {
            if (group.toys.empty())
                continue;
            int index = int(grid.sections.size());
            ListSection section{ group.kind, group.value, {} };
            section.firstRow = grid.rows;
            section.count = int(group.toys.size());
            grid.sections.push_back(section);
            int col = 0;
            for (const Toy* toy : group.toys)
            {
                if (col == columns)
                {
                    col = 0;
                    grid.rows++;
                }
                grid.cells.push_back({ Cell::Kind::Figure, int(toys.size()), index, grid.rows, col++ });
                toys.push_back(toy);
            }
            grid.rows++;
        }
        if (showAll)
        {
            Cell cell{ Cell::Kind::Fold, -1, std::max(int(grid.sections.size()) - 1, 0), grid.rows, 0 };
            grid.cells.push_back(cell);
            grid.rows++;
        }
        return grid;
    }
}
