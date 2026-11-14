#pragma once

#include <filesystem>
#include <string>
#include <string_view>

// The running game's small snapshot for the launcher (docs/getting-started/settings.md).
namespace presence
{
    struct State
    {
        std::string level;
        bool menu = true;
        int players = 1;
        bool operator==(const State&) const = default;
    };

    class Writer
    {
    public:
        explicit Writer(const std::filesystem::path& folder) : m_folder(folder) {}
        // True only when an atomic replacement succeeded. An unchanged state does not touch the filesystem;
        // a failed write leaves the previous snapshot and can be retried with the same state.
        bool Write(const State& state);

    private:
        std::filesystem::path m_folder;
        State m_written;
        bool m_hasWritten = false;
    };

    // Start with an empty level, menu=1 and players=1, replacing any previous session's snapshot.
    void Begin(const std::filesystem::path& folder);
    // Level-start event: retains the exact id supplied by the game and writes outside the frame loop.
    void LevelStarted(std::string_view level, bool menu);
    // Main thread's seat observation: only posts a count change, with no allocation or filesystem access.
    void SetSecondPlayer(bool present);
    // Toy Box link worker: consumes posted changes, including when its socket is disabled or unavailable.
    void Flush();
}
