// Checks Add a figure's list for a seat (host/toybox/add_list.h), from the catalogue; no game data.
// usage: ArkeyanToyboxAddListTest
#include "host/toybox/add_list.h"
#include <algorithm>
#include <cstdio>

namespace
{
    int g_checks = 0;
    int g_failures = 0;

#define CHECK(condition)                                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        g_checks++;                                                                                                    \
        if (!(condition))                                                                                              \
        {                                                                                                              \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition);                                               \
            g_failures++;                                                                                              \
        }                                                                                                              \
    } while (0)

    const collection::Seat* FindSeat(const char* game, const char* id)
    {
        const collection::Game* g = collection::Catalogue::Get().FindGame(game);
        if (g == nullptr)
            return nullptr;
        for (const collection::Seat& seat : g->seats)
            if (seat.id == id)
                return &seat;
        return nullptr;
    }

    bool Has(const std::vector<const collection::Toy*>& toys, collection::Category category)
    {
        return std::any_of(
            toys.begin(), toys.end(), [category](const collection::Toy* toy) { return toy->category == category; });
    }
}

int main()
{
    using namespace collection;
    using namespace toybox;
    constexpr int Columns = 4;

    // Trap Team's Trap seat: only traps, then the row that shows every toy.
    const Seat* trap = FindSeat("trapteam", "trap");
    CHECK(trap != nullptr);
    if (trap != nullptr)
    {
        std::vector<const Toy*> toys;
        Grid grid = AddGrid(AddGroups("trapteam", trap), Columns, true, toys);
        CHECK(!toys.empty());
        CHECK(std::all_of(toys.begin(), toys.end(), [](const Toy* toy) { return toy->category == Category::Trap; }));
        CHECK(!grid.cells.empty() && grid.cells.back().kind == Cell::Kind::Fold);
        if (!grid.cells.empty())
        {
            const Cell& last = grid.cells.back();
            CHECK(last.column == 0 && last.row == grid.rows - 1);
            CHECK(last.row > grid.cells[grid.cells.size() - 2].row); // a row of its own
            CHECK(last.section >= 0 && last.section < int(grid.sections.size()));
        }
        int figures = int(std::count_if(
            grid.cells.begin(), grid.cells.end(), [](const Cell& cell) { return cell.kind == Cell::Kind::Figure; }));
        CHECK(figures == int(toys.size()));

        // Show all figures: every toy the game reads, no extra row.
        std::vector<const Toy*> all;
        Grid every = AddGrid(AddGroups("trapteam", nullptr), Columns, false, all);
        CHECK(all.size() > toys.size());
        CHECK(Has(all, Category::Trap) && Has(all, Category::TrapMaster));
        CHECK(std::none_of(
            every.cells.begin(), every.cells.end(), [](const Cell& cell) { return cell.kind == Cell::Kind::Fold; }));
    }

    // Player 1: characters only, no trap, vehicle or magic item.
    const Seat* p1 = FindSeat("trapteam", "p1");
    CHECK(p1 != nullptr);
    if (p1 != nullptr)
    {
        std::vector<const Toy*> toys;
        AddGrid(AddGroups("trapteam", p1), Columns, true, toys);
        CHECK(Has(toys, Category::Core) && Has(toys, Category::TrapMaster));
        CHECK(!Has(toys, Category::Trap) && !Has(toys, Category::Vehicle) && !Has(toys, Category::MagicItem));
    }

    // Swap Force's Player 1: a swapper's top is listed, its bottom (made with it) is not.
    const Seat* swapP1 = FindSeat("swapforce", "p1");
    CHECK(swapP1 != nullptr);
    if (swapP1 != nullptr)
    {
        std::vector<const Toy*> toys;
        AddGrid(AddGroups("swapforce", swapP1), Columns, true, toys);
        CHECK(Has(toys, Category::SwapTop) && !Has(toys, Category::SwapBottom));
        CHECK(!Has(toys, Category::MagicItem));
    }

    // SuperChargers' and Imaginators' vehicle seats: the real vehicles, never a racing villain's (3470-3497), which the
    // games define but which crash SuperChargers once one arrives outside a race. Show all figures still lists them.
    for (const char* game : { "superchargers", "imaginators" })
    {
        const Seat* vehicle1 = FindSeat(game, "vehicle1");
        CHECK(vehicle1 != nullptr);
        if (vehicle1 == nullptr)
            continue;
        auto villainVehicle = [](const Toy* toy) { return toy->type >= 3470 && toy->type <= 3497; };
        std::vector<const Toy*> toys;
        AddGrid(AddGroups(game, vehicle1), Columns, true, toys);
        CHECK(Has(toys, Category::Vehicle));
        CHECK(std::any_of(toys.begin(), toys.end(), [](const Toy* toy) { return toy->type == 3225; })); // Shark Tank
        CHECK(std::none_of(toys.begin(), toys.end(), villainVehicle));
        std::vector<const Toy*> all;
        AddGrid(AddGroups(game, nullptr), Columns, false, all);
        CHECK(std::any_of(all.begin(), all.end(), villainVehicle));
    }

    printf("toybox add list test: %d checks, %d failures\n", g_checks, g_failures);
    puts(g_failures ? "FAIL: add a figure for a seat" : "PASS: add a figure for a seat: its families, then show all");
    return g_failures == 0 ? 0 : 1;
}
