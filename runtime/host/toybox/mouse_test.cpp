// Test the actual drawing hit registry and pointer/click handlers without initializing the renderer.
#include "draw.cpp"

// No game model is needed here: the model's actions only record what the hits asked for.
namespace
{
    std::vector<std::string> g_calls;
}
void toybox::Model::Run(Command command) { g_calls.push_back("run=" + std::to_string(int(command))); }
void toybox::Model::FocusAddCell(int cell) { g_calls.push_back("focus add=" + std::to_string(cell)); }
void toybox::Model::HoverAddCell(int cell) { g_calls.push_back("hover add=" + std::to_string(cell)); }
void toybox::Model::FocusColumnRow(int row) { g_calls.push_back("seat=" + std::to_string(row)); }

int main()
{
    using namespace toybox;
    Model model;
    int failures = 0;
    auto check = [&](bool pass) {
        if (!pass)
            failures++;
    };
    AddHit({ 0, 0 }, { 20, 20 }, [&] { model.list.cursor = 3; });
    g_hits.back().target = "card=3";
    check(Pointer(5, 5, true));
    check(HoverTarget() == "card=3" && model.list.cursor == 0);
    Click(model, 5, 5, 1, false);
    check(model.list.cursor == 3);
    AddHit({ 30, 0 }, { 50, 20 }, [&] { model.menuRow = Model::MainSettings; });
    g_hits.back().target = "menu=4";
    Pointer(35, 5, true);
    check(HoverTarget() == "menu=4" && model.menuRow == Model::MainResume); // the panel opens on Resume
    Click(model, 35, 5, 1, false);
    check(model.menuRow == Model::MainSettings);
    // A Settings row and a rail row: hover names them, a click does them.
    AddHit({ 90, 0 }, { 110, 20 }, [&] { model.settingsRow = 6; });
    g_hits.back().target = "settings=6";
    AddHit({ 120, 0 }, { 140, 20 }, [&] { model.achievementTab = 2; });
    g_hits.back().target = "rail=1";
    Pointer(95, 5, true);
    check(HoverTarget() == "settings=6" && model.settingsRow == 0);
    Click(model, 95, 5, 1, false);
    Click(model, 125, 5, 1, false);
    check(model.settingsRow == 6 && model.achievementTab == 2);
    AddHit({ 60, 0 }, { 80, 20 }, [&] {
        model.commandRow = 7;
        model.commandColumn = 2;
    });
    g_hits.back().target = "commands=7/2";
    Pointer(65, 5, true);
    check(HoverTarget() == "commands=7/2" && model.commandRow == 0);
    Click(model, 65, 5, 1, false);
    check(model.commandRow == 7 && model.commandColumn == 2);
    // A non-hoverable dialog backdrop blocks both hover and the card's action.
    AddHit({ 0, 0 }, { 20, 20 }, {});
    g_hits.back().hover = false;
    model.list.cursor = 0;
    Pointer(5, 5, true);
    Click(model, 5, 5, 1, false);
    check(HoverTarget() == "none" && model.list.cursor == 0);
    Pointer(35, 5, false);
    check(HoverTarget() == "none");
    // The pointer moving onto a focusable thing moves the focus there, once: a key moving the focus away is not undone
    // while the pointer stays on the same thing; moving onto another one focuses it.
    g_hits.clear();
    g_hovered = -1;
    int focused = -1;
    AddHit({ 0, 0 }, { 20, 20 }, [] {});
    g_hits.back().focus = [&] { focused = 0; };
    AddHit({ 30, 0 }, { 50, 20 }, [] {});
    g_hits.back().focus = [&] { focused = 1; };
    AddHit({ 60, 0 }, { 80, 20 }, [] {}); // no focus: a seat (a choice, not a focus)
    Pointer(5, 5, true);
    check(focused == 0);
    g_hovered = HoveredHit(); // the frame drawn
    focused = 7;              // a key moved the focus
    Pointer(8, 6, true);
    check(focused == 7);
    Pointer(35, 5, true);
    check(focused == 1);
    g_hovered = HoveredHit();
    Pointer(65, 5, true);
    check(focused == 1);
    // Add a figure: while a toy's versions are open, the other cards neither light up nor take the focus under the
    // pointer; a click still picks one. With none open, the pointer focuses the card it moves onto.
    g_hits.clear();
    g_hovered = -1;
    g_calls.clear();
    model.add.cursor = 0;
    model.addVersions = true;
    AddCardHit(model, 0, { 0, 100 }, { 20, 120 });
    AddCardHit(model, 1, { 30, 100 }, { 50, 120 });
    Pointer(35, 105, true);
    check(HoveredHit() == -1 && g_calls.empty());
    Click(model, 35, 105, 1, false);
    check(g_calls == std::vector<std::string>{ "focus add=1" });
    Pointer(5, 105, true);
    check(HoveredHit() == 0); // the toy whose versions show keeps its outline
    // A click on none of the cards nor the versions panel closes the versions; the panel's own gaps keep them.
    g_hits.clear();
    g_hovered = -1;
    g_calls.clear();
    model.addVersions = true;
    AddBackdropHit(model);
    AddCardHit(model, 0, { 0, 100 }, { 20, 120 });
    AddCardHit(model, 1, { 30, 100 }, { 50, 120 });
    AddVersionsPanelHit({ 60, 100 }, { 90, 140 });
    Click(model, 75, 130, 1, false);
    check(model.addVersions && g_calls.empty());
    Click(model, 25, 110, 1, false); // the gap between the cards
    check(!model.addVersions && g_calls.empty());
    g_hits.clear();
    g_hovered = -1;
    g_calls.clear();
    model.addVersions = false;
    AddCardHit(model, 0, { 0, 100 }, { 20, 120 });
    AddCardHit(model, 1, { 30, 100 }, { 50, 120 });
    Pointer(35, 105, true);
    check(HoveredHit() == 1 && g_calls == std::vector<std::string>{ "hover add=1" });
    // A double click on a seat with a figure takes it off (Delete); an empty seat, a real portal's or a physical
    // figure's is chosen for (Accept), as before.
    model.seats.assign(2, {});
    model.seats[0].path = "a.bin";
    auto seatHits = [&] { // drawn again each frame, from the seats as they are
        g_hits.clear();
        g_hovered = -1;
        SeatHit(model, 0, { 0, 200 }, { 20, 220 });
        SeatHit(model, 1, { 30, 200 }, { 50, 220 });
    };
    seatHits();
    const std::string del = "run=" + std::to_string(int(Command::Delete)),
                      accept = "run=" + std::to_string(int(Command::Accept));
    g_calls.clear();
    Click(model, 5, 205, 1, true);
    check(g_calls == std::vector<std::string>{ "seat=0", del });
    g_calls.clear();
    Click(model, 35, 205, 1, true);
    check(g_calls == std::vector<std::string>{ "seat=1", accept });
    g_calls.clear();
    Click(model, 5, 205, 1, false);
    check(g_calls == std::vector<std::string>{ "seat=0" });
    model.seats[0].physical = true;
    seatHits();
    g_calls.clear();
    Click(model, 5, 205, 1, true);
    check(g_calls == std::vector<std::string>{ "seat=0", accept });
    model.seats[0].physical = false;
    model.realPortal = true;
    seatHits();
    g_calls.clear();
    Click(model, 5, 205, 1, true);
    check(g_calls == std::vector<std::string>{ "seat=0", accept });
    printf("%d checks, %d failures\n", 26, failures);
    puts(failures
             ? "FAIL: menu pointer and click"
             : "PASS: menu/Settings/rail/card/Commands hover and click, blocked and outside hits, hover focus, Add "
               "cards' hover and a click beside them while versions show, a seat's double click");
    return failures ? 1 : 0;
}
