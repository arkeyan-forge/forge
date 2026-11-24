#include "netplay/clock.h"
#include "netplay/delivery.h"
#include "netplay/events.h"
#include "netplay/inputs.h"
#include "netplay/state_hash.h"
#include "heap_walk.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <thread>

namespace
{
    int g_failures = 0;

    void Check(bool condition, const char* message)
    {
        if (!condition)
        {
            fprintf(stderr, "FAIL: %s\n", message);
            ++g_failures;
        }
    }

    void ClockTest()
    {
        std::array<uint64_t, 64> times{}, ticks{};
        for (int run = 0; run < 2; ++run)
        {
            netplay::Clock clock;
            netplay::ReadCursor cursor;
            size_t index = 0;
            for (const uint32_t interval : { 0u, 1u, 2u, 0x80000000u })
            {
                clock.Advance(interval);
                for (int read = 0; read < 16; ++read, ++index)
                {
                    const auto time = clock.SystemTime();
                    const auto tick = cursor.Read(clock.Steps());
                    if (run == 0)
                    {
                        times[index] = time;
                        ticks[index] = tick;
                    }
                    else
                        Check(time == times[index] && tick == ticks[index], "clock replay identical");
                }
            }
            Check(clock.Steps() == 5, "present intervals");
            Check(clock.SystemTime() == netplay::EpochFileTime + 833333, "fractional FILETIME");
        }
        Check(std::is_sorted(ticks.begin(), ticks.end()), "monotonic clock");
        netplay::ReadCursor busy;
        uint64_t last = 0;
        for (uint64_t i = 0; i < 900000; ++i)
        {
            const uint64_t tick = busy.Read(i == 899999 ? 1 : 0);
            Check(tick > last, "busy wait and frame crossover monotonic");
            last = tick;
        }
        netplay::Clock second;
        for (int i = 0; i < 60; ++i)
            second.Advance(1);
        Check(second.Elapsed() == 10000000, "no frame rounding drift");
        // A host file's write time is the host's: two replays must still read the same one.
        Check(netplay::HostFileTime(true, 5000, 1234) == netplay::EpochFileTime &&
                  netplay::HostFileTime(true, 5000, 99) == netplay::EpochFileTime,
            "host file times do not depend on the host clock");
        Check(
            netplay::HostFileTime(false, 5000, 1234) == 3766, "host file times follow the file's age outside netplay");
        Check(netplay::TitleDiskSpace(true, 7, 3).capacity == 120ull << 30 &&
                  netplay::TitleDiskSpace(true, 9, 1).available == 100ull << 30,
            "a deterministic run's disk is the same whatever the host's");
        Check(netplay::TitleDiskSpace(false, 7, 3).capacity == 7 && netplay::TitleDiskSpace(false, 7, 3).available == 3,
            "outside netplay the title sees the host's disk");
        // A cooperative flip waits for its blanks' wall time; a late one shows at once and paces the next from now.
        Check(netplay::FlipDue(0, 7000, 2, 100) == 7000, "the first cooperative flip is not held");
        Check(netplay::FlipDue(1000, 1050, 2, 100) == 1200,
            "a flip of two blanks is shown two blank periods after the last");
        Check(netplay::FlipDue(1000, 1050, 0, 100) == 1100, "a flip without a blank still waits one");
        Check(netplay::FlipDue(1000, 1300, 1, 100) == 1300, "a late flip is shown at once");
        std::array<std::array<uint64_t, 100>, 2> sequences{};
        std::thread a([&] {
            for (auto& tick : sequences[0])
                tick = netplay::ReadTimeBase();
        });
        std::thread b([&] {
            for (auto& tick : sequences[1])
                tick = netplay::ReadTimeBase();
        });
        a.join();
        b.join();
        Check(sequences[0] == sequences[1], "independent thread read sequences");
        for (size_t i = 1; i < sequences[0].size(); ++i)
            Check(sequences[0][i] > sequences[0][i - 1], "per thread monotonic");
    }

