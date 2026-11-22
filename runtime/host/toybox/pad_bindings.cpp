#include "host/toybox/pad_bindings.h"

namespace toybox::pad
{
    Binding Parse(int action, std::string_view value)
    {
        const Binding fallback = Bindings{}.buttons[size_t(action)];
        const size_t plus = value.find('+');
        if (action == Wheel)
        {
            const int button = PadButtonIndex(value);
            return button < 0 ? fallback : Binding{ button, -1 };
        }
        if (value == "none")
            return Binding{ -1, -1 };
        if (plus == std::string_view::npos)
            return fallback;
        const int first = PadButtonIndex(value.substr(0, plus)), second = PadButtonIndex(value.substr(plus + 1));
        return first < 0 || second < 0 || first == second ? fallback : Binding{ first, second };
    }

    std::string Format(Binding binding)
    {
        if (binding.first < 0)
            return "none";
        std::string text = g_padButtons[binding.first];
        if (binding.second >= 0)
            text += std::string("+") + g_padButtons[binding.second];
        return text;
    }

    int Line(std::string_view name)
    {
        for (int action = 0; action < Count; ++action)
            if (name == Lines[action])
                return action;
        return -1;
    }

    namespace
    {
        uint16_t Bit(int button) { return uint16_t(1u << button); }
        void Add(GamepadState& state, int button)
        {
            if (button == 6)
                state.leftTrigger = 255;
            else if (button == 7)
                state.rightTrigger = 255;
            else
                state.buttons |= PadButtonMask(button);
        }
    }

    Events State::Step(
        const Bindings& bindings, const GamepadState& physical, const PadMap& map, uint64_t now, Context context)
    {
        Events event;
        uint16_t held = 0, reserved = 0, hidden = 0;
        for (int button = 0; button < PadButtonCount; ++button)
        {
            const bool pressed = button == 6   ? physical.leftTrigger > 30
                                 : button == 7 ? physical.rightTrigger > 30
                                               : (physical.buttons & PadButtonMask(button)) != 0;
            if (pressed)
                held |= Bit(button);
        }
        const uint16_t pressed = held & ~previous, released = previous & ~held;
        for (const Binding& binding : bindings.buttons)
        {
            if (binding.first < 0)
                continue;
            reserved |= Bit(binding.first);
            if (held & Bit(binding.first))
                hidden |= Bit(binding.second < 0 ? binding.first : binding.second);
        }
        hidden |= reserved & held;
        for (int button = 0; button < PadButtonCount; ++button)
            if (pressed & Bit(button))
            {
                down[size_t(button)] = now;
                used &= uint16_t(~Bit(button));
            }

        // Chords win over a wheel hold sampled on the same frame. Once used, a first button cannot open again
        // until released, but the close chord can still be used inside an overlay.
        // Add and Clear act on the game's portal: only while it plays.
        for (int action = Toybox; action < Count; ++action)
        {
            const Binding binding = bindings.buttons[size_t(action)];
            if (binding.first < 0 || (CanUnbind(action) && context != Context::Playing))
                continue;
            if ((held & Bit(binding.first)) && (pressed & Bit(binding.second)) &&
                (context != Context::Playing || !(used & Bit(binding.first))))
            {
                event.toybox = action == Toybox;
                event.menu = action == Menu;
                event.add = action == ToyboxAdd;
                event.clear = action == ToyboxClear;
                used |= held & reserved;
                break;
            }
        }
        const int wheel = bindings.buttons[Wheel].first;
        const bool chord = event.toybox || event.menu || event.add || event.clear;
        if (context == Context::Playing && !chord && (held & Bit(wheel)) && !(used & Bit(wheel)) &&
            now - down[size_t(wheel)] >= HoldForWheel)
        {
            event.wheel = true;
            used |= held & reserved;
        }
        event.confirm = (released & Bit(wheel)) != 0;
        for (int button = 0; button < PadButtonCount; ++button)
        {
            const uint16_t bit = Bit(button);
            if ((released & reserved & bit) && !(used & bit) && context == Context::Playing)
                tapUntil[size_t(button)] = now + TapLength;
            if (context != Context::Playing || event.wheel || chord)
                tapUntil[size_t(button)] = 0;
            if (map.target[button] < 0) // turned off: no job to hide or give back
                continue;
            const int target = uint8_t(map.target[button]);
            if (hidden & bit)
                Add(event.hide, target);
            if (now < tapUntil[size_t(button)])
                Add(event.tap, target);
        }
        if (context != Context::Playing)
            used |= held & reserved;
        previous = held;
        return event;
    }
}
