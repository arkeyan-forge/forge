#include "wii/audio_out.h"
#include <SDL3/SDL.h>
#include <atomic>
#include <cstdio>
#include <mutex>

namespace wii::audio_out
{
    namespace
    {
        std::mutex g_mutex;
        SDL_AudioStream* g_stream = nullptr;
        std::atomic<uint64_t> g_dropped{ 0 };
        constexpr uint32_t BytesPerFrame = 4; // 16-bit stereo
        constexpr uint32_t MostQueuedBytes = SampleRate * BytesPerFrame / 1000 * MostQueuedMs;
        constexpr size_t LargestBuffer = 4096; // AX's are 384 bytes; anything larger goes in pieces
    } // namespace

    void ToHost(const uint8_t* bigEndian, size_t bytes, int16_t* out)
    {
        for (size_t i = 0; i + 1 < bytes; i += 2)
            out[i / 2] = int16_t(uint16_t(bigEndian[i] << 8 | bigEndian[i + 1]));
    }

    bool Start()
    {
        std::lock_guard lock(g_mutex);
        if (g_stream != nullptr)
            return true;
        // SDL would turn SIGINT and SIGTERM into a quit event that nothing reads here: the process could not be
        // stopped. The host's own handlers keep them.
        SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
        if (!SDL_WasInit(SDL_INIT_AUDIO) && !SDL_InitSubSystem(SDL_INIT_AUDIO))
        {
            fprintf(stderr, "[audio] no audio: %s\n", SDL_GetError());
            return false;
        }
        const SDL_AudioSpec spec{ SDL_AUDIO_S16, 2, int(SampleRate) };
        g_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
        if (g_stream == nullptr)
        {
            fprintf(stderr, "[audio] no audio device: %s\n", SDL_GetError());
            return false;
        }
        SDL_ResumeAudioStreamDevice(g_stream);
        fprintf(stderr, "[audio] %s, 16-bit stereo at %u Hz\n", SDL_GetCurrentAudioDriver(), SampleRate);
        return true;
    }

    void Stop()
    {
        std::lock_guard lock(g_mutex);
        if (g_stream != nullptr)
            SDL_DestroyAudioStream(g_stream);
        g_stream = nullptr;
    }

    void Play(const uint8_t* bigEndian, size_t bytes)
    {
        std::lock_guard lock(g_mutex);
        if (g_stream == nullptr)
            return;
        if (SDL_GetAudioStreamQueued(g_stream) >= int(MostQueuedBytes))
        {
            g_dropped++;
            return;
        }
        int16_t samples[LargestBuffer / 2];
        for (size_t at = 0; at < bytes; at += LargestBuffer)
        {
            const size_t piece = bytes - at < LargestBuffer ? bytes - at : LargestBuffer;
            ToHost(bigEndian + at, piece & ~size_t(1), samples);
            SDL_PutAudioStreamData(g_stream, samples, int(piece & ~size_t(1)));
        }
    }

    uint32_t Queued()
    {
        std::lock_guard lock(g_mutex);
        const int queued = g_stream != nullptr ? SDL_GetAudioStreamQueued(g_stream) : 0;
        return queued > 0 ? uint32_t(queued) : 0;
    }

    uint64_t Dropped() { return g_dropped.load(); }
}