    void InputsTest()
    {
        netplay::InputFrame frame{};
        frame[0] = { { 0xA123, 1, 255, -32768, 32767, -1, 1234 }, true };
        frame[1] = { { 0x1234, 44, 88, 100, -200, 300, -400 }, false };
        auto bytes = netplay::InputHeader("probe", 2);
        const auto encoded = netplay::EncodeFrame(frame);
        bytes.insert(bytes.end(), encoded.begin(), encoded.end());
        frame[1].connected = true;
        const auto encoded2 = netplay::EncodeFrame(frame);
        bytes.insert(bytes.end(), encoded2.begin(), encoded2.end());
        netplay::Replay replay;
        std::string error;
        Check(netplay::ParseReplay(bytes, replay, error), "round trip parse");
        Check(replay.gameId == "probe" && replay.frames.size() == 2, "header fields");
        Check(netplay::EncodeFrame(replay.frames[0]) == encoded, "all pad fields round trip");
        Check(netplay::EncodeFrame(replay.frames[1]) == encoded2, "connections round trip");
        const auto neutral = netplay::ReplayFrame(replay, 100);
        Check(neutral[0].state.buttons == 0 && neutral[1].state.thumbLX == 0 && neutral[0].connected &&
                  neutral[1].connected,
            "EOF neutral with recorded connections");
        netplay::InputLatch latch;
        uint32_t packet = 0;
        latch.Latch(frame);
        Check(latch.Get(0, packet).state.buttons == frame[0].state.buttons && packet == 1, "latched state");
        latch.Latch(frame);
        latch.Get(0, packet);
        Check(packet == 1, "unchanged packet");
        ++frame[0].state.thumbRX;
        latch.Latch(frame);
        latch.Get(0, packet);
        Check(packet == 2, "changed packet");
        frame[0].connected = false;
        latch.Latch(frame);
        Check(!latch.Get(0, packet).connected && packet == 3, "connection changes packet");
        Check(!latch.Get(MaxUsers, packet).connected && !latch.Get(-1, packet).connected, "invalid users");
        for (size_t size = 0; size < bytes.size(); ++size)
            Check(!netplay::ParseReplay(std::span(bytes).first(size), replay, error), "every truncation refused");
        for (size_t offset : { size_t(0), size_t(8), size_t(12), size_t(16), size_t(24), size_t(28), size_t(33) })
        {
            auto bad = bytes;
            bad[offset] = 0xFF;
            Check(!netplay::ParseReplay(bad, replay, error) && !error.empty(), "malformed field refused");
        }
        auto bad = bytes;
        std::fill(bad.begin() + 16, bad.begin() + 24, 0xFF);
        Check(!netplay::ParseReplay(bad, replay, error), "oversize frame count refused");
        bad = bytes;
        bad.push_back(0);
        Check(!netplay::ParseReplay(bad, replay, error), "trailing bytes refused");
        auto empty = netplay::InputHeader("probe", 0);
        Check(netplay::ParseReplay(empty, replay, error), "empty recording accepted");
        Check(!netplay::ReplayFrame(replay, 0)[0].connected, "empty recording disconnected");
        std::mt19937 random(0x25);
        for (int i = 0; i < 10000; ++i)
        {
            std::vector<uint8_t> noise(random() % 1024);
            for (auto& byte : noise)
                byte = uint8_t(random());
            Check(!netplay::ParseReplay(noise, replay, error), "random bytes refused");
        }
    }

