#pragma once

#include "netplay/session.h"
#include <utility>
#include <vector>

namespace netplay
{
    struct NetSim
    {
        uint32_t delayMs = 0, jitterMs = 0;
        double lossPercent = 0, duplicatePercent = 0;
        std::vector<std::pair<uint64_t, uint64_t>> cutsMs;
        uint64_t seed = 0;
    };

    bool ParseNetSim(std::string_view text, NetSim& sim, std::string& error);
    std::unique_ptr<Transport> SimulateNetwork(
        std::unique_ptr<Transport> inner, NetSim sim, std::function<uint64_t()> nowMs);
}
