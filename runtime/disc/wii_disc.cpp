#include "disc/wii_disc.h"
#include <algorithm>
#include <cstring>

namespace disc
{
    namespace
    {
        constexpr uint64_t PartitionTable = 0x40000;
        constexpr uint32_t MaxPartitions = 64;
        constexpr uint32_t MaxFstEntries = 1u << 18;
        constexpr uint32_t MaxFstSize = 64u << 20;
        // The sector's hashes: 31 H0 (one per 0x400 bytes of data), then the 8 H1 of its group of 8 sectors, then
        // the 8 H2 of its group of 64.
        constexpr uint32_t H0Count = WiiSectorDataSize / 0x400, H0Offset = 0, H1Offset = 0x280, H2Offset = 0x340;
        constexpr uint32_t HashTableSize = 0x18000; // H3: one hash per 64 sectors

        uint32_t Be32(const uint8_t* p)
        {
            return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3];
        }

        void Patch(uint8_t* hashes, const std::vector<RvzImage::HashException>& exceptions, uint32_t from, uint32_t to)
        {
            for (const RvzImage::HashException& exception : exceptions)
                if (exception.offset >= from && exception.offset + 20 <= to)
                    memcpy(hashes + exception.offset, exception.hash.data(), 20);
        }
    }

    uint64_t WiiDisc::GameDataSize() const { return m_image.Partitions()[m_partitions[m_game].image].DataSize(); }

    bool WiiDisc::Read(uint64_t offset, void* out, uint64_t size) const
    {
        return m_image.ReadPartition(m_partitions[m_game].image, offset, out, size);
    }

    bool WiiDisc::Open(const char* path, std::string& error)
    {
        m_partitions.clear();
        m_files.clear();
        m_gameId.clear();
        m_title.clear();
        if (!m_image.Open(path, error))
            return false;
        if (m_image.DiscType() != 2)
        {
            error = "a GameCube disc, not a Wii one";
            return false;
        }
        return ReadPartitions(error) && ReadFileSystem(error);
    }

    bool WiiDisc::ReadPartitions(std::string& error)
    {
        uint8_t groups[32];
        if (!m_image.ReadRaw(PartitionTable, groups, sizeof(groups)))
        {
            error = "the disc has no partition table";
            return false;
        }
        for (int group = 0; group < 4; group++)
        {
            const uint32_t count = Be32(groups + 8 * group);
            const uint64_t table = uint64_t(Be32(groups + 8 * group + 4)) << 2;
            if (count > MaxPartitions || m_partitions.size() + count > MaxPartitions)
            {
                error = "the disc's partition table is damaged";
                return false;
            }
            std::vector<uint8_t> entries(size_t(count) * 8);
            if (count != 0 && !m_image.ReadRaw(table, entries.data(), entries.size()))
            {
                error = "the disc's partition table is damaged";
                return false;
            }
            for (uint32_t i = 0; i < count; i++)
            {
                PartitionInfo partition;
                partition.offset = uint64_t(Be32(&entries[8 * i])) << 2;
                partition.type = Be32(&entries[8 * i + 4]);
                // The partition's header: the title metadata's size and place, its hash table, its data.
                uint8_t header[0x2C0];
                if (!m_image.ReadRaw(partition.offset, header, sizeof(header)))
                {
                    error = "a partition's header is missing";
                    return false;
                }
                const uint32_t tmdSize = Be32(header + 0x2A4);
                const uint64_t tmd = partition.offset + (uint64_t(Be32(header + 0x2A8)) << 2);
                partition.hashTable = partition.offset + (uint64_t(Be32(header + 0x2B4)) << 2);
                const uint64_t data = partition.offset + (uint64_t(Be32(header + 0x2B8)) << 2);
                // The hash of the first content (the partition's data): its record starts at 0x1E4.
                if (tmdSize < 0x1F4 + 20 || !m_image.ReadRaw(tmd + 0x1F4, partition.titleHash.data(), 20))
                {
                    error = "a partition's title metadata is missing";
                    return false;
                }
                const auto& stored = m_image.Partitions();
                auto it = std::find_if(stored.begin(), stored.end(),
                    [&](const RvzImage::Partition& p) { return p.firstSector * WiiSectorSize == data; });
                if (it == stored.end())
                {
                    error = "a partition's data is not in the image";
                    return false;
                }
                partition.image = size_t(it - stored.begin());
                m_partitions.push_back(partition);
            }
        }
        auto game =
            std::find_if(m_partitions.begin(), m_partitions.end(), [](const PartitionInfo& p) { return p.type == 0; });
        if (game == m_partitions.end())
        {
            error = "the disc has no game partition";
            return false;
        }
        m_game = size_t(game - m_partitions.begin());
        return true;
    }

    bool WiiDisc::ReadFileSystem(std::string& error)
    {
        const uint64_t dataSize = GameDataSize();
        uint8_t boot[0x440];
        if (!Read(0, boot, sizeof(boot)))
        {
            error = "the game partition is damaged";
            return false;
        }
        m_gameId.assign(reinterpret_cast<const char*>(boot), 6);
        const char* title = reinterpret_cast<const char*>(boot + 0x20);
        m_title.assign(title, strnlen(title, 0x40));
        const uint64_t dol = uint64_t(Be32(boot + 0x420)) << 2;
        const uint64_t fst = uint64_t(Be32(boot + 0x424)) << 2;
        const uint64_t fstSize = uint64_t(Be32(boot + 0x428)) << 2;

        uint8_t apploader[0x20], dolHeader[0x100];
        if (fstSize < 12 || fstSize > MaxFstSize || fst > dataSize || fstSize > dataSize - fst ||
            !Read(0x2440, apploader, sizeof(apploader)) || dol > dataSize - sizeof(dolHeader) ||
            !Read(dol, dolHeader, sizeof(dolHeader)))
        {
            error = "the game partition's file system is damaged";
            return false;
        }
        const uint64_t apploaderSize = 0x20 + uint64_t(Be32(apploader + 0x14)) + Be32(apploader + 0x18);
        uint64_t dolSize = sizeof(dolHeader);
        for (int section = 0; section < 18; section++) // 7 code sections, then 11 data sections
            dolSize = std::max(dolSize, uint64_t(Be32(dolHeader + 4 * section)) + Be32(dolHeader + 0x90 + 4 * section));
        if (0x2440 + apploaderSize > dataSize || dolSize > dataSize - dol)
        {
            error = "the game partition's file system is damaged";
            return false;
        }
        m_files.push_back({ "sys", 0, 0, true });
        m_files.push_back({ "sys/boot.bin", 0, 0x440, false });
        m_files.push_back({ "sys/bi2.bin", 0x440, 0x2000, false });
        m_files.push_back({ "sys/apploader.img", 0x2440, uint32_t(apploaderSize), false });
        m_files.push_back({ "sys/main.dol", dol, uint32_t(dolSize), false });
        m_files.push_back({ "sys/fst.bin", fst, uint32_t(fstSize), false });
        m_files.push_back({ "files", 0, 0, true });

        std::vector<uint8_t> table(fstSize);
        if (!Read(fst, table.data(), table.size()))
        {
            error = "the game partition's file system is damaged";
            return false;
        }
        // Entries of 12 bytes: the kind (1 a directory) and the name's offset, then a file's data offset (in 4-byte
        // units) and size, or a directory's parent and the index past its last entry. The names follow the entries.
        const uint32_t count = Be32(&table[8]);
        if (count == 0 || count > MaxFstEntries || uint64_t(count) * 12 > fstSize)
        {
            error = "the game partition's file system is damaged";
            return false;
        }
        const size_t names = size_t(count) * 12;
        struct Open
        {
            uint32_t end;
            std::string prefix;
        };
        std::vector<Open> open{ { count, "files/" } };
        for (uint32_t i = 1; i < count; i++)
        {
            while (open.back().end <= i)
                open.pop_back();
            const uint8_t* entry = &table[size_t(i) * 12];
            const size_t name = names + (Be32(entry) & 0xFFFFFF);
            const uint32_t first = Be32(entry + 4), second = Be32(entry + 8);
            const void* nul = name < table.size() ? memchr(&table[name], 0, table.size() - name) : nullptr;
            if (nul == nullptr)
            {
                error = "the game partition's file system is damaged";
                return false;
            }
            const std::string path = open.back().prefix + reinterpret_cast<const char*>(&table[name]);
            if (entry[0] == 1)
            {
                if (second <= i || second > open.back().end)
                {
                    error = "the game partition's file system is damaged";
                    return false;
                }
                m_files.push_back({ path, 0, 0, true });
                open.push_back({ second, path + "/" });
                continue;
            }
            const uint64_t offset = uint64_t(first) << 2;
            if (offset > dataSize || second > dataSize - offset)
            {
                error = "the game partition's file system is damaged";
                return false;
            }
            m_files.push_back({ path, offset, second, false });
        }
        return true;
    }

    bool WiiDisc::VerifyPartition(const PartitionInfo& partition, Check& check, uint64_t total,
        const std::function<bool(uint64_t, uint64_t)>& progress) const
    {
        std::vector<uint8_t> h3(HashTableSize);
        if (!m_image.ReadRaw(partition.hashTable, h3.data(), h3.size()))
        {
            check.problem = "a partition's hash table is missing";
            return false;
        }
        if (Sha1::Of(h3.data(), h3.size()) != partition.titleHash)
        {
            check.problem = "a partition's hash table does not match its title";
            return false;
        }
        const uint64_t sectors = m_image.Partitions()[partition.image].sectors;
        if ((sectors + 63) / 64 * 20 > HashTableSize)
        {
            check.problem = "a partition is larger than its hash table";
            return false;
        }
        std::vector<uint8_t> data(WiiSectorDataSize);
        std::vector<RvzImage::HashException> exceptions[64];
        uint8_t hashes[64][WiiSectorHashSize];
        uint8_t zeroHashes[WiiSectorHashSize] = {};
        {
            const std::vector<uint8_t> zeros(0x400, 0);
            const Sha1Digest zero = Sha1::Of(zeros.data(), zeros.size());
            for (uint32_t h = 0; h < H0Count; h++)
                memcpy(&zeroHashes[H0Offset + 20 * h], zero.data(), 20);
        }
        for (uint64_t first = 0; first < sectors; first += 64)
        {
            const uint32_t count = uint32_t(std::min<uint64_t>(64, sectors - first));
            memset(hashes, 0, sizeof(hashes));
            for (uint32_t s = 0; s < count; s++)
            {
                if (!m_image.ReadSector(partition.image, first + s, data.data(), exceptions[s]))
                {
                    check.problem = "a partition's data cannot be read (sector " + std::to_string(first + s) + ")";
                    return false;
                }
                for (uint32_t h = 0; h < H0Count; h++)
                {
                    const Sha1Digest digest = Sha1::Of(&data[h * 0x400], 0x400);
                    memcpy(&hashes[s][H0Offset + 20 * h], digest.data(), 20);
                }
                Patch(hashes[s], exceptions[s], 0, H1Offset);
                check.exceptions += exceptions[s].size();
            }
            // A partition's last group of 64 sectors, when short, is hashed as if sectors of zeros filled it.
            for (uint32_t s = count; s < 64; s++)
            {
                exceptions[s].clear();
                memcpy(hashes[s], zeroHashes, sizeof(zeroHashes));
            }
            // H1: each group of 8 sectors hashes its sectors' H0; every sector of the group keeps the 8 H1.
            uint8_t h2[8 * 20] = {};
            for (uint32_t group = 0; group < 8; group++)
            {
                uint8_t h1[8 * 20] = {};
                for (uint32_t s = 0; s < 8; s++)
                {
                    const Sha1Digest digest = Sha1::Of(&hashes[group * 8 + s][H0Offset], H0Count * 20);
                    memcpy(&h1[20 * s], digest.data(), 20);
                }
                for (uint32_t s = 0; s < 8; s++)
                {
                    memcpy(&hashes[group * 8 + s][H1Offset], h1, sizeof(h1));
                    Patch(hashes[group * 8 + s], exceptions[group * 8 + s], H1Offset, H2Offset);
                }
                const Sha1Digest digest = Sha1::Of(&hashes[group * 8][H1Offset], sizeof(h1));
                memcpy(&h2[20 * group], digest.data(), 20);
            }
            memcpy(&hashes[0][H2Offset], h2, sizeof(h2));
            Patch(hashes[0], exceptions[0], H2Offset, WiiSectorHashSize);
            const Sha1Digest digest = Sha1::Of(&hashes[0][H2Offset], sizeof(h2));
            if (memcmp(digest.data(), &h3[(first / 64) * 20], 20) != 0)
            {
                check.problem = "a partition's data does not match its hashes (sectors " + std::to_string(first) +
                                " to " + std::to_string(first + count - 1) + ")";
                return false;
            }
            check.sectors += count;
            if (progress && !progress(check.sectors, total))
            {
                check.problem = "stopped";
                return false;
            }
        }
        return true;
    }

    WiiDisc::Check WiiDisc::Verify(const std::function<bool(uint64_t, uint64_t)>& progress) const
    {
        Check check;
        uint64_t total = 0;
        for (const PartitionInfo& partition : m_partitions)
            total += m_image.Partitions()[partition.image].sectors;
        for (const PartitionInfo& partition : m_partitions)
            if (!VerifyPartition(partition, check, total, progress))
                return check;
        check.intact = true;
        return check;
    }
}
