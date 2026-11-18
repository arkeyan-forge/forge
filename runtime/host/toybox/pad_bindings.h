#pragma once

#include "host/pad_map.h"
#include <array>
#include <string>

// Shared physical-pad shortcuts; no window, clock or device dependencies.
namespace toybox::pad
{
    inline constexpr uint64_t HoldForWheel = 250'000'000, TapLength = 120'000'000;
    enum Action
    {
        Wheel,
        Toybox,
        Menu,
        Count
    };
    inline constexpr const char* Lines[Count] = { "pad_wheel", "pad_toybox", "pad_menu" };
    struct Binding
    {
        int first = 9, second = -1;
        bool operator==(const Binding&) const = default;
    };
    struct Bindings
    {
        std::array<Binding, Count> buttons = { Binding{ 9, -1 }, Binding{ 9, 3 }, Binding{ 9, 8 } };
        bool operator==(const Bindings&) const = default;
    };
    Binding Parse(int action, std::string_view value);
    std::string Format(Binding binding);
    int Line(std::string_view name);
    enum class Context
    {
        Playing,
        Wheel,
        Overlay
    };
    struct Events
    {
        bool wheel = false, toybox = false, menu = false;
        bool confirm = false; // the physical wheel button was released (also clears ownership after leaving the wheel)
        GamepadState hide, tap; // game-side buttons/triggers, after remapping
    };
    struct State
    {
        uint16_t previous = 0, used = 0;
        std::array<uint64_t, PadButtonCount> down{}, tapUntil{};
        Events Step(const Bindings& bindings, const GamepadState& physical, const PadMap& map, uint64_t now,
            Context context = Context::Playing);
    };
}
