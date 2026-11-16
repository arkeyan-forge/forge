#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <string_view>

namespace netplay
{
    // What the frame-stamped event log (SKY_NETPLAY_EVENTS) counts in each frame: the host-paced sources a replay
    // must deliver at the same frames for two runs to match.
    enum class Counter : uint32_t
    {
        VBlank,
        GraphicsInterrupt,
        AudioCallback,
        TimeBase,
        Count
    };

    class EventLog
    {
    public:
        explicit EventLog(FILE* file) : m_file(file) {}
        void Count(Counter counter) { m_counts[size_t(counter)].fetch_add(1, std::memory_order_relaxed); }
        // One line stamped with the running frame; the order of notes from different threads is part of what a
        // replay compares.
        bool Note(std::string_view kind, std::string_view detail);
        // Writes the counters of `frame`, the frame that just ended, and stamps later notes with the next one.
        bool EndFrame(uint64_t frame);
        // A thread that loaded the log before it was cleared may still note: it then finds no file.
        void Close();

    private:
        FILE* m_file;
        std::mutex m_mutex;
        uint64_t m_frame = 0;
        std::array<std::atomic<uint64_t>, size_t(Counter::Count)> m_counts{};
    };

    extern std::atomic<EventLog*> g_events;
    inline void CountEvent(Counter counter)
    {
        if (EventLog* log = g_events.load(std::memory_order_acquire))
            log->Count(counter);
    }
    inline void NoteEvent(std::string_view kind, std::string_view detail)
    {
        if (EventLog* log = g_events.load(std::memory_order_acquire))
            log->Note(kind, detail);
    }
}
