// /dev/fs, the NAND's files and /dev/es on the host's NAND (wii/nand.h). The requests' layouts are the SDK's (its ISFS
// and ES clients in the game): a path is 64 bytes; an entry's attributes are 76 bytes (owner, group, path at 6, the
// three permissions at 70, the flags at 73).
#include "wii/ios_nand.h"
#include "memory.h"
#include "wii/nand.h"
#include "wii/settings.h"
#include <cstdio>
#include <cstring>
#include <set>

namespace wii
{
    namespace
    {
        uint8_t* At(ios::Buffer buffer, uint32_t size)
        {
            return buffer.address != 0 && buffer.size >= size
                       ? static_cast<uint8_t*>(g_memory.Translate(buffer.address))
                       : nullptr;
        }

        void Put32(uint8_t* p, uint32_t value)
        {
            p[0] = uint8_t(value >> 24), p[1] = uint8_t(value >> 16), p[2] = uint8_t(value >> 8), p[3] = uint8_t(value);
        }
        uint32_t Get32(const uint8_t* p)
        {
            return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3];
        }

        // A path in a request: up to its NUL, 64 bytes at most; empty when it has none.
        std::string PathAt(const uint8_t* p, size_t size)
        {
            size_t length = 0;
            while (length < size && length < 64 && p[length] != 0)
                length++;
            return length < size && length < 64 ? std::string(reinterpret_cast<const char*>(p), length) : std::string();
        }

        constexpr uint32_t PathSize = 64, AttributesSize = 76;

        void Unknown(const char* device, uint32_t command)
        {
            static std::set<std::string> logged;
            const std::string what = std::string(device) + " " + std::to_string(command);
            if (logged.insert(what).second)
                fprintf(stderr, "[ios] %s: request %u not known\n", device, command);
        }

        class FileSystemDevice : public ios::Device
        {
        public:
            explicit FileSystemDevice(nand::FileSystem& nand) : m_nand(nand) {}

            int32_t Ioctl(uint32_t command, ios::Buffer in, ios::Buffer out) override
            {
                switch (command)
                {
                case 2: // GetStats: the NAND's clusters and entries, plenty free
                {
                    uint8_t* stats = At(out, 28);
                    if (stats == nullptr)
                        return nand::Invalid;
                    memset(stats, 0, 28);
                    Put32(stats, nand::ClusterSize);
                    Put32(stats + 4, 0x4000); // free clusters
                    Put32(stats + 20, 0x1000); // free entries
                    return nand::Ok;
                }
                case 3: // CreateDir
                case 9: // CreateFile
                {
                    const uint8_t* attributes = At(in, AttributesSize);
                    if (attributes == nullptr)
                        return nand::Invalid;
                    const std::string path = PathAt(attributes + 6, PathSize);
                    return command == 3 ? m_nand.CreateFolder(path) : m_nand.CreateFile(path);
                }
                case 5: // SetAttr: the host keeps no owners or permissions
                {
                    const uint8_t* attributes = At(in, AttributesSize);
                    nand::Attributes kept;
                    return attributes == nullptr ? nand::Invalid
                                                 : m_nand.GetAttributes(PathAt(attributes + 6, PathSize), kept);
                }
                case 6: // GetAttr
                {
                    const uint8_t* path = At(in, 1);
                    uint8_t* attributes = At(out, AttributesSize);
                    if (path == nullptr || attributes == nullptr)
                        return nand::Invalid;
                    const std::string name = PathAt(path, in.size);
                    nand::Attributes found;
                    const int32_t result = m_nand.GetAttributes(name, found);
                    if (result != nand::Ok)
                        return result;
                    memset(attributes, 0, AttributesSize);
                    Put32(attributes, found.owner);
                    attributes[4] = uint8_t(found.group >> 8), attributes[5] = uint8_t(found.group);
                    memcpy(attributes + 6, name.data(), name.size());
                    attributes[70] = found.ownerAccess, attributes[71] = found.groupAccess;
                    attributes[72] = found.otherAccess, attributes[73] = found.flags;
                    return nand::Ok;
                }
                case 7: // Delete
                {
                    const uint8_t* path = At(in, 1);
                    return path == nullptr ? nand::Invalid : m_nand.Delete(PathAt(path, in.size));
                }
                case 8: // Rename: the old path, then the new one at 64
                {
                    const uint8_t* paths = At(in, 2 * PathSize);
                    return paths == nullptr
                               ? nand::Invalid
                               : m_nand.Rename(PathAt(paths, PathSize), PathAt(paths + PathSize, PathSize));
                }
                case 13: // Shutdown: the host writes each file as it goes
                    return nand::Ok;
                default: Unknown("/dev/fs", command); return nand::Invalid;
                }
            }

