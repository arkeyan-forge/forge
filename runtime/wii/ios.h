#pragma once

// IOS, the Wii's second processor's system, answered by the host: the game asks it through the SDK's IOS_* calls (open
// a device or a file by path, then read, write, seek, ioctl, ioctlv on the descriptor), which the host takes over
// (docs/runtime/wii/ios.md). Each device is a class; a path no device serves is not found, as on the console.
#include <climits>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace wii::ios
{
    // IOS's results: a descriptor or a count when >= 0, an error when < 0.
    constexpr int32_t Ok = 0;
    constexpr int32_t NoAccess = -1;
    constexpr int32_t Invalid = -4;
    constexpr int32_t NotFound = -6;
    constexpr int32_t NoDescriptor = -22; // every descriptor in use

    // A buffer in guest memory (an ioctlv's vectors, an ioctl's input and output).
    struct Buffer
    {
        uint32_t address = 0;
        uint32_t size = 0;
    };

    class Device
    {
    public:
        virtual ~Device() = default;
        virtual int32_t Open(uint32_t mode) { return Ok; }
        virtual int32_t Close() { return Ok; }
        virtual int32_t Read(Buffer buffer) { return Invalid; }
        virtual int32_t Write(Buffer buffer) { return Invalid; }
        virtual int32_t Seek(int32_t offset, uint32_t whence) { return Invalid; }
        virtual int32_t Ioctl(uint32_t command, Buffer in, Buffer out) { return Invalid; }
        virtual int32_t Ioctlv(uint32_t command, const std::vector<Buffer>& in, const std::vector<Buffer>& out)
        {
            return Invalid;
        }
        // An asynchronous request this device answers later (an event hook): no reply now.
        static constexpr int32_t Pending = INT32_MIN;
    };

    class Ios
    {
    public:
        // A device served at `path`: each open makes one from `make`.
        void Serve(const std::string& path, std::function<std::unique_ptr<Device>()> make);

        int32_t Open(const std::string& path, uint32_t mode);
        int32_t Close(int32_t fd);
        int32_t Read(int32_t fd, Buffer buffer);
        int32_t Write(int32_t fd, Buffer buffer);
        int32_t Seek(int32_t fd, int32_t offset, uint32_t whence);
        int32_t Ioctl(int32_t fd, uint32_t command, Buffer in, Buffer out);
        int32_t Ioctlv(int32_t fd, uint32_t command, const std::vector<Buffer>& in, const std::vector<Buffer>& out);

        // The path a descriptor was opened with ("" for none): for the log.
        std::string PathOf(int32_t fd) const;

        static constexpr int32_t MaxDescriptors = 32;

    private:
        std::map<std::string, std::function<std::unique_ptr<Device>()>> m_paths;
        struct Open_
        {
            std::string path;
            std::unique_ptr<Device> device;
        };
        std::vector<Open_> m_open = std::vector<Open_>(MaxDescriptors);
        Device* Find(int32_t fd);
    };
}
