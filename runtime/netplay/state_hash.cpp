#include "netplay/state_hash.h"
#include <algorithm>
#include <charconv>
#include <cstring>

namespace netplay
{
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

    void HashAllocation(std::span<const uint8_t> bytes, uint32_t address, Heap heap, std::span<const Range> skips,
        std::vector<Block>& blocks, uint32_t blockBytes)
    {
        size_t offset = 0;
        while (offset < bytes.size())
        {
            const uint64_t at = uint64_t(address) + offset;
            const size_t size = std::min(bytes.size() - offset, size_t(blockBytes - (at & (blockBytes - 1))));
            const bool excluded = std::any_of(skips.begin(), skips.end(),
                [=](const Range& range) { return range.begin <= at && range.end >= at + size; });
            if (!excluded)
                blocks.push_back({ uint32_t(at), heap, HashBytes(bytes.subspan(offset, size), at, skips) });
            offset += size;
        }
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
