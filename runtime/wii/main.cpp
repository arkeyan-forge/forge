// The Wii host: loads a game from its disc image and runs its translated code. For now it boots to the game's first
// request to IOS and stops there (docs/runtime/wii/memory_boot.md).
//   <host> <disc.rvz>
#include "memory.h"
#include "platform/platform.h"
#include "ppc_recomp_shared.h"
#include "vfs.h"
#include "disc/wii_disc.h"
#include "wii/boot_info.h"
#include "wii/executable.h"
#include "wii/memory_map.h"
#include "wii/os.h"
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <thread>
#include <vector>

GuestMemory g_memory; // the guest's memory, for the code it shares with the Xbox host (the guest's printf)

namespace
{
    std::mutex g_stopMutex;
    std::condition_variable g_stopped;
    bool g_stop = false;
    int g_stopCode = 0;

    void Unmapped(PPCContext& ctx, uint8_t* base)
    {
        fprintf(stderr, "[wii] a call to 0x%08X, which is not a translated function (lr=0x%08X)\n", ctx.ctr.u32,
            uint32_t(ctx.lr));
        wii::StopBoot(1);
    }

    bool LoadGame(const char* path, uint8_t*& base, uint32_t& entry)
    {
        DiscFileSystem disc;
        const disc::WiiDisc* wii = disc.Open(path) ? disc.Wii() : nullptr;
        if (wii == nullptr)
        {
            fprintf(stderr, "[wii] %s: not a Wii disc's RVZ image\n", path);
            return false;
        }
        const disc::WiiDisc::File* dol = nullptr;
        for (const auto& file : wii->Files())
            if (file.path == "sys/main.dol")
                dol = &file;
        std::vector<uint8_t> executable(dol != nullptr ? dol->size : 0);
        if (dol == nullptr || !wii->Read(dol->offset, executable.data(), executable.size()))
        {
            fprintf(stderr, "[wii] %s: no executable (sys/main.dol)\n", path);
            return false;
        }
        // The translated code's function table, after the image: 8 bytes for each 4 of code.
        const uint64_t table = PPC_IMAGE_BASE + PPC_IMAGE_SIZE;
        std::string error;
        if (!wii::MapAddressSpace(base, { { uint32_t(table), uint32_t(PPC_CODE_SIZE * 2 + 8) } }, error))
        {
            fprintf(stderr, "[wii] %s\n", error.c_str());
            return false;
        }
        g_memory.base = base;
        wii::Executable loaded;
        if (!wii::LoadExecutable(executable.data(), executable.size(), base, loaded, error))
        {
            fprintf(stderr, "[wii] sys/main.dol: %s\n", error.c_str());
            return false;
        }
        wii::BootInfo info;
        info.ios = wii->Ios();
        if (!wii->Read(0, info.discHeader, sizeof(info.discHeader)))
            return false;
        wii::WriteBootInfo(base, info);
        for (uint64_t address = PPC_CODE_BASE; address < PPC_CODE_BASE + PPC_CODE_SIZE; address += 4)
            PPC_LOOKUP_FUNC(base, address) = Unmapped;
        for (const PPCFuncMapping* mapping = PPCFuncMappings; mapping->host != nullptr; mapping++)
            PPC_LOOKUP_FUNC(base, mapping->guest) = mapping->host;
        entry = loaded.entry;
        fprintf(stderr, "[wii] %s, IOS %u: %zu sections, entry 0x%08X\n", wii->GameId().c_str(), info.ios,
            loaded.sections.size(), entry);
        return true;
    }
} // namespace

void wii::StopBoot(int code)
{
    {
        std::lock_guard lock(g_stopMutex);
        if (!g_stop)
            g_stopCode = code;
        g_stop = true;
    }
    g_stopped.notify_all();
    for (;;) // the guest thread waits here for the process to end
        std::this_thread::sleep_for(std::chrono::hours(1));
}

// The timebase: a quarter of the bus clock, from the host's steady clock.
extern "C" uint64_t ppc_mftb()
{
    using namespace std::chrono;
    static const auto start = steady_clock::now();
    const uint64_t ns = uint64_t(duration_cast<nanoseconds>(steady_clock::now() - start).count());
    return ns / 1000000000 * wii::TimeBaseClock + ns % 1000000000 * wii::TimeBaseClock / 1000000000;
}

extern "C" void ppc_spin_hint() {}

int main(int argc, char** argv)
{
    PlatformInit();
    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <disc.rvz>\n", argv[0]);
        return 2;
    }
    uint8_t* base = nullptr;
    uint32_t entry = 0;
    if (!LoadGame(argv[1], base, entry))
        return 1;
    // The game's first thread, on a host stack deep enough for the translated code's calls.
    PlatformStartThread(64 << 20, [base, entry] {
        static PPCContext ctx{};
        ctx.r1.u64 = wii::Mem1Cached + wii::Mem1Size - 0x100; // a scratch stack: the entry sets its own
        PPCFunc* start = PPC_LOOKUP_FUNC(base, entry);
        start(ctx, base);
        fprintf(stderr, "[wii] the entry returned\n");
        wii::StopBoot(1);
    });
    std::unique_lock lock(g_stopMutex);
    g_stopped.wait(lock, [] { return g_stop; });
    return g_stopCode;
}
