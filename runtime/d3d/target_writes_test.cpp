// Checks the write masks of the targets a pixel shader does not write and how its exports are clamped. usage:
// ArkeyanTargetWritesTest
#include "d3d/target_writes.h"
#include <cstdio>
#include <initializer_list>

namespace
{
    int g_failures = 0;

#define CHECK(condition)                                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(condition))                                                                                              \
        {                                                                                                              \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition);                                               \
            g_failures++;                                                                                              \
        }                                                                                                              \
    } while (0)
}

int main()
{
    // The header's outputs: oC0-oC3 in bits 0-3; the depth (bit 4) and the rest are not colour targets.
    CHECK(d3d::PixelShaderColorOutputs(0x1) == 0x1);
    CHECK(d3d::PixelShaderColorOutputs(0x17) == 0x7);
    CHECK(d3d::PixelShaderColorOutputs(0x10) == 0);
    CHECK(d3d::PixelShaderColorOutputs(0xFFFFFFF0) == 0);

    // Trap Team's title glow (pixel shaders 3BFF1608, 1B6CF1AD): oC0 only, two targets bound, RB_COLOR_MASK 0xFF.
    CHECK(d3d::TargetWriteMask(0xFF, 0x1, 0) == 0xF);
    CHECK(d3d::TargetWriteMask(0xFF, 0x1, 1) == 0);

    // A G-buffer pass writing three targets keeps each target's own mask.
    CHECK(d3d::TargetWriteMask(0x7F3F, 0x7, 0) == 0xF);
    CHECK(d3d::TargetWriteMask(0x7F3F, 0x7, 1) == 0x3);
    CHECK(d3d::TargetWriteMask(0x7F3F, 0x7, 2) == 0xF);
    CHECK(d3d::TargetWriteMask(0x7F3F, 0x7, 3) == 0);

    // No pixel shader (a depth-only pass): no colour target is written.
    for (uint32_t slot = 0; slot < 4; slot++)
        CHECK(d3d::TargetWriteMask(0xFFFF, 0, slot) == 0);

    // A target the game masks stays masked even when the shader writes it.
    CHECK(d3d::TargetWriteMask(0xF0, 0x3, 0) == 0);
    CHECK(d3d::TargetWriteMask(0xF0, 0x3, 1) == 0xF);

    // Exports to the formats the host keeps as half floats are stored as the console would store them.
    CHECK(d3d::ExportClamp(3) == 1);
    CHECK(d3d::ExportClamp(12) == 1);
    CHECK(d3d::ExportClamp(4) == 2);
    CHECK(d3d::ExportClamp(5) == 2);
    for (uint32_t format : { 0u, 1u, 2u, 6u, 7u, 10u, 14u, 15u })
        CHECK(d3d::ExportClamp(format) == 0);

    // A lighting pass: a 7e3 light buffer, an 8888 colour target and a 16_16_16_16 one, two bits each.
    const uint32_t formats[] = { 3, 0, 5, 12 };
    CHECK(d3d::TargetClamps(formats, 3) == (1u | 0u << 2 | 2u << 4));
    CHECK(d3d::TargetClamps(formats, 4) == (1u | 2u << 4 | 1u << 6));
    CHECK(d3d::TargetClamps(formats, 0) == 0);

    if (g_failures == 0)
        printf("ArkeyanTargetWritesTest: all checks passed\n");
    return g_failures == 0 ? 0 : 1;
}
