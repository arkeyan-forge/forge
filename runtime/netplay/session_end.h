#pragma once

#include "netplay/protocol.h"
#include <filesystem>
#include <fstream>

namespace netplay
{
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
