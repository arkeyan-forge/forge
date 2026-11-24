#include "wii/ios.h"

namespace wii::ios
{
    void Ios::Serve(const std::string& path, std::function<std::unique_ptr<Device>()> make)
    {
        m_paths[path] = std::move(make);
    }

    void Ios::ServeFiles(std::function<std::unique_ptr<Device>(const std::string& path)> make)
    {
        m_files = std::move(make);
    }

    int32_t Ios::Open(const std::string& path, uint32_t mode)
    {
        auto served = m_paths.find(path);
        const bool file = served == m_paths.end() && m_files && path.rfind("/dev/", 0) != 0;
        if (served == m_paths.end() && !file)
            return NotFound;
        for (int32_t fd = 0; fd < MaxDescriptors; fd++)
        {
            if (m_open[size_t(fd)].device != nullptr)
                continue;
            std::unique_ptr<Device> device = file ? m_files(path) : served->second();
            const int32_t result = device->Open(mode);
            if (result < 0)
                return result;
            m_open[size_t(fd)] = { path, std::move(device) };
            return fd;
        }
        return NoDescriptor;
    }

    Device* Ios::Find(int32_t fd) { return fd >= 0 && fd < MaxDescriptors ? m_open[size_t(fd)].device.get() : nullptr; }

    int32_t Ios::Close(int32_t fd)
    {
        Device* device = Find(fd);
        if (device == nullptr)
            return Invalid;
        const int32_t result = device->Close();
        m_open[size_t(fd)] = {};
        return result;
    }

    int32_t Ios::Read(int32_t fd, Buffer buffer)
    {
        Device* device = Find(fd);
        return device != nullptr ? device->Read(buffer) : Invalid;
    }

    int32_t Ios::Write(int32_t fd, Buffer buffer)
    {
        Device* device = Find(fd);
        return device != nullptr ? device->Write(buffer) : Invalid;
    }

    int32_t Ios::Seek(int32_t fd, int32_t offset, uint32_t whence)
    {
        Device* device = Find(fd);
        return device != nullptr ? device->Seek(offset, whence) : Invalid;
    }

    int32_t Ios::Ioctl(int32_t fd, uint32_t command, Buffer in, Buffer out)
    {
        Device* device = Find(fd);
        return device != nullptr ? device->Ioctl(command, in, out) : Invalid;
    }

    int32_t Ios::Ioctlv(int32_t fd, uint32_t command, const std::vector<Buffer>& in, const std::vector<Buffer>& out)
    {
        Device* device = Find(fd);
        return device != nullptr ? device->Ioctlv(command, in, out) : Invalid;
    }

    std::string Ios::PathOf(int32_t fd) const
    {
        return fd >= 0 && fd < MaxDescriptors ? m_open[size_t(fd)].path : std::string();
    }
}
