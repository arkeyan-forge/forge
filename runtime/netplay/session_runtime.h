#pragma once
#include <cstdint>
#include <string_view>
#include <string>

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
    // The version the player sees (window title, F3 menu): "-o" after it in an online session.
    inline std::string VersionLabel(std::string_view version, bool online)
    {
        return std::string(version) + (online ? "-o" : "");
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
    // The player's answer while the other player is lost or stuck (the game's banner).
    void ContinueAlone();
    void KeepWaiting();
    // Leaves the session and quits the game; the launcher sets the room up again for both players.
    void PlayTogetherAgain();
    // The other player's name (their launcher profile), for the banner.
    std::string OtherPlayerName();
    bool QueuePortalEvents(std::span<const PortalEvent> events);
    // Guest memory a game's hook marks as render-only during a session: what the host GPU computed (a depth read back
    // for the game's CPU) and what the game derives from it for drawing alone. Its bytes differ between two GPUs, so it
    // is left out of the state hash and the repair. Called from guest code, at the same point on both computers.
    void SkipRenderOnly(uint32_t begin, uint32_t end);
}
