// The host's sound queue: byte order, the bound on what is queued (docs/runtime/wii/audio.md). SDL's dummy audio
// driver, no device needed. No game data.
#include "wii/audio_out.h"
#include <SDL3/SDL.h>
#include <cstdio>
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

    using namespace wii::audio_out;

    void ByteOrder()
    {
        const uint8_t bigEndian[8] = { 0x12, 0x34, 0xFF, 0xFE, 0x80, 0x00, 0x7F, 0xFF };
        int16_t out[4] = {};
        ToHost(bigEndian, sizeof(bigEndian), out);
        EXPECT(out[0] == 0x1234 && out[1] == -2 && out[2] == -32768 && out[3] == 32767);
    }

    void Queue()
    {
        std::vector<uint8_t> buffer(384, 0); // one AX frame: 96 stereo samples
        Play(buffer.data(), buffer.size());  // before Start: ignored
        EXPECT(Queued() == 0);
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
        EXPECT(Start());
        EXPECT(Start()); // a second Start keeps the device
        // Far more than the device plays meanwhile: the queue stops at its bound, the rest dropped.
        constexpr uint32_t Bound = SampleRate * 4 / 1000 * MostQueuedMs;
        for (int i = 0; i < 1000; i++)
            Play(buffer.data(), buffer.size());
        EXPECT(Dropped() > 0);
        EXPECT(Queued() <= Bound + 384);
        EXPECT(Queued() >= Bound / 2);
        // A buffer larger than the converter's piece goes in whole.
        Stop();
        EXPECT(Start());
        std::vector<uint8_t> large(10000, 0);
        Play(large.data(), large.size());
        EXPECT(Queued() + 4 * 96 >= large.size() / 2); // the device may have taken some already
        Stop();
        EXPECT(Queued() == 0);
    }
}

int main()
{
    ByteOrder();
    Queue();
    SDL_Quit();
    printf("audio out test: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
