// Exercise the actual adapter's offered actions and the host's pad mapping, without starting a game.
#include "game.h"
#include "trapteam.h"
#include "host/input.h"
#include "host/pad_map.h"
#include <cstdio>

bool trapteam::SetTextLanguage(std::string_view) { return true; }
bool trapteam::SetVoiceLanguage(std::string_view) { return true; }
std::string game::LevelArcOfFile(std::string_view) { return {}; }

int main()
{
    bool keyboard = false, pad = false;
    for (const commands::Row& row : game::Current().inputRows)
    {
        if (std::string_view(row.action) == "lb")
        {
            keyboard = true;
            pad = row.pad != nullptr && std::string_view(row.pad) == "lb";
        }
    }
    const int lb = PadButtonIndex("lb"), a = PadButtonIndex("a");
    bool mapped = lb >= 0 && a >= 0 && PadButtonMask(lb) == Button::LeftShoulder;
    PadMap map;
    GamepadState state{};
    state.buttons = Button::LeftShoulder;
    mapped = mapped && RemapPad(state, map).buttons == Button::LeftShoulder;
    if (lb >= 0 && a >= 0)
    {
        map.target[a] = int8_t(lb);
        map.target[lb] = int8_t(a);
        state.buttons = Button::A;
        state = RemapPad(state, map);
        mapped = mapped && state.buttons == Button::LeftShoulder;
        for (int user = 0; user < MaxUsers; ++user)
        {
            SetHostGamepad(state, user);
            uint32_t packet = 0;
            mapped = mapped && GetGamepad(packet, user).buttons == Button::LeftShoulder;
        }
    }
    if (!keyboard)
        fprintf(stderr, "FAIL: Trap Team must offer an LB keyboard action\n");
    if (!pad)
        fprintf(stderr, "FAIL: Trap Team must offer an LB pad action\n");
    if (!mapped)
        fprintf(stderr, "FAIL: default and rebound LB must reach both controllers as 0x0100\n");
    printf("Trap Team input: %s\n", keyboard && pad && mapped ? "pass" : "FAIL");
    return keyboard && pad && mapped ? 0 : 1;
}
