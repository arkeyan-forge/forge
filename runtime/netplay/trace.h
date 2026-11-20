#pragma once

#include <array>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <string_view>
#include <type_traits>

namespace netplay
{
    bool ParseTraceFrames(std::string_view text, uint32_t& first, uint32_t& last);
    uint32_t ProbeFrame();

    class Trace
    {
    public:
        Trace(FILE* records, FILE* frames, uint32_t first = 0, uint32_t last = UINT32_MAX);
        ~Trace();
        Trace(const Trace&) = delete;
        Trace& operator=(const Trace&) = delete;
        void Observe(uint32_t address, const void* bytes, uint8_t size, uint32_t lr, uint32_t frame);
        void Remember(uint32_t address, const void* bytes, uint8_t size);
        void EndFrame(uint32_t frame);
        void Finish(uint32_t frame);
        uint64_t Count() const { return m_count; }
        uint64_t Hash() const { return m_hash; }

    private:
        uint8_t* Shadow(uint32_t address, uint8_t size);
        void FlushFrame(uint32_t frame);
        uint8_t* m_shadow = nullptr;
        [[maybe_unused]] std::array<uint8_t, 65536> m_committed{};
        FILE* m_records;
        FILE* m_frames;
        uint32_t m_first, m_last;
        uint64_t m_sequence = 0, m_count = 0, m_hash;
        bool m_finished = false;
        std::mutex m_outputMutex;
    };

    extern Trace* const g_trace;
    void FinishTrace(uint32_t frame);

    template <typename T>
    inline T GuestEndian(T value)
    {
        static_assert(std::is_unsigned_v<T>);
        if constexpr (sizeof(T) == 1)
            return value;
        else if constexpr (sizeof(T) == 2)
            return __builtin_bswap16(value);
        else if constexpr (sizeof(T) == 4)
            return __builtin_bswap32(value);
        else
        {
            static_assert(sizeof(T) == 8);
            return __builtin_bswap64(value);
        }
    }

    template <typename T>
    inline T Load(const uint8_t* base, uint32_t address, uint32_t lr, Trace* trace, uint32_t frame)
    {
        const T raw = *reinterpret_cast<const volatile T*>(base + address);
        if (trace)
            trace->Observe(address, &raw, sizeof(T), lr, frame);
        return GuestEndian(raw);
    }

    template <typename T>
    inline void Store(uint8_t* base, uint32_t address, T value, Trace* trace)
    {
        const T raw = GuestEndian(value);
        *reinterpret_cast<volatile T*>(base + address) = raw;
        if (trace)
            trace->Remember(address, &raw, sizeof(T));
    }
}
