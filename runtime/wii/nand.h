#pragma once

// The console's NAND, as IOS's file system serves it (ISFS): folders and files in a folder of the host, one host file
// for each NAND file. The game's saves, the console's settings and the title's folders live there
// (docs/runtime/wii/ios.md). Paths are the NAND's: absolute, '/'-separated, each name 12 characters at most.
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace wii::nand
{
    // ISFS's results (the SDK's NAND library turns them into its own).
    constexpr int32_t Ok = 0;
    constexpr int32_t Invalid = -101;   // a bad path or argument
    constexpr int32_t NoAccess = -102;  // the mode the file was opened with does not allow it
    constexpr int32_t AlreadyExists = -105;   // created where something already is
    constexpr int32_t NotFound = -106;
    constexpr int32_t NotEmpty = -115;  // a folder deleted with something in it
    constexpr int32_t Failed = -103;    // the host's file system refused

    constexpr uint32_t ClusterSize = 0x4000; // the unit the NAND counts space in

    // An entry's owner and permissions, as the console keeps them (the host keeps none: every entry the title's own,
    // read and write for all).
    struct Attributes
    {
        uint32_t owner = 0;
        uint16_t group = 0;
        uint8_t ownerAccess = 3, groupAccess = 3, otherAccess = 3; // bit 0 read, bit 1 write
        uint8_t flags = 0;
        bool folder = false;
    };

    // A file opened by IOS_Open: mode bit 0 read, bit 1 write.
    class File
    {
    public:
        File(std::FILE* file, uint32_t mode) : m_file(file), m_mode(mode) {}
        ~File();
        File(const File&) = delete;
        File& operator=(const File&) = delete;

        int32_t Read(uint8_t* out, uint32_t size); // the bytes read, or an error
        int32_t Write(const uint8_t* data, uint32_t size); // the bytes written, or an error
        int32_t Seek(int32_t offset, uint32_t whence); // whence 0 start, 1 current, 2 end: the new position
        uint32_t Size();
        uint32_t Position();

    private:
        std::FILE* m_file;
        uint32_t m_mode;
    };

    class FileSystem
    {
    public:
        explicit FileSystem(std::filesystem::path root) : m_root(std::move(root)) {}

        bool CreateRoot(); // the host folder the NAND lives in, made when missing
        int32_t CreateFolder(const std::string& path);
        int32_t CreateFile(const std::string& path);
        int32_t Delete(const std::string& path); // a file, or an empty folder
        int32_t Rename(const std::string& from, const std::string& to);
        int32_t GetAttributes(const std::string& path, Attributes& out);
        int32_t ReadFolder(const std::string& path, std::vector<std::string>& names); // sorted
        // The clusters the files under `path` take and the entries there, the folder itself included.
        int32_t Usage(const std::string& path, uint32_t& clusters, uint32_t& entries);
        int32_t Open(const std::string& path, uint32_t mode, std::unique_ptr<File>& out);

        // A file written whole (made with its folders): the host's own files, such as the settings.
        bool Put(const std::string& path, const std::vector<uint8_t>& data);
        bool Exists(const std::string& path);

        // The host path of a NAND path, false for a path the NAND refuses (not absolute, an empty, "." or ".." name, a
        // name longer than 12 characters, a backslash).
        bool HostPath(const std::string& path, std::filesystem::path& out) const;

    private:
        std::filesystem::path m_root;
    };
}
