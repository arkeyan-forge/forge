#pragma once

#include <atomic>
#include <cstdint>

namespace netplay
{
    inline constexpr uint64_t TimeBaseFrequency = 49875000;
    inline constexpr uint64_t EpochFileTime = 134116992000000000;

    extern const bool g_enabled;
    inline bool Enabled() { return g_enabled; }

    // The write time of a file on the host (a save, a figure), in FILETIME at guest time `now`, `age` 100 ns intervals
    // after the host wrote it. Deterministic mode: the session's start, since the age follows the host's wall clock.
    inline uint64_t HostFileTime(bool deterministic, uint64_t now, uint64_t age)
    {
        return deterministic ? EpochFileTime : now - age;
    }

    // The bytes a deterministic run gives for the title's random requests (XNetRandom): SplitMix64 from a fixed seed, so
    // both computers, and a replay, give the same ones in the same order.
    inline constexpr uint64_t DeterministicRandomSeed = 0x9E3779B97F4A7C15ull;
    inline uint8_t DeterministicByte(uint64_t& state)
    {
        uint64_t z = (state += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return uint8_t(z ^ (z >> 31));
    }

    // The time zone the title is told about (it stamps its saves with it): the host's, or UTC in deterministic mode, since
    // two players' zones differ. `bias` in minutes to add to local time to get UTC; `name` four characters.
    inline int32_t TitleTimeZoneBias(bool deterministic, int32_t hostBias) { return deterministic ? 0 : hostBias; }
    inline const char* TitleTimeZoneName(bool deterministic, const char* hostName)
    {
        return deterministic ? "UTC" : hostName;
    }

    // The storage the title is told about (XamContentGetDeviceData, NtQueryVolumeInformationFile): the host disk's, or in
    // deterministic mode a fixed 120 GB drive with 100 GB free: two PCs' disks differ, and the title keeps what it reads.
    struct DiskSpace
    {
        uint64_t capacity = 0, available = 0;
    };
    inline DiskSpace TitleDiskSpace(bool deterministic, uint64_t capacity, uint64_t available)
    {
        return deterministic ? DiskSpace{ 120ull << 30, 100ull << 30 } : DiskSpace{ capacity, available };
    }

    struct ReadCursor
    {
        uint64_t frame = UINT64_MAX;
        uint64_t reads = 0;
        uint64_t last = 0;
        uint64_t Read(uint64_t steps);
    };

    class Clock
    {
    public:
        void Advance(uint32_t presentInterval);
        uint64_t Steps() const { return m_steps.load(std::memory_order_acquire); }
        uint64_t Elapsed() const;
        void AdvanceTo(uint64_t elapsed);
        void SetAdvanceHook(void (*hook)()) { m_advanceHook.store(hook, std::memory_order_release); }
        uint64_t Idle() const { return m_idle.load(std::memory_order_acquire); }
        uint64_t SystemTime() const { return EpochFileTime + Elapsed(); }

    private:
        std::atomic<uint64_t> m_steps{ 0 };
        std::atomic<uint64_t> m_idle{ 0 };
        std::atomic<void (*)()> m_advanceHook{ nullptr };
    };

    extern Clock g_clock;
    uint64_t ReadTimeBase();
    void OnFrame(uint32_t presentInterval);
    void Finish();
    void SetTimeStampBundle(uint32_t address);
}
