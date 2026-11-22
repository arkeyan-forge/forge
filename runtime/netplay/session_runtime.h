#pragma once

#include "host/env.h"
#include "netplay/session.h"

namespace netplay
{
    inline std::atomic<bool> g_continuingSolo{ false };
    inline SavePolicy g_savePolicy;
    inline bool SessionContinues() { return g_continuingSolo.load(std::memory_order_acquire); }
    inline bool SaveWritesAllowed() { return g_savePolicy.Allowed(); }
    inline bool Online()
    {
        static const bool enabled = skyenv::Get("SKY_NETPLAY_HOST") || skyenv::Get("SKY_NETPLAY_JOIN");
        return enabled && !SessionContinues();
    }
    inline bool Hosting()
    {
        static const bool host = skyenv::Get("SKY_NETPLAY_HOST") != nullptr;
        return host;
    }
    using OnlineStatus = SessionStats;
    void PrepareSessionSetup();
    OnlineStatus SessionStatus();
    EndReason SessionEndReason();
    const char* SessionEndMessage();
    void LeaveSession();
    void SetMenuOpen(bool open);
    bool QueuePortalEvents(std::span<const PortalEvent> events);
}
