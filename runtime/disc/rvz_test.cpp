// The Wii disc reader (disc/rvz.cpp, disc/wii_disc.cpp, and vfs.cpp on top) on synthetic RVZ images: groups stored,
// Zstandard-compressed, packed, all zeros, padding from its seed, a hash the disc stores differently from its data, a
// short last hash group; then damaged and crafted images, each refused without reading out of bounds.
// ./ArkeyanRvzTest [scratch dir] [--disc <image.rvz>] (with a real disc: its files listed, its hashes checked).
#include "disc/wii_disc.h"
#include "vfs.h"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace
{
    int g_checks = 0, g_failures = 0;
#define EXPECT(cond)                                                                                                   \
    do                                                                                                                 \
    {                                                                                                                  \
        g_checks++;                                                                                                    \
        if (!(cond))                                                                                                   \
        {                                                                                                              \
            g_failures++;                                                                                              \
            printf("FAIL line %d: %s\n", __LINE__, #cond);                                                             \
        }                                                                                                              \
    } while (0)

    using Bytes = std::vector<uint8_t>;
    using disc::WiiSectorDataSize;
    using disc::WiiSectorSize;

    void Put16(Bytes& b, size_t at, uint32_t v)
    {
        b[at] = uint8_t(v >> 8);
        b[at + 1] = uint8_t(v);
    }
    void Put32(Bytes& b, size_t at, uint32_t v)
    {
        for (int i = 0; i < 4; i++)
            b[at + i] = uint8_t(v >> (24 - 8 * i));
    }
    void Put64(Bytes& b, size_t at, uint64_t v)
    {
        Put32(b, at, uint32_t(v >> 32));
        Put32(b, at + 4, uint32_t(v));
    }
    void Append32(Bytes& b, uint32_t v)
    {
        b.resize(b.size() + 4);
        Put32(b, b.size() - 4, v);
    }
    void Append(Bytes& b, const Bytes& more) { b.insert(b.end(), more.begin(), more.end()); }

    // A Zstandard frame of raw blocks: what a compressor writes for data it cannot shrink.
    Bytes ZstdFrame(const Bytes& data)
    {
        Bytes frame{ 0x28, 0xB5, 0x2F, 0xFD, 0xA0 }; // magic, then one segment with a 4-byte content size
        for (int i = 0; i < 4; i++)
            frame.push_back(uint8_t(data.size() >> (8 * i)));
        size_t at = 0;
        do
        {
            const size_t size = std::min<size_t>(data.size() - at, 128 * 1024);
            const uint32_t header = uint32_t(size) << 3 | (at + size == data.size() ? 1 : 0); // raw block, last?
            for (int i = 0; i < 3; i++)
                frame.push_back(uint8_t(header >> (8 * i)));
            frame.insert(frame.end(), data.begin() + std::ptrdiff_t(at), data.begin() + std::ptrdiff_t(at + size));
            at += size;
        } while (at < data.size());
        return frame;
    }

    // The test disc: one game partition of 70 sectors (one full group of 64 for the hashes, then a short one).
    constexpr uint32_t Chunk = 0x10000; // two sectors per group
    constexpr uint64_t PartitionOffset = 0x50000, DataStart = PartitionOffset + 0x20000;
    constexpr uint32_t Sectors = 70;
    constexpr uint32_t ZeroGroup = 20, PackedGroup = 5, CompressedGroup = 1;
    constexpr uint32_t ExceptionSector = 3, ExceptionHash = 5; // the disc's H0 number 5 of sector 3 is not its data's

    struct Disc
    {
        Bytes raw = Bytes(DataStart, 0); // the disc before the partition's data
        Bytes data = Bytes(size_t(Sectors) * WiiSectorDataSize, 0);
        uint32_t compression = 5;
    };

    Bytes Seed(uint32_t salt)
    {
        Bytes seed(68);
        for (uint32_t i = 0; i < 17; i++)
            Put32(seed, 4 * i, 0x12345678u + (i + salt) * 0x9E3779B9u);
        return seed;
    }

    // `dataEnd`: the index past the "data" directory's last entry (3; past the file system's end in a crafted one).
    Disc MakeDisc(uint32_t dataEnd = 3)
    {
        Disc d;
        memcpy(&d.raw[0], "RTST01", 6);
        Put32(d.raw, 0x18, 0x5D1C9EA3); // the Wii disc's magic
        Put32(d.raw, 0x40000, 1);
        Put32(d.raw, 0x40004, 0x40020 >> 2);
        Put32(d.raw, 0x40020, uint32_t(PartitionOffset >> 2));
        Put32(d.raw, 0x40024, 0);
        const size_t p = PartitionOffset;
        Put32(d.raw, p + 0x2A4, 0x208);
        Put32(d.raw, p + 0x2A8, 0x2C0 >> 2);
        Put32(d.raw, p + 0x2B4, 0x8000 >> 2);
        Put32(d.raw, p + 0x2B8, 0x20000 >> 2);
        Put32(d.raw, p + 0x2BC, uint32_t(d.data.size() >> 2));
        Put32(d.raw, p + 0x2C0 + 0x184, 1); // the title metadata's system version: IOS 58
        Put32(d.raw, p + 0x2C0 + 0x188, 58);

        Bytes& g = d.data;
        for (size_t i = 0x30000; i < g.size(); i++)
            g[i] = uint8_t(i * 13 + (i >> 9));
        std::fill(g.begin() + std::ptrdiff_t(ZeroGroup) * 2 * WiiSectorDataSize,
            g.begin() + (std::ptrdiff_t(ZeroGroup) + 1) * 2 * WiiSectorDataSize, 0);
        memcpy(&g[0], "RTST01", 6);
        memcpy(&g[0x20], "Test", 4);
        Put32(g, 0x420, 0x3000 >> 2);
        Put32(g, 0x424, 0x4000 >> 2);
        Put32(g, 0x2440 + 0x14, 0x100);
        Put32(g, 0x3000 + 0, 0x100); // text section 0: 0x200 bytes at 0x100
        Put32(g, 0x3000 + 0x90, 0x200);
        // The file system: the root, "data" holding "a.bin", then "b.txt".
        const size_t fst = 0x4000;
        const char names[] = "data\0a.bin\0b.txt";
        auto entry = [&](uint32_t index, uint8_t kind, uint32_t name, uint32_t first, uint32_t second) {
            Put32(g, fst + 12 * index, uint32_t(kind) << 24 | name);
            Put32(g, fst + 12 * index + 4, first);
            Put32(g, fst + 12 * index + 8, second);
        };
        entry(0, 1, 0, 0, 4);
        entry(1, 1, 0, 0, dataEnd);
        entry(2, 0, 5, 0x10000 >> 2, 0x9000);
        entry(3, 0, 11, 0x20000 >> 2, 5);
        memcpy(&g[fst + 48], names, sizeof(names));
        Put32(g, 0x428, uint32_t((48 + sizeof(names) + 3) / 4));
        for (size_t i = 0; i < 0x9000; i++)
            g[0x10000 + i] = uint8_t(i * 7);
        memcpy(&g[0x20000], "hello", 5);
        return d;
    }

    // The partition's hash tree as the disc stores it (sector 3's H0 number 5 replaced), into the H3 table and the
    // title metadata's hash of it.
    void HashDisc(Disc& d, const disc::Sha1Digest& stored)
    {
        Bytes h3(0x18000, 0);
        Bytes zeros(0x400, 0);
        for (uint32_t first = 0; first < Sectors; first += 64)
        {
            Bytes h0(64 * 31 * 20);
            for (uint32_t s = 0; s < 64; s++)
                for (uint32_t h = 0; h < 31; h++)
                {
                    const uint8_t* block =
                        first + s < Sectors ? &d.data[size_t(first + s) * WiiSectorDataSize + h * 0x400] : zeros.data();
                    disc::Sha1Digest digest = disc::Sha1::Of(block, 0x400);
                    if (first + s == ExceptionSector && h == ExceptionHash)
                        digest = stored;
                    memcpy(&h0[(s * 31 + h) * 20], digest.data(), 20);
                }
            Bytes h2(8 * 20);
            for (uint32_t group = 0; group < 8; group++)
            {
                Bytes h1(8 * 20);
                for (uint32_t s = 0; s < 8; s++)
                {
                    const disc::Sha1Digest digest = disc::Sha1::Of(&h0[((group * 8 + s) * 31) * 20], 31 * 20);
                    memcpy(&h1[20 * s], digest.data(), 20);
                }
                const disc::Sha1Digest digest = disc::Sha1::Of(h1.data(), h1.size());
                memcpy(&h2[20 * group], digest.data(), 20);
            }
            const disc::Sha1Digest digest = disc::Sha1::Of(h2.data(), h2.size());
            memcpy(&h3[(first / 64) * 20], digest.data(), 20);
        }
        memcpy(&d.raw[PartitionOffset + 0x8000], h3.data(), h3.size());
        const disc::Sha1Digest title = disc::Sha1::Of(h3.data(), h3.size());
        memcpy(&d.raw[PartitionOffset + 0x2C0 + 0x1F4], title.data(), 20);
    }

    struct Group
    {
        Bytes stored;
        bool compressed = false;
        uint32_t packed = 0;
    };

    // The RVZ of the disc: the plain region in groups (the second one packed as padding), the partition in groups.
    Bytes MakeRvz(const Disc& d, const disc::Sha1Digest& stored, const Bytes& junkA, const Bytes& junkB)
    {
        std::vector<Group> groups;
        const uint32_t rawGroups = uint32_t((DataStart + Chunk - 1) / Chunk);
        for (uint32_t i = 0; i < rawGroups; i++)
        {
            Group group;
            const size_t from = size_t(i) * Chunk, to = std::min<size_t>(from + Chunk, DataStart);
            if (i == 1) // 0x10000: a block of padding, then data
            {
                Append32(group.stored, 0x80000000u | 0x8000);
                Append(group.stored, junkA);
                Append32(group.stored, 0x8000);
                group.stored.insert(group.stored.end(), d.raw.begin() + 0x18000, d.raw.begin() + 0x20000);
                group.packed = uint32_t(group.stored.size());
            }
            else if (i == 2) // 0x20000: data, padding from the middle of a block (0x24000), data
            {
                Append32(group.stored, 0x4000);
                group.stored.insert(group.stored.end(), d.raw.begin() + 0x20000, d.raw.begin() + 0x24000);
                Append32(group.stored, 0x80000000u | 0x4000);
                Append(group.stored, junkB);
                Append32(group.stored, 0x8000);
                group.stored.insert(group.stored.end(), d.raw.begin() + 0x28000, d.raw.begin() + 0x30000);
                group.packed = uint32_t(group.stored.size());
            }
            else
                group.stored.assign(d.raw.begin() + std::ptrdiff_t(from), d.raw.begin() + std::ptrdiff_t(to));
            groups.push_back(group);
        }
        const uint32_t firstPartitionGroup = uint32_t(groups.size());
        for (uint32_t i = 0; i < Sectors / 2; i++)
        {
            Group group;
            const Bytes data(d.data.begin() + std::ptrdiff_t(size_t(i) * 2 * WiiSectorDataSize),
                d.data.begin() + std::ptrdiff_t(size_t(i + 1) * 2 * WiiSectorDataSize));
            if (i == ZeroGroup)
            {
                groups.push_back(group);
                continue;
            }
            Bytes exceptions{ 0, 0 };
            if (i == ExceptionSector / 2)
            {
                exceptions = { 0, 1 };
                exceptions.resize(4);
                Put16(exceptions, 2, (ExceptionSector % 2) * 0x400 + ExceptionHash * 20);
                exceptions.insert(exceptions.end(), stored.begin(), stored.end());
            }
            if (i == CompressedGroup)
            {
                Bytes plain = exceptions;
                Append(plain, data);
                group.stored = ZstdFrame(plain);
                group.compressed = true;
            }
            else
            {
                group.stored = exceptions;
                group.stored.resize((group.stored.size() + 3) & ~size_t(3));
                if (i == PackedGroup) // two runs of data
                {
                    for (int half = 0; half < 2; half++)
                    {
                        Append32(group.stored, WiiSectorDataSize);
                        group.stored.insert(group.stored.end(), data.begin() + half * WiiSectorDataSize,
                            data.begin() + (half + 1) * WiiSectorDataSize);
                    }
                    group.packed = uint32_t(group.stored.size());
                }
                else
                    Append(group.stored, data);
            }
            groups.push_back(group);
        }

        Bytes header1(0x48, 0), header2(0xDC, 0), partitions(0x30, 0), raw(0x18, 0);
        Put32(header1, 0, 0x52565A01);
        Put32(header1, 4, 0x01000000);
        Put32(header1, 8, 0x00030000);
        Put32(header1, 12, 0xDC);
        Put64(header1, 0x24, DataStart + uint64_t(Sectors) * WiiSectorSize);
        Put32(header2, 0, 2);
        Put32(header2, 4, d.compression);
        Put32(header2, 8, 19);
        Put32(header2, 12, Chunk);
        memcpy(&header2[16], d.raw.data(), 0x80);
        // The partition's data: every sector in the first range, an empty second one.
        Put32(partitions, 16, uint32_t(DataStart / WiiSectorSize));
        Put32(partitions, 20, Sectors);
        Put32(partitions, 24, firstPartitionGroup);
        Put32(partitions, 28, Sectors / 2);
        Put32(partitions, 32, uint32_t(DataStart / WiiSectorSize) + Sectors);
        Put32(partitions, 40, firstPartitionGroup + Sectors / 2);
        Put64(raw, 0, 0x80);
        Put64(raw, 8, DataStart - 0x80);
        Put32(raw, 16, 0);
        Put32(raw, 20, rawGroups);

        Bytes file(0x48 + 0xDC, 0);
        const uint64_t partitionAt = file.size();
        Append(file, partitions);
        const Bytes rawTable = ZstdFrame(raw);
        const uint64_t rawAt = file.size();
        Append(file, rawTable);
        Bytes groupTable(groups.size() * 12, 0);
        const uint64_t groupAt = (file.size() + 3) & ~uint64_t(3);
        // The groups' table goes first, their data after it (each at a multiple of 4 bytes).
        uint64_t at = groupAt + ZstdFrame(groupTable).size();
        for (size_t i = 0; i < groups.size(); i++)
        {
            at = (at + 3) & ~uint64_t(3);
            Put32(groupTable, 12 * i, groups[i].stored.empty() ? 0 : uint32_t(at >> 2));
            Put32(groupTable, 12 * i + 4, uint32_t(groups[i].stored.size()) | (groups[i].compressed ? 0x80000000u : 0));
            Put32(groupTable, 12 * i + 8, groups[i].packed);
            at += groups[i].stored.size();
        }
        file.resize(groupAt);
        Append(file, ZstdFrame(groupTable));
        for (const Group& group : groups)
        {
            file.resize((file.size() + 3) & ~size_t(3));
            Append(file, group.stored);
        }
        Put32(header2, 0x90, 1);
        Put32(header2, 0x94, 0x30);
        Put64(header2, 0x98, partitionAt);
        const disc::Sha1Digest partitionHash = disc::Sha1::Of(partitions.data(), partitions.size());
        memcpy(&header2[0xA0], partitionHash.data(), 20);
        Put32(header2, 0xB4, 1);
        Put64(header2, 0xB8, rawAt);
        Put32(header2, 0xC0, uint32_t(rawTable.size()));
        Put32(header2, 0xC4, uint32_t(groups.size()));
        Put64(header2, 0xC8, groupAt);
        Put32(header2, 0xD0, uint32_t(ZstdFrame(groupTable).size()));
        const disc::Sha1Digest header2Hash = disc::Sha1::Of(header2.data(), header2.size());
        memcpy(&header1[16], header2Hash.data(), 20);
        Put64(header1, 0x2C, file.size());
        const disc::Sha1Digest header1Hash = disc::Sha1::Of(header1.data(), 0x34);
        memcpy(&header1[0x34], header1Hash.data(), 20);
        memcpy(&file[0], header1.data(), header1.size());
        memcpy(&file[0x48], header2.data(), header2.size());
        return file;
    }

    bool Write(const std::filesystem::path& path, const Bytes& bytes)
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
        return bool(out);
    }

    std::string Text(const std::filesystem::path& path) { return path.string(); }

    // A real disc: its game, its files, every partition checked against its hashes.
    int CheckRealDisc(const char* path)
    {
        disc::WiiDisc wii;
        std::string error;
        if (!wii.Open(path, error))
        {
            printf("FAIL: %s: %s\n", path, error.c_str());
            return 1;
        }
        uint64_t bytes = 0;
        for (const disc::WiiDisc::File& file : wii.Files())
            bytes += file.size;
        printf("%s \"%s\", IOS %u: %zu entries, %llu bytes of files\n", wii.GameId().c_str(), wii.Title().c_str(),
            wii.Ios(), wii.Files().size(), (unsigned long long)bytes);
        // The game's file reader on it: the executable in the file system starts as one.
        DiscFileSystem files;
        bool elf = false;
        if (files.Open(path))
            for (const auto& [name, entry] : files.Files())
                if (name.ends_with(".elf"))
                {
                    uint8_t magic[4] = {};
                    elf = files.Read(*entry, 0, magic, 4) == 4 && memcmp(magic,
                                                                      "\x7F"
                                                                      "ELF",
                                                                      4) == 0;
                }
        printf("file reader: %zu files, an ELF executable: %s\n", files.Files().size(), elf ? "yes" : "no");
        const auto start = std::chrono::steady_clock::now();
        const disc::WiiDisc::Check check = wii.Verify();
        printf("hashes: %s, %llu sectors, %llu exceptions, %.1f s%s%s\n", check.intact ? "intact" : "NOT intact",
            (unsigned long long)check.sectors, (unsigned long long)check.exceptions,
            std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count(), check.intact ? "" : ": ",
            check.problem.c_str());
        return check.intact ? 0 : 1;
    }
}

