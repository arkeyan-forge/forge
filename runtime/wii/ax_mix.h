#pragma once

// AX's voices rendered as the DSP would: decoded, through the rate converter, the envelope and the mix, into the
// buses of one 3 ms frame (docs/runtime/wii/audio.md).
#include "wii/ax.h"
#include <cstdint>

namespace wii::ax
{
    struct Gain
    {
        uint16_t volume = 0;
        int16_t step = 0;
    };

    struct Voice
    {
        Format format = Adpcm;
        bool loops = false;
        bool stream = false;
        uint32_t loopAddress = 0;
        uint32_t endAddress = 0;
        uint32_t currentAddress = 0;
        int16_t coefficients[16]{};
        uint16_t predictorScale = 0;
        int16_t history1 = 0;
        int16_t history2 = 0;
        uint16_t loopPredictorScale = 0;
        int16_t loopHistory1 = 0;
        int16_t loopHistory2 = 0;
        uint16_t converter = 0;
        uint32_t ratio = 0x10000;
        uint16_t fraction = 0;
        int16_t lastInputs[4]{};
        uint16_t volume = 0;
        int16_t volumeStep = 0;
        Gain main[3]{}; // L, R, S; likewise for each aux bus
        Gain aux[3][3]{};
        bool playing = false;
        bool looped = false; // in the last frame rendered: it reached its end and went back to its loop
    };

    struct Bus
    {
        int32_t main[3][SamplesPerFrame]{};
        int32_t aux[3][3][SamplesPerFrame]{};
    };

    Voice ReadVoice(const uint8_t* base, uint32_t block);
    void WriteVoice(uint8_t* base, uint32_t block, const Voice& voice);
    void RenderVoice(const uint8_t* base, Voice& voice, Bus& bus);
    void FinalMix(const Bus& bus, uint8_t* out384, uint16_t volume = 0x8000);
}
