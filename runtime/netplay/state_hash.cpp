#include "netplay/state_hash.h"
#include <algorithm>
#include <thread>
#include <charconv>
#include <cstring>

namespace netplay
{
    namespace
    {
        template <typename F>
        size_t Included(size_t size, uint32_t address, std::span<const Range> skips, F&& visit)
        {
            uint64_t at = address, end = at + size;
            size_t count = 0;
            const auto segment = [&](uint64_t stop) {
                if (at < stop)
                {
                    const auto length = size_t(stop - at);
                    visit(size_t(at - address), length, count);
                    count += length;
                }
            };
            for (const auto& range : skips)
            {
                if (range.end <= at)
                    continue;
                if (range.begin >= end)
                    break;
                segment(std::min(range.begin, end));
                at = std::max(at, range.end);
            }
            segment(end);
            return count;
        }
    }
    void PackBlock(
        std::span<const uint8_t> memory, uint32_t address, std::span<const Range> skips, std::vector<uint8_t>& bytes)
    {
        bytes.clear();
        Included(memory.size(), address, skips, [&](size_t offset, size_t length, size_t) {
            const auto part = memory.subspan(offset, length);
            bytes.insert(bytes.end(), part.begin(), part.end());
        });
    }
    bool ApplyBlock(
        std::span<uint8_t> memory, uint32_t address, std::span<const Range> skips, std::span<const uint8_t> bytes)
    {
        if (Included(memory.size(), address, skips, [](size_t, size_t, size_t) {}) != bytes.size())
            return false;
        Included(memory.size(), address, skips, [&](size_t offset, size_t length, size_t packed) {
            std::memcpy(memory.data() + offset, bytes.data() + packed, length);
        });
        return true;
    }
    uint64_t Mix(uint64_t hash, uint64_t word)
    {
        hash ^= word;
        hash *= 0xD6E8FEB86659FD93ull;
        return hash ^ (hash >> 29);
    }

    uint64_t HashBytes(std::span<const uint8_t> bytes, uint64_t address, std::span<const Range> skips)
    {
        // Hundreds of megabytes per frame: whole words between the skipped ranges (ParseSkips sorts and merges them).
        uint64_t hash = HashSeed, count = 0;
        const uint64_t end = address + bytes.size();
        const auto segment = [&](uint64_t begin, uint64_t stop) {
            if (begin >= stop)
                return;
            hash = Mix(hash, begin);
            const uint8_t* data = bytes.data() + (begin - address);
            const size_t size = size_t(stop - begin);
            size_t i = 0;
            // One chain waits for each multiply: four chains side by side hash about four times faster.
            if (size >= 32)
            {
                uint64_t lanes[4] = { hash, hash ^ 1, hash ^ 2, hash ^ 3 };
                for (; i + 32 <= size; i += 32)
                {
                    uint64_t words[4];
                    std::memcpy(words, data + i, 32);
                    for (size_t lane = 0; lane < 4; ++lane)
                        lanes[lane] = Mix(lanes[lane], words[lane]);
                }
                for (const uint64_t lane : lanes)
                    hash = Mix(hash, lane);
            }
            for (; i + 8 <= size; i += 8)
            {
                uint64_t word;
                std::memcpy(&word, data + i, 8);
                hash = Mix(hash, word);
            }
            if (i < size)
            {
                uint64_t word = 0;
                std::memcpy(&word, data + i, size - i);
                hash = Mix(hash, word);
            }
            count += size;
        };
        uint64_t at = address;
        for (const auto& range : skips)
        {
            if (range.end <= at)
                continue;
            if (range.begin >= end)
                break;
            segment(at, std::min(range.begin, end));
            at = std::max(at, range.end);
        }
        segment(at, end);
        return Mix(hash, count);
    }

