#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace netplay
{
    struct Range
    {
        uint64_t begin, end;
    };
    enum class Heap : uint32_t
    {
        Virtual4K = 1,
        Virtual64K = 2,
        Physical = 3,
        Image = 4
    };
    struct Block
    {
        uint32_t address;
        Heap heap;
        uint64_t hash;
        bool operator==(const Block&) const = default;
    };
    class BlockSource
    {
    public:
        virtual ~BlockSource() = default;
        virtual std::span<const Block> ListBlocks() = 0;
        // Bytes are packed in address order, excluding the same ranges as the hash.
        virtual bool ReadBlock(size_t index, std::vector<uint8_t>& bytes) = 0;
        virtual bool WriteBlock(size_t index, std::span<const uint8_t> bytes) = 0;
        virtual uint64_t TotalHash() = 0;
    };
    inline constexpr uint64_t HashSeed = 0x6A09E667F3BCC909ull;
    uint64_t Mix(uint64_t hash, uint64_t word);
    uint64_t HashBytes(std::span<const uint8_t> bytes, uint64_t address, std::span<const Range> skips);
    void PackBlock(
        std::span<const uint8_t> memory, uint32_t address, std::span<const Range> skips, std::vector<uint8_t>& bytes);
    bool ApplyBlock(
        std::span<uint8_t> memory, uint32_t address, std::span<const Range> skips, std::span<const uint8_t> bytes);
    struct Allocation
    {
        std::span<const uint8_t> bytes;
        uint32_t address;
        Heap heap;
    };
    // Threads hashing 4 MB or more (the guest is held meanwhile: they only read).
    inline constexpr size_t HashThreads = 4;
    // Appends the blocks of `allocations`, in order, and with `ends` each block's end address.
    void HashAllocations(std::span<const Allocation> allocations, std::span<const Range> skips,
        std::vector<Block>& blocks, uint32_t blockBytes = 0x10000, std::vector<uint64_t>* ends = nullptr);
    void HashAllocation(std::span<const uint8_t> bytes, uint32_t address, Heap heap, std::span<const Range> skips,
        std::vector<Block>& blocks, uint32_t blockBytes = 0x10000);
    // A decimal count in [min, max] (a power of two when asked): the probe's block size and sampling period.
    bool ParseCount(std::string_view text, uint64_t min, uint64_t max, bool powerOfTwo, uint64_t& value);
    bool ParseSkips(std::string_view text, std::vector<Range>& ranges, std::string& error);
    bool WritableSections(std::span<const uint8_t> image, uint32_t base, std::vector<Range>& ranges, std::string& error,
        uint64_t mappedSize = 0);
}
