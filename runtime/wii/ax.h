#pragma once

// The DSP's side of AX, the SDK's audio library, run by the host (docs/runtime/wii/audio.md): each 3 ms frame, AX
// sends the DSP a command list (the studio, the voices' parameter blocks, the aux buses, the output buffer); the
// host runs it. Voices render into a scratch bus until the command list's buffers and host output are connected.
// The layouts are the SDK's own: its command list builder (__AXNextFrame) and its voice code
// (AXSetVoice*, __AXServiceVPB) in the game.
#include <cstdint>

namespace wii::ax
{
    constexpr uint32_t SamplesPerFrame = 96; // 3 ms at 32 kHz
    constexpr uint32_t SampleRate = 32000;
    constexpr uint32_t BlockSize = 320; // a voice's parameter block

    // A parameter block's fields, by offset (16-bit words unless said).
    namespace pb
    {
        constexpr uint32_t Next = 0;     // the next block's address (32 bits), 0 for the last
        constexpr uint32_t Source = 8;   // the rate converter: 0 polyphase, 1 linear, 2 none
        constexpr uint32_t State = 16;   // 1 playing, 0 stopped
        constexpr uint32_t Type = 18;    // 1 a stream: the CPU keeps its loop and end addresses
        constexpr uint32_t Volume = 106; // the envelope: its volume, then its signed step for each sample
        constexpr uint32_t VolumeStep = 108;
        constexpr uint32_t Loops = 110;
        constexpr uint32_t Format = 112;
        constexpr uint32_t LoopAddress = 114; // 32 bits each, in the format's units (nibbles, samples, bytes)
        constexpr uint32_t EndAddress = 118;
        constexpr uint32_t CurrentAddress = 122;
        constexpr uint32_t Coefficients = 126; // 8 pairs: history 1, then history 2 (signed 1.11)
        constexpr uint32_t PredictorScale = 160; // ADPCM: the frame header in use, the last two samples
        constexpr uint32_t History1 = 162;
        constexpr uint32_t History2 = 164;
        constexpr uint32_t Ratio = 166;    // 32 bits: input samples for each output one, 16.16
        constexpr uint32_t Fraction = 170; // the converter's position between two input samples
        constexpr uint32_t LastInputs = 172; // four signed input samples, oldest first
        constexpr uint32_t LoopPredictorScale = 180; // ADPCM: the state the loop starts from (not a stream's)
        constexpr uint32_t LoopHistory1 = 182;
        constexpr uint32_t LoopHistory2 = 184;
    }

    enum Format : uint16_t
    {
        Adpcm = 0, // 8-byte frames: a header byte (predictor, scale), a padding byte, 14 4-bit samples
        Pcm16 = 10,
        Pcm8 = 25,
    };

    // The DSP's addresses are physical: the host reads them through MEM1's or MEM2's cached address.
    constexpr uint32_t Cached(uint32_t address) { return 0x80000000u | (address & 0x1FFFFFFFu); }

    struct Frame
    {
        uint32_t voices = 0;       // the blocks in the list
        uint32_t playing = 0;      // of them, playing at the frame's start
        uint32_t output = 0;       // the output buffer (16-bit stereo, for the audio interface)
        bool complete = false;     // the list reached its end command
        int32_t unknownCommand = -1; // the first command not known, which stops the list
    };

    // Runs the command list at `address` (`base`: the guest's memory, address A at base + A).
    Frame RunCommandList(uint8_t* base, uint32_t address);

    // A voice's frame: its position advanced by one frame of output at its rate, stopped at its end unless it loops.
    void AdvanceVoice(uint8_t* base, uint32_t block);
}
