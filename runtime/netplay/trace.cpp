#include "netplay/trace.h"
#include "netplay/state_hash.h"
#include <charconv>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <optional>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

namespace netplay
{
    namespace
    {
        constexpr uint64_t ShadowBytes = uint64_t(1) << 32;

        [[noreturn]] void Refuse(const char* message)
        {
            fprintf(stderr, "[netplay] refused: %s\n", message);
            fflush(stderr);
            std::_Exit(1);
        }

        void Write(FILE* file, const void* bytes, size_t size)
        {
            if (fwrite(bytes, 1, size, file) != size)
                Refuse("trace write failed");
        }

        void Encode(uint8_t*& bytes, uint64_t value, unsigned size)
        {
            for (unsigned i = 0; i < size; ++i)
                *bytes++ = uint8_t(value >> (8 * i));
        }

        uint64_t Value(const void* bytes, uint8_t size)
        {
            uint64_t value = 0;
            const auto* data = static_cast<const uint8_t*>(bytes);
            for (uint8_t i = 0; i < size; ++i)
                value = (value << 8) | data[i];
            return value;
        }

        using File = std::unique_ptr<FILE, decltype(&fclose)>;
        File g_records(nullptr, fclose), g_frames(nullptr, fclose);
        std::optional<Trace> g_session;
        std::mutex g_finishMutex;

#ifdef ARKEYAN_NETPLAY_TRACE
        std::array<char, 65536> g_recordBuffer{};
        std::array<char, 4096> g_frameBuffer{};

        File Open(const char* path, const char* mode, char* buffer, size_t size)
        {
            File file(fopen(path, mode), fclose);
            if (!file)
                Refuse("cannot open trace file");
            if (setvbuf(file.get(), buffer, _IOFBF, size) != 0)
                Refuse("cannot buffer trace file");
            return file;
        }
#endif
    }

    Trace* const g_trace = []() -> Trace* {
#ifdef ARKEYAN_NETPLAY_TRACE
        const char* path = std::getenv("SKY_NETPLAY_TRACE"); // NOLINT(concurrency-mt-unsafe)
        if (!path)
            return nullptr;
        const char* schedule = std::getenv("SKY_NETPLAY_SCHEDULE"); // NOLINT(concurrency-mt-unsafe)
        const char* deterministic = std::getenv("SKY_NETPLAY_DETERMINISTIC"); // NOLINT(concurrency-mt-unsafe)
        const char* delivery = std::getenv("SKY_NETPLAY_DELIVERY"); // NOLINT(concurrency-mt-unsafe)
        if (!schedule || std::string_view(schedule) != "cooperative" || !deterministic ||
            std::string_view(deterministic) != "1" || !delivery || std::string_view(delivery) != "frame")
            Refuse("trace requires schedule=cooperative, deterministic=1 and delivery=frame");
        uint32_t first = 0, last = UINT32_MAX;
        const char* range = std::getenv("SKY_NETPLAY_TRACE_FRAMES"); // NOLINT(concurrency-mt-unsafe)
        if (range && !ParseTraceFrames(range, first, last))
            Refuse("SKY_NETPLAY_TRACE_FRAMES is not an inclusive decimal frame range");
        g_records = Open(path, "wb", g_recordBuffer.data(), g_recordBuffer.size());
        g_frames = Open((std::string(path) + ".frames").c_str(), "w", g_frameBuffer.data(), g_frameBuffer.size());
        g_session.emplace(g_records.get(), g_frames.get(), first, last);
        return &*g_session;
#else
        return nullptr;
#endif
    }();

    bool ParseTraceFrames(std::string_view text, uint32_t& first, uint32_t& last)
    {
        const auto split = text.find('-');
        if (split == std::string_view::npos || split == 0 || split + 1 == text.size())
            return false;
        const auto a = std::from_chars(text.data(), text.data() + split, first);
        const auto b = std::from_chars(text.data() + split + 1, text.data() + text.size(), last);
        return a.ec == std::errc{} && a.ptr == text.data() + split && b.ec == std::errc{} &&
               b.ptr == text.data() + text.size() && first <= last;
    }