    void HashTest()
    {
        std::vector<uint8_t> bytes(0x20002, 0x5A);
        std::vector<netplay::Range> skips;
        std::string error;
        {
            std::array<uint8_t, 20> host{}, guest{};
            host.fill(0x91);
            guest.fill(0x42);
            const std::array<netplay::Range, 3> excluded{ { { 0xFF, 0x103 }, { 0x107, 0x10B }, { 0x111, 0x120 } } };
            std::vector<uint8_t> packed;
            netplay::PackBlock(host, 0x100, excluded, packed);
            Check(packed.size() == 10, "repair sends only hashed bytes across skip boundaries");
            Check(!netplay::ApplyBlock(guest, 0x100, excluded, std::span(packed).first(9)),
                "repair refuses truncated bytes before writing");
            Check(std::all_of(guest.begin(), guest.end(), [](uint8_t byte) { return byte == 0x42; }),
                "invalid repair leaves memory untouched");
            Check(netplay::ApplyBlock(guest, 0x100, excluded, packed), "repair writes packed bytes");
            for (size_t i = 0; i < guest.size(); ++i)
                Check(guest[i] == ((i < 3 || (i >= 7 && i < 11) || i >= 17) ? 0x42 : 0x91),
                    "repair preserves all excluded bytes");
            Check(netplay::HashBytes(host, 0x100, excluded) == netplay::HashBytes(guest, 0x100, excluded),
                "packed repair restores the hashed state");
        }
        {
            // Every byte counts, whichever of the four lanes reads it, and words do not trade lanes unseen.
            std::vector<uint8_t> lanes(96);
            for (size_t i = 0; i < lanes.size(); ++i)
                lanes[i] = uint8_t(i * 13 + 1);
            const uint64_t base = netplay::HashBytes(lanes, 0x2000, {});
            bool all = true;
            for (size_t i = 0; i < lanes.size(); ++i)
            {
                lanes[i] ^= 0x40;
                all = all && netplay::HashBytes(lanes, 0x2000, {}) != base;
                lanes[i] ^= 0x40;
            }
            Check(all, "a change at any offset of the lanes changes the hash");
            std::swap_ranges(lanes.begin(), lanes.begin() + 8, lanes.begin() + 8);
            Check(netplay::HashBytes(lanes, 0x2000, {}) != base, "two words swapped between lanes change the hash");
        }
        {
            // A checkpoint spread over 30 frames: its parts, hashed one after the other, give the whole hash's blocks.
            std::vector<uint8_t> first(3u << 20), second(700000), third(5u << 20);
            for (auto* memory : { &first, &second, &third })
                for (size_t i = 0; i < memory->size(); ++i)
                    (*memory)[i] = uint8_t((i * 2654435761u) >> 11);
            const std::vector<netplay::Allocation> all = { { first, 0x10000, netplay::Heap::Virtual4K },
                { second, 0x40008000, netplay::Heap::Virtual64K }, { third, 0x50000000, netplay::Heap::Physical } };
            const std::vector<netplay::Range> holes = { { 0x40010000, 0x40010010 } };
            std::vector<netplay::Block> whole, spread;
            netplay::HashAllocations(all, holes, whole);
            std::array<std::vector<netplay::Allocation>, 30> parts;
            netplay::SliceAllocations(all, 0x10000, parts);
            size_t nonEmpty = 0;
            for (const auto& part : parts)
            {
                nonEmpty += !part.empty();
                netplay::HashAllocations(part, holes, spread);
            }
            Check(spread == whole && nonEmpty >= 25, "a checkpoint spread over its frames hashes the same blocks");
        }
        {
            // The probe's walk over a heap: the allocation holding the address, else the next one.
            const std::map<uint32_t, uint32_t> used = { { 0x10000, 0x2000 }, { 0x40000, 0x1000 } };
            Check(AllocationAtOrAfter(used, 0x10000)->first == 0x10000 &&
                      AllocationAtOrAfter(used, 0x11FFF)->first == 0x10000 &&
                      AllocationAtOrAfter(used, 0x12000)->first == 0x40000 &&
                      AllocationAtOrAfter(used, 0x0)->first == 0x10000 &&
                      AllocationAtOrAfter(used, 0x41000) == used.end(),
                "allocation at or after an address");
        }
        const uint64_t original = netplay::HashBytes(bytes, 0x1FFFF, skips);
        Check(original == netplay::HashBytes(bytes, 0x1FFFF, skips), "stable hash");
        bytes[10] ^= 1;
        Check(original != netplay::HashBytes(bytes, 0x1FFFF, skips), "one byte changes hash");
        Check(netplay::ParseSkips("20000-20010,0x20008-0x20020,40000-40001", skips, error), "skip parse");
        Check(skips.size() == 2 && skips[0].end == 0x20020, "skip merging");
        const auto skipped = netplay::HashBytes(bytes, 0x1FFFF, skips);
        bytes[10] ^= 1;
        Check(skipped == netplay::HashBytes(bytes, 0x1FFFF, skips), "skipped byte ignored");
        bytes[33] ^= 1;
        Check(skipped != netplay::HashBytes(bytes, 0x1FFFF, skips), "skip end is exclusive");
        bytes[33] ^= 1;
        bytes[0] ^= 1;
        Check(skipped != netplay::HashBytes(bytes, 0x1FFFF, skips), "range begins precisely");
        std::vector<netplay::Block> blocks;
        {
            // A large allocation is hashed on several threads: the same blocks, in order, as one block at a time.
            std::vector<uint8_t> large(5u << 20);
            for (size_t i = 0; i < large.size(); ++i)
                large[i] = uint8_t(i * 2654435761u >> 13);
            const std::vector<netplay::Range> holes = { { 0x40123456, 0x40123500 } };
            std::vector<netplay::Block> threaded;
            netplay::HashAllocation(large, 0x40100000, netplay::Heap::Physical, holes, threaded);
            bool same = threaded.size() == large.size() / 0x10000;
            for (size_t i = 0; same && i < threaded.size(); ++i)
            {
                const uint32_t at = 0x40100000 + uint32_t(i) * 0x10000;
                same =
                    threaded[i].address == at && threaded[i].heap == netplay::Heap::Physical &&
                    threaded[i].hash == netplay::HashBytes(std::span(large).subspan(i * 0x10000, 0x10000), at, holes);
            }
            Check(same, "threaded block hashes equal one block at a time");
        }
        netplay::HashAllocation(bytes, 0x1FFFF, netplay::Heap::Virtual4K, {}, blocks);
        Check(blocks.size() == 4 && blocks[0].address == 0x1FFFF && blocks[1].address == 0x20000 &&
                  blocks[2].address == 0x30000 && blocks[3].address == 0x40000,
            "block edges");
        blocks.clear();
        {
            // Render-only memory a game marks during a session: kept sorted and merged, a covered range is no change.
            std::vector<netplay::Range> marked;
            Check(netplay::AddSkip(marked, { 0x3000, 0x4000 }) && netplay::AddSkip(marked, { 0x1000, 0x2000 }),
                "skips added");
            Check(!netplay::AddSkip(marked, { 0x3100, 0x3200 }) && !netplay::AddSkip(marked, { 0x5000, 0x5000 }),
                "a covered or empty skip changes nothing");
            Check(netplay::AddSkip(marked, { 0x1800, 0x3000 }) && marked.size() == 1 && marked[0].begin == 0x1000 &&
                      marked[0].end == 0x4000,
                "a skip touching two others merges them");
            Check(netplay::AddSkip(marked, { 0x8000, 0x9000 }) && netplay::AddSkip(marked, { 0x6000, 0x7000 }) &&
                      marked.size() == 3 && marked[1].begin == 0x6000 && marked[2].begin == 0x8000,
                "skips stay sorted");
        }
        Check(netplay::ParseSkips("20000-30000", skips, error), "whole block skip parse");
        netplay::HashAllocation(bytes, 0x1FFFF, netplay::Heap::Physical, skips, blocks);
        Check(blocks.size() == 3 && blocks[1].address == 0x30000 && blocks[1].heap == netplay::Heap::Physical,
            "fully skipped block omitted");
        for (const char* text : { "x-10", "20-10", "10-10", "0-100000001", "1-2,", "1", "-2", "1-" })
            Check(!netplay::ParseSkips(text, skips, error), "bad skips refused");
        std::vector<uint8_t> image(1024);
        const auto put = [&](size_t at, uint32_t value, size_t size) {
            for (size_t i = 0; i < size; ++i)
                image[at + i] = uint8_t(value >> (8 * i));
        };
        put(0, 0x5A4D, 2);
        put(0x3C, 64, 4);
        put(64, 0x4550, 4);
        put(70, 2, 2);
        put(84, 0, 2);
        put(88 + 8, 64, 4);
        put(88 + 12, 512, 4);
        put(88 + 36, 0x80000000, 4);
        put(128 + 8, 64, 4);
        put(128 + 12, 768, 4);
        put(128 + 36, 0x60000000, 4);
        Check(netplay::WritableSections(image, 0x82000000, skips, error), "mapped PE sections");
        Check(skips.size() == 1 && skips[0].begin == 0x82000200 && skips[0].end == 0x82000240,
            "only writable section included");
        // A section that ends past the mapped image (the executable's last, read-only one) is not mapped there: it
        // is cut at the image's end, not a reason to refuse the hash.
        put(128 + 12, 1000, 4);
        Check(netplay::WritableSections(image, 0x82000000, skips, error, 1024) && skips.size() == 1,
            "read-only section past the mapped image ignored");
        put(128 + 36, 0x80000000, 4);
        Check(netplay::WritableSections(image, 0x82000000, skips, error, 1024) && skips.size() == 2 &&
                  skips[1].begin == 0x820003E8 && skips[1].end == 0x82000400,
            "writable section cut at the mapped end");
        put(128 + 12, 2000, 4);
        Check(netplay::WritableSections(image, 0x82000000, skips, error, 1024) && skips.size() == 1,
            "section wholly past the mapped image ignored");
        put(88 + 8, 0xFFFFFFFF, 4);
        Check(!netplay::WritableSections(image, 0x82000000, skips, error), "invalid PE section refused");
    }

