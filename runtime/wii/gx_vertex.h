#pragma once

// A draw's vertices as the command processor hands them (its vertex descriptor and formats, the indexed arrays in
// guest memory) decoded to floats, and its primitives made triangle lists, for the host renderer (docs/runtime/wii/gx.md).
#include "wii/gx_fifo.h"
#include <cstdint>

namespace wii::gx
{
    struct Vertex
    {
        float position[3];
        float normal[3], binormal[3], tangent[3];
        uint8_t colour[2][4];
        float texture[8][2];
        uint8_t positionMatrix;
        uint8_t textureMatrix[8];
    };

    // Absent components are zero. Indexed attributes must fit wholly in MEM1 or MEM2; false may leave partial output.
    bool DecodeVertices(const CommandProcessor& gp, const uint8_t* base, const DrawCall& draw, Vertex* out);

    uint32_t TriangleListSize(Primitive primitive, uint32_t count);
    // Indices include first. Strip triangle i uses (i, i+1, i+2) when even, (i+1, i, i+2) when odd.
    // Incomplete quads/triangles are omitted; lines, line strips and points produce no indices.
    void TriangleList(Primitive primitive, uint32_t count, uint32_t first, uint32_t* indices);
}
