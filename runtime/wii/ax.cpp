#include "wii/ax.h"
#include "wii/memory_map.h"
#include <algorithm>

namespace wii::ax
{
    namespace
    {
        uint16_t Get16(const uint8_t* base, uint32_t address)
        {
            return uint16_t(base[address] << 8 | base[address + 1]);
        }
        uint32_t Get32(const uint8_t* base, uint32_t address)
        {
            return uint32_t(Get16(base, address)) << 16 | Get16(base, address + 2);
        }
        void Put16(uint8_t* base, uint32_t address, uint16_t value)
        {
            base[address] = uint8_t(value >> 8);
            base[address + 1] = uint8_t(value);
        }
        void Put32(uint8_t* base, uint32_t address, uint32_t value)
        {
            Put16(base, address, uint16_t(value >> 16));
            Put16(base, address + 2, uint16_t(value));
        }

        // Each command's arguments, in 16-bit words (an address takes two). From the SDK's list builder: 0 the
        // studio, 1-3 the output mode's buffer, 4 the voices, 5-7 the aux buses A-C (volume, the buffer the DSP
        // fills, the one the CPU returns), 8-9 A and B in Dolby Pro Logic II (volume and six buffers), 10 the
        // compressor, 11-12 the final mix (volume, the mode's buffer, the output buffer), 13 the four remotes'
        // speakers, 14 the end.
        constexpr int ArgumentWords[] = { 2, 2, 2, 2, 2, 5, 5, 5, 13, 13, 4, 5, 5, 8, 0 };
        constexpr uint16_t End = 14;
        constexpr uint32_t MostVoices = 512; // a list longer than any AX makes: a loop in the links

        // The address of one sample, and the byte holding it.
        uint32_t SampleByte(uint16_t format, uint32_t address)
        {
            return Cached(format == Adpcm ? address >> 1 : format == Pcm16 ? address << 1 : address);
        }
    } // namespace

    void AdvanceVoice(uint8_t* base, uint32_t block)
    {
        if (Get16(base, block + pb::State) != 1)
            return;
        const uint16_t format = Get16(base, block + pb::Format);
        const bool loops = Get16(base, block + pb::Loops) != 0;
        const bool stream = Get16(base, block + pb::Type) == 1;
        const uint32_t loop = Get32(base, block + pb::LoopAddress), end = Get32(base, block + pb::EndAddress);
        uint32_t current = Get32(base, block + pb::CurrentAddress);
        const uint32_t ratio = Get16(base, block + pb::Source) == 2 ? 0x10000u : Get32(base, block + pb::Ratio);
        uint32_t fraction = Get16(base, block + pb::Fraction);
        uint16_t predictorScale = Get16(base, block + pb::PredictorScale);
        uint16_t history1 = Get16(base, block + pb::History1), history2 = Get16(base, block + pb::History2);
        bool playing = true;

        // One input sample consumed: an ADPCM frame's header first (2 nibbles), then the sample; at the end the
        // voice loops or stops.
        auto step = [&] {
            if (format == Adpcm && (current & 0xF) == 0)
            {
                const uint32_t header = SampleByte(format, current);
                if (InMemory(header, 1))
                    predictorScale = base[header];
                current += 2;
            }
            if (current == end)
            {
                if (!loops)
                {
                    playing = false;
                    return;
                }
                current = loop;
                if (format == Adpcm && !stream)
                {
                    predictorScale = Get16(base, block + pb::LoopPredictorScale);
                    history1 = Get16(base, block + pb::LoopHistory1);
                    history2 = Get16(base, block + pb::LoopHistory2);
                }
                return;
            }
            current++;
        };
        for (uint32_t i = 0; i < SamplesPerFrame && playing; i++)
        {
            fraction += ratio;
            for (uint32_t n = fraction >> 16; n > 0 && playing; n--)
                step();
            fraction &= 0xFFFF;
        }

        const int32_t volume = int32_t(Get16(base, block + pb::Volume)) +
                               int32_t(int16_t(Get16(base, block + pb::VolumeStep))) * int32_t(SamplesPerFrame);
        Put16(base, block + pb::Volume, uint16_t(std::clamp(volume, 0, 0xFFFF)));
        Put32(base, block + pb::CurrentAddress, current);
        Put16(base, block + pb::Fraction, uint16_t(fraction));
        Put16(base, block + pb::PredictorScale, predictorScale);
        Put16(base, block + pb::History1, history1);
        Put16(base, block + pb::History2, history2);
        if (!playing)
            Put16(base, block + pb::State, 0);
    }

    Frame RunCommandList(uint8_t* base, uint32_t address)
    {
        Frame frame;
        uint32_t at = Cached(address);
        for (int commands = 0; commands < 64 && InMemory(at, 2); commands++)
        {
            const uint16_t command = Get16(base, at);
            if (command == End)
            {
                frame.complete = true;
                break;
            }
            if (command >= sizeof(ArgumentWords) / sizeof(ArgumentWords[0]) ||
                !InMemory(at + 2, uint32_t(ArgumentWords[command]) * 2))
            {
                frame.unknownCommand = command;
                break;
            }
            const uint32_t arguments = at + 2;
            if (command == 4) // the voices: a linked list of parameter blocks
            {
                uint32_t block = Get32(base, arguments);
                while (block != 0 && frame.voices < MostVoices && InMemory(Cached(block), BlockSize))
                {
                    block = Cached(block);
                    frame.voices++;
                    if (Get16(base, block + pb::State) == 1)
                        frame.playing++;
                    AdvanceVoice(base, block);
                    block = Get32(base, block + pb::Next);
                }
            }
            else if (command == 11 || command == 12)
                frame.output = Cached(Get32(base, arguments + 6));
            at = arguments + uint32_t(ArgumentWords[command]) * 2;
        }
        return frame;
    }
}
