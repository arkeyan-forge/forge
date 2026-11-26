#include "wii/ax_mix.h"
#include "wii/memory_map.h"
#include <algorithm>
#include <array>
#include <limits>

namespace wii::ax
{
    namespace
    {
        uint16_t Get16(const uint8_t* base, uint32_t at) { return uint16_t(uint16_t(base[at]) << 8 | base[at + 1]); }
        uint32_t Get32(const uint8_t* base, uint32_t at)
        {
            return uint32_t(Get16(base, at)) << 16 | Get16(base, at + 2);
        }
        void Put16(uint8_t* base, uint32_t at, uint16_t value)
        {
            base[at] = uint8_t(value >> 8);
            base[at + 1] = uint8_t(value);
        }
        void Put32(uint8_t* base, uint32_t at, uint32_t value)
        {
            Put16(base, at, uint16_t(value >> 16));
            Put16(base, at + 2, uint16_t(value));
        }
        int16_t Clamp16(int64_t sample) { return int16_t(std::clamp<int64_t>(sample, -32768, 32767)); }
        uint16_t Ramp(uint16_t volume, int16_t step) { return uint16_t(std::clamp(int32_t(volume) + step, 0, 0xFFFF)); }

        // Lagrange cubic at t in [0, 1), on input times -1, 0, 1, 2. A two-input delay makes it causal.
        // Q15 weights sum to unity exactly; integer construction avoids startup work and floating-point drift.
        constexpr auto MakePhases()
        {
            constexpr int64_t Phases = 128;
            std::array<std::array<int32_t, 4>, Phases> table{};
            for (int64_t p = 0; p < Phases; ++p)
            {
                auto& weights = table[size_t(p)];
                weights[0] = int32_t(-p * (p - Phases) * (p - 2 * Phases) * 32768 / (6 * Phases * Phases * Phases));
                weights[1] =
                    int32_t((p + Phases) * (p - Phases) * (p - 2 * Phases) * 32768 / (2 * Phases * Phases * Phases));
                weights[2] = int32_t(-(p + Phases) * p * (p - 2 * Phases) * 32768 / (2 * Phases * Phases * Phases));
                weights[3] = 32768 - weights[0] - weights[1] - weights[2];
            }
            return table;
        }
        constexpr auto Phases = MakePhases();

        bool SourceAddress(uint32_t current, Format format, uint32_t& address)
        {
            const uint64_t byte = format == Adpcm   ? uint64_t(current) / 2
                                  : format == Pcm16 ? uint64_t(current) * 2
                                                    : current;
            if (byte > 0x1FFFFFFF)
                return false;
            address = Cached(uint32_t(byte));
            return InMemory(address, format == Pcm16 ? 2 : 1);
        }

        bool Consume(const uint8_t* base, Voice& voice, int16_t& sample)
        {
            uint32_t address = 0;
            if (voice.format != Adpcm && voice.format != Pcm16 && voice.format != Pcm8)
            {
                voice.playing = false;
                return false;
            }
            if (voice.format == Adpcm && (voice.currentAddress & 15) < 2)
            {
                if (!SourceAddress(voice.currentAddress & ~1u, Adpcm, address))
                {
                    voice.playing = false;
                    return false;
                }
                voice.predictorScale = base[address];
                voice.currentAddress = (voice.currentAddress & ~15u) + 2;
            }
            if (!SourceAddress(voice.currentAddress, voice.format, address))
            {
                voice.playing = false;
                return false;
            }
            if (voice.format == Adpcm)
            {
                const uint32_t predictor = voice.predictorScale >> 4;
                if (predictor >= 8)
                {
                    voice.playing = false;
                    return false;
                }
                const int nibble = (base[address] >> ((voice.currentAddress & 1) ? 0 : 4)) & 15;
                const int32_t residual = (nibble < 8 ? nibble : nibble - 16) * (1 << (voice.predictorScale & 15));
                sample = Clamp16((int64_t(voice.coefficients[2 * predictor]) * voice.history1 +
                                     int64_t(voice.coefficients[2 * predictor + 1]) * voice.history2 +
                                     int64_t(residual) * 2048 + 1024) >>
                                 11);
                voice.history2 = voice.history1;
                voice.history1 = sample;
            }
            else if (voice.format == Pcm16)
                sample = int16_t(Get16(base, address));
            else
                sample = int16_t(int8_t(base[address]) * 256);

            if (voice.currentAddress == voice.endAddress)
            {
                if (!voice.loops)
                    voice.playing = false;
                else
                {
                    voice.currentAddress = voice.loopAddress;
                    voice.looped = true;
                    if (voice.format == Adpcm && !voice.stream)
                    {
                        voice.predictorScale = voice.loopPredictorScale;
                        voice.history1 = voice.loopHistory1;
                        voice.history2 = voice.loopHistory2;
                    }
                }
            }
            else
                ++voice.currentAddress;
            return true;
        }

        int32_t Interpolate(const Voice& voice)
        {
            if (voice.converter == 2)
                return voice.lastInputs[3];
            if (voice.converter == 1)
                return int32_t((int64_t(voice.lastInputs[2]) * (65536 - voice.fraction) +
                                   int64_t(voice.lastInputs[3]) * voice.fraction) >>
                               16);
            int64_t value = 0;
            const auto& weights = Phases[voice.fraction >> 9];
            for (int tap = 0; tap < 4; ++tap)
                value += int64_t(voice.lastInputs[tap]) * weights[tap];
            return Clamp16(value >> 15);
        }

        void Mix(int32_t sample, Gain& gain, int32_t& destination)
        {
            const int64_t sum = int64_t(destination) + ((int64_t(sample) * gain.volume) >> 15);
            destination = int32_t(
                std::clamp<int64_t>(sum, std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max()));
            gain.volume = Ramp(gain.volume, gain.step);
        }
    }