            int32_t Ioctlv(
                uint32_t command, const std::vector<ios::Buffer>& in, const std::vector<ios::Buffer>& out) override
            {
                if (in.empty() || At(in[0], 1) == nullptr)
                    return nand::Invalid;
                const std::string path = PathAt(At(in[0], 1), in[0].size);
                if (command == 4) // ReadDir: the count; with a buffer, as many names (NUL after each) as it allows
                {
                    std::vector<std::string> names;
                    const int32_t result = m_nand.ReadFolder(path, names);
                    if (result != nand::Ok)
                        return result;
                    uint8_t* count = out.empty() ? nullptr : At(out.back(), 4);
                    if (count == nullptr)
                        return nand::Invalid;
                    if (out.size() >= 2 && in.size() >= 2 && At(in[1], 4) != nullptr)
                    {
                        const size_t most = std::min<size_t>(Get32(At(in[1], 4)), names.size());
                        uint8_t* list = At(out[0], 0);
                        size_t at = 0, written = 0;
                        for (; written < most && list != nullptr && at + names[written].size() + 1 <= out[0].size;
                            written++)
                        {
                            memcpy(list + at, names[written].c_str(), names[written].size() + 1);
                            at += names[written].size() + 1;
                        }
                        Put32(count, uint32_t(written));
                    }
                    else
                        Put32(count, uint32_t(names.size()));
                    return nand::Ok;
                }
                if (command == 12) // GetUsage: the clusters, then the entries
                {
                    uint32_t clusters = 0, entries = 0;
                    if (out.size() < 2 || At(out[0], 4) == nullptr || At(out[1], 4) == nullptr)
                        return nand::Invalid;
                    const int32_t result = m_nand.Usage(path, clusters, entries);
                    if (result == nand::Ok)
                    {
                        Put32(At(out[0], 4), clusters);
                        Put32(At(out[1], 4), entries);
                    }
                    return result;
                }
                Unknown("/dev/fs", command);
                return nand::Invalid;
            }

        private:
            nand::FileSystem& m_nand;
        };

        class NandFile : public ios::Device
        {
        public:
            NandFile(nand::FileSystem& nand, std::string path) : m_nand(nand), m_path(std::move(path)) {}

            int32_t Open(uint32_t mode) override { return m_nand.Open(m_path, mode, m_file); }
            int32_t Read(ios::Buffer buffer) override
            {
                uint8_t* data = At(buffer, 0);
                return data == nullptr && buffer.size != 0 ? nand::Invalid : m_file->Read(data, buffer.size);
            }
            int32_t Write(ios::Buffer buffer) override
            {
                const uint8_t* data = At(buffer, 0);
                return data == nullptr && buffer.size != 0 ? nand::Invalid : m_file->Write(data, buffer.size);
            }
            int32_t Seek(int32_t offset, uint32_t whence) override { return m_file->Seek(offset, whence); }
            int32_t Ioctl(uint32_t command, ios::Buffer in, ios::Buffer out) override
            {
                uint8_t* stats = At(out, 8);
                if (command != 11 || stats == nullptr) // GetFileStats: the size, then the position
                    return nand::Invalid;
                Put32(stats, m_file->Size());
                Put32(stats + 4, m_file->Position());
                return nand::Ok;
            }

        private:
            nand::FileSystem& m_nand;
            std::string m_path;
            std::unique_ptr<nand::File> m_file;
        };

        constexpr int32_t EsInvalid = -1017;
        constexpr const char* SettingTextPath = "/title/00000001/00000002/data/setting.txt"; // the system menu's

        class TitleService : public ios::Device
        {
        public:
            explicit TitleService(uint64_t titleId) : m_title(titleId) {}

            int32_t Ioctlv(
                uint32_t command, const std::vector<ios::Buffer>& in, const std::vector<ios::Buffer>& out) override
            {
                switch (command)
                {
                case 32: // GetTitleID
                {
                    uint8_t* id = out.empty() ? nullptr : At(out[0], 8);
                    if (id == nullptr)
                        return EsInvalid;
                    Put32(id, uint32_t(m_title >> 32));
                    Put32(id + 4, uint32_t(m_title));
                    return ios::Ok;
                }
                case 29: // GetDataDir: the title's data folder, 30 bytes
                {
                    const uint8_t* id = in.empty() ? nullptr : At(in[0], 8);
                    uint8_t* folder = out.empty() ? nullptr : At(out[0], 30);
                    if (id == nullptr || folder == nullptr)
                        return EsInvalid;
                    const std::string path = TitleDataFolder(uint64_t(Get32(id)) << 32 | Get32(id + 4));
                    memset(folder, 0, 30);
                    memcpy(folder, path.data(), std::min<size_t>(path.size(), 29));
                    return ios::Ok;
                }
                default: Unknown("/dev/es", command); return EsInvalid;
                }
            }

        private:
            uint64_t m_title;
        };
    } // namespace

    std::string TitleDataFolder(uint64_t titleId)
    {
        char path[40];
        snprintf(path, sizeof(path), "/title/%08x/%08x/data", uint32_t(titleId >> 32), uint32_t(titleId));
        return path;
    }

    std::unique_ptr<ios::Device> MakeFileSystemDevice(nand::FileSystem& nand)
    {
        return std::make_unique<FileSystemDevice>(nand);
    }

    std::unique_ptr<ios::Device> MakeNandFile(nand::FileSystem& nand, const std::string& path)
    {
        return std::make_unique<NandFile>(nand, path);
    }

    std::unique_ptr<ios::Device> MakeTitleService(uint64_t titleId) { return std::make_unique<TitleService>(titleId); }

    bool PrepareNand(nand::FileSystem& nand, uint64_t titleId, const ConsoleSettings& settings, char region)
    {
        bool ready = nand.CreateRoot();
        for (const std::string& folder : { std::string("/tmp"), std::string("/shared2"), std::string("/shared2/sys"),
                 std::string("/title"), TitleDataFolder(titleId).substr(0, 15), TitleDataFolder(titleId).substr(0, 24),
                 TitleDataFolder(titleId) })
            if (!nand.Exists(folder))
                ready = nand.CreateFolder(folder) == nand::Ok && ready;
        if (!nand.Exists("/shared2/sys/SYSCONF"))
            ready = nand.Put("/shared2/sys/SYSCONF", MakeSysconf(settings)) && ready;
        if (!nand.Exists(SettingTextPath))
            ready = nand.Put(SettingTextPath, MakeSettingText(region)) && ready;
        return ready;
    }
}
