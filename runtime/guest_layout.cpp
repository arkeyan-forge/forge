#include "guest_layout.h"
#include <algorithm>
#include <cstdio>

namespace guest_layout_detail
{
    GuestLayout g_active;
}

GuestLayout ParsedGuestLayout(uint32_t imageBase, uint32_t imageSize, std::span<const GuestSection> sections)
{
    uint64_t codeMin = UINT64_MAX, codeMax = 0;
    for (const GuestSection& section : sections)
    {
        if (!section.code)
            continue;
        codeMin = std::min<uint64_t>(codeMin, section.base);
        codeMax = std::max<uint64_t>(codeMax, uint64_t(section.base) + section.size);
    }
    if (codeMax == 0)
        return { imageBase, imageSize, 0, 0 };
    return { imageBase, imageSize, uint32_t(codeMin), uint32_t(codeMax - codeMin) };
}

std::string CheckGuestLayout(const GuestLayoutRecord& record, const GuestLayout& parsed, std::string_view imageSha256)
{
    const GuestLayout& expected = record.layout;
    char text[160];
    if (record.version != GuestLayoutVersion)
    {
        snprintf(
            text, sizeof(text), "layout record version %u, this runtime reads %u", record.version, GuestLayoutVersion);
        return text;
    }
    if (record.imageSha256[0] != '\0' && imageSha256 != record.imageSha256)
        return "another executable (image hash)";
    if (expected.imageBase != parsed.imageBase || expected.imageSize != parsed.imageSize)
    {
        snprintf(text, sizeof(text), "image 0x%08X+0x%X, the code was translated from 0x%08X+0x%X", parsed.imageBase,
            parsed.imageSize, expected.imageBase, expected.imageSize);
        return text;
    }
    if (expected.codeBase != parsed.codeBase || expected.codeSize != parsed.codeSize)
    {
        snprintf(text, sizeof(text), "code 0x%08X+0x%X, the code was translated from 0x%08X+0x%X", parsed.codeBase,
            parsed.codeSize, expected.codeBase, expected.codeSize);
        return text;
    }

    const uint64_t imageEnd = uint64_t(expected.imageBase) + expected.imageSize;
    const uint64_t codeEnd = uint64_t(expected.codeBase) + expected.codeSize;
    const uint64_t tableEnd = imageEnd + uint64_t(expected.codeSize) * 2;
    if (expected.imageSize == 0 || expected.codeSize == 0)
        return "empty image or code range";
    if (expected.codeBase % 4 != 0 || expected.codeSize % 4 != 0)
        return "code range not word aligned";
    if (expected.codeBase < expected.imageBase || codeEnd > imageEnd)
        return "code range outside the image";
    if (expected.imageBase < GuestImageFloor || tableEnd > GuestImageCeiling)
    {
        snprintf(text, sizeof(text), "image and function table 0x%08X-0x%09llX outside 0x%08llX-0x%08llX",
            expected.imageBase, (unsigned long long)tableEnd, (unsigned long long)GuestImageFloor,
            (unsigned long long)GuestImageCeiling);
        return text;
    }
    return {};
}

bool ActivateGuestLayout(const GuestLayout& layout)
{
    if (guest_layout_detail::g_active.codeSize != 0)
        return guest_layout_detail::g_active == layout;
    guest_layout_detail::g_active = layout;
    return true;
}
