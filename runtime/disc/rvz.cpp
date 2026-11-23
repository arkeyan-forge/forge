#include "disc/rvz.h"
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <zstd.h>

namespace disc
{
    namespace
    {
        constexpr uint32_t Magic = 0x52565A01; // "RVZ\1"
        constexpr uint32_t Header1Size = 0x48;
        constexpr uint32_t Header2Size = 0xDC;
        constexpr uint32_t PartitionEntrySize = 0x30;
        constexpr uint32_t RawEntrySize = 0x18;
        constexpr uint32_t GroupEntrySize = 0x0C;
        constexpr uint32_t CompressionNone = 0, CompressionZstd = 5;
        constexpr uint32_t ExceptionSize = 2 + 20;
        constexpr uint32_t HashGroupSectors = 64; // one exception list covers at most this many sectors
        // Bounds no real disc comes near: a damaged or crafted image is refused rather than read out of bounds.
        constexpr uint64_t MaxDiscSize = 16ull << 30;
        constexpr uint32_t MaxPartitions = 64;
        constexpr uint32_t MaxRawEntries = 4096;
        constexpr uint32_t MaxChunkSize = 64u << 20;

        uint16_t Be16(const uint8_t* p) { return uint16_t(p[0] << 8 | p[1]); }
        uint32_t Be32(const uint8_t* p)
        {
            return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3];
        }
        uint64_t Be64(const uint8_t* p) { return uint64_t(Be32(p)) << 32 | Be32(p + 4); }

        // The discs' padding generator: a lagged Fibonacci generator (lags 521 and 32) seeded with 17 words. An RVZ
        // keeps the seed of each run of padding and where in the generator's output the run starts.
        class Junk
        {
        public:
            explicit Junk(const uint8_t* seed)
            {
                for (size_t i = 0; i < SeedWords; i++)
                    m_words[i] = Be32(seed + 4 * i);
                for (size_t i = SeedWords; i < Long; i++)
                    m_words[i] = (m_words[i - 17] << 23) ^ (m_words[i - 16] >> 9) ^ m_words[i - 1];
                for (int i = 0; i < 4; i++)
                    Step();
            }

            void Skip(uint64_t bytes)
            {
                m_position += bytes;
                while (m_position >= BufferBytes)
                {
                    Step();
                    m_position -= BufferBytes;
                }
            }

            void Fill(uint8_t* out, uint64_t bytes)
            {
                for (uint64_t i = 0; i < bytes; i++)
                {
                    const uint32_t word = m_words[m_position / 4];
                    // The bytes of a word, high first; the second one is taken from bit 18, not 16.
                    static constexpr int Shift[4] = { 24, 18, 8, 0 };
                    out[i] = uint8_t(word >> Shift[m_position % 4]);
                    if (++m_position == BufferBytes)
                    {
                        Step();
                        m_position = 0;
                    }
                }
            }

        private:
            static constexpr size_t SeedWords = 17, Long = 521, Short = 32, BufferBytes = Long * 4;
            uint32_t m_words[Long];
            uint64_t m_position = 0;