    void BlockSizeTest()
    {
        std::vector<uint8_t> bytes(0x2101, 0x5A);
        std::vector<netplay::Block> blocks;
        netplay::HashAllocation(bytes, 0x10F00, netplay::Heap::Virtual64K, {}, blocks, 0x1000);
        Check(blocks.size() == 4 && blocks[0].address == 0x10F00 && blocks[1].address == 0x11000 &&
                  blocks[2].address == 0x12000 && blocks[3].address == 0x13000,
            "4 KiB block edges");
        uint64_t value = 0;
        Check(netplay::ParseCount("4096", 256, 0x10000, true, value) && value == 4096, "block size parse");
        Check(netplay::ParseCount("30", 1, 100000, false, value) && value == 30, "sampling parse");
        for (const char* text : { "", "0", "100", "3000", "131072", "4k", "-4096", "4096 " })
            Check(!netplay::ParseCount(text, 256, 0x10000, true, value), "bad block size refused");
        Check(!netplay::ParseCount("100001", 1, 100000, false, value), "oversize sampling refused");
    }

    void DeliveriesTest()
    {
        netplay::Deliveries audio(48000, 256);
        Check(audio.GrantSteps(4) == 12 && audio.WaitDone(12, std::chrono::nanoseconds(0)),
            "no wait before the delivering thread exists");
        audio.Start();
        Check(!audio.TryTake(), "grants before the start dropped");
        uint64_t granted = 0;
        std::array<uint64_t, 8> totals{};
        for (auto& total : totals)
            total = granted = audio.GrantSteps(1);
        Check(totals == std::array<uint64_t, 8>{ 15, 18, 21, 25, 28, 31, 34, 37 },
            "audio periods per frame, remainder kept");
        uint64_t taken = 0;
        while (audio.TryTake())
            ++taken;
        Check(taken == 25, "every granted delivery taken once");
        Check(!audio.WaitDone(granted, std::chrono::nanoseconds(0)), "flip waits for the callbacks");
        for (uint64_t i = 0; i < taken; ++i)
            audio.Done();
        Check(audio.WaitDone(granted, std::chrono::nanoseconds(0)), "flip released once all are done");
        netplay::Deliveries vblanks(60, 1);
        vblanks.Start();
        Check(vblanks.Grant(2) == 2 && vblanks.GrantSteps(1) == 3, "vblanks per frame");
        std::thread deliverer([&] {
            for (int i = 0; i < 3;)
                if (vblanks.TryTake())
                {
                    vblanks.Done();
                    ++i;
                }
        });
        Check(vblanks.WaitDone(3, std::chrono::seconds(5)), "flip released by another thread");
        deliverer.join();
        Check(!vblanks.TryTake(), "no delivery without a grant");
    }

