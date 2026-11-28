// The Wii host: loads a game from its disc image and runs its translated code (docs/runtime/wii/memory_boot.md). The
// NAND folder holds the console's NAND: the game's saves, the console's settings (made on the first run).
//   <host> <disc.rvz> <NAND folder>
#include "memory.h"
#include "platform/platform.h"
#include "ppc_recomp_shared.h"
#include "vfs.h"
#include "disc/wii_disc.h"
#include "wii/boot_info.h"
#include "wii/executable.h"
#include "wii/ios_nand.h"
#include "wii/nand.h"
#include "wii/settings.h"
#include "wii/video.h"
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
    uint8_t* g_base = nullptr;

    // The translated code's function table, after the image: 8 bytes for each 4 of code.
    constexpr uint64_t FunctionTable = PPC_IMAGE_BASE + PPC_IMAGE_SIZE;

    // A fault: where in the guest's address space, then the platform's backtrace (the translated functions' names).
    // One in the function table past its end is an indirect call to code that was not translated (a module loaded at
    // run time): its target.
    void OnFault(const char* description, const void* address)
    {
        const auto* at = static_cast<const uint8_t*>(address);
        const uint64_t guest = g_base != nullptr ? uint64_t(at - g_base) : 0;
        if (g_base != nullptr && at >= g_base + FunctionTable + PPC_CODE_SIZE * 2 && at < g_base + 0x100000000ull)
            fprintf(stderr, "[wii] %s: an indirect call to 0x%08X, outside the translated code\n", description,
                uint32_t(PPC_CODE_BASE + (guest - FunctionTable) / 2));
        else if (g_base != nullptr && at >= g_base && at < g_base + 0x100000000ull)
            fprintf(stderr, "[wii] %s at guest address 0x%08X\n", description, uint32_t(guest));
        else
            fprintf(stderr, "[wii] %s at host address %p\n", description, address);
    }

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

    DiscFileSystem g_disc; // the game's disc, read for as long as it runs

    std::unique_ptr<wii::nand::FileSystem> g_nand;

    bool LoadGame(const char* path, const char* nandFolder, uint8_t*& base, uint32_t& entry)
    {
        DiscFileSystem& disc = g_disc;
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
        std::string error;
        std::vector<wii::Range> extra = { { uint32_t(FunctionTable), uint32_t(PPC_CODE_SIZE * 2 + 8) } };
        if (wii::os::g_game.moduleAreaSize != 0) // the game's module, where its translation is for
        {
            const uint64_t area = wii::os::g_game.moduleArea, end = area + wii::os::g_game.moduleAreaSize;
            if (end > FunctionTable && area < FunctionTable + PPC_CODE_SIZE * 2 + 8)
            {
                fprintf(stderr, "[wii] the module's area 0x%08llX-0x%08llX meets the function table\n",
                    (unsigned long long)area, (unsigned long long)end);
                return false;
            }
            extra.push_back({ wii::os::g_game.moduleArea, wii::os::g_game.moduleAreaSize });
        }
        if (!wii::MapAddressSpace(base, extra, error))
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
        info.settingText = wii::MakeSettingText(char(info.discHeader[3]));
        // The file system table: its offset and size in the partition's header, in words.
        uint8_t words[8];
        if (!wii->Read(0x424, words, sizeof(words)))
            return false;
        const uint64_t fstOffset = uint64_t(uint32_t(words[0]) << 24 | words[1] << 16 | words[2] << 8 | words[3]) << 2;
        const uint64_t fstSize = uint64_t(uint32_t(words[4]) << 24 | words[5] << 16 | words[6] << 8 | words[7]) << 2;
        info.fileSystemTable.resize(size_t(fstSize));
        if (fstSize == 0 || fstSize > wii::Mem1Size / 2 || !wii->Read(fstOffset, info.fileSystemTable.data(), fstSize))
        {
            fprintf(stderr, "[wii] %s: no file system table\n", path);
            return false;
        }
        wii::WriteBootInfo(base, info);
        wii::SetDisc(*wii);
        // The title's ID, from its metadata: the NAND's folder for it, and what ES tells the game.
        std::vector<uint8_t> tmd;
        if (!wii->TitleMetadata(tmd) || tmd.size() < 0x194)
        {
            fprintf(stderr, "[wii] %s: no title metadata\n", path);
            return false;
        }
        uint64_t title = 0;
        for (size_t i = 0x18C; i < 0x194; i++)
            title = title << 8 | tmd[i];
        g_nand = std::make_unique<wii::nand::FileSystem>(nandFolder);
        if (!wii::PrepareNand(*g_nand, title, wii::ConsoleSettings{}, char(info.discHeader[3])))
        {
            fprintf(stderr, "[wii] %s: the NAND folder could not be made\n", nandFolder);
            return false;
        }
        wii::SetNand(*g_nand, title);
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
    PlatformInstallFaultHandler(OnFault);
    if (argc != 3)
    {
        fprintf(stderr, "usage: %s <disc.rvz> <NAND folder>\n", argv[0]);
        return 2;
    }
    uint8_t* base = nullptr;
    uint32_t entry = 0;
    if (!LoadGame(argv[1], argv[2], base, entry))
        return 1;
    g_base = base;
    if (!wii::video::Start("Arkeyan (Wii)", base))
        return 1;
    // The game's first thread, on a host stack deep enough for the translated code's calls.
    PlatformStartThread(64 << 20, [base, entry] {
        wii::os::StartProcessor();
        static PPCContext ctx{};
        ctx.r1.u64 = wii::Mem1Cached + wii::Mem1Size - 0x100; // a scratch stack: the entry sets its own
        PPCFunc* start = PPC_LOOKUP_FUNC(base, entry);
        start(ctx, base);
        fprintf(stderr, "[wii] the entry returned\n");
        wii::StopBoot(1);
    });
    // The main thread keeps the window's events (SDL wants them there) until the game stops or the window closes.
    std::unique_lock lock(g_stopMutex);
    while (!g_stopped.wait_for(lock, std::chrono::milliseconds(10), [] { return g_stop; }))
    {
        lock.unlock();
        const bool open = wii::video::Pump();
        lock.lock();
        if (!open)
            return 0;
    }
    return g_stopCode;
}
