#include "wii/ax_mix.h"
#include "wii/memory_map.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace
{
    namespace ax = wii::ax;
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

    uint8_t* g_base = nullptr;
    constexpr uint32_t Data = 0x00200000;
    constexpr uint32_t Block = 0x80100000;
    constexpr double Pi = 3.14159265358979323846;

    void Put16(uint32_t at, uint16_t value)
    {
        g_base[at] = uint8_t(value >> 8);
        g_base[at + 1] = uint8_t(value);
    }
    uint16_t Get16(uint32_t at) { return uint16_t(uint16_t(g_base[at]) << 8 | g_base[at + 1]); }
    void Put32(uint32_t at, uint32_t value)
    {
        Put16(at, uint16_t(value >> 16));
        Put16(at + 2, uint16_t(value));
    }
    ax::Voice MakeVoice(ax::Format format, uint32_t count)
    {
        ax::Voice voice;
        voice.format = format;
        if (format == ax::Adpcm)
            voice.predictorScale = g_base[ax::Cached(Data)];
        voice.currentAddress = format == ax::Pcm16 ? Data / 2 : format == ax::Adpcm ? Data * 2 + 2 : Data;
        voice.endAddress = voice.currentAddress + count - 1;
        voice.loopAddress = voice.currentAddress;
        voice.converter = 2;
        voice.volume = 0x8000;
        voice.main[0].volume = 0x8000;
        voice.playing = true;
        return voice;
    }
    void FillPcm(uint32_t count, int16_t value)
    {
        for (uint32_t i = 0; i < count; ++i)
            Put16(ax::Cached(Data + 2 * i), uint16_t(value));
    }

    void ParameterBlocks()
    {
        memset(g_base + Block, 0xA5, ax::BlockSize);
        ax::Voice original = MakeVoice(ax::Adpcm, 1000);
        original.loops = original.stream = true;
        original.predictorScale = 0x75;
        original.history1 = -12345;
        original.history2 = 23456;
        original.loopPredictorScale = 0x32;
        original.loopHistory1 = -23456;
        original.loopHistory2 = 12345;
        original.converter = 1;
        original.ratio = 0x12345;
        original.fraction = 0xFEDC;
        original.volumeStep = -123;
        for (int i = 0; i < 16; ++i)
            original.coefficients[i] = int16_t(1234 * (i - 8));
        for (int i = 0; i < 4; ++i)
            original.lastInputs[i] = int16_t(5000 * (i - 2));
        ax::WriteVoice(g_base, Block & 0x1FFFFFFF, original);
        EXPECT(Get16(Block + 8) == 1 && Get16(Block + 16) == 1 && Get16(Block + 18) == 1);
        EXPECT(Get16(Block + 106) == 0x8000 && Get16(Block + 108) == uint16_t(-123));
        EXPECT(Get16(Block + 110) == 1 && Get16(Block + 112) == ax::Adpcm);
        EXPECT(Get16(Block + 114) == uint16_t(original.loopAddress >> 16));
        EXPECT(Get16(Block + 116) == uint16_t(original.loopAddress));
        EXPECT(Get16(Block + 118) == uint16_t(original.endAddress >> 16));
        EXPECT(Get16(Block + 120) == uint16_t(original.endAddress));
        EXPECT(Get16(Block + 122) == uint16_t(original.currentAddress >> 16));
        EXPECT(Get16(Block + 124) == uint16_t(original.currentAddress));
        for (uint32_t i = 0; i < 16; ++i)
            EXPECT(Get16(Block + 126 + 2 * i) == uint16_t(original.coefficients[i]));
        EXPECT(Get16(Block + 160) == 0x75 && int16_t(Get16(Block + 162)) == -12345);
        EXPECT(int16_t(Get16(Block + 164)) == 23456 && Get16(Block + 166) == 1);
        EXPECT(Get16(Block + 168) == 0x2345 && Get16(Block + 170) == 0xFEDC);
        EXPECT(Get16(Block + 180) == 0x32 && int16_t(Get16(Block + 182)) == -23456);
        EXPECT(int16_t(Get16(Block + 184)) == 12345);
        EXPECT(Get16(Block) == 0xA5A5 && Get16(Block + 12) == 0xA5A5 && Get16(Block + 14) == 0xA5A5);
        EXPECT(Get16(Block + 20) == 0x8000 && Get16(Block + 68) == 0xA5A5 && Get16(Block + 104) == 0xA5A5);
        EXPECT(Get16(Block + 158) == 0xA5A5 && Get16(Block + 186) == 0xA5A5);
        // Independent bytes keep a symmetric read/write mistake from hiding a wrong field offset.
        Put16(Block + 112, ax::Pcm8);
        Put16(Block + 162, uint16_t(-2222));
        const ax::Voice read = ax::ReadVoice(g_base, Block);
        EXPECT(read.format == ax::Pcm8 && read.loops && read.stream && read.playing);
        EXPECT(read.currentAddress == original.currentAddress && read.endAddress == original.endAddress);
        EXPECT(read.loopAddress == original.loopAddress && read.predictorScale == 0x75);
        EXPECT(read.history1 == -2222 && read.history2 == original.history2);
        EXPECT(read.loopPredictorScale == 0x32 && read.loopHistory1 == -23456 && read.loopHistory2 == 12345);
        EXPECT(read.converter == 1 && read.ratio == 0x12345 && read.fraction == 0xFEDC);
        EXPECT(read.volume == 0x8000 && read.volumeStep == -123);
        for (int i = 0; i < 16; ++i)
            EXPECT(read.coefficients[i] == original.coefficients[i]);
        for (int i = 0; i < 4; ++i)
        {
            EXPECT(read.lastInputs[i] == original.lastInputs[i]);
            EXPECT(Get16(Block + 172 + uint32_t(2 * i)) == uint16_t(original.lastInputs[i]));
        }
        EXPECT(read.main[0].volume == 0x8000 && read.aux[2][2].volume == 0);
        EXPECT(!ax::ReadVoice(g_base, wii::Mem1Size).playing);
        ax::WriteVoice(g_base, wii::Mem1Size, original);
    }

    void MixParameterBlocks()
    {
        constexpr uint32_t Ramps[] = { 0x4, 0x4, 0x40000, 0x40000, 0x800000, 0x800000, 0x10000000, 0x10000000, 0x10,
            0x100000, 0x2000000, 0x40000000 };
        FillPcm(1000, 16384);
        for (uint32_t control : { 0u, 0x4u, 0x10u, 0x40000u, 0x100000u, 0x800000u, 0x2000000u, 0x10000000u, 0x40000000u,
                 0x2D6B000Bu, 0x7FFFFFFFu })
        {
            memset(g_base + Block, 0xA5, ax::BlockSize);
            ax::WriteVoice(g_base, Block, MakeVoice(ax::Pcm16, 1000));
            Put32(Block + 12, control);
            for (uint32_t pair = 0; pair < 12; ++pair)
            {
                Put16(Block + 20 + 4 * pair, uint16_t(1000 * (pair + 1)));
                Put16(Block + 22 + 4 * pair, uint16_t(int16_t(pair % 2 ? -200 : 1000)));
            }
            ax::Voice voice = ax::ReadVoice(g_base, Block);
            const std::array<ax::Gain, 12> gains = { voice.main[0], voice.main[1], voice.aux[0][0], voice.aux[0][1],
                voice.aux[1][0], voice.aux[1][1], voice.aux[2][0], voice.aux[2][1], voice.main[2], voice.aux[0][2],
                voice.aux[1][2], voice.aux[2][2] };
            for (uint32_t pair = 0; pair < 12; ++pair)
            {
                EXPECT(gains[pair].volume == 1000 * (pair + 1));
                EXPECT(gains[pair].step == ((control & Ramps[pair]) ? (pair % 2 ? -200 : 1000) : 0));
            }
            ax::Bus bus;
            ax::RenderVoice(g_base, voice, bus);
            ax::WriteVoice(g_base, Block, voice);
            for (uint32_t pair = 0; pair < 12; ++pair)
            {
                const int expected = std::clamp(int(gains[pair].volume) + 96 * gains[pair].step, 0, 65535);
                EXPECT(Get16(Block + 20 + 4 * pair) == expected);
                EXPECT(int16_t(Get16(Block + 22 + 4 * pair)) == (pair % 2 ? -200 : 1000));
            }
            EXPECT(Get16(Block + 12) == uint16_t(control >> 16) && Get16(Block + 14) == uint16_t(control));
            EXPECT(Get16(Block + 68) == 0xA5A5);
        }
    }

    void CommandListOutput()
    {
        constexpr uint32_t List = 0x80110000, Output = 0x80120000, Second = Block + 0x200;
        const auto prepare = [&](uint16_t command, uint16_t volume, uint32_t output = Output) {
            memset(g_base + Block, 0, ax::BlockSize);
            ax::Voice voice = MakeVoice(ax::Pcm16, 96);
            ax::WriteVoice(g_base, Block, voice);
            // Independent PB bytes exercise the command parser and mixer together, with channel hint bits clear.
            Put16(Block + 20, 0x4000);
            Put16(Block + 24, 0x2000);
            for (uint32_t at : { 28u, 32u, 36u, 40u, 44u, 48u, 56u, 60u, 64u })
                Put16(Block + at, 0x8000);
            Put16(Block + 52, 0x1000);
            Put16(List, 4);
            Put32(List + 2, Block & 0x1FFFFFFF);
            Put16(List + 6, command);
            Put16(List + 8, volume);
            Put32(List + 10, 0);
            Put32(List + 14, output & 0x1FFFFFFF);
            Put16(List + 18, 14);
            memset(g_base + Output - 1, 0xA5, 386);
        };
        for (uint16_t command : { 11, 12 })
        {
            for (uint16_t volume : { 0, 0x4000, 0x8000, 0xFFFF })
            {
                FillPcm(96, 16384);
                prepare(command, volume);
                const ax::Frame frame = ax::RunCommandList(g_base, List & 0x1FFFFFFF);
                EXPECT(frame.complete && frame.voices == 1 && frame.playing == 1 && frame.output == Output);
                for (uint32_t i = 0; i < 96; ++i)
                {
                    // Surround: (16384 * 0x1000 / 32768) * 23170 / 32768 = 1448.
                    EXPECT(Get16(Output + 4 * i) == (((8192 + 1448) * uint32_t(volume)) >> 15));
                    EXPECT(Get16(Output + 4 * i + 2) == (((4096 + 1448) * uint32_t(volume)) >> 15));
                }
                EXPECT(g_base[Output - 1] == 0xA5 && g_base[Output + 384] == 0xA5);
                EXPECT(Get16(Block + 16) == 0);
            }
        }

        FillPcm(96, 16384);
        prepare(11, 0x8000);
        Put32(Block + 12, 0x4);
        Put16(Block + 22, 64);
        Put16(Block + 26, uint16_t(-32));
        Put16(Block + 54, 1000); // surround's ramp bit is clear
        ax::RunCommandList(g_base, List);
        for (uint32_t i = 0; i < 96; ++i)
        {
            EXPECT(Get16(Output + 4 * i) == 8192 + 32 * i + 1448);
            EXPECT(Get16(Output + 4 * i + 2) == 4096 - 16 * i + 1448);
        }
        EXPECT(Get16(Block + 20) == 0x4000 + 96 * 64 && Get16(Block + 24) == 0x2000 - 96 * 32);
        EXPECT(Get16(Block + 22) == 64 && Get16(Block + 26) == uint16_t(-32));
        EXPECT(Get16(Block + 52) == 0x1000 && Get16(Block + 54) == 1000);

        prepare(12, 0x8000);
        for (uint32_t pair = 0; pair < 12; ++pair)
            Put16(Block + 20 + 4 * pair, 0);
        ax::RunCommandList(g_base, List);
        for (uint32_t i = 0; i < 384; ++i)
            EXPECT(g_base[Output + i] == 0);

        prepare(11, 0x8000);
        memset(g_base + Second, 0, ax::BlockSize);
        ax::Voice muted = MakeVoice(ax::Pcm16, 96);
        muted.main[0].volume = 0;
        ax::WriteVoice(g_base, Second, muted);
        Put32(Block, Second & 0x1FFFFFFF);
        const ax::Frame withMuted = ax::RunCommandList(g_base, List);
        EXPECT(withMuted.complete && withMuted.voices == 2 && withMuted.playing == 2);
        for (uint32_t i = 0; i < 96; ++i)
        {
            EXPECT(Get16(Output + 4 * i) == 9640);
            EXPECT(Get16(Output + 4 * i + 2) == 5544);
        }

        for (uint16_t volume : { 0x4000, 0x8000 })
        {
            prepare(11, volume);
            for (uint32_t i = 0; i < 96; ++i)
                Put16(ax::Cached(Data + 2 * i), uint16_t(i % 2 ? -24000 : 24000));
            for (uint32_t block : { Block, Second })
            {
                memset(g_base + block, 0, ax::BlockSize);
                ax::Voice voice = MakeVoice(ax::Pcm16, 96);
                voice.main[1].volume = 0x8000;
                ax::WriteVoice(g_base, block, voice);
            }
            Put32(Block, Second & 0x1FFFFFFF);
            const ax::Frame frame = ax::RunCommandList(g_base, List);
            EXPECT(frame.complete && frame.voices == 2 && frame.playing == 2);
            for (uint32_t i = 0; i < 96; ++i)
            {
                const int expected = volume == 0x4000 ? (i % 2 ? -24000 : 24000) : (i % 2 ? -32768 : 32767);
                EXPECT(int16_t(Get16(Output + 4 * i)) == expected);
                EXPECT(int16_t(Get16(Output + 4 * i + 2)) == expected);
            }
        }

        // A stopped voice leaves the next frame silent and its PB untouched, even with enabled ramps.
        Put32(Block + 12, 0x4);
        Put16(Block + 22, 64);
        std::array<uint8_t, ax::BlockSize> stopped{};
        memcpy(stopped.data(), g_base + Block, stopped.size());
        const ax::Frame silent = ax::RunCommandList(g_base, List);
        EXPECT(silent.complete && silent.playing == 0);
        EXPECT(memcmp(stopped.data(), g_base + Block, stopped.size()) == 0);
        for (uint32_t i = 0; i < 384; ++i)
            EXPECT(g_base[Output + i] == 0);

        for (uint32_t output : { wii::Mem1Cached + wii::Mem1Size - 382, wii::Mem1Cached + wii::Mem1Size,
                 wii::Mem2Cached + wii::Mem2Size, 0x9FFFFFFEu })
        {
            prepare(11, 0x8000, output);
            const uint32_t edge = wii::Mem1Cached + wii::Mem1Size - 384;
            memset(g_base + edge, 0x5A, 384);
            const ax::Frame frame = ax::RunCommandList(g_base, List);
            EXPECT(frame.complete && frame.output == output);
            for (uint32_t i = 0; i < 384; ++i)
            {
                EXPECT(g_base[Output + i] == 0xA5);
                EXPECT(g_base[edge + i] == 0x5A);
            }
        }
        FillPcm(96, 16384);
        prepare(12, 0x8000, 0x90010000);
        const ax::Frame mem2 = ax::RunCommandList(g_base, List);
        EXPECT(mem2.complete && mem2.output == 0x90010000);
        EXPECT(Get16(mem2.output) == 9640 && Get16(mem2.output + 382) == 5544);
    }

    void PcmAndStops()
    {
        const int16_t values[] = { -32768, -12345, -1, 0, 1, 12345, 32767 };
        for (uint32_t i = 0; i < 96; ++i)
            Put16(ax::Cached(Data + 2 * i), uint16_t(values[i % 7]));
        ax::Voice voice = MakeVoice(ax::Pcm16, 96);
        ax::Bus bus;
        ax::RenderVoice(g_base, voice, bus);
        for (uint32_t i = 0; i < 96; ++i)
            EXPECT(bus.main[0][i] == values[i % 7] && bus.main[1][i] == 0 && bus.aux[2][2][i] == 0);
        EXPECT(!voice.playing && voice.currentAddress == Data / 2 + 95);
        const ax::Bus before = bus;
        const uint16_t volume = voice.volume;
        ax::RenderVoice(g_base, voice, bus);
        EXPECT(memcmp(&before, &bus, sizeof(bus)) == 0 && voice.volume == volume);
        for (uint32_t i = 0; i < 96; ++i)
            g_base[ax::Cached(Data + i)] = uint8_t(i * 3);
        voice = MakeVoice(ax::Pcm8, 5);
        bus = {};
        ax::RenderVoice(g_base, voice, bus);
        for (uint32_t i = 0; i < 96; ++i)
            EXPECT(bus.main[0][i] == (i < 5 ? int8_t(uint8_t(i * 3)) * 256 : 0));
        EXPECT(!voice.playing && voice.currentAddress == Data + 4);
        voice = MakeVoice(ax::Pcm8, 96);
        bus = {};
        ax::RenderVoice(g_base, voice, bus);
        EXPECT(bus.main[0][43] == -127 * 256 && bus.main[0][85] == -256);
        FillPcm(10, 1234);
        voice = MakeVoice(ax::Pcm16, 10);
        voice.loops = true;
        voice.loopAddress += 4;
        bus = {};
        ax::RenderVoice(g_base, voice, bus);
        EXPECT(voice.playing && voice.currentAddress == Data / 2 + 6 && voice.looped);
        for (int32_t sample : bus.main[0])
            EXPECT(sample == 1234);
    }

    void AdpcmKnownAndLoops()
    {
        // Two header nibbles and fourteen signed sample nibbles fill eight bytes; no additional byte fits.
        const uint8_t frame[] = { 0x01, 0x01, 0x78, 0xF2, 0xE3, 0xD4, 0xC5, 0xB6 };
        const int16_t expected[] = { 0, 2, 14, -16, -2, 4, -4, 6, -6, 8, -8, 10, -10, 12 };
        memcpy(g_base + ax::Cached(Data), frame, sizeof(frame));
        ax::Voice voice = MakeVoice(ax::Adpcm, 14);
        ax::Bus bus;
        ax::RenderVoice(g_base, voice, bus);
        for (uint32_t i = 0; i < 96; ++i)
            EXPECT(bus.main[0][i] == (i < 14 ? expected[i] : 0));
        EXPECT(!voice.playing && voice.history1 == 12 && voice.history2 == -10);
        voice = MakeVoice(ax::Adpcm, 3);
        bus = {};
        ax::RenderVoice(g_base, voice, bus);
        EXPECT(bus.main[0][2] == 14 && bus.main[0][3] == 0 && !voice.playing);
        voice = MakeVoice(ax::Adpcm, 14);
        voice.loops = true;
        voice.loopPredictorScale = 0x10;
        voice.loopHistory1 = 1000;
        voice.loopHistory2 = -500;
        voice.coefficients[2] = 2048;
        bus = {};
        ax::RenderVoice(g_base, voice, bus);
        EXPECT(bus.main[0][14] == 1000 && bus.main[0][15] == 1001);
        EXPECT(bus.main[0][28] == 1000 && voice.playing && voice.looped);
        EXPECT(voice.history1 != 1000 && voice.predictorScale == 0x10);
        voice = MakeVoice(ax::Adpcm, 14);
        voice.loops = voice.stream = true;
        voice.loopPredictorScale = 0x70;
        voice.loopHistory1 = 2345;
        voice.coefficients[0] = 2048;
        bus = {};
        ax::RenderVoice(g_base, voice, bus);
        EXPECT(bus.main[0][14] == bus.main[0][13]);
        EXPECT(bus.main[0][15] == bus.main[0][14] + 2 && voice.predictorScale == 1);
        // A stream looping to the header takes the new frame's scale but retains its predictor history.
        voice = MakeVoice(ax::Adpcm, 14);
        voice.loops = voice.stream = true;
        voice.loopAddress -= 2;
        voice.coefficients[0] = 2048;
        bus = {};
        ax::RenderVoice(g_base, voice, bus);
        EXPECT(bus.main[0][14] == bus.main[0][13] && bus.main[0][15] == bus.main[0][14] + 2);
        EXPECT(voice.predictorScale == 1);
        // The second coefficient, rounding, and both clipping limits have independent small cases.
        memset(g_base + ax::Cached(Data), 0, 16);
        voice = MakeVoice(ax::Adpcm, 1);
        voice.coefficients[0] = 1024;
        voice.coefficients[1] = -512;
        voice.history1 = 9;
        voice.history2 = -5;
        bus = {};
        ax::RenderVoice(g_base, voice, bus);
        EXPECT(bus.main[0][0] == 6 && voice.history2 == 9);
        for (int sign : { -1, 1 })
        {
            voice = MakeVoice(ax::Adpcm, 1);
            voice.coefficients[0] = 32767;
            voice.history1 = int16_t(sign * 30000);
            bus = {};
            ax::RenderVoice(g_base, voice, bus);
            EXPECT(bus.main[0][0] == (sign < 0 ? -32768 : 32767));
        }
        // Starting on a header and crossing another one must skip exactly two nibbles each time.
        g_base[ax::Cached(Data)] = 1;
        g_base[ax::Cached(Data + 8)] = 2;
        g_base[ax::Cached(Data + 9)] = 0x70;
        voice = MakeVoice(ax::Adpcm, 100);
        voice.currentAddress -= 2;
        voice.endAddress = voice.currentAddress + 18;
        bus = {};
        ax::RenderVoice(g_base, voice, bus);
        EXPECT(bus.main[0][14] == 28 && bus.main[0][15] == 0 && voice.predictorScale == 2 && !voice.playing);
    }

    constexpr std::array<int16_t, 16> Coefficients = { 0, 0, 2048, 0, 4096, -2048, 3072, -1024, 3584, -1536, 1536, 512,
        1024, 1024, 0, 2048 };

    void Encode(const std::vector<int16_t>& input)
    {
        int h1 = 0, h2 = 0;
        for (size_t start = 0; start < input.size(); start += 14)
        {
            uint64_t bestError = std::numeric_limits<uint64_t>::max();
            std::array<uint8_t, 8> best{};
            int bestH1 = 0, bestH2 = 0;
            for (int predictor = 0; predictor < 8; ++predictor)
            {
                for (int scale = 0; scale < 16; ++scale)
                {
                    std::array<uint8_t, 8> frame{};
                    frame[0] = uint8_t(predictor * 16 + scale);
                    int a = h1, b = h2;
                    uint64_t error = 0;
                    for (size_t i = 0; i < 14; ++i)
                    {
                        int bestNibble = 0, bestSample = 0;
                        int64_t smallest = std::numeric_limits<int64_t>::max();
                        for (int nibble = -8; nibble < 8; ++nibble)
                        {
                            const int64_t prediction =
                                int64_t(Coefficients[2 * predictor]) * a + int64_t(Coefficients[2 * predictor + 1]) * b;
                            const int decoded = int(std::clamp<int64_t>(
                                (prediction + int64_t(nibble) * (1 << scale) * 2048 + 1024) >> 11, -32768, 32767));
                            const int64_t difference = decoded - int(input[start + i]);
                            if (difference * difference < smallest)
                            {
                                smallest = difference * difference;
                                bestNibble = nibble;
                                bestSample = decoded;
                            }
                        }
                        error += uint64_t(smallest);
                        frame[1 + i / 2] |= uint8_t((bestNibble & 15) << ((i & 1) ? 0 : 4));
                        b = a;
                        a = bestSample;
                    }
                    if (error < bestError)
                    {
                        bestError = error;
                        best = frame;
                        bestH1 = a;
                        bestH2 = b;
                    }
                }
            }
            memcpy(g_base + ax::Cached(Data + uint32_t(start / 14 * 8)), best.data(), best.size());
            h1 = bestH1;
            h2 = bestH2;
        }
    }

    void AdpcmSine()
    {
        constexpr uint32_t Count = 1344;
        std::vector<int16_t> input(Count);
        for (uint32_t i = 0; i < Count; ++i)
            input[i] = int16_t(std::lround(16000 * std::sin(2 * Pi * i / 64)));
        Encode(input);
        ax::Voice voice = MakeVoice(ax::Adpcm, Count);
        std::copy(Coefficients.begin(), Coefficients.end(), voice.coefficients);
        voice.endAddress = Data * 2 + (Count / 14) * 16 - 1;
        double squaredError = 0;
        int maxError = 0;
        for (uint32_t start = 0; start < Count; start += 96)
        {
            ax::Bus bus;
            ax::RenderVoice(g_base, voice, bus);
            for (uint32_t i = 0; i < 96; ++i)
            {
                const int error = bus.main[0][i] - input[start + i];
                squaredError += double(error) * error;
                maxError = std::max(maxError, std::abs(error));
            }
        }
        const double rms = std::sqrt(squaredError / Count);
        printf("ADPCM sine: RMS %.2f, maximum error %d\n", rms, maxError);
        EXPECT(rms < 100 && maxError < 400);
        EXPECT(!voice.playing && voice.currentAddress == voice.endAddress);
        for (uint16_t converter = 0; converter < 3; ++converter)
        {
            for (uint32_t ratio : { 0x8000u, 0x10000u, 0x20000u, uint32_t(std::lround(1.37 * 65536)) })
            {
                voice = MakeVoice(ax::Adpcm, Count);
                std::copy(Coefficients.begin(), Coefficients.end(), voice.coefficients);
                voice.endAddress = Data * 2 + (Count / 14) * 16 - 1;
                voice.loops = true;
                voice.loopPredictorScale = g_base[ax::Cached(Data)];
                voice.predictorScale = voice.loopPredictorScale;
                voice.converter = converter;
                voice.ratio = ratio;
                const uint32_t effectiveRatio = converter == 2 ? 0x10000 : ratio;
                uint64_t position = 0;
                double errorSum = 0;
                for (uint32_t frame = 0; frame < 28; ++frame)
                {
                    ax::Bus bus;
                    ax::RenderVoice(g_base, voice, bus);
                    position += uint64_t(96) * effectiveRatio;
                    const uint32_t consumed = uint32_t((position >> 16) % Count);
                    const uint32_t offset =
                        consumed == 0 ? 2 : (consumed / 14) * 16 + (consumed % 14 == 0 ? 0 : 2 + consumed % 14);
                    EXPECT(voice.currentAddress == Data * 2 + offset && voice.fraction == uint16_t(position));
                    EXPECT(voice.playing);
                    for (uint32_t i = 0; i < 96; ++i)
                    {
                        const uint32_t at = frame * 96 + i;
                        if (at < 32)
                            continue;
                        const double source = converter == 2
                                                  ? double(at)
                                                  : double(uint64_t(at + 1) * ratio) / 65536 - (converter == 1 ? 2 : 3);
                        const double difference = bus.main[0][i] - 16000 * std::sin(2 * Pi * source / 64);
                        errorSum += difference * difference;
                    }
                }
                EXPECT(std::sqrt(errorSum / (28 * 96 - 32)) < 120);
            }
        }
    }

    void Converters()
    {
        constexpr uint32_t Frames = 48;
        for (uint32_t i = 0; i < 12000; ++i)
            Put16(ax::Cached(Data + 2 * i), uint16_t(int16_t(std::lround(12000 * std::sin(2 * Pi * i / 64)))));
        for (uint16_t converter = 0; converter < 3; ++converter)
        {
            for (uint32_t ratio : { 0x8000u, 0x10000u, 0x20000u, uint32_t(std::lround(1.37 * 65536)) })
            {
                ax::Voice voice = MakeVoice(ax::Pcm16, 12000);
                voice.converter = converter;
                voice.ratio = ratio;
                uint64_t position = 0;
                const uint32_t effectiveRatio = converter == 2 ? 0x10000 : ratio;
                double squaredError = 0;
                int crossings = 0, largestJump = 0, previous = 0;
                for (uint32_t frame = 0; frame < Frames; ++frame)
                {
                    ax::Bus bus;
                    ax::RenderVoice(g_base, voice, bus);
                    position += uint64_t(96) * effectiveRatio;
                    EXPECT(voice.currentAddress == Data / 2 + (position >> 16));
                    EXPECT(voice.fraction == uint16_t(position) && voice.playing);
                    for (uint32_t i = 0; i < 96; ++i)
                    {
                        const uint32_t at = frame * 96 + i;
                        const int sample = bus.main[0][i];
                        if (at >= 32)
                        {
                            const double source =
                                converter == 2 ? double(at)
                                               : double(uint64_t(at + 1) * ratio) / 65536 - (converter == 1 ? 2 : 3);
                            const double expected = 12000 * std::sin(2 * Pi * source / 64);
                            const double error = sample - expected;
                            squaredError += error * error;
                            largestJump = std::max(largestJump, std::abs(sample - previous));
                            if (previous <= 0 && sample > 0)
                                ++crossings;
                        }
                        previous = sample;
                    }
                    // Reconstructing from a PB between frames must retain the converter's full input history.
                    ax::WriteVoice(g_base, Block, voice);
                    voice = ax::ReadVoice(g_base, Block);
                }
                const double expectedCrossings = double(Frames * 96 - 32) * effectiveRatio / (65536 * 64);
                EXPECT(std::abs(crossings - expectedCrossings) < 1.5);
                EXPECT(std::sqrt(squaredError / (Frames * 96 - 32)) < 35);
                EXPECT(largestJump < 2600);
            }
        }
        // A fraction just below its wrap and the maximum ratio both need a carry wider than 32 bits.
        for (uint16_t converter = 0; converter < 3; ++converter)
        {
            ax::Voice voice = MakeVoice(ax::Pcm16, 12000);
            voice.converter = converter;
            voice.ratio = 0x8000;
            voice.fraction = 0xFFFF;
            ax::Bus bus;
            ax::RenderVoice(g_base, voice, bus);
            EXPECT(voice.currentAddress == Data / 2 + (converter == 2 ? 96 : 48));
            EXPECT(voice.fraction == 0xFFFF);
        }
        ax::Voice voice = MakeVoice(ax::Pcm16, 1);
        voice.converter = 1;
        voice.ratio = 0xFFFFFFFF;
        voice.fraction = 0xFFFF;
        ax::Bus bus;
        ax::RenderVoice(g_base, voice, bus);
        EXPECT(!voice.playing && voice.fraction == 0xFFFE);
        voice = MakeVoice(ax::Pcm16, 1000);
        voice.converter = 0;
        voice.ratio = 0;
        voice.lastInputs[1] = 1234;
        bus = {};
        ax::RenderVoice(g_base, voice, bus);
        EXPECT(voice.currentAddress == Data / 2 && bus.main[0][95] == 1234);
    }

    void RampsAndBuses()
    {
        FillPcm(1000, 16384);
        ax::Voice voice = MakeVoice(ax::Pcm16, 1000);
        voice.volume = 1000;
        voice.volumeStep = 100;
        for (int channel = 0; channel < 3; ++channel)
        {
            voice.main[channel] = { uint16_t(1000 + 100 * channel), -20 };
            for (int aux = 0; aux < 3; ++aux)
                voice.aux[aux][channel] = { uint16_t(0x8000 + 100 * aux + channel), 500 };
        }
        ax::Bus bus;
        bus.main[0][0] = 17;
        for (auto& channels : bus.aux)
            for (auto& samples : channels)
                std::fill(std::begin(samples), std::end(samples), -13);
        ax::RenderVoice(g_base, voice, bus);
        for (int i = 0; i < 96; ++i)
        {
            const int envelope = (16384 * (1000 + i * 100)) >> 15;
            for (int channel = 0; channel < 3; ++channel)
            {
                const int mainGain = std::max(1000 + 100 * channel - i * 20, 0);
                EXPECT(bus.main[channel][i] == ((envelope * mainGain) >> 15) + (channel == 0 && i == 0 ? 17 : 0));
                for (int aux = 0; aux < 3; ++aux)
                {
                    const int gain = std::min(0x8000 + 100 * aux + channel + i * 500, 0xFFFF);
                    EXPECT(bus.aux[aux][channel][i] == int((int64_t(envelope) * gain) >> 15) - 13);
                }
            }
        }
        EXPECT(voice.volume == 10600 && voice.main[0].volume == 0 && voice.aux[2][2].volume == 0xFFFF);
        for (int sign : { -1, 1 })
        {
            voice = MakeVoice(ax::Pcm16, 1);
            voice.volume = sign < 0 ? 100 : 65000;
            voice.volumeStep = int16_t(sign * 1000);
            bus = {};
            ax::RenderVoice(g_base, voice, bus);
            EXPECT(voice.volume == (sign < 0 ? 0 : 0xFFFF));
            EXPECT(bus.main[0][1] == 0 && !voice.playing);
        }
        voice = MakeVoice(ax::Pcm16, 1000);
        bus = {};
        ax::RenderVoice(g_base, voice, bus);
        ax::RenderVoice(g_base, voice, bus);
        EXPECT(bus.main[0][0] == 32768 && bus.main[0][95] == 32768);
    }

    void Bounds()
    {
        for (ax::Format format : { ax::Adpcm, ax::Pcm16, ax::Pcm8 })
        {
            for (uint32_t byte : { wii::Mem1Size, 0x14000000u, 0x1FFFFFFEu })
            {
                ax::Voice voice = MakeVoice(format, 100);
                voice.currentAddress = format == ax::Adpcm ? byte * 2 + 2 : format == ax::Pcm16 ? byte / 2 : byte;
                voice.endAddress = voice.currentAddress + 100;
                ax::Bus bus;
                ax::RenderVoice(g_base, voice, bus);
                EXPECT(!voice.playing);
                EXPECT(std::all_of(std::begin(bus.main[0]), std::end(bus.main[0]), [](int32_t v) { return v == 0; }));
            }
            // Last valid sample, then an unmapped address: neither a speculative read nor stale output is allowed.
            const uint32_t end = wii::Mem1Cached + wii::Mem1Size;
            g_base[end - 1] = 0x11;
            Put16(end - 2, 0x1111);
            ax::Voice voice = MakeVoice(format, 100);
            voice.currentAddress = format == ax::Adpcm   ? (wii::Mem1Size * 2 - 1)
                                   : format == ax::Pcm16 ? (wii::Mem1Size / 2 - 1)
                                                         : (wii::Mem1Size - 1);
            voice.endAddress = voice.currentAddress + 100;
            ax::Bus bus;
            ax::RenderVoice(g_base, voice, bus);
            EXPECT(!voice.playing && bus.main[0][0] != 0 && bus.main[0][1] == 0);
        }
        ax::Voice voice = MakeVoice(ax::Pcm16, 96);
        voice.currentAddress = 0x10000100 / 2;
        voice.endAddress = voice.currentAddress + 95;
        for (uint32_t i = 0; i < 96; ++i)
            Put16(0x90000100 + 2 * i, uint16_t(-1234));
        ax::Bus bus;
        ax::RenderVoice(g_base, voice, bus);
        EXPECT(bus.main[0][0] == -1234 && bus.main[0][95] == -1234 && !voice.playing);
        voice = MakeVoice(ax::Adpcm, 100);
        voice.predictorScale = 0xF0;
        bus = {};
        ax::RenderVoice(g_base, voice, bus);
        EXPECT(!voice.playing && bus.main[0][0] == 0);
    }

    void FinalOutput()
    {
        ax::Bus bus;
        bus.main[0][0] = 0x1234;
        bus.main[1][0] = -2;
        bus.main[0][1] = 40000;
        bus.main[1][1] = -40000;
        bus.main[2][2] = 32768;
        bus.main[2][3] = -32768;
        bus.main[0][4] = std::numeric_limits<int32_t>::max();
        bus.main[2][4] = std::numeric_limits<int32_t>::max();
        std::fill(std::begin(bus.aux[0][0]), std::end(bus.aux[0][0]), 100000);
        std::array<uint8_t, 386> output{};
        output.front() = output.back() = 0xAA;
        ax::FinalMix(bus, output.data() + 1);
        EXPECT(output.front() == 0xAA && output.back() == 0xAA);
        EXPECT(output[1] == 0x12 && output[2] == 0x34 && output[3] == 0xFF && output[4] == 0xFE);
        EXPECT(output[5] == 0x7F && output[6] == 0xFF && output[7] == 0x80 && output[8] == 0);
        EXPECT(output[9] == uint8_t(23170 >> 8) && output[10] == uint8_t(23170));
        EXPECT(output[11] == output[9] && output[12] == output[10]);
        EXPECT(output[13] == uint8_t(uint16_t(-23170) >> 8) && output[14] == uint8_t(-23170));
        EXPECT(output[15] == output[13] && output[16] == output[14]);
        EXPECT(output[17] == 0x7F && output[18] == 0xFF);
        EXPECT(output[381] == 0 && output[384] == 0);
    }

    void Speed()
    {
        memset(g_base + ax::Cached(Data), 0x11, 1024);
        for (uint32_t frame = 0; frame < 100; ++frame)
            g_base[ax::Cached(Data + 8 * frame)] = 0x18;
        std::array<ax::Voice, 96> voices;
        for (auto& voice : voices)
        {
            voice = MakeVoice(ax::Adpcm, 100);
            voice.endAddress = Data * 2 + 16 * 100 - 1;
            voice.loops = true;
            voice.converter = 0;
            voice.ratio = 0x15EB8;
            voice.coefficients[0] = 1024;
            voice.coefficients[2] = 1024;
            voice.loopPredictorScale = 0x18;
            for (auto& gain : voice.main)
                gain.volume = 0x1000;
            for (auto& channels : voice.aux)
                for (auto& gain : channels)
                    gain.volume = 0x1000;
        }
        const auto start = std::chrono::steady_clock::now();
        ax::Bus bus;
        std::array<uint8_t, 384> output{};
        for (int frame = 0; frame < 1000; ++frame)
        {
            bus = {};
            for (auto& voice : voices)
                ax::RenderVoice(g_base, voice, bus);
            ax::FinalMix(bus, output.data());
        }
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        printf("96 ADPCM voices, 1000 frames: %.3f s (output %u)\n", seconds, unsigned(output[0]));
#ifdef NDEBUG
        EXPECT(seconds < 1.0);
#endif
        EXPECT(voices[0].playing && voices[95].playing && bus.main[0][0] != 0);
    }
}

int main()
{
    std::string error;
    EXPECT(wii::MapAddressSpace(g_base, {}, error));
    if (g_base == nullptr || !error.empty())
    {
        fprintf(stderr, "address space: %s\n", error.c_str());
        printf("%d checks, %d failures\n", checks, failures);
        return 1;
    }
    ParameterBlocks();
    MixParameterBlocks();
    CommandListOutput();
    PcmAndStops();
    AdpcmKnownAndLoops();
    AdpcmSine();
    Converters();
    RampsAndBuses();
    Bounds();
    FinalOutput();
    Speed();
    printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
