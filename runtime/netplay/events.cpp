#include "netplay/events.h"

namespace netplay
{
    std::atomic<EventLog*> g_events{ nullptr };

    bool EventLog::Note(std::string_view kind, std::string_view detail)
    {
        // A line per note: a path holding a line break must not split it.
        char line[320];
        size_t used = 0;
        for (const char c : detail.substr(0, 256))
            line[used++] = c == '\n' || c == '\r' ? '_' : c;
        std::lock_guard lock(m_mutex);
        if (!m_file)
            return false;
        return fprintf(m_file, "%llu %.*s %.*s\n", static_cast<unsigned long long>(m_frame), int(kind.size()),
                   kind.data(), int(used), line) > 0;
    }

    bool EventLog::EndFrame(uint64_t frame)
    {
        std::lock_guard lock(m_mutex);
        uint64_t counts[size_t(Counter::Count)];
        for (size_t i = 0; i < size_t(Counter::Count); ++i)
            counts[i] = m_counts[i].exchange(0, std::memory_order_relaxed);
        m_frame = frame + 1;
        if (!m_file)
            return false;
        return fprintf(m_file, "%llu counts vblank=%llu gfxint=%llu audio=%llu timebase=%llu\n",
                   static_cast<unsigned long long>(frame), static_cast<unsigned long long>(counts[0]),
                   static_cast<unsigned long long>(counts[1]), static_cast<unsigned long long>(counts[2]),
                   static_cast<unsigned long long>(counts[3])) > 0 &&
               fflush(m_file) == 0;
    }

    void EventLog::Close()
    {
        std::lock_guard lock(m_mutex);
        if (m_file)
            fclose(m_file);
        m_file = nullptr;
    }
}
