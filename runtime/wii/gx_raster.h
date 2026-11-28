#pragma once

// GX's draws drawn into the host's EFB on the CPU (docs/runtime/wii/gx.md): the first renderer, for correctness before
// speed. Each draw's vertices transformed by the transform unit's state (the position matrix, the projection, the
// viewport), its triangles clipped to the near plane and scissor and rasterised with their colours and texture coordinates
// interpolated, the texture environment (TEV) run per pixel, then the alpha test, the depth test and the blend.
#include "wii/gx_efb.h"
#include "wii/gx_fifo.h"
#include "wii/gx_vertex.h"
#include <cstdint>
#include <vector>

namespace wii::gx
{
    struct RasterStats
    {
        uint32_t triangles = 0, culled = 0, pixels = 0, textured = 0, badTextures = 0;
    };

    class Rasterizer
    {
    public:
        // One draw, its state the command processor's now.
        void Draw(const CommandProcessor& gp, const uint8_t* base, const DrawCall& draw, Efb& efb);
        // Each BP register written (CommandProcessor::onBp): the TEV's colour and konst registers share their indices.
        void Bp(uint8_t index, uint32_t value);
        RasterStats stats;

        // A vertex once transformed: its screen position (EFB pixels), depth (0-0xFFFFFF), 1/w, colours (0-255) and
        // texture coordinates (normalised). The clip position is retained for clipping before the divide.
        struct Screen
        {
            float x, y, z, inverseW;
            float colour[2][4];
            float texture[8][2];
            float clip[4];
        };

    private:
        struct Texture
        {
            uint32_t address = 0, width = 0, height = 0, format = ~0u, mode = 0;
            std::vector<uint8_t> rgba;
            bool valid = false;
        };
        const Texture& Map(const CommandProcessor& gp, const uint8_t* base, uint32_t map);
        void Triangle(const CommandProcessor& gp, const Screen& a, const Screen& b, const Screen& c, Efb& efb);
        void RasterTriangle(const CommandProcessor& gp, const Screen& a, const Screen& b, const Screen& c, Efb& efb);

        std::vector<Vertex> vertices;
        std::vector<Screen> screen;
        std::vector<uint32_t> indices;
        Texture textures[8];
        uint32_t textureDraw[8] = {}; // the draw each map was last looked up for
        uint32_t draws = 0;
        const uint8_t* base_ = nullptr;
        int32_t tevRegisters[4][4] = {};   // PREV, C0-C2: R G B A, signed 11 bits
        int32_t konstRegisters[4][4] = {}; // K0-K3
    };

    // The transform unit's work for one vertex (also the test's): the position matrix, the projection, the viewport.
    Rasterizer::Screen Transform(const CommandProcessor& gp, const Vertex& vertex);
}
