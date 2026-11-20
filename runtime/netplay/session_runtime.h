#pragma once

#include "host/env.h"
#include "netplay/protocol.h"

namespace netplay
{
    inline bool Online()
    {
        static const bool enabled = skyenv::Get("SKY_NETPLAY_HOST") || skyenv::Get("SKY_NETPLAY_JOIN");
        return enabled;
    }
    inline bool Hosting()
    {
        static const bool host = skyenv::Get("SKY_NETPLAY_HOST") != nullptr;
        return host;
    }
    struct OnlineStatus
    {
        uint64_t roundTripMs = 0, goodHash = 0;
        uint32_t delay = 0;
    };
    void PrepareSessionSetup();
    OnlineStatus SessionStatus();
    EndReason SessionEndReason();
    const char* SessionEndMessage();
    void LeaveSession();
    bool QueuePortalEvents(std::span<const PortalEvent> events);
}