int main(int argc, char** argv)
{
    std::filesystem::path scratch = std::filesystem::temp_directory_path();
    const char* realDisc = nullptr;
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--disc") == 0 && i + 1 < argc)
            realDisc = argv[++i];
        else
            scratch = argv[i];
    }
    if (realDisc != nullptr)
        return CheckRealDisc(realDisc);
    const std::filesystem::path root = scratch / "arkeyan-rvz-test";
    std::error_code removeError;
    std::filesystem::remove_all(root, removeError);
    std::filesystem::create_directories(root);

    disc::Sha1Digest stored{};
    for (size_t i = 0; i < stored.size(); i++)
        stored[i] = uint8_t(0xA0 + i);
    const Bytes junkA = Seed(0), junkB = Seed(0); // the same seed: the second run is the first one's second half
    Disc d = MakeDisc();
    HashDisc(d, stored);
    const Bytes rvz = MakeRvz(d, stored, junkA, junkB);
    EXPECT(Write(root / "good.rvz", rvz));
    const std::string good = Text(root / "good.rvz");

    // The disc, its partition, its files.
    {
        EXPECT(disc::RvzImage::IsRvz(good.c_str()));
        disc::WiiDisc wii;
        std::string error;
        EXPECT(wii.Open(good.c_str(), error));
        printf("open: %s\n", error.empty() ? "ok" : error.c_str());
        EXPECT(wii.GameId() == "RTST01" && wii.Title() == "Test");
        EXPECT(wii.Ios() == 58);
        std::vector<uint8_t> tmd;
        EXPECT(wii.TitleMetadata(tmd) && tmd.size() == 0x208 && tmd[0x188 + 3] == 58);
        uint8_t header[8] = {};
        EXPECT(wii.ReadRaw(0, header, sizeof(header)) && memcmp(header, "RTST01", 6) == 0);
        EXPECT(wii.GamePartition() == PartitionOffset);
        EXPECT(wii.GameDataSize() == d.data.size());
        size_t found = 0;
        for (const disc::WiiDisc::File& file : wii.Files())
        {
            found += file.path == "sys/main.dol" && file.size == 0x300 && file.offset == 0x3000;
            found += file.path == "sys/apploader.img" && file.size == 0x120;
            found += file.path == "files/data" && file.directory;
            found += file.path == "files/data/a.bin" && file.size == 0x9000;
            found += file.path == "files/b.txt" && file.size == 5;
        }
        EXPECT(found == 5);

        // Every byte of the partition as it was made: stored, compressed, packed and zero groups alike.
        Bytes back(d.data.size());
        EXPECT(wii.Read(0, back.data(), back.size()));
        EXPECT(back == d.data);
        EXPECT(!wii.Read(d.data.size() - 4, back.data(), 8));

        // The hash tree holds, with the disc's own odd hash taken from the image; a short last group too.
        const disc::WiiDisc::Check check = wii.Verify();
        EXPECT(check.intact);
        EXPECT(check.sectors == Sectors && check.exceptions == 1);
        if (!check.intact)
            printf("check: %s\n", check.problem.c_str());
        int calls = 0;
        const disc::WiiDisc::Check stopped = wii.Verify([&](uint64_t, uint64_t) {
            calls++;
            return false;
        });
        EXPECT(!stopped.intact && stopped.problem == "stopped" && calls == 1);
    }

    // The plain region: the header, the partition table, and padding rebuilt from its seed. Padding that starts in
    // the middle of a 0x8000 block continues the generator from there, as the block's own padding would.
    {
        disc::RvzImage image;
        std::string error;
        EXPECT(image.Open(good.c_str(), error));
        uint8_t id[6], table[8];
        EXPECT(image.ReadRaw(0, id, 6) && memcmp(id, "RTST01", 6) == 0);
        EXPECT(image.ReadRaw(0x40000, table, 8) && table[3] == 1);
        Bytes block(0x8000), tail(0x4000);
        EXPECT(image.ReadRaw(0x10000, block.data(), block.size()));
        EXPECT(image.ReadRaw(0x24000, tail.data(), tail.size()));
        EXPECT(memcmp(block.data() + 0x4000, tail.data(), tail.size()) == 0);
        // The generator's first bytes for this seed, from the reader whose padding matches a real disc's hashes.
        const uint8_t known[8] = { 0x9F, 0xE8, 0x92, 0x09, 0x35, 0x5B, 0x8F, 0x95 };
        EXPECT(memcmp(block.data(), known, sizeof(known)) == 0);
        uint8_t past[4];
        EXPECT(!image.ReadRaw(DataStart, past, 4)); // the partition's data is not plain
    }

    // Through the game's file reader: paths in any case, the files' bytes.
    {
        DiscFileSystem files;
        EXPECT(files.Open(good.c_str()));
        EXPECT(files.Wii() != nullptr);
        EXPECT(files.Find("FILES/Data/A.BIN") != nullptr && files.Find("sys/main.dol") != nullptr);
        const std::vector<uint8_t> a = files.ReadAll("files/data/a.bin");
        EXPECT(a.size() == 0x9000 && a[0x8FFF] == uint8_t(0x8FFF * 7));
        const std::vector<uint8_t> b = files.ReadAll("files/b.txt");
        EXPECT(std::string(b.begin(), b.end()) == "hello");
        EXPECT(files.List("files").size() == 2);
    }

    // A byte of data changed after the disc was hashed: the check names the group of sectors.
    {
        Disc changed = d;
        changed.data[30 * WiiSectorDataSize + 100] ^= 1;
        EXPECT(Write(root / "changed.rvz", MakeRvz(changed, stored, junkA, junkB)));
        disc::WiiDisc wii;
        std::string error;
        EXPECT(wii.Open(Text(root / "changed.rvz").c_str(), error));
        const disc::WiiDisc::Check check = wii.Verify();
        EXPECT(!check.intact && check.problem.find("sectors 0 to 63") != std::string::npos);
    }

    // Damaged and crafted images: refused, with a reason.
    {
        Bytes header = rvz;
        header[0x48 + 0x20] ^= 1; // the disc header inside the RVZ's header: its hash no longer holds
        EXPECT(Write(root / "header.rvz", header));
        Bytes cut(rvz.begin(), rvz.end() - 100);
        EXPECT(Write(root / "cut.rvz", cut));
        Disc other = MakeDisc();
        other.compression = 2; // bzip2
        HashDisc(other, stored);
        EXPECT(Write(root / "bzip2.rvz", MakeRvz(other, stored, junkA, junkB)));
        Disc crafted = MakeDisc(9);
        HashDisc(crafted, stored);
        EXPECT(Write(root / "fst.rvz", MakeRvz(crafted, stored, junkA, junkB)));
        EXPECT(Write(root / "plain.iso", Bytes(4096, 0)));

        for (const char* name : { "header.rvz", "cut.rvz", "bzip2.rvz", "fst.rvz" })
        {
            disc::WiiDisc wii;
            std::string error;
            EXPECT(!wii.Open(Text(root / name).c_str(), error));
            EXPECT(!error.empty());
            printf("%s: %s\n", name, error.c_str());
            DiscFileSystem files;
            EXPECT(!files.Open(Text(root / name).c_str()));
            EXPECT(files.Files().empty() && files.Wii() == nullptr);
        }
        EXPECT(!disc::RvzImage::IsRvz(Text(root / "plain.iso").c_str()));
    }

    std::filesystem::remove_all(root, removeError);
    printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
