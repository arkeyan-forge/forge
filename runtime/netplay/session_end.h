#pragma once

#include "netplay/protocol.h"
#include <atomic>
#include <filesystem>
#include <fstream>

namespace netplay
{
    inline bool ContinueSolo(bool established, EndReason reason, bool leftLocally)
    {
        return established && !leftLocally &&
               (reason == EndReason::Quit || reason == EndReason::Timeout || reason == EndReason::Transport);
    }

    class SavePolicy
    {
    public:
        bool Allowed() const { return !m_suppressed.load(std::memory_order_acquire); }
        void End(bool hosting, bool established, EndReason reason, bool leftLocally)
        {
            if (!hosting && ContinueSolo(established, reason, leftLocally))
                m_suppressed.store(true, std::memory_order_release);
        }

    private:
        std::atomic<bool> m_suppressed{ false };
    };

    inline bool WriteOpenOnlineFile(const std::filesystem::path& folder, std::string_view game)
    {
        if (game.empty() || game.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_-") != std::string_view::npos)
            return false;
        std::error_code error;
        if (!folder.empty())
            std::filesystem::create_directories(folder, error);
        if (error)
            return false;
        std::ofstream file(folder / "open-online.txt", std::ios::trunc);
        file << game << '\n';
        file.close();
        return !file.fail();
    }

    class EndScreenClock
    {
    public:
        void Show(uint64_t nowMs)
        {
            if (!m_shown)
            {
                m_shown = true;
                m_start = nowMs;
            }
        }
        bool QuitDue(uint64_t nowMs) const { return m_shown && nowMs >= m_start && nowMs - m_start >= 4000; }

    private:
        bool m_shown = false;
        uint64_t m_start = 0;
    };

    inline const char* ReasonCode(EndReason reason)
    {
        switch (reason)
        {
        case EndReason::None: return "none";
        case EndReason::Protocol: return "protocol";
        case EndReason::Game: return "game";
        case EndReason::Image: return "image";
        case EndReason::Runtime: return "runtime";
        case EndReason::Platform: return "platform";
        case EndReason::Fps: return "fps";
        case EndReason::Seats: return "seats";
        case EndReason::Timeout: return "timeout";
        case EndReason::Desync: return "desync";
        case EndReason::Quit: return "quit";
        case EndReason::Transport: return "transport";
        case EndReason::Invalid: return "invalid";
        }
        return "invalid";
    }
    inline const char* EndMessage(EndReason reason)
    {
        switch (reason)
        {
        case EndReason::Quit: return "The other player left the game";
        case EndReason::Timeout: return "The other player stopped responding";
        case EndReason::Desync: return "The games went out of sync";
        case EndReason::Transport: return "The connection was lost";
        case EndReason::Invalid: return "The session received invalid data";
        default: return "The online session was refused";
        }
    }
    inline bool WriteEndFile(const std::filesystem::path& folder, EndReason reason)
    {
        std::error_code error;
        if (!folder.empty())
            std::filesystem::create_directories(folder, error);
        if (error)
            return false;
        std::ofstream file(folder / "online-ended.txt", std::ios::trunc);
        file << "reason=" << ReasonCode(reason) << '\n';
        file.close();
        return !file.fail();
    }
}
