// The SDK's IOS_* calls on the host's IOS (wii/ios.h): the guest's arguments from its registers, the result in r3. An
// asynchronous call returns at once and its callback runs later, as IOS's reply interrupt would make it run: an event
// for the processor's holder, on a copy of its registers (docs/runtime/wii/ios.md).
#include "wii/ios.h"
#include "wii/os.h"
#include "ppc_recomp_shared.h"
#include <cstdio>
#include <map>
#include <mutex>
#include <string>
#include <utility>

namespace wii::os
{
    namespace
    {
        uint32_t Read32(const uint8_t* base, uint32_t address)
        {
            const uint8_t* p = base + address;
            return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3];
        }

        std::string GuestPath(const uint8_t* base, uint32_t address)
        {
            std::string path;
            for (char c; address != 0 && path.size() < 64 && (c = char(base[address])) != 0; address++)
                path += c;
            return path;
        }

        // Each kind of request logged its first few times, then counted silently.
        void Log(const std::string& what, int32_t result)
        {
            static std::mutex mutex;
            static std::map<std::string, int> seen;
            std::lock_guard lock(mutex);
            if (++seen[what] > 3)
                return;
            if (result == ios::Device::Pending)
                fprintf(stderr, "[ios] %s: pending\n", what.c_str());
            else
                fprintf(stderr, "[ios] %s -> %d\n", what.c_str(), result);
        }

        void Reply(PPCContext& ctx, uint8_t* base, int32_t result, uint32_t callback, uint32_t argument)
        {
            if (callback == 0 || result == ios::Device::Pending)
            {
                ctx.r3.u64 = uint64_t(int64_t(result == ios::Device::Pending ? ios::Ok : result));
                return;
            }
            PostEvent([base, result, callback, argument] { CallGuest(base, callback, uint32_t(result), argument); });
            ctx.r3.u64 = ios::Ok;
        }

        std::string Name(int32_t fd)
        {
            const std::string path = Hosted().PathOf(fd);
            return path.empty() ? "fd " + std::to_string(fd) : path;
        }
    } // namespace

    void IosOpen(PPCContext& ctx, uint8_t* base, bool async)
    {
        const std::string path = GuestPath(base, ctx.r3.u32);
        const int32_t result = Hosted().Open(path, ctx.r4.u32);
        Log("open " + path, result);
        Reply(ctx, base, result, async ? ctx.r5.u32 : 0, ctx.r6.u32);
    }

    void IosClose(PPCContext& ctx, uint8_t* base, bool async)
    {
        const int32_t fd = ctx.r3.s32;
        const std::string name = Name(fd);
        const int32_t result = Hosted().Close(fd);
        Log("close " + name, result);
        Reply(ctx, base, result, async ? ctx.r4.u32 : 0, ctx.r5.u32);
    }

    void IosRead(PPCContext& ctx, uint8_t* base, bool async)
    {
        const int32_t result = Hosted().Read(ctx.r3.s32, { ctx.r4.u32, ctx.r5.u32 });
        Log("read " + Name(ctx.r3.s32), result);
        Reply(ctx, base, result, async ? ctx.r6.u32 : 0, ctx.r7.u32);
    }

    void IosWrite(PPCContext& ctx, uint8_t* base, bool async)
    {
        const int32_t result = Hosted().Write(ctx.r3.s32, { ctx.r4.u32, ctx.r5.u32 });
        Log("write " + Name(ctx.r3.s32), result);
        Reply(ctx, base, result, async ? ctx.r6.u32 : 0, ctx.r7.u32);
    }

    void IosSeek(PPCContext& ctx, uint8_t* base, bool async)
    {
        const int32_t result = Hosted().Seek(ctx.r3.s32, ctx.r4.s32, ctx.r5.u32);
        Log("seek " + Name(ctx.r3.s32), result);
        Reply(ctx, base, result, async ? ctx.r6.u32 : 0, ctx.r7.u32);
    }

    void IosIoctl(PPCContext& ctx, uint8_t* base, bool async)
    {
        const int32_t fd = ctx.r3.s32;
        const uint32_t command = ctx.r4.u32;
        const int32_t result = Hosted().Ioctl(fd, command, { ctx.r5.u32, ctx.r6.u32 }, { ctx.r7.u32, ctx.r8.u32 });
        Log("ioctl " + Name(fd) + " " + std::to_string(command), result);
        Reply(ctx, base, result, async ? ctx.r9.u32 : 0, ctx.r10.u32);
    }

    void IosIoctlv(PPCContext& ctx, uint8_t* base, bool async)
    {
        const int32_t fd = ctx.r3.s32;
        const uint32_t command = ctx.r4.u32, inCount = ctx.r5.u32, outCount = ctx.r6.u32, vectors = ctx.r7.u32;
        std::vector<ios::Buffer> in, out;
        for (uint32_t i = 0; i < inCount + outCount && i < 32; i++)
        {
            const ios::Buffer buffer = { Read32(base, vectors + 8 * i), Read32(base, vectors + 8 * i + 4) };
            (i < inCount ? in : out).push_back(buffer);
        }
        const int32_t result = Hosted().Ioctlv(fd, command, in, out);
        Log("ioctlv " + Name(fd) + " " + std::to_string(command), result);
        Reply(ctx, base, result, async ? ctx.r8.u32 : 0, ctx.r9.u32);
    }
}