    Voice ReadVoice(const uint8_t* base, uint32_t block)
    {
        Voice voice;
        block = Cached(block);
        if (!InMemory(block, BlockSize))
            return voice;
        voice.format = Format(Get16(base, block + pb::Format));
        voice.loops = Get16(base, block + pb::Loops) != 0;
        voice.stream = Get16(base, block + pb::Type) == 1;
        voice.loopAddress = Get32(base, block + pb::LoopAddress);
        voice.endAddress = Get32(base, block + pb::EndAddress);
        voice.currentAddress = Get32(base, block + pb::CurrentAddress);
        for (uint32_t i = 0; i < 16; ++i)
            voice.coefficients[i] = int16_t(Get16(base, block + pb::Coefficients + 2 * i));
        voice.predictorScale = Get16(base, block + pb::PredictorScale);
        voice.history1 = int16_t(Get16(base, block + pb::History1));
        voice.history2 = int16_t(Get16(base, block + pb::History2));
        voice.loopPredictorScale = Get16(base, block + pb::LoopPredictorScale);
        voice.loopHistory1 = int16_t(Get16(base, block + pb::LoopHistory1));
        voice.loopHistory2 = int16_t(Get16(base, block + pb::LoopHistory2));
        voice.converter = Get16(base, block + pb::Source);
        voice.ratio = Get32(base, block + pb::Ratio);
        voice.fraction = Get16(base, block + pb::Fraction);
        for (uint32_t i = 0; i < 4; ++i)
            voice.lastInputs[i] = int16_t(Get16(base, block + pb::LastInputs + 2 * i));
        voice.volume = Get16(base, block + pb::Volume);
        voice.volumeStep = int16_t(Get16(base, block + pb::VolumeStep));
        voice.playing = Get16(base, block + pb::State) == 1;
        // Mix gains remain host-side until the parameter block's mix layout has been checked.
        return voice;
    }

    void WriteVoice(uint8_t* base, uint32_t block, const Voice& voice)
    {
        block = Cached(block);
        if (!InMemory(block, BlockSize))
            return;
        Put16(base, block + pb::Format, voice.format);
        Put16(base, block + pb::Loops, voice.loops);
        Put16(base, block + pb::Type, voice.stream);
        Put32(base, block + pb::LoopAddress, voice.loopAddress);
        Put32(base, block + pb::EndAddress, voice.endAddress);
        Put32(base, block + pb::CurrentAddress, voice.currentAddress);
        for (uint32_t i = 0; i < 16; ++i)
            Put16(base, block + pb::Coefficients + 2 * i, uint16_t(voice.coefficients[i]));
        Put16(base, block + pb::PredictorScale, voice.predictorScale);
        Put16(base, block + pb::History1, uint16_t(voice.history1));
        Put16(base, block + pb::History2, uint16_t(voice.history2));
        Put16(base, block + pb::LoopPredictorScale, voice.loopPredictorScale);
        Put16(base, block + pb::LoopHistory1, uint16_t(voice.loopHistory1));
        Put16(base, block + pb::LoopHistory2, uint16_t(voice.loopHistory2));
        Put16(base, block + pb::Source, voice.converter);
        Put32(base, block + pb::Ratio, voice.ratio);
        Put16(base, block + pb::Fraction, voice.fraction);
        for (uint32_t i = 0; i < 4; ++i)
            Put16(base, block + pb::LastInputs + 2 * i, uint16_t(voice.lastInputs[i]));
        Put16(base, block + pb::Volume, voice.volume);
        Put16(base, block + pb::VolumeStep, uint16_t(voice.volumeStep));
        Put16(base, block + pb::State, voice.playing);
        // Mix gains are deliberately excluded, as in ReadVoice.
    }

    void RenderVoice(const uint8_t* base, Voice& voice, Bus& bus)
    {
        voice.looped = false;
        if (!voice.playing)
            return;
        for (uint32_t i = 0; i < SamplesPerFrame; ++i)
        {
            int32_t output = 0;
            if (voice.playing)
            {
                const uint64_t position = uint64_t(voice.fraction) + (voice.converter == 2 ? 0x10000 : voice.ratio);
                voice.fraction = uint16_t(position);
                bool valid = true;
                for (uint32_t n = uint32_t(position >> 16); n > 0 && voice.playing; --n)
                {
                    int16_t sample = 0;
                    valid = Consume(base, voice, sample);
                    for (int tap = 0; tap < 3; ++tap)
                        voice.lastInputs[tap] = voice.lastInputs[tap + 1];
                    voice.lastInputs[3] = sample;
                }
                if (valid)
                    output = Interpolate(voice);
            }
            output = int32_t((int64_t(output) * voice.volume) >> 15);
            voice.volume = Ramp(voice.volume, voice.volumeStep);
            for (int channel = 0; channel < 3; ++channel)
            {
                Mix(output, voice.main[channel], bus.main[channel][i]);
                for (int aux = 0; aux < 3; ++aux)
                    Mix(output, voice.aux[aux][channel], bus.aux[aux][channel][i]);
            }
        }
    }

    void FinalMix(const Bus& bus, uint8_t* out384)
    {
        for (uint32_t i = 0; i < SamplesPerFrame; ++i)
        {
            const int64_t surround = (int64_t(bus.main[2][i]) * 23170) >> 15;
            for (uint32_t channel = 0; channel < 2; ++channel)
                Put16(out384, 4 * i + 2 * channel, uint16_t(Clamp16(int64_t(bus.main[channel][i]) + surround)));
        }
    }
}
