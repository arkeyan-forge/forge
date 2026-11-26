#include "wii/remote.h"
#include <algorithm>
#include <bit>
#include <cstdlib>
#include <cstring>

namespace wii::remote
{
    namespace
    {
        void Put32(uint8_t* at, uint32_t value)
        {
            for (int i = 0; i < 4; i++)
                at[i] = uint8_t(value >> (24 - 8 * i));
        }
        void PutFloat(uint8_t* at, float value) { Put32(at, std::bit_cast<uint32_t>(value)); }

        uint32_t ButtonNamed(char name)
        {
            switch (name)
            {
            case 'A': return button::A;
            case 'B': return button::B;
            case '1': return button::One;
            case '2': return button::Two;
            case '+': return button::Plus;
            case '-': return button::Minus;
            case 'H': return button::Home;
            case 'U': return button::Up;
            case 'D': return button::Down;
            case 'L': return button::Left;
            case 'R': return button::Right;
            default: return 0;
            }
        }

        // KPADStatus's fields the game reads: the buttons held, pressed and released since the last sample, the
        // accelerometer at rest (gravity along -Y, the remote held flat), the device and its error.
        namespace field
        {
            constexpr uint32_t Hold = 0x00, Trigger = 0x04, Release = 0x08, Acceleration = 0x0C,
                               AccelerationValue = 0x18;
            constexpr uint32_t DeviceType = 0x5C, Error = 0x5D;
        }
    }

    bool ParseScript(const std::string& text, std::vector<Press>& out, std::string& error)
    {
        out.clear();
        size_t at = 0;
        while (at < text.size())
        {
            const size_t end = std::min(text.find(',', at), text.size());
            const std::string item = text.substr(at, end - at);
            at = end + 1;
            const size_t sign = item.find('@');
            if (sign == std::string::npos || sign == 0)
            {
                error = "no buttons@read in \"" + item + "\"";
                return false;
            }
            Press press;
            for (size_t i = 0; i < sign; i++)
            {
                const uint32_t bit = ButtonNamed(item[i]);
                if (bit == 0)
                {
                    error = std::string("unknown button '") + item[i] + "'";
                    return false;
                }
                press.buttons |= bit;
            }
            char* rest = nullptr;
            const char* number = item.c_str() + sign + 1;
            press.first = uint32_t(strtoul(number, &rest, 10));
            press.count = 1;
            if (rest == number)
            {
                error = "no read number in \"" + item + "\"";
                return false;
            }
            if (*rest == '+')
            {
                const char* count = rest + 1;
                press.count = uint32_t(strtoul(count, &rest, 10));
                if (rest == count || press.count == 0)
                {
                    error = "no count after '+' in \"" + item + "\"";
                    return false;
                }
            }
            if (*rest != '\0')
            {
                error = "unexpected \"" + std::string(rest) + "\" in \"" + item + "\"";
                return false;
            }
            out.push_back(press);
        }
        return true;
    }

    void Remote::Sample(uint8_t* status)
    {
        uint32_t buttons = host.load(std::memory_order_relaxed);
        for (const Press& press : script)
            if (reads >= press.first && reads - press.first < press.count)
                buttons |= press.buttons;
        reads++;
        memset(status, 0, StatusSize);
        Put32(status + field::Hold, buttons);
        Put32(status + field::Trigger, buttons & ~held);
        Put32(status + field::Release, held & ~buttons);
        held = buttons;
        PutFloat(status + field::Acceleration + 4, -1.0f);
        PutFloat(status + field::AccelerationValue, 1.0f);
        status[field::DeviceType] = 0; // the remote alone, no extension
        status[field::Error] = 0;
    }

    Remote& First()
    {
        static Remote& remote = *new Remote; // read by the guest's threads until the process ends
        return remote;
    }
}