    Trace::Trace(FILE* records, FILE* frames, uint32_t first, uint32_t last)
        : m_records(records), m_frames(frames), m_first(first), m_last(last), m_hash(HashSeed)
    {
#ifdef _WIN32
        m_shadow = static_cast<uint8_t*>(VirtualAlloc(nullptr, ShadowBytes, MEM_RESERVE, PAGE_READWRITE));
        if (!m_shadow)
            Refuse("cannot reserve trace shadow");
#else
        void* shadow = mmap(nullptr, ShadowBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
        if (shadow == MAP_FAILED)
            Refuse("cannot reserve trace shadow");
        m_shadow = static_cast<uint8_t*>(shadow);
#endif
        uint8_t header[12] = { 'S', 'K', 'Y', 'T', 'R', 'C', '0', '1' };
        auto* bytes = header + 8;
        Encode(bytes, 1, 4);
        Write(m_records, header, sizeof(header));
    }

    Trace::~Trace()
    {
#ifdef _WIN32
        VirtualFree(m_shadow, 0, MEM_RELEASE);
#else
        munmap(m_shadow, ShadowBytes);
#endif
    }

    uint8_t* Trace::Shadow(uint32_t address, uint8_t size)
    {
        if (size == 0 || size > 8 || uint64_t(address) + size > ShadowBytes)
            Refuse("trace access outside guest memory");
#ifdef _WIN32
        // Commit whole reservation granules so even a load crossing a page needs no host heap allocation.
        const uint32_t first = address >> 16, last = uint32_t((uint64_t(address) + size - 1) >> 16);
        for (uint32_t page = first; page <= last; ++page)
        {
            if (m_committed[page])
                continue;
            if (!VirtualAlloc(m_shadow + uint64_t(page) * 65536, 65536, MEM_COMMIT, PAGE_READWRITE))
                Refuse("cannot commit trace shadow");
            m_committed[page] = 1;
        }
#endif
        return m_shadow + address;
    }

    void Trace::Remember(uint32_t address, const void* bytes, uint8_t size)
    {
        std::memcpy(Shadow(address, size), bytes, size);
    }

    void Trace::Observe(uint32_t address, const void* bytes, uint8_t size, uint32_t lr, uint32_t frame)
    {
        auto* shadow = Shadow(address, size);
        if (std::memcmp(shadow, bytes, size) == 0)
            return;
        const uint64_t before = Value(shadow, size), value = Value(bytes, size);
        std::memcpy(shadow, bytes, size);
        // Shutdown runs on the host main thread, outside the guest's cooperative baton.
        std::lock_guard lock(m_outputMutex);
        if (m_finished)
            return;
        ++m_sequence;
        ++m_count;
        m_hash = Mix(Mix(Mix(m_hash, address), size), value);
        if (frame < m_first || frame > m_last)
            return;
        uint8_t record[37];
        auto* out = record;
        Encode(out, m_sequence, 8);
        Encode(out, frame, 4);
        Encode(out, address, 4);
        Encode(out, size, 1);
        Encode(out, value, 8);
        Encode(out, before, 8);
        Encode(out, lr, 4);
        Write(m_records, record, sizeof(record));
    }

    void Trace::EndFrame(uint32_t frame)
    {
        std::lock_guard lock(m_outputMutex);
        FlushFrame(frame);
    }

    void Trace::FlushFrame(uint32_t frame)
    {
        if (m_finished)
            return;
        if (fprintf(m_frames, "frame %u %llu %016llx\n", frame, static_cast<unsigned long long>(m_count),
                static_cast<unsigned long long>(m_hash)) < 0 ||
            fflush(m_records) != 0 || fflush(m_frames) != 0)
            Refuse("trace frame flush failed");
        m_count = 0;
        m_hash = HashSeed;
    }

    void Trace::Finish(uint32_t frame)
    {
        std::lock_guard lock(m_outputMutex);
        FlushFrame(frame);
        m_finished = true;
    }

    void FinishTrace(uint32_t frame)
    {
        std::lock_guard lock(g_finishMutex);
        if (!g_records)
            return;
        g_trace->Finish(frame);
        if (fclose(g_records.release()) != 0 || fclose(g_frames.release()) != 0)
            Refuse("trace close failed");
    }
}
