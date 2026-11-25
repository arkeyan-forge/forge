// The controllers as the title reads them (host/input.h): a press made by the runtime (PressButtons) adds to the
// player's own buttons on that controller only, for PressLength, then lets go. No SDL, no game data: ./ArkeyanInputTest
#include "host/input.h"
#include <chrono>
#include <cstdio>
#include <thread>

namespace
{
    int g_checks = 0, g_failures = 0;
#define EXPECT(condition)                                                                                              \
    do                                                                                                                 \
    {                                                                                                                  \
        g_checks++;                                                                                                    \
        if (!(condition))                                                                                              \
        {                                                                                                              \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition);                                               \
            g_failures++;                                                                                              \
        }                                                                                                              \
    } while (0)
}

int main()
{
    GamepadState held;
    held.buttons = Button::DpadUp;
    SetHostGamepad(held);
    uint32_t before = 0, packet = 0;
    EXPECT(GetGamepad(before).buttons == Button::DpadUp);

    PressButtons(Button::A);
    GamepadState state = GetGamepad(packet);
    EXPECT(state.buttons == (Button::DpadUp | Button::A)); // over the player's own buttons
    EXPECT(packet != before); // the title sees a new state
    EXPECT(GetGamepad(packet, 1).buttons == 0); // the other controller is untouched

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT(GetGamepad(packet).buttons == Button::DpadUp); // let go

    PressButtons(Button::Start, 1);
    EXPECT(GetGamepad(packet, 1).buttons == Button::Start);
    EXPECT(GetGamepad(packet).buttons == Button::DpadUp);

    printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