    void HashAllocations(std::span<const Allocation> allocations, std::span<const Range> skips,
        std::vector<Block>& blocks, uint32_t blockBytes, std::vector<uint64_t>* ends)
    {
        // The blocks first, then their hashes, block by block on several threads: the result is the same whatever the
        // number of threads.
        struct Extent
        {
            size_t allocation, offset, size;
        };
        std::vector<Extent> extents;
        size_t total = 0;
        for (size_t index = 0; index < allocations.size(); ++index)
        {
            const auto& allocation = allocations[index];
            size_t offset = 0;
            while (offset < allocation.bytes.size())
            {
                const uint64_t at = uint64_t(allocation.address) + offset;
                const size_t size =
                    std::min(allocation.bytes.size() - offset, size_t(blockBytes - (at & (blockBytes - 1))));
                const bool excluded = std::any_of(skips.begin(), skips.end(),
                    [=](const Range& range) { return range.begin <= at && range.end >= at + size; });
                if (!excluded)
                {
                    extents.push_back({ index, offset, size });
                    total += size;
                }
                offset += size;
            }
        }
        const size_t first = blocks.size();
        blocks.resize(first + extents.size());
        if (ends)
            ends->resize(first + extents.size());
        const auto hash = [&](size_t begin, size_t end) {
            for (size_t i = begin; i < end; ++i)
            {
                const auto& extent = extents[i];
                const auto& allocation = allocations[extent.allocation];
                const uint64_t at = uint64_t(allocation.address) + extent.offset;
                blocks[first + i] = { uint32_t(at), allocation.heap,
                    HashBytes(allocation.bytes.subspan(extent.offset, extent.size), at, skips) };
                if (ends)
                    (*ends)[first + i] = at + extent.size;
            }
        };
        constexpr size_t ThreadedBytes = 4 << 20;
        const size_t threads = total >= ThreadedBytes ? std::min<size_t>(HashThreads, extents.size()) : 1;
        if (threads <= 1)
        {
            hash(0, extents.size());
            return;
        }
        // Shares of equal bytes, not of equal block counts: the image's sections end in short blocks.
        std::vector<size_t> bounds = { 0 };
        size_t done = 0;
        for (size_t i = 0; i < extents.size() && bounds.size() < threads; ++i)
        {
            done += extents[i].size;
            if (done * threads >= total * bounds.size())
                bounds.push_back(i + 1);
        }
        while (bounds.size() <= threads)
            bounds.push_back(extents.size());
        std::vector<std::thread> workers;
        workers.reserve(threads - 1);
        for (size_t t = 1; t < threads; ++t)
            workers.emplace_back(hash, bounds[t], bounds[t + 1]);
        hash(bounds[0], bounds[1]);
        for (auto& worker : workers)
            worker.join();
    }

    uint64_t CombineBlocks(std::span<const Block> blocks)
    {
        uint64_t total = HashSeed;
        for (const auto& block : blocks)
        {
            total = Mix(total, (uint64_t(block.address) << 32) | uint32_t(block.heap));
            total = Mix(total, block.hash);
        }
        return total;
    }

    void SliceAllocations(
        std::span<const Allocation> allocations, uint32_t blockBytes, std::span<std::vector<Allocation>> parts)
    {
        for (auto& part : parts)
            part.clear();
        if (parts.empty())
            return;
        uint64_t total = 0;
        for (const auto& allocation : allocations)
            total += allocation.bytes.size();
        const uint64_t share = total / parts.size() + 1;
        size_t part = 0;
        uint64_t filled = 0;
        for (const auto& allocation : allocations)
        {
            size_t offset = 0;
            while (offset < allocation.bytes.size())
            {
                const uint64_t at = uint64_t(allocation.address) + offset;
                const uint64_t left = allocation.bytes.size() - offset;
                uint64_t take = part + 1 < parts.size() ? std::min(left, share - filled) : left;
                if (take < left)
                {
                    const uint64_t mask = ~uint64_t(blockBytes - 1);
                    const uint64_t end = std::max((at + take) & mask, (at & mask) + blockBytes);
                    take = std::min(end - at, left);
                }
                parts[part].push_back(
                    { allocation.bytes.subspan(offset, size_t(take)), uint32_t(at), allocation.heap });
                offset += size_t(take);
                filled += take;
                if (filled >= share && part + 1 < parts.size())
                {
                    ++part;
                    filled = 0;
                }
            }
        }
    }

    void HashAllocation(std::span<const uint8_t> bytes, uint32_t address, Heap heap, std::span<const Range> skips,
        std::vector<Block>& blocks, uint32_t blockBytes)
    {
        const Allocation allocation{ bytes, address, heap };
        HashAllocations(std::span(&allocation, 1), skips, blocks, blockBytes);
    }

    bool ParseCount(std::string_view text, uint64_t min, uint64_t max, bool powerOfTwo, uint64_t& value)
    {
        if (text.empty() || text.size() > 12)
            return false;
        uint64_t parsed = 0;
        for (const char c : text)
        {
            if (c < '0' || c > '9')
                return false;
            parsed = parsed * 10 + uint64_t(c - '0');
        }
        if (parsed < min || parsed > max || (powerOfTwo && (parsed & (parsed - 1)) != 0))
            return false;
        value = parsed;
        return true;
    }

