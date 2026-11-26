#pragma once

// The Wii host's sound (docs/runtime/wii/audio.md): each buffer the audio interface's DMA plays (16-bit big-endian
// stereo at 32 kHz, what AX's final mix writes) queued to the host's audio device through SDL.
#include <cstddef>
#include <cstdint>

namespace wii::audio_out
{
    constexpr uint32_t SampleRate = 32000;
    // Past this much queued (the host's timers running ahead of the device), a buffer is dropped rather than letting
    // the delay grow.
    constexpr uint32_t MostQueuedMs = 100;

    // Big-endian 16-bit samples to the host's order.
    void ToHost(const uint8_t* bigEndian, size_t bytes, int16_t* out);

    // The device opened (SDL's audio started if it is not); false, logged, when there is none: the game then runs
    // silent.
    bool Start();
    void Stop();

    // One buffer, from any thread; ignored before Start, dropped when MostQueuedMs is queued already.
    void Play(const uint8_t* bigEndian, size_t bytes);

    // What the device has not played yet, in bytes of host samples (the tests'); 0 with no device.
    uint32_t Queued();
    uint64_t Dropped();
}
