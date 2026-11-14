#include "presence.h"
#include "save_backup.h"
#include <atomic>
#include <memory>
#include <mutex>

namespace presence
{
    bool Writer::Write(const State& state)
    {
        if (m_folder.empty() || (m_hasWritten && m_written == state) || (state.players != 1 && state.players != 2) ||
            state.level.find_first_of("\r\n") != std::string::npos)
            return false;
        std::string text = "level=" + state.level + "\nmenu=" + (state.menu ? "1" : "0") +
                           "\nplayers=" + std::to_string(state.players) + "\n";
        std::error_code error;
        std::filesystem::create_directories(m_folder, error);
        if (error || !savebackup::WriteFileAtomic(m_folder / "presence.txt", text.data(), text.size()))
            return false;
        m_written = state;
        m_hasWritten = true;
        return true;
    }

    namespace
    {
        std::mutex g_mutex;
        std::unique_ptr<Writer> g_writer;
        State g_state;
        std::atomic<int> g_players{ 1 };
        std::atomic<bool> g_pending{ false };
    }

    void Begin(const std::filesystem::path& folder)
    {
        std::lock_guard lock(g_mutex);
        g_players = 1;
        g_pending = false;
        g_state = {};
        g_writer = std::make_unique<Writer>(folder);
        g_writer->Write(g_state);
    }

    void LevelStarted(std::string_view level, bool menu)
    {
        std::lock_guard lock(g_mutex);
        g_state.level = level;
        g_state.menu = menu;
        g_state.players = g_players.load();
        if (g_writer != nullptr)
            g_writer->Write(g_state);
    }

    void SetSecondPlayer(bool present)
    {
        int players = present ? 2 : 1;
        if (g_players.exchange(players) != players)
            g_pending = true;
    }

    void Flush()
    {
        if (!g_pending.exchange(false))
            return;
        std::lock_guard lock(g_mutex);
        g_state.players = g_players.load();
        if (g_writer != nullptr)
            g_writer->Write(g_state);
    }
}
