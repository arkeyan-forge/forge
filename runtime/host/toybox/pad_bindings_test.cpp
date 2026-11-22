// Physical shortcuts, deterministic nanosecond timing, and the game-side buttons they reserve.
#include "host/toybox/pad_bindings.h"
#include <cstdio>

namespace
{
    int checks = 0, failures = 0;
#define CHECK(condition)                                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        ++checks;                                                                                                      \
        if (!(condition))                                                                                              \
        {                                                                                                              \
            ++failures;                                                                                                \
            fprintf(stderr, "FAIL %d: %s\n", __LINE__, #condition);                                                    \
        }                                                                                                              \
    } while (0)
    GamepadState Press(const char* name)
    {
        GamepadState out;
        int button = PadButtonIndex(name);
        if (button == 6)
            out.leftTrigger = 255;
        else if (button == 7)
            out.rightTrigger = 255;
        else
            out.buttons = PadButtonMask(button);
        return out;
    }
}

int main()
{
    using namespace toybox::pad;
    PadMap map;
    for (int i = 0; i < PadButtonCount; ++i)
    {
        CHECK(Parse(Wheel, g_padButtons[i]).first == i);
        CHECK(Format(Parse(Wheel, g_padButtons[i])) == g_padButtons[i]);
        CHECK(Parse(Toybox, std::string("lt+") + g_padButtons[i]).second == (i == 6 ? 3 : i));
    }
    for (const char* invalid : { "", "none", "RB", "rb+a", "unknown" })
        CHECK(Parse(Wheel, invalid) == Bindings{}.buttons[Wheel]);
    for (const char* invalid : { "", "rb", "rb+rb", "+a", "rb+", "rb+a+x", "rb+none" })
        CHECK(Parse(Toybox, invalid) == Bindings{}.buttons[Toybox]);
    CHECK(Line("pad_wheel") == Wheel && Line("pad_menu") == Menu && Line("pad_back") == -1);
    Bindings bindings;
    bindings.buttons[Wheel] = Parse(Wheel, "rb");
    State state;
    auto event = state.Step(bindings, Press("rb"), map, 0);
    CHECK(event.hide.buttons == Button::RightShoulder && !event.wheel);
    event = state.Step(bindings, Press("rb"), map, HoldForWheel);
    CHECK(event.wheel);
    CHECK(state.Step(bindings, {}, map, HoldForWheel + 1, Context::Wheel).confirm);
    CHECK(state.Step(bindings, {}, map, HoldForWheel + 2).tap.buttons == 0);
    state = {};
    state.Step(bindings, Press("rb"), map, 0);
    event = state.Step(bindings, {}, map, 1);
    CHECK(event.tap.buttons == Button::RightShoulder);
    CHECK(state.Step(bindings, {}, map, TapLength + 1).tap.buttons == 0);
    // With separate first buttons, only the configured shortcuts reserve View.
    bindings.buttons[Toybox] = Parse(Toybox, "lb+a");
    bindings.buttons[Menu] = Parse(Menu, "lb+start");
    state = {};
    CHECK(state.Step(bindings, Press("back"), map, 0).hide.buttons == 0);
    CHECK(!state.Step(bindings, Press("back"), map, HoldForWheel).wheel);
    CHECK(state.Step(bindings, {}, map, HoldForWheel + 1).tap.buttons == 0);
    // Physical View still owns the default wheel when View sends A and RB sends View.
    bindings = {};
    map.target[9] = 0;
    map.target[5] = 9;
    state = {};
    CHECK(state.Step(bindings, Press("back"), map, 0).hide.buttons == (Button::A | Button::Y | Button::Start));
    CHECK(state.Step(bindings, Press("back"), map, HoldForWheel).wheel);
    state = {};
    state.Step(bindings, Press("back"), map, 0);
    CHECK(state.Step(bindings, {}, map, 1).tap.buttons == Button::A);
    state = {};
    state.Step(bindings, Press("rb"), map, 0);
    CHECK(!state.Step(bindings, Press("rb"), map, HoldForWheel).wheel);
    map = {};
    // Defaults reserve View and its chord seconds; chords consume the first button's tap.
    for (int action : { Toybox, Menu })
    {
        state = {};
        event = state.Step(bindings, Press("back"), map, 0);
        CHECK(event.hide.buttons == (Button::Back | Button::Y | Button::Start));
        auto chord = Press("back");
        chord.buttons |= PadButtonMask(bindings.buttons[size_t(action)].second);
        event = state.Step(bindings, chord, map, 1);
        CHECK(action == Toybox ? event.toybox : event.menu);
        CHECK(!state.Step(bindings, Press("back"), map, HoldForWheel).wheel);
        CHECK(state.Step(bindings, {}, map, HoldForWheel + 1).tap.buttons == 0);
        state = {};
        state.Step(bindings, Press("back"), map, 0, Context::Overlay);
        event = state.Step(bindings, chord, map, 1, Context::Overlay);
        CHECK(action == Toybox ? event.toybox : event.menu);
    }
    // A second already held before the first does not fire a chord.
    state = {};
    state.Step(bindings, Press("y"), map, 0);
    auto chord = Press("back");
    chord.buttons |= Button::Y;
    CHECK(!state.Step(bindings, chord, map, 1).toybox);
    // Trigger threshold, analog hiding and a digital tap returned to its remap target.
    bindings.buttons[Wheel] = Parse(Wheel, "lt");
    state = {};
    GamepadState trigger;
    trigger.leftTrigger = 30;
    CHECK(!state.Step(bindings, trigger, map, 0).wheel);
    trigger.leftTrigger = 31;
    CHECK(state.Step(bindings, trigger, map, 1).hide.leftTrigger == 255);
    CHECK(state.Step(bindings, trigger, map, HoldForWheel + 1).wheel);
    CHECK(state.Step(bindings, {}, map, HoldForWheel + 2, Context::Wheel).confirm);
    state = {};
    map.target[6] = 7;
    state.Step(bindings, trigger, map, 0);
    CHECK(state.Step(bindings, {}, map, 1).tap.rightTrigger == 255);
    // A separate chord prefix gets an unused tap; its second is hidden at its own remap target.
    bindings.buttons[Wheel] = Parse(Wheel, "rb");
    bindings.buttons[Toybox] = Parse(Toybox, "lb+a");
    bindings.buttons[Menu] = Parse(Menu, "lt+rt");
    map = {};
    map.target[4] = 6;
    map.target[0] = 7;
    state = {};
    event = state.Step(bindings, Press("lb"), map, 0);
    CHECK(event.hide.leftTrigger == 255 && event.hide.rightTrigger == 255);
    CHECK(!state.Step(bindings, Press("lb"), map, HoldForWheel).wheel);
    CHECK(state.Step(bindings, {}, map, HoldForWheel + 1).tap.leftTrigger == 255);
    state = {};
    state.Step(bindings, Press("lb"), map, 0);
    chord = Press("lb");
    chord.buttons |= Button::A;
    CHECK(state.Step(bindings, chord, map, 1).toybox);
    CHECK(state.Step(bindings, {}, map, 2).tap.leftTrigger == 0);
    // Seconds remain ordinary game buttons when the first is up.
    state = {};
    CHECK(state.Step(bindings, Press("a"), map, 0).hide.rightTrigger == 0);
    // A trigger can be a chord prefix or second, with the same threshold as remapping.
    state = {};
    state.Step(bindings, Press("lt"), map, 0);
    auto triggers = Press("lt");
    triggers.rightTrigger = 30;
    CHECK(!state.Step(bindings, triggers, map, 1).menu);
    triggers.rightTrigger = 31;
    CHECK(state.Step(bindings, triggers, map, 2).menu);
    // Default single taps and simultaneous default chords match the original shortcuts.
    bindings = {};
    map = {};
    state = {};
    CHECK(!state.Step(bindings, Press("back"), map, 0).wheel);
    CHECK(state.Step(bindings, {}, map, 1).tap.buttons == Button::Back);
    state = {};
    chord = Press("back");
    chord.buttons |= Button::Y;
    CHECK(state.Step(bindings, chord, map, 0).toybox);
    // Add and Clear: unbound by default, "none" unbinds any chord, and they fire only while playing.
    CHECK(
        Bindings{}.buttons[ToyboxAdd] == (Binding{ -1, -1 }) && Bindings{}.buttons[ToyboxClear] == (Binding{ -1, -1 }));
    CHECK(Parse(ToyboxClear, "back+b") == (Binding{ 9, 1 }));
    CHECK(Parse(ToyboxClear, "none") == (Binding{ -1, -1 }) && Parse(Menu, "none") == (Binding{ -1, -1 }));
    CHECK(Parse(ToyboxAdd, "rb") == Bindings{}.buttons[ToyboxAdd]);
    CHECK(Format(Binding{ -1, -1 }) == "none");
    CHECK(Line("pad_toybox_add") == ToyboxAdd && Line("pad_toybox_clear") == ToyboxClear);
    state = {};
    chord = Press("back");
    chord.buttons |= Button::B;
    for (int i = 0; i < 2; ++i)
    {
        event = state.Step(bindings, chord, map, uint64_t(i));
        CHECK(!event.add && !event.clear && !event.toybox && !event.menu);
        CHECK((event.hide.buttons & Button::B) == 0);
    }
    bindings.buttons[ToyboxClear] = Parse(ToyboxClear, "back+b");
    bindings.buttons[ToyboxAdd] = Parse(ToyboxAdd, "back+x");
    state = {};
    state.Step(bindings, Press("back"), map, 0);
    event = state.Step(bindings, chord, map, 1);
    CHECK(event.clear && !event.add && !event.toybox && !event.menu && !event.wheel);
    CHECK((event.hide.buttons & Button::B) != 0);
    CHECK(!state.Step(bindings, chord, map, 2).clear);
    CHECK(state.Step(bindings, {}, map, 3).tap.buttons == 0);
    state = {};
    state.Step(bindings, Press("back"), map, 0);
    auto add = Press("back");
    add.buttons |= Button::X;
    CHECK(state.Step(bindings, add, map, 1).add);
    for (Context context : { Context::Overlay, Context::Wheel })
    {
        state = {};
        state.Step(bindings, Press("back"), map, 0, context);
        event = state.Step(bindings, chord, map, 1, context);
        CHECK(!event.clear && !event.add);
    }
    // An unbound Menu reserves nothing: its old second is the game's again.
    bindings = {};
    bindings.buttons[Wheel] = Parse(Wheel, "rb");
    bindings.buttons[Toybox] = Parse(Toybox, "rb+y");
    bindings.buttons[Menu] = Parse(Menu, "none");
    state = {};
    CHECK(state.Step(bindings, Press("back"), map, 0).hide.buttons == 0);
    fprintf(stderr, "%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
