// Remote 1's samples and the test script's presses (docs/runtime/wii/remotes.md). No game data.
#include "wii/remote.h"
#include <bit>
#include <cstdio>
#include <string>
#include <vector>

namespace
{
    int checks = 0, failures = 0;
#define EXPECT(condition)                                                                                              \
    do                                                                                                                 \
    {                                                                                                                  \
        ++checks;                                                                                                      \
        if (!(condition))                                                                                              \
        {                                                                                                              \
            ++failures;                                                                                                \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition);                                               \
        }                                                                                                              \
    } while (0)

    using namespace wii::remote;

    uint32_t Get32(const uint8_t* at)
    {
        return uint32_t(at[0]) << 24 | uint32_t(at[1]) << 16 | uint32_t(at[2]) << 8 | at[3];
    }

    struct Sample
    {
        uint32_t hold, trigger, release;
        float accelerationY;
    };
    Sample Read(Remote& remote)
    {
        uint8_t status[StatusSize];
        for (uint8_t& byte : status)
            byte = 0xCD;
        remote.Sample(status);
        return { Get32(status), Get32(status + 4), Get32(status + 8), std::bit_cast<float>(Get32(status + 0x10)) };
    }

    void Script()
    {
        std::vector<Press> presses;
        std::string error;
        EXPECT(ParseScript("A@120+10,B2@300", presses, error));
        EXPECT(presses.size() == 2);
        EXPECT(presses[0].buttons == button::A && presses[0].first == 120 && presses[0].count == 10);
        EXPECT(presses[1].buttons == (button::B | button::Two) && presses[1].first == 300 && presses[1].count == 1);
        EXPECT(ParseScript("", presses, error) && presses.empty());
        for (const char* bad : { "A", "@5", "X@5", "A@", "A@5+", "A@5+0", "A@5x" })
        {
            error.clear();
            EXPECT(!ParseScript(bad, presses, error) && !error.empty());
        }
    }

    void Samples()
    {
        Remote remote;
        std::vector<Press> presses;
        std::string error;
        EXPECT(ParseScript("A@1+2", presses, error));
        remote.SetScript(presses);
        Sample s = Read(remote); // read 0: nothing held
        EXPECT(s.hold == 0 && s.trigger == 0 && s.release == 0);
        EXPECT(s.accelerationY == -1.0f); // at rest: gravity along -Y
        s = Read(remote); // read 1: A pressed
        EXPECT(s.hold == button::A && s.trigger == button::A && s.release == 0);
        s = Read(remote); // read 2: still held, no new press
        EXPECT(s.hold == button::A && s.trigger == 0 && s.release == 0);
        s = Read(remote); // read 3: released
        EXPECT(s.hold == 0 && s.trigger == 0 && s.release == button::A);
        EXPECT(remote.Reads() == 4);

        // The host's buttons add to the script's.
        remote.SetHostButtons(button::Two | button::Up);
        s = Read(remote);
        EXPECT(s.hold == (button::Two | button::Up) && s.trigger == (button::Two | button::Up));
        remote.SetHostButtons(button::Up);
        s = Read(remote);
        EXPECT(s.hold == button::Up && s.release == button::Two && s.trigger == 0);
    }
}

int main()
{
    Script();
    Samples();
    printf("remote test: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
