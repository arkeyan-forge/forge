// Checks the write watch: a write to a watched page faults once, proceeds, and makes every range on that page changed;
// pages next to it stay unchanged; WillWrite counts as a write; a page written right after being watched cools down;
// writes from several threads at once are all seen; a file read into watched pages works inside a HostWrite (issue #26).
// usage: ArkeyanWriteWatchTest
#include "write_watch.h"
#include "platform/platform.h"
#include <atomic>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <thread>
#include <vector>

namespace
{
    int g_failures = 0;

#define CHECK(condition)                                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(condition))                                                                                              \
        {                                                                                                              \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition);                                               \
            g_failures++;                                                                                              \
        }                                                                                                              \
    } while (0)

    // Frames past the cool-down of every page the previous checks wrote.
    void SkipFrames(uint64_t frames)
    {
        for (uint64_t i = 0; i < frames; i++)
            writewatch::FrameDone();
    }
}

int main()
{
    constexpr uint64_t Size = 64ull << 20;
    uint8_t* base = PlatformReserveMemory(Size);
    const size_t page = PlatformPageSize();
    CHECK(base != nullptr);
    writewatch::Init(base, Size);
    if (!writewatch::Enabled())
    {
        printf("write watch test: not available on this platform (SKIP)\n");
        return 0;
    }
    uint8_t* memory = base + (1 << 20);

    // Outside the block, at its bottom (the null-pointer guard) or empty: not watched.
    uint8_t local[16];
    CHECK(writewatch::Watch(local, sizeof(local)) == 0);
    CHECK(writewatch::Watch(base, 16) == 0);
    CHECK(writewatch::Watch(memory, 0) == 0);
    CHECK(!writewatch::Unchanged(memory, 16, 0));

    // A range over three pages: unchanged until a write to any byte of them, whatever the page.
    for (size_t offset : { size_t(0), page + 5, 3 * page - 1 })
    {
        uint8_t* range = memory + 8 * page;
        uint64_t stamp = writewatch::Watch(range, 3 * page);
        CHECK(stamp != 0);
        CHECK(writewatch::Unchanged(range, 3 * page, stamp));
        range[offset] = 0x5A; // faults once, then proceeds
        CHECK(range[offset] == 0x5A);
        CHECK(!writewatch::Unchanged(range, 3 * page, stamp));
        range[offset + (offset == 0 ? 1 : -1)] = 1; // the page is writable again: no fault
        SkipFrames(writewatch::HotFrames + writewatch::CoolFrames + 1);
    }

    // A write next to the range, on another page, leaves it unchanged; one on a page it shares changes it.
    {
        uint8_t* range = memory + 20 * page + 16;
        uint64_t stamp = writewatch::Watch(range, 64);
        writewatch::Watch(memory + 21 * page, 64);
        memory[21 * page] = 1;
        CHECK(writewatch::Unchanged(range, 64, stamp));
        memory[20 * page] = 1;
        CHECK(!writewatch::Unchanged(range, 64, stamp));
        SkipFrames(writewatch::HotFrames + writewatch::CoolFrames + 1);
    }

    // A later watch of a written page gives a new stamp that holds until the next write; older stamps stay changed.
    {
        uint8_t* range = memory + 30 * page;
        uint64_t first = writewatch::Watch(range, 128);
        SkipFrames(writewatch::HotFrames);
        range[3] = 7;
        uint64_t second = writewatch::Watch(range, 128);
        CHECK(second != 0);
        CHECK(!writewatch::Unchanged(range, 128, first));
        CHECK(writewatch::Unchanged(range, 128, second));
    }

    // WillWrite: the pages become writable without a fault and count as written.
    {
        uint8_t* range = memory + 40 * page;
        uint64_t stamp = writewatch::Watch(range, 2 * page);
        writewatch::WillWrite(range + page, 8);
        CHECK(!writewatch::Unchanged(range, 2 * page, stamp));
        writewatch::TakeStats();
        memset(range + page, 3, 8);
        CHECK(writewatch::TakeStats().faults == 0);
    }

    // A page written within HotFrames of being watched is not watched again for CoolFrames: Watch returns 0.
    {
        uint8_t* range = memory + 50 * page;
        uint64_t stamp = writewatch::Watch(range, 64);
        CHECK(stamp != 0);
        range[0] = 1;
        CHECK(writewatch::Watch(range, 64) == 0);
        SkipFrames(writewatch::CoolFrames - 1);
        CHECK(writewatch::Watch(range, 64) == 0);
        SkipFrames(2);
        stamp = writewatch::Watch(range, 64);
        CHECK(stamp != 0);
        CHECK(writewatch::Unchanged(range, 64, stamp));
    }

    // Four threads write their own page of a watched range at once, many times: the range is changed, every write
    // landed, and each page faulted at most once.
    {
        SkipFrames(writewatch::HotFrames + writewatch::CoolFrames + 1);
        uint8_t* range = memory + 60 * page;
        writewatch::TakeStats();
        uint64_t stamp = writewatch::Watch(range, 4 * page);
        std::atomic<int> ready{ 0 };
        std::vector<std::thread> threads;
        for (int t = 0; t < 4; t++)
            threads.emplace_back([&, t] {
                ready++;
                while (ready < 4)
                {}
                for (int i = 0; i < 1000; i++)
                    range[t * page + i] = uint8_t(i + t);
            });
        for (std::thread& thread : threads)
            thread.join();
        bool landed = true;
        for (int t = 0; t < 4; t++)
            for (int i = 0; i < 1000; i++)
                landed &= range[t * page + i] == uint8_t(i + t);
        CHECK(landed);
        CHECK(!writewatch::Unchanged(range, 4 * page, stamp));
        CHECK(writewatch::TakeStats().faults <= 4);
    }

    // A file read into guest memory (NtReadFile). WillWrite alone: a Watch of a range on the same pages (a texture's
    // check on the renderer's thread) protects them again before the system call writes, which then fails: the guest
    // saw the end of its file (#26). In a HostWrite that Watch returns 0, the read lands, and the old stamp is changed.
    {
        SkipFrames(writewatch::HotFrames + writewatch::CoolFrames + 1);
        const std::filesystem::path path =
            std::filesystem::temp_directory_path() / ("arkeyan_write_watch_" + std::to_string(uintptr_t(base)));
        std::vector<uint8_t> contents(3 * page);
        for (size_t i = 0; i < contents.size(); i++)
            contents[i] = uint8_t(i * 7 + 1);
        FILE* out = fopen(path.string().c_str(), "wb");
        CHECK(out != nullptr && fwrite(contents.data(), 1, contents.size(), out) == contents.size());
        if (out != nullptr)
            fclose(out);
        PlatformFile file = PlatformOpenFile(path.string().c_str());
        CHECK(file != InvalidPlatformFile);

        uint8_t* buffer = memory + 70 * page + 100;     // a read buffer that shares its pages...
        uint8_t* texture = memory + 70 * page;          // ...with a texture the renderer checks
        const uint32_t bytes = uint32_t(2 * page);
        uint64_t stamp = writewatch::Watch(texture, 64);
        CHECK(stamp != 0);
        writewatch::WillWrite(buffer, bytes);
        SkipFrames(writewatch::HotFrames); // not hot: the page may be watched again
        CHECK(writewatch::Watch(texture, 64) != 0);
        CHECK(PlatformReadFile(file, buffer, bytes, 0) < 0); // what the guest saw before 0.9.6
        SkipFrames(writewatch::HotFrames + writewatch::CoolFrames + 1);

        stamp = writewatch::Watch(texture, 64);
        CHECK(stamp != 0);
        {
            writewatch::HostWrite writing(buffer, bytes);
            SkipFrames(writewatch::HotFrames);
            CHECK(writewatch::Watch(texture, 64) == 0);
            CHECK(PlatformReadFile(file, buffer, bytes, 0) == int64_t(bytes));
        }
        CHECK(memcmp(buffer, contents.data(), bytes) == 0);
        CHECK(!writewatch::Unchanged(texture, 64, stamp));
        SkipFrames(writewatch::HotFrames + writewatch::CoolFrames + 1);
        CHECK(writewatch::Watch(texture, 64) != 0); // watched again once the read is done
        PlatformCloseFile(file);
        std::filesystem::remove(path);
    }

    if (g_failures == 0)
        printf("write watch test: all checks passed\n");
    return g_failures == 0 ? 0 : 1;
}
