#pragma once

// Reader of RVZ disc images, the compressed format players make of their Wii discs. An RVZ keeps the disc's plain
// regions (headers, partition table, partition headers) and each partition's data already decrypted, without its
// hashes (0x7C00 bytes of each 0x8000-byte sector), in compressed groups; padding the disc filled from its
// junk generator is kept as the generator's seed. The layout: docs/runtime/wii/disc_rvz.md.
#include "disc/sha1.h"
#include "platform/platform.h"
#include <cstdint>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace disc
{
    constexpr uint32_t WiiSectorSize = 0x8000;     // a sector of a Wii partition: its hashes, then its data
    constexpr uint32_t WiiSectorHashSize = 0x400;  // the hashes (H0, H1, H2) at the start of each sector
    constexpr uint32_t WiiSectorDataSize = 0x7C00; // the data that follows them

    class RvzImage
    {
    public:
        // A stored hash of a sector that differs from the one its data gives (the disc itself has it so).
        struct HashException
        {
            uint32_t offset; // in the sector's 0x400 bytes of hashes
            Sha1Digest hash;
        };

        // One Wii partition's data: `sectors` sectors from `firstSector` (a disc sector number, 0x8000 bytes each).
        struct Partition
        {
            uint64_t firstSector = 0;
            uint64_t sectors = 0;
            struct Range
            {
                uint32_t firstSector, sectors, firstGroup, groups;
            };
            Range ranges[2]{};
            uint64_t DataSize() const { return sectors * WiiSectorDataSize; }
        };

        RvzImage() = default;
        RvzImage(const RvzImage&) = delete;
        RvzImage& operator=(const RvzImage&) = delete;
        ~RvzImage();

        // False with `error` set: not an RVZ, a damaged one, or one this reader does not take (its compression).
        bool Open(const char* path, std::string& error);
        static bool IsRvz(const char* path);

        uint32_t DiscType() const { return m_discType; } // 1 GameCube, 2 Wii
        uint64_t DiscSize() const { return m_discSize; }
        const std::vector<Partition>& Partitions() const { return m_partitions; }

        // The disc's bytes outside the partitions' data. False past what the image holds.
        bool ReadRaw(uint64_t offset, void* out, uint64_t size) const;
        // Partition `index`'s decrypted data, from `offset` in it (WiiSectorDataSize bytes per sector).
        bool ReadPartition(size_t index, uint64_t offset, void* out, uint64_t size) const;
        // One sector's data (WiiSectorDataSize bytes) and the hash exceptions recorded for it, for the hash check.
        bool ReadSector(size_t index, uint64_t sector, uint8_t* data, std::vector<HashException>& exceptions) const;

    private:
        struct RawRange
        {
            uint64_t start, end; // disc offsets: start is sector-aligned, the groups begin there
            uint32_t firstGroup, groups;
        };
        struct GroupEntry
        {
            uint64_t fileOffset;
            uint32_t size;
            bool compressed;
            uint32_t packedSize;
        };
        struct Group
        {
            uint32_t index;
            std::vector<uint8_t> data;
            std::vector<std::vector<HashException>> sectorExceptions; // per sector, for a partition's group
        };

        PlatformFile m_file = InvalidPlatformFile;
        uint64_t m_fileSize = 0;
        uint32_t m_discType = 0;
        uint32_t m_compression = 0;
        uint32_t m_chunkSize = 0;
        uint64_t m_discSize = 0;
        uint8_t m_discHeader[0x80]{};
        std::vector<Partition> m_partitions;
        std::vector<RawRange> m_raw;
        std::vector<GroupEntry> m_groups;

        mutable std::mutex m_mutex;
        mutable std::list<std::shared_ptr<const Group>> m_cache; // most recent first
        mutable void* m_zstd = nullptr;
        static constexpr size_t CacheGroups = 16;

        bool ReadFile(uint64_t offset, void* out, uint64_t size) const;
        bool Decompress(const std::vector<uint8_t>& in, std::vector<uint8_t>& out, size_t expected) const;
        bool ReadEntries(uint64_t offset, uint32_t storedSize, size_t count, size_t entrySize,
            std::vector<uint8_t>& out, std::string& error) const;
        // Group `index` decoded: `size` bytes of data that start at `dataOffset` (a disc offset for the plain regions,
        // an offset in the partition's data otherwise); a partition's group starts with its hash exceptions.
        std::shared_ptr<const Group> LoadGroup(
            uint32_t index, uint64_t size, uint64_t dataOffset, uint32_t sectors) const;
        std::shared_ptr<const Group> DecodeGroup(
            uint32_t index, uint64_t size, uint64_t dataOffset, uint32_t sectors) const;
        // The group holding the partition's sector, and the sector's place in it.
        std::shared_ptr<const Group> SectorGroup(size_t index, uint64_t sector, size_t& inGroup) const;
        void Close();
    };
}