    bool AddSkip(std::vector<Range>& ranges, Range range)
    {
        if (range.begin >= range.end)
            return false;
        auto at =
            std::lower_bound(ranges.begin(), ranges.end(), range, [](Range a, Range b) { return a.end < b.begin; });
        if (at != ranges.end() && at->begin <= range.begin && range.end <= at->end)
            return false;
        // Every range that touches the new one is folded into it.
        auto last = at;
        while (last != ranges.end() && last->begin <= range.end)
        {
            range.begin = std::min(range.begin, last->begin);
            range.end = std::max(range.end, last->end);
            ++last;
        }
        at = ranges.erase(at, last);
        ranges.insert(at, range);
        return true;
    }

    bool ParseSkips(std::string_view text, std::vector<Range>& ranges, std::string& error)
    {
        ranges.clear();
        if (text.empty())
            return true;
        while (!text.empty())
        {
            const size_t comma = text.find(',');
            const auto item = text.substr(0, comma);
            const size_t dash = item.find('-');
            uint64_t begin = 0, end = 0;
            const auto parse = [](std::string_view value, uint64_t& result) {
                if (value.starts_with("0x") || value.starts_with("0X"))
                    value.remove_prefix(2);
                const auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), result, 16);
                return ec == std::errc{} && ptr == value.data() + value.size();
            };
            if (dash == std::string_view::npos || !parse(item.substr(0, dash), begin) ||
                !parse(item.substr(dash + 1), end) || begin >= end || end > 0x100000000ull)
            {
                ranges.clear();
                error = "invalid hash skip range (expected hex start-end, end exclusive)";
                return false;
            }
            ranges.push_back({ begin, end });
            if (comma == std::string_view::npos)
                break;
            text.remove_prefix(comma + 1);
            if (text.empty())
            {
                ranges.clear();
                error = "empty hash skip range";
                return false;
            }
        }
        std::sort(ranges.begin(), ranges.end(), [](Range a, Range b) { return a.begin < b.begin; });
        size_t count = 0;
        for (const auto range : ranges)
        {
            if (count && ranges[count - 1].end >= range.begin)
                ranges[count - 1].end = std::max(ranges[count - 1].end, range.end);
            else
                ranges[count++] = range;
        }
        ranges.resize(count);
        error.clear();
        return true;
    }

    bool WritableSections(std::span<const uint8_t> image, uint32_t base, std::vector<Range>& ranges, std::string& error,
        uint64_t mappedSize)
    {
        ranges.clear();
        if (!mappedSize)
            mappedSize = image.size();
        const auto get = [&](size_t at, size_t count) {
            uint64_t value = 0;
            for (size_t i = 0; i < count; ++i)
                value |= uint64_t(image[at + i]) << (i * 8);
            return value;
        };
        const auto fail = [&] {
            ranges.clear();
            error = "invalid mapped PE section table";
            return false;
        };
        if (image.size() < 64 || get(0, 2) != 0x5A4D)
            return fail();
        const uint64_t pe = get(0x3C, 4);
        if (pe + 24 > image.size() || get(size_t(pe), 4) != 0x4550)
            return fail();
        const uint64_t count = get(size_t(pe + 6), 2);
        const uint64_t table = pe + 24 + get(size_t(pe + 20), 2);
        if (count > 4096 || table + count * 40 > image.size())
            return fail();
        for (uint64_t i = 0; i < count; ++i)
        {
            const size_t at = size_t(table + i * 40);
            const uint64_t size = get(at + 8, 4), rva = get(at + 12, 4);
            if (uint64_t(base) + rva + size > 0x100000000ull)
                return fail();
            // A section may run past the mapped image (Giants' last, read-only one): only its mapped bytes exist.
            const uint64_t end = std::min(rva + size, mappedSize);
            if (rva < end && (get(at + 36, 4) & 0x80000000u))
                ranges.push_back({ uint64_t(base) + rva, uint64_t(base) + end });
        }
        std::sort(ranges.begin(), ranges.end(), [](Range a, Range b) { return a.begin < b.begin; });
        for (size_t i = 1; i < ranges.size(); ++i)
            if (ranges[i - 1].end > ranges[i].begin)
                return fail();
        error.clear();
        return true;
    }
}