            void Step()
            {
                for (size_t i = 0; i < Short; i++)
                    m_words[i] ^= m_words[i + Long - Short];
                for (size_t i = Short; i < Long; i++)
                    m_words[i] ^= m_words[i - Short];
            }
        };
    }

    // A run of padding restarts the generator every JunkBlock bytes of the data it fills.
    static constexpr uint64_t JunkBlock = 0x8000;

    RvzImage::~RvzImage() { Close(); }

    void RvzImage::Close()
    {
        if (m_file != InvalidPlatformFile)
            PlatformCloseFile(m_file);
        m_file = InvalidPlatformFile;
        if (m_zstd != nullptr)
            ZSTD_freeDCtx(static_cast<ZSTD_DCtx*>(m_zstd));
        m_zstd = nullptr;
        m_partitions.clear();
        m_raw.clear();
        m_groups.clear();
        m_cache.clear();
    }

    bool RvzImage::IsRvz(const char* path)
    {
        PlatformFile file = PlatformOpenFile(path);
        if (file == InvalidPlatformFile)
            return false;
        uint8_t magic[4];
        const bool rvz = PlatformReadFile(file, magic, 4, 0) == 4 && Be32(magic) == Magic;
        PlatformCloseFile(file);
        return rvz;
    }

    bool RvzImage::ReadFile(uint64_t offset, void* out, uint64_t size) const
    {
        if (offset > m_fileSize || size > m_fileSize - offset)
            return false;
        uint8_t* bytes = static_cast<uint8_t*>(out);
        while (size != 0)
        {
            const uint32_t piece = uint32_t(std::min<uint64_t>(size, 1u << 30));
            if (PlatformReadFile(m_file, bytes, piece, offset) != int64_t(piece))
                return false;
            bytes += piece;
            offset += piece;
            size -= piece;
        }
        return true;
    }

    bool RvzImage::Decompress(const std::vector<uint8_t>& in, std::vector<uint8_t>& out, size_t expected) const
    {
        if (m_zstd == nullptr)
            m_zstd = ZSTD_createDCtx();
        if (m_zstd == nullptr)
            return false;
        const unsigned long long declared = ZSTD_getFrameContentSize(in.data(), in.size());
        if (declared == ZSTD_CONTENTSIZE_ERROR || (declared != ZSTD_CONTENTSIZE_UNKNOWN && declared > expected))
            return false;
        out.resize(expected);
        const size_t got =
            ZSTD_decompressDCtx(static_cast<ZSTD_DCtx*>(m_zstd), out.data(), out.size(), in.data(), in.size());
        if (ZSTD_isError(got))
            return false;
        out.resize(got);
        return true;
    }

    bool RvzImage::ReadEntries(uint64_t offset, uint32_t storedSize, size_t count, size_t entrySize,
        std::vector<uint8_t>& out, std::string& error) const
    {
        const size_t size = count * entrySize;
        std::vector<uint8_t> stored(storedSize);
        if (!ReadFile(offset, stored.data(), storedSize))
        {
            error = "a table runs past the end of the file";
            return false;
        }
        if (m_compression == CompressionNone)
            out = std::move(stored);
        else if (!Decompress(stored, out, size))
        {
            error = "a table does not decompress";
            return false;
        }
        if (out.size() < size)
        {
            error = "a table is shorter than its entries";
            return false;
        }
        return true;
    }

    bool RvzImage::Open(const char* path, std::string& error)
    {
        Close();
        m_file = PlatformOpenFile(path);
        if (m_file == InvalidPlatformFile)
        {
            error = "the file cannot be opened";
            return false;
        }
        std::error_code sizeError;
        m_fileSize = std::filesystem::file_size(
            std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(path))), sizeError);
        if (sizeError)
            m_fileSize = 0;

        uint8_t header1[Header1Size], header2[Header2Size];
        if (!ReadFile(0, header1, sizeof(header1)) || Be32(header1) != Magic)
        {
            error = "not an RVZ image";
            Close();
            return false;
        }
        // Version 1.0 is the first; 0.3 the oldest a reader of 1.0 may read.
        const uint32_t compatible = Be32(header1 + 8);
        const uint32_t header2Size = Be32(header1 + 12);
        if (compatible > 0x01000000 || header2Size < Header2Size ||
            Sha1::Of(header1, 0x34) != *reinterpret_cast<const Sha1Digest*>(header1 + 0x34))
        {
            error = "the RVZ's header is damaged or from a newer version";
            Close();
            return false;
        }
        std::vector<uint8_t> fullHeader2(header2Size);
        if (!ReadFile(Header1Size, fullHeader2.data(), header2Size) ||
            Sha1::Of(fullHeader2.data(), header2Size) != *reinterpret_cast<const Sha1Digest*>(header1 + 16))
        {
            error = "the RVZ's header is damaged";
            Close();
            return false;
        }
        memcpy(header2, fullHeader2.data(), sizeof(header2));
        m_discSize = Be64(header1 + 0x24);
        m_discType = Be32(header2);
        m_compression = Be32(header2 + 4);
        m_chunkSize = Be32(header2 + 12);
        memcpy(m_discHeader, header2 + 16, sizeof(m_discHeader));
        if (m_compression != CompressionNone && m_compression != CompressionZstd)
        {
            error = "the RVZ is compressed with bzip2 or LZMA; only Zstandard is read";
            Close();
            return false;
        }
        // RVZ chunks: 32 KiB to 2 MiB, a power of two; larger ones are whole multiples of 2 MiB.
        const bool powerOfTwo = (m_chunkSize & (m_chunkSize - 1)) == 0;
        if (m_chunkSize < WiiSectorSize || m_chunkSize > MaxChunkSize ||
            (m_chunkSize <= 0x200000 ? !powerOfTwo : m_chunkSize % 0x200000 != 0) || m_discSize > MaxDiscSize ||
            (m_discType != 1 && m_discType != 2))
        {
            error = "the RVZ's header is damaged";
            Close();
            return false;
        }

        const uint32_t partitionCount = Be32(header2 + 0x90);
        const uint32_t partitionEntrySize = Be32(header2 + 0x94);
        const uint64_t partitionOffset = Be64(header2 + 0x98);
        const uint32_t rawCount = Be32(header2 + 0xB4);
        const uint64_t rawOffset = Be64(header2 + 0xB8);
        const uint32_t rawSize = Be32(header2 + 0xC0);
        const uint32_t groupCount = Be32(header2 + 0xC4);
        const uint64_t groupOffset = Be64(header2 + 0xC8);
        const uint32_t groupSize = Be32(header2 + 0xD0);
        const uint64_t maxGroups = m_discSize / m_chunkSize + MaxRawEntries + 2 * MaxPartitions + 1;
        if (partitionCount > MaxPartitions || (partitionCount != 0 && partitionEntrySize < PartitionEntrySize) ||
            rawCount > MaxRawEntries || groupCount > maxGroups)
        {
            error = "the RVZ's tables are damaged";
            Close();
            return false;
        }

        // The partitions: stored as they are, checked by the hash in the header.
        std::vector<uint8_t> partitions(size_t(partitionCount) * partitionEntrySize);
        if (!ReadFile(partitionOffset, partitions.data(), partitions.size()) ||
            Sha1::Of(partitions.data(), partitions.size()) != *reinterpret_cast<const Sha1Digest*>(header2 + 0xA0))
        {
            error = "the RVZ's partition table is damaged";
            Close();
            return false;
        }
        for (uint32_t i = 0; i < partitionCount; i++)
        {
            const uint8_t* entry = partitions.data() + size_t(i) * partitionEntrySize;
            Partition partition;
            for (int r = 0; r < 2; r++)
            {
                const uint8_t* range = entry + 16 + 16 * r;
                partition.ranges[r] = { Be32(range), Be32(range + 4), Be32(range + 8), Be32(range + 12) };
            }
            partition.firstSector = partition.ranges[0].firstSector;
            partition.sectors = partition.ranges[0].sectors + uint64_t(partition.ranges[1].sectors);
            const uint32_t sectorsPerGroup = m_chunkSize / WiiSectorSize;
            const bool joined = partition.ranges[1].sectors == 0 ||
                                uint64_t(partition.ranges[0].firstSector) + partition.ranges[0].sectors ==
                                    partition.ranges[1].firstSector;
            bool fits = joined && (partition.firstSector + partition.sectors) * WiiSectorSize <= m_discSize;
            for (const Partition::Range& range : partition.ranges)
                fits = fits && range.groups == (range.sectors + sectorsPerGroup - 1) / sectorsPerGroup &&
                       uint64_t(range.firstGroup) + range.groups <= groupCount;
            if (!fits)
            {
                error = "the RVZ's partition table is damaged";
                Close();
                return false;
            }
            m_partitions.push_back(partition);
        }

        std::vector<uint8_t> raw, groups;
        if (!ReadEntries(rawOffset, rawSize, rawCount, RawEntrySize, raw, error) ||
            !ReadEntries(groupOffset, groupSize, groupCount, GroupEntrySize, groups, error))
        {
            Close();
            return false;
        }
        for (uint32_t i = 0; i < rawCount; i++)
        {
            const uint8_t* entry = raw.data() + size_t(i) * RawEntrySize;
            const uint64_t offset = Be64(entry), size = Be64(entry + 8);
            RawRange range{ offset - offset % WiiSectorSize, offset + size, Be32(entry + 16), Be32(entry + 20) };
            if (size > m_discSize || offset > m_discSize - size ||
                range.groups != (range.end - range.start + m_chunkSize - 1) / m_chunkSize ||
                uint64_t(range.firstGroup) + range.groups > groupCount)
            {
                error = "the RVZ's table of plain regions is damaged";
                Close();
                return false;
            }
            m_raw.push_back(range);
        }
        for (uint32_t i = 0; i < groupCount; i++)
        {
            const uint8_t* entry = groups.data() + size_t(i) * GroupEntrySize;
            const uint32_t size = Be32(entry + 4);
            m_groups.push_back({ uint64_t(Be32(entry)) << 2, size & 0x7FFFFFFF, (size >> 31) != 0, Be32(entry + 8) });
            if (m_groups.back().fileOffset + m_groups.back().size > m_fileSize)
            {
                error = "the RVZ is cut short (a group runs past the end of the file)";
                Close();
                return false;
            }
        }
        return true;
    }

    std::shared_ptr<const RvzImage::Group> RvzImage::DecodeGroup(
        uint32_t index, uint64_t size, uint64_t dataOffset, uint32_t sectors) const
    {
        auto group = std::make_shared<Group>();
        group->index = index;
        group->sectorExceptions.resize(sectors);
        const GroupEntry& entry = m_groups[index];
        if (entry.size == 0) // a group of zeros
        {
            group->data.assign(size, 0);
            return group;
        }
        std::vector<uint8_t> stored(entry.size), decoded;
        if (!ReadFile(entry.fileOffset, stored.data(), stored.size()))
            return nullptr;
        if (entry.compressed)
        {
            // The exceptions come first: at most 64 sectors' worth of them per list, 22 bytes each.
            const size_t lists = (sectors + HashGroupSectors - 1) / HashGroupSectors;
            const size_t bound = size_t(size) + lists * (2 + size_t(HashGroupSectors) * 31 * 3 * ExceptionSize) + 4096;
            if (!Decompress(stored, decoded, bound))
                return nullptr;
        }
        else
            decoded = std::move(stored);

        // A partition's group starts with one list of hash exceptions per 64 sectors (one at least).
        size_t at = 0;
        if (sectors != 0)
        {
            const size_t lists = std::max<size_t>(1, (sectors + HashGroupSectors - 1) / HashGroupSectors);
            for (size_t list = 0; list < lists; list++)
            {
                if (at + 2 > decoded.size())
                    return nullptr;
                const uint32_t count = Be16(&decoded[at]);
                at += 2;
                if (at + size_t(count) * ExceptionSize > decoded.size())
                    return nullptr;
                for (uint32_t i = 0; i < count; i++, at += ExceptionSize)
                {
                    const uint32_t offset = Be16(&decoded[at]);
                    const size_t sector = list * HashGroupSectors + offset / WiiSectorHashSize;
                    if (sector >= sectors)
                        return nullptr;
                    HashException exception{ offset % WiiSectorHashSize, {} };
                    memcpy(exception.hash.data(), &decoded[at + 2], 20);
                    group->sectorExceptions[sector].push_back(exception);
                }
            }
            if (!entry.compressed)
                at = (at + 3) & ~size_t(3);
        }

        if (entry.packedSize == 0)
        {
            if (decoded.size() < at + size)
                return nullptr;
            group->data.assign(decoded.begin() + std::ptrdiff_t(at), decoded.begin() + std::ptrdiff_t(at + size));
            return group;
        }
        // Packed: runs of data as they are, and runs of padding as the seed of their generator.
        group->data.resize(size);
        uint64_t filled = 0;
        while (filled < size)
        {
            if (at + 4 > decoded.size())
                return nullptr;
            const uint32_t word = Be32(&decoded[at]);
            at += 4;
            const uint64_t length = word & 0x7FFFFFFF;
            if (length > size - filled)
                return nullptr;
            if (word & 0x80000000)
            {
                if (at + 68 > decoded.size())
                    return nullptr;
                Junk junk(&decoded[at]);
                at += 68;
                junk.Skip((dataOffset + filled) % JunkBlock);
                junk.Fill(&group->data[filled], length);
            }
            else
            {
                if (at + length > decoded.size())
                    return nullptr;
                memcpy(&group->data[filled], &decoded[at], length);
                at += length;
            }
            filled += length;
        }
        return group;
    }

    std::shared_ptr<const RvzImage::Group> RvzImage::LoadGroup(
        uint32_t index, uint64_t size, uint64_t dataOffset, uint32_t sectors) const
    {
        std::lock_guard lock(m_mutex);
        for (auto it = m_cache.begin(); it != m_cache.end(); ++it)
            if ((*it)->index == index)
            {
                m_cache.splice(m_cache.begin(), m_cache, it);
                return m_cache.front();
            }
        std::shared_ptr<const Group> group = DecodeGroup(index, size, dataOffset, sectors);
        if (group == nullptr)
            return nullptr;
        m_cache.push_front(group);
        if (m_cache.size() > CacheGroups)
            m_cache.pop_back();
        return group;
    }

    bool RvzImage::ReadRaw(uint64_t offset, void* out, uint64_t size) const
    {
        uint8_t* bytes = static_cast<uint8_t*>(out);
        while (size != 0)
        {
            if (offset < sizeof(m_discHeader))
            {
                const uint64_t take = std::min<uint64_t>(size, sizeof(m_discHeader) - offset);
                memcpy(bytes, m_discHeader + offset, take);
                bytes += take, offset += take, size -= take;
                continue;
            }
            const RawRange* range = nullptr;
            for (const RawRange& candidate : m_raw)
                if (offset >= candidate.start && offset < candidate.end)
                    range = &candidate;
            if (range == nullptr)
                return false;
            const uint64_t inRange = offset - range->start;
            const uint32_t groupInRange = uint32_t(inRange / m_chunkSize);
            const uint64_t groupStart = range->start + uint64_t(groupInRange) * m_chunkSize;
            const uint64_t groupSize = std::min<uint64_t>(m_chunkSize, range->end - groupStart);
            std::shared_ptr<const Group> group = LoadGroup(range->firstGroup + groupInRange, groupSize, groupStart, 0);
            if (group == nullptr)
                return false;
            const uint64_t at = offset - groupStart;
            const uint64_t take = std::min(size, groupSize - at);
            memcpy(bytes, group->data.data() + at, take);
            bytes += take, offset += take, size -= take;
        }
        return true;
    }

    std::shared_ptr<const RvzImage::Group> RvzImage::SectorGroup(size_t index, uint64_t sector, size_t& inGroup) const
    {
        if (index >= m_partitions.size() || sector >= m_partitions[index].sectors)
            return nullptr;
        const Partition& partition = m_partitions[index];
        const uint32_t sectorsPerGroup = m_chunkSize / WiiSectorSize;
        const uint64_t absolute = partition.firstSector + sector;
        for (const Partition::Range& range : partition.ranges)
        {
            if (absolute < range.firstSector || absolute >= uint64_t(range.firstSector) + range.sectors)
                continue;
            const uint64_t inRange = absolute - range.firstSector;
            const uint32_t groupInRange = uint32_t(inRange / sectorsPerGroup);
            const uint32_t firstOfGroup = groupInRange * sectorsPerGroup;
            const uint32_t sectors = std::min(sectorsPerGroup, range.sectors - firstOfGroup);
            const uint64_t groupData =
                (range.firstSector - partition.firstSector + uint64_t(firstOfGroup)) * WiiSectorDataSize;
            inGroup = size_t(inRange - firstOfGroup);
            return LoadGroup(
                range.firstGroup + groupInRange, uint64_t(sectors) * WiiSectorDataSize, groupData, sectors);
        }
        return nullptr;
    }

    bool RvzImage::ReadSector(
        size_t index, uint64_t sector, uint8_t* data, std::vector<HashException>& exceptions) const
    {
        size_t inGroup = 0;
        std::shared_ptr<const Group> group = SectorGroup(index, sector, inGroup);
        if (group == nullptr)
            return false;
        memcpy(data, group->data.data() + inGroup * WiiSectorDataSize, WiiSectorDataSize);
        exceptions = group->sectorExceptions[inGroup];
        return true;
    }

    bool RvzImage::ReadPartition(size_t index, uint64_t offset, void* out, uint64_t size) const
    {
        if (index >= m_partitions.size() || offset > m_partitions[index].DataSize() ||
            size > m_partitions[index].DataSize() - offset)
            return false;
        uint8_t* bytes = static_cast<uint8_t*>(out);
        while (size != 0)
        {
            size_t inGroup = 0;
            std::shared_ptr<const Group> group = SectorGroup(index, offset / WiiSectorDataSize, inGroup);
            if (group == nullptr)
                return false;
            const uint64_t at = inGroup * uint64_t(WiiSectorDataSize) + offset % WiiSectorDataSize;
            const uint64_t take = std::min<uint64_t>(size, group->data.size() - at);
            memcpy(bytes, group->data.data() + at, take);
            bytes += take, offset += take, size -= take;
        }
        return true;
    }
}
