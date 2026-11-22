#pragma once

#include <cstdint>
#include <string_view>

namespace netplay
{
    inline uint32_t SessionFrameRate(const char* value)
    {
        if (value && std::string_view(value) == "30")
            return 30;
        if (value && std::string_view(value) == "60")
            return 60;
        return 0;
    }
}
