#pragma once

// A Wii disc read from its RVZ image: the game's partition, its files, and the check of every partition's data against
// the disc's own hash tree. Files are listed as a disc extractor lays them out: the partition's system files under
// sys/ (boot.bin, bi2.bin, apploader.img, main.dol, fst.bin) and its file system under files/.
#include "disc/rvz.h"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace disc
{
    class WiiDisc
    {
    public:
        struct File
        {
            std::string path;    // "files/Data/a.pkz", "sys/main.dol"; directories too, "files/Data"
            uint64_t offset = 0; // in the game partition's data
            uint32_t size = 0;
            bool directory = false;
        };

        struct Check
        {
            bool intact = false;
            uint64_t sectors = 0;    // checked, every partition together
            uint64_t exceptions = 0; // hashes the disc itself stores differently from its data (kept by the image)
            std::string problem;     // what failed, when not intact
        };

        // False with `error` set: not a Wii disc's RVZ, or one whose game partition or file system is damaged.
        bool Open(const char* path, std::string& error);

        const std::string& GameId() const { return m_gameId; } // "SKNP52"
        const std::string& Title() const { return m_title; }
        const std::vector<File>& Files() const { return m_files; }
        uint64_t GameDataSize() const;
        // The IOS the game asks for (its title metadata's system version, low half): 56 for IOS56. 0 before Open.
        uint32_t Ios() const { return m_partitions.empty() ? 0 : uint32_t(m_partitions[m_game].systemVersion); }

        // The game partition's data from `offset`.
        bool Read(uint64_t offset, void* out, uint64_t size) const;
        // The disc itself from `offset`, outside the partitions' data (its header, the partition table).
        bool ReadRaw(uint64_t offset, void* out, uint64_t size) const { return m_image.ReadRaw(offset, out, size); }
        // The game partition: where it starts on the disc, and its title metadata (what IOS's ES checks it with).
        uint64_t GamePartition() const { return m_partitions.empty() ? 0 : m_partitions[m_game].offset; }
        bool TitleMetadata(std::vector<uint8_t>& out) const;

        // Every partition's data against its hash tree, up to the title's own hash of it. `progress(done, total)`, in
        // sectors, returns false to stop (the check then reports the stop as its problem).
        Check Verify(const std::function<bool(uint64_t, uint64_t)>& progress = {}) const;

    private:
        struct PartitionInfo
        {
            uint64_t offset = 0; // on the disc
            uint32_t type = 0;   // 0 the game, 1 a system update
            size_t image = 0;    // its index in the image's partitions
            uint64_t hashTable = 0;
            Sha1Digest titleHash{}; // the title metadata's hash of the hash table (H3)
            uint64_t systemVersion = 0; // the title metadata's: the IOS it runs on
            uint64_t titleMetadata = 0; // on the disc
            uint32_t titleMetadataSize = 0;
        };

        RvzImage m_image;
        std::vector<PartitionInfo> m_partitions;
        size_t m_game = 0; // in m_partitions
        std::string m_gameId, m_title;
        std::vector<File> m_files;

        bool ReadPartitions(std::string& error);
        bool ReadFileSystem(std::string& error);
        bool VerifyPartition(const PartitionInfo& partition, Check& check, uint64_t total,
            const std::function<bool(uint64_t, uint64_t)>& progress) const;
    };
}
