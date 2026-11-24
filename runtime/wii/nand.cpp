#include "wii/nand.h"
#include <algorithm>
#include <system_error>

namespace fs = std::filesystem;

namespace wii::nand
{
    File::~File() { std::fclose(m_file); }

    int32_t File::Read(uint8_t* out, uint32_t size)
    {
        if ((m_mode & 1) == 0)
            return NoAccess;
        return int32_t(std::fread(out, 1, size, m_file));
    }

    int32_t File::Write(const uint8_t* data, uint32_t size)
    {
        if ((m_mode & 2) == 0)
            return NoAccess;
        const size_t written = std::fwrite(data, 1, size, m_file);
        std::fflush(m_file);
        return written == size ? int32_t(written) : Failed;
    }

    int32_t File::Seek(int32_t offset, uint32_t whence)
    {
        const int64_t from = whence == 0 ? 0 : whence == 1 ? int64_t(Position()) : whence == 2 ? int64_t(Size()) : -1;
        const int64_t to = from + offset;
        if (from < 0 || to < 0 || to > int64_t(Size())) // the NAND seeks inside the file only
            return Invalid;
        std::fseek(m_file, long(to), SEEK_SET);
        return int32_t(to);
    }

    uint32_t File::Size()
    {
        const long at = std::ftell(m_file);
        std::fseek(m_file, 0, SEEK_END);
        const long size = std::ftell(m_file);
        std::fseek(m_file, at, SEEK_SET);
        return uint32_t(size);
    }

    uint32_t File::Position() { return uint32_t(std::ftell(m_file)); }

    bool FileSystem::HostPath(const std::string& path, fs::path& out) const
    {
        if (path.empty() || path[0] != '/' || path.find('\\') != std::string::npos)
            return false;
        out = m_root;
        size_t at = 1;
        while (at < path.size())
        {
            const size_t end = std::min(path.find('/', at), path.size());
            const std::string name = path.substr(at, end - at);
            if (name.empty() || name == "." || name == ".." || name.size() > 12)
                return false;
            out /= name;
            at = end + 1;
        }
        return path.back() != '/' || path.size() == 1;
    }

    bool FileSystem::CreateRoot()
    {
        std::error_code error;
        fs::create_directories(m_root, error);
        return fs::is_directory(m_root, error);
    }

    int32_t FileSystem::CreateFolder(const std::string& path)
    {
        fs::path host;
        if (!HostPath(path, host) || path == "/")
            return Invalid;
        std::error_code error;
        if (fs::exists(host, error))
            return AlreadyExists;
        if (!fs::is_directory(host.parent_path(), error))
            return NotFound;
        return fs::create_directory(host, error) ? Ok : Failed;
    }

    int32_t FileSystem::CreateFile(const std::string& path)
    {
        fs::path host;
        if (!HostPath(path, host) || path == "/")
            return Invalid;
        std::error_code error;
        if (fs::exists(host, error))
            return AlreadyExists;
        if (!fs::is_directory(host.parent_path(), error))
            return NotFound;
        std::FILE* file = std::fopen(host.string().c_str(), "wb");
        if (file == nullptr)
            return Failed;
        std::fclose(file);
        return Ok;
    }

    int32_t FileSystem::Delete(const std::string& path)
    {
        fs::path host;
        if (!HostPath(path, host) || path == "/")
            return Invalid;
        std::error_code error;
        if (!fs::exists(host, error))
            return NotFound;
        if (fs::is_directory(host, error) && !fs::is_empty(host, error))
            return NotEmpty;
        return fs::remove(host, error) ? Ok : Failed;
    }

    int32_t FileSystem::Rename(const std::string& from, const std::string& to)
    {
        fs::path source, target;
        if (!HostPath(from, source) || !HostPath(to, target) || from == "/" || to == "/")
            return Invalid;
        std::error_code error;
        if (!fs::exists(source, error))
            return NotFound;
        if (!fs::is_directory(target.parent_path(), error))
            return NotFound;
        if (fs::exists(target, error))
        {
            if (fs::is_directory(target, error) != fs::is_directory(source, error))
                return Invalid;
            fs::remove_all(target, error); // a rename replaces what was there
        }
        fs::rename(source, target, error);
        return error ? Failed : Ok;
    }

    int32_t FileSystem::GetAttributes(const std::string& path, Attributes& out)
    {
        fs::path host;
        if (!HostPath(path, host))
            return Invalid;
        std::error_code error;
        if (!fs::exists(host, error))
            return NotFound;
        out = {};
        out.folder = fs::is_directory(host, error);
        return Ok;
    }

    int32_t FileSystem::ReadFolder(const std::string& path, std::vector<std::string>& names)
    {
        fs::path host;
        if (!HostPath(path, host))
            return Invalid;
        std::error_code error;
        if (!fs::exists(host, error))
            return NotFound;
        if (!fs::is_directory(host, error))
            return Invalid;
        names.clear();
        for (const fs::directory_entry& entry : fs::directory_iterator(host, error))
            names.push_back(entry.path().filename().string());
        std::sort(names.begin(), names.end());
        return error ? Failed : Ok;
    }

    int32_t FileSystem::Usage(const std::string& path, uint32_t& clusters, uint32_t& entries)
    {
        fs::path host;
        if (!HostPath(path, host))
            return Invalid;
        std::error_code error;
        if (!fs::is_directory(host, error))
            return fs::exists(host, error) ? Invalid : NotFound;
        clusters = 0;
        entries = 1;
        for (const fs::directory_entry& entry : fs::recursive_directory_iterator(host, error))
        {
            entries++;
            if (entry.is_regular_file(error))
                clusters += uint32_t((entry.file_size(error) + ClusterSize - 1) / ClusterSize);
        }
        return error ? Failed : Ok;
    }

    int32_t FileSystem::Open(const std::string& path, uint32_t mode, std::unique_ptr<File>& out)
    {
        fs::path host;
        if (!HostPath(path, host) || mode > 3)
            return Invalid;
        std::error_code error;
        if (!fs::exists(host, error))
            return NotFound;
        if (fs::is_directory(host, error))
            return Invalid;
        std::FILE* file = std::fopen(host.string().c_str(), (mode & 2) != 0 ? "r+b" : "rb");
        if (file == nullptr)
            return Failed;
        out = std::make_unique<File>(file, mode);
        return Ok;
    }

    bool FileSystem::Put(const std::string& path, const std::vector<uint8_t>& data)
    {
        fs::path host;
        if (!HostPath(path, host))
            return false;
        std::error_code error;
        fs::create_directories(host.parent_path(), error);
        std::FILE* file = std::fopen(host.string().c_str(), "wb");
        if (file == nullptr)
            return false;
        const bool written = std::fwrite(data.data(), 1, data.size(), file) == data.size();
        return std::fclose(file) == 0 && written;
    }

    bool FileSystem::Exists(const std::string& path)
    {
        fs::path host;
        std::error_code error;
        return HostPath(path, host) && fs::exists(host, error);
    }
}
