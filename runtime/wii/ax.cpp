#include "wii/ax.h"
#include "wii/ax_mix.h"
#include "wii/memory_map.h"

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
        // Each command's arguments, in 16-bit words (an address takes two). From the SDK's list builder: 0 the
        // studio, 1-3 the output mode's buffer, 4 the voices, 5-7 the aux buses A-C (volume, the buffer the DSP
        // fills, the one the CPU returns), 8-9 A and B in Dolby Pro Logic II (volume and six buffers), 10 the
        // compressor, 11-12 the final mix (volume, the mode's buffer, the output buffer), 13 the four remotes'
        // speakers, 14 the end.
        constexpr int ArgumentWords[] = { 2, 2, 2, 2, 2, 5, 5, 5, 13, 13, 4, 5, 5, 8, 0 };
        constexpr uint16_t End = 14;
        constexpr uint32_t MostVoices = 512; // a list longer than any AX makes: a loop in the links
    } // namespace

    void AdvanceVoice(uint8_t* base, uint32_t block)
    {
        Voice voice = ReadVoice(base, block);
        if (!voice.playing)
            return;
        Bus scratch;
        RenderVoice(base, voice, scratch);
        WriteVoice(base, block, voice);
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