    std::string LogText(FILE* file)
    {
        std::string text;
        if (fflush(file) != 0 || fseek(file, 0, SEEK_SET) != 0)
            return {};
        for (int c = fgetc(file); c != EOF; c = fgetc(file))
            text += char(c);
        return text;
    }

    void EventsTest()
    {
        FILE* file = tmpfile();
        Check(file != nullptr, "event scratch file");
        if (!file)
            return;
        netplay::EventLog log(file);
        log.Count(netplay::Counter::VBlank);
        log.Count(netplay::Counter::VBlank);
        log.Count(netplay::Counter::TimeBase);
        log.Note("thread", "0x82001000");
        Check(log.EndFrame(0), "frame 0 written");
        log.Note("open", "game:\\levels\\a b.str");
        log.Count(netplay::Counter::AudioCallback);
        Check(log.EndFrame(1), "frame 1 written");
        Check(log.EndFrame(2), "empty frame written");
        Check(LogText(file) == "0 thread 0x82001000\n"
                               "0 counts vblank=2 gfxint=0 audio=0 timebase=1\n"
                               "1 open game:\\levels\\a b.str\n"
                               "1 counts vblank=0 gfxint=0 audio=1 timebase=0\n"
                               "2 counts vblank=0 gfxint=0 audio=0 timebase=0\n",
            "frame-stamped events and counters reset per frame");
        fclose(file);
    }
}

int main()
{
    ClockTest();
    InputsTest();
    HashTest();
    BlockSizeTest();
    EventsTest();
    DeliveriesTest();
    if (g_failures)
    {
        fprintf(stderr, "ArkeyanNetplayTest: %d failures\n", g_failures);
        return 1;
    }
    puts("ArkeyanNetplayTest: all passed (10000 malformed random buffers)");
    return 0;
}
