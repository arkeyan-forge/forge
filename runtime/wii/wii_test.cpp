// The Wii host's address space, executable loader and boot information, on synthetic executables; its NAND in a
// scratch folder; the console's settings. No game data.
// usage: ArkeyanWiiTest [scratch dir]
#include "wii/ax.h"
#include "wii/boot_info.h"
#include "wii/executable.h"
#include "wii/ios.h"
#include "wii/memory_map.h"
#include "wii/nand.h"
#include "wii/processor.h"
#include "wii/settings.h"
#include <atomic>
#include <filesystem>
#include <thread>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace
{
    int checks = 0, failures = 0;
#define EXPECT(condition)                                                                                              \
    do                                                                                                                 \
    {                                                                                                                  \
        ++checks;                                                                                                      \
        if (!(condition))                                                                                              \
        {                                                                                                              \
            ++failures;                                                                                                \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition);                                               \
        }                                                                                                              \
    } while (0)

    uint8_t* g_base = nullptr;
    constexpr uint32_t TableAddress = 0x81EE1E60; // an indirect-call table after MEM1, unaligned as it comes

    uint32_t Get32(uint32_t address)
    {
        const uint8_t* p = g_base + address;
        return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3];
    }

    using Bytes = std::vector<uint8_t>;
    void Put32(Bytes& b, size_t at, uint32_t v)
    {
        if (b.size() < at + 4)
            b.resize(at + 4);
        for (int i = 0; i < 4; i++)
            b[at + i] = uint8_t(v >> (24 - 8 * i));
    }
    void Put16(Bytes& b, size_t at, uint32_t v)
    {
        if (b.size() < at + 2)
            b.resize(at + 2);
        b[at] = uint8_t(v >> 8);
        b[at + 1] = uint8_t(v);
    }

    // An ELF: .text (code) at 0x80004000, .data at 0x80005000, .bss (no bits) at 0x80006000, a non-allocated section.
    Bytes MakeElf(uint32_t textAddress = 0x80004000, uint32_t entry = 0x80004004, bool cut = false)
    {
        Bytes b(0x200, 0);
        memcpy(b.data(),
            "\x7F"
            "ELF\x01\x02\x01",
            7);
        Put16(b, 16, 2);
        Put16(b, 18, 20); // PowerPC
        Put32(b, 24, entry);
        Put32(b, 32, 0x100); // the section table
        Put16(b, 46, 40);
        Put16(b, 48, 5);
        auto section = [&](int i, uint32_t type, uint32_t flags, uint32_t address, uint32_t offset, uint32_t size) {
            const size_t at = 0x100 + size_t(i) * 40;
            Put32(b, at + 4, type);
            Put32(b, at + 8, flags);
            Put32(b, at + 12, address);
            Put32(b, at + 16, offset);
            Put32(b, at + 20, size);
        };
        section(1, 1, 6, textAddress, 0x40, 8);        // .text: alloc, exec
        section(2, 1, 3, 0x80005000, 0x48, 8);         // .data: alloc, write
        section(3, 8, 3, 0x80006000, 0, 0x20);         // .bss
        section(4, 1, 0, 0, 0x50, cut ? 0x1000 : 4);   // a comment: not loaded
        Put32(b, 0x40, 0x38600007);
        Put32(b, 0x44, 0x4E800020);
        Put32(b, 0x48, 0x11223344);
        Put32(b, 0x4C, 0x55667788);
        b.resize(0x100 + 5 * 40);
        return b;
    }

    // A DOL: one text section, one data section inside the zeroed range (as .sdata sits in the real one's .bss).
    Bytes MakeDol(uint32_t dataAddress = 0x80008000)
    {
        Bytes b(0x100, 0);
        Put32(b, 0x00, 0x100);       // text 0: offset
        Put32(b, 0x48, 0x80004000);  // its address
        Put32(b, 0x90, 8);           // its size
        Put32(b, 0x1C, 0x108);       // data 0 (index 7)
        Put32(b, 0x48 + 28, dataAddress);
        Put32(b, 0x90 + 28, 8);
        Put32(b, 0xD8, 0x80007000);  // zeroed: 0x80007000-0x8000A000
        Put32(b, 0xDC, 0x3000);
        Put32(b, 0xE0, 0x80004000);
        Put32(b, 0x100, 0x60000000);
        Put32(b, 0x104, 0x4E800020);
        Put32(b, 0x108, 0xCAFEF00D);
        Put32(b, 0x10C, 0x0BADBEEF);
        return b;
    }

    void AddressSpace()
    {
        std::string error;
        const bool mapped = wii::MapAddressSpace(g_base, { { TableAddress, 0x1000 } }, error);
        EXPECT(mapped && error.empty());
        if (!mapped)
        {
            fprintf(stderr, "%s\n", error.c_str());
            return;
        }
        // Each memory at both of its addresses: the same bytes.
        g_base[0x80001234] = 0x5A;
        EXPECT(g_base[0xC0001234] == 0x5A);
        g_base[0xD3FFFFFF] = 0xA5;
        EXPECT(g_base[0x93FFFFFF] == 0xA5);
        g_base[0x90000000] = 1;
        EXPECT(g_base[0x80000000] == 0 && g_base[0xD0000000] == 1); // MEM2 is not MEM1
        EXPECT(g_base[0x817FFFFF] == 0 && g_base[0x80001235] == 0); // zero-filled
        // The register blocks, the locked cache and the table: plain memory.
        for (uint32_t address :
            { 0xCC00FFFFu, 0xCD000000u, 0xCD80FFFFu, 0xE0003FFFu, TableAddress, TableAddress + 0xFFF })
        {
            g_base[address] = 7;
            EXPECT(g_base[address] == 7);
        }
        EXPECT(wii::InMemory(0x80000000, wii::Mem1Size) && wii::InMemory(0xD0000000, wii::Mem2Size));
        // A range over a memory, even in part, is refused: it would hide the memory's bytes at one of its addresses.
        uint8_t* other = nullptr;
        EXPECT(!wii::MapAddressSpace(other, { { 0x817F0000, 0x20000 } }, error) &&
               error == "cannot map 0x817F0000-0x81810000");
        EXPECT(!wii::InMemory(0x817FFFFF, 2) && !wii::InMemory(0x70000000, 4) && !wii::InMemory(0xCC000000, 4));
    }

    void Elf()
    {
        memset(g_base + 0x80006000, 0xAA, 0x40);
        wii::Executable executable;
        std::string error;
        const Bytes elf = MakeElf();
        EXPECT(wii::LoadExecutable(elf.data(), elf.size(), g_base, executable, error));
        EXPECT(executable.entry == 0x80004004 && executable.sections.size() == 3 && executable.end == 0x80006020);
        EXPECT(Get32(0x80004000) == 0x38600007 && Get32(0xC0004004) == 0x4E800020); // through the mirror too
        EXPECT(Get32(0x80005000) == 0x11223344 && Get32(0x80005004) == 0x55667788);
        EXPECT(Get32(0x80006000) == 0 && Get32(0x8000601C) == 0 && g_base[0x80006020] == 0xAA); // .bss, not past it
        EXPECT(executable.sections[0].code && !executable.sections[1].code && executable.sections[2].zeroed);

        const Bytes outside = MakeElf(0x70000000);
        EXPECT(!wii::LoadExecutable(outside.data(), outside.size(), g_base, executable, error) &&
               error.find("outside MEM1 and MEM2") != std::string::npos);
        const Bytes entry = MakeElf(0x80004000, 0x80005000);
        EXPECT(!wii::LoadExecutable(entry.data(), entry.size(), g_base, executable, error) &&
               error == "the entry point is not in the code");
        Bytes cut = MakeElf();
        Put32(cut, 0x100 + 40 + 16, 0x1F0); // .text's bytes past the file's end
        EXPECT(
            !wii::LoadExecutable(cut.data(), cut.size(), g_base, executable, error) && error == "the ELF is cut short");
        Bytes table = MakeElf();
        Put32(table, 32, 0x180); // the section table runs past the end
        EXPECT(!wii::LoadExecutable(table.data(), table.size(), g_base, executable, error) &&
               error == "the ELF's section table is cut short");
        const uint8_t garbage[8] = {};
        EXPECT(!wii::LoadExecutable(garbage, sizeof(garbage), g_base, executable, error) &&
               error == "not a DOL or an ELF");
    }

    void Dol()
    {
        memset(g_base + 0x80007000, 0xAA, 0x3010);
        wii::Executable executable;
        std::string error;
        const Bytes dol = MakeDol();
        EXPECT(wii::LoadExecutable(dol.data(), dol.size(), g_base, executable, error));
        EXPECT(executable.entry == 0x80004000 && executable.sections.size() == 3 && executable.end == 0x8000A000);
        EXPECT(Get32(0x80004004) == 0x4E800020);
        // The data section inside the zeroed range keeps its bytes; the rest of the range clears, nothing past it.
        EXPECT(Get32(0x80008000) == 0xCAFEF00D && Get32(0x80008004) == 0x0BADBEEF);
        EXPECT(Get32(0x80007000) == 0 && Get32(0x80007FFC) == 0 && Get32(0x80008008) == 0 && Get32(0x80009FFC) == 0);
        EXPECT(g_base[0x8000A000] == 0xAA);

        Bytes outside = MakeDol(0x81800000);
        EXPECT(!wii::LoadExecutable(outside.data(), outside.size(), g_base, executable, error) &&
               error.find("outside MEM1 and MEM2") != std::string::npos);
        Bytes cut = MakeDol();
        cut.resize(0x10C);
        EXPECT(
            !wii::LoadExecutable(cut.data(), cut.size(), g_base, executable, error) && error == "the DOL is cut short");
    }

    void BootInformation()
    {
        wii::BootInfo info;
        memcpy(info.discHeader, "RTST01", 6);
        info.discHeader[0x18] = 0x5D;
        info.discHeader[0x19] = 0x1C;
        info.discHeader[0x1A] = 0x9E;
        info.discHeader[0x1B] = 0xA3;
        info.ios = 56;
        info.iosRevision = 5918;
        info.settingText = { 0x41, 0x42 };
        info.fileSystemTable = { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 }; // a root holding nothing
        const uint32_t table = wii::FileSystemTableAddress(12);
        g_base[0x80003FFF] = 1;
        wii::WriteBootInfo(g_base, info);
        EXPECT(memcmp(g_base + 0x80000000, "RTST01", 6) == 0 && Get32(0x80000018) == 0x5D1C9EA3);
        EXPECT(Get32(0x80000020) == 0x0D15EA5E && Get32(0x80000028) == wii::Mem1Size);
        EXPECT(Get32(0x800000F8) == 243000000 && Get32(0x800000FC) == 729000000);
        EXPECT(
            Get32(0x80003118) == wii::Mem2Size && Get32(0x80003128) == 0x933E0000 && Get32(0x80003134) == 0x93400000);
        EXPECT(Get32(0x80003140) == (56u << 16 | 5918) && Get32(0x80003188) == (56u << 16 | 5918));
        EXPECT(memcmp(g_base + 0x80003180, "RTST", 4) == 0);
        EXPECT(g_base[0x80003800] == 0x41 && g_base[0x80003801] == 0x42 && g_base[0x80003802] == 0);
        EXPECT(table == 0x817FFFE0 && Get32(0x80000038) == table && Get32(0x8000003C) == 12);
        EXPECT(Get32(0x80003110) == table && g_base[table] == 1 && Get32(table + 8) == 1);
        EXPECT(g_base[0x80003FFF] == 0 && Get32(0xC0000020) == 0x0D15EA5E); // the rest cleared; through the mirror
    }

    void Set16(uint32_t address, uint16_t value)
    {
        g_base[address] = uint8_t(value >> 8);
        g_base[address + 1] = uint8_t(value);
    }
    void Set32(uint32_t address, uint32_t value)
    {
        Set16(address, uint16_t(value >> 16));
        Set16(address + 2, uint16_t(value));
    }
    uint16_t Get16(uint32_t address) { return uint16_t(g_base[address] << 8 | g_base[address + 1]); }

    // A voice's parameter block at `block` (a cached address), cleared, then playing `format` from `current` to `end`.
    void Voice(uint32_t block, uint16_t format, uint32_t current, uint32_t end, uint32_t ratio, uint16_t source = 1)
    {
        namespace pb = wii::ax::pb;
        memset(g_base + block, 0, wii::ax::BlockSize);
        Set16(block + pb::State, 1);
        Set16(block + pb::Format, format);
        Set16(block + pb::Source, source);
        Set32(block + pb::CurrentAddress, current);
        Set32(block + pb::EndAddress, end);
        Set32(block + pb::Ratio, ratio);
    }

    // AX's command lists as the host's DSP runs them: the voices' positions over a frame, their ends and loops, ADPCM's
    // frame headers, the volume ramps; the list's commands and its end.
    void AxFrames()
    {
        namespace ax = wii::ax;
        namespace pb = wii::ax::pb;
        const uint32_t block = 0x80100000;
        Voice(block, ax::Pcm16, 0x1000, 0x2000, 0x10000, 2); // no converter: one input sample for each output one
        ax::AdvanceVoice(g_base, block);
        EXPECT(Get32(block + pb::CurrentAddress) == 0x1000 + 96 && Get16(block + pb::State) == 1);
        Voice(block, ax::Pcm16, 0x1000, 0x2000, 0x8000); // half the rate: 48 samples
        ax::AdvanceVoice(g_base, block);
        EXPECT(Get32(block + pb::CurrentAddress) == 0x1000 + 48 && Get16(block + pb::Fraction) == 0);
        Voice(block, ax::Pcm8, 0x1000, 0x2000, 0x18000); // one and a half: 144, the fraction kept
        Set16(block + pb::Fraction, 0x8000);
        ax::AdvanceVoice(g_base, block);
        EXPECT(Get32(block + pb::CurrentAddress) == 0x1000 + 144 && Get16(block + pb::Fraction) == 0x8000);

        Voice(block, ax::Pcm16, 0x1000, 0x1009, 0x10000); // ends after ten samples
        ax::AdvanceVoice(g_base, block);
        EXPECT(Get16(block + pb::State) == 0 && Get32(block + pb::CurrentAddress) == 0x1009);
        Voice(block, ax::Pcm16, 0x1000, 0x1009, 0x10000); // loops to 0x1004: 96 = 10 + 86, 86 = 14 * 6 + 2
        Set16(block + pb::Loops, 1);
        Set32(block + pb::LoopAddress, 0x1004);
        ax::AdvanceVoice(g_base, block);
        EXPECT(Get16(block + pb::State) == 1 && Get32(block + pb::CurrentAddress) == 0x1006);

        // ADPCM: nibble addresses, two header nibbles at each 16; the header read into the predictor and scale.
        const uint32_t samples = 0x80200000;
        for (uint32_t frame = 0; frame < 16; frame++)
            g_base[samples + 8 * frame] = uint8_t(0x40 + frame);
        const uint32_t start = (samples & 0x1FFFFFFF) * 2; // the DSP's nibble address of the samples
        Voice(block, ax::Adpcm, start + 2, start + 0x1000, 0x10000);
        ax::AdvanceVoice(g_base, block);
        // 96 samples: 14 in frames 0-5, the 7th's first 12 (its header read): the next nibble is 2 + 12 into it.
        EXPECT(
            Get32(block + pb::CurrentAddress) == start + 6 * 16 + 2 + 12 && Get16(block + pb::PredictorScale) == 0x46);
        Voice(block, ax::Adpcm, start + 2, start + 15, 0x10000); // one frame, looped: its loop state, not a stream
        Set16(block + pb::Loops, 1);
        Set32(block + pb::LoopAddress, start + 2);
        Set16(block + pb::LoopPredictorScale, 0x77);
        Set16(block + pb::LoopHistory1, 0x1234);
        ax::AdvanceVoice(g_base, block);
        EXPECT(Get16(block + pb::History1) == 0x1234 && Get16(block + pb::State) == 1);
        EXPECT(Get16(block + pb::PredictorScale) == 0x77); // past the header: the loop state's, not read again

        Voice(block, ax::Pcm16, 0x1000, 0x2000, 0x10000); // a ramp: the volume's step for each of the 96 samples
        Set16(block + pb::Volume, 0x1000);
        Set16(block + pb::VolumeStep, uint16_t(-16));
        ax::AdvanceVoice(g_base, block);
        EXPECT(Get16(block + pb::Volume) == 0x1000 - 16 * 96);
        Set16(block + pb::VolumeStep, uint16_t(-0x100));
        ax::AdvanceVoice(g_base, block);
        EXPECT(Get16(block + pb::Volume) == 0); // never below silence
        Voice(block, ax::Pcm16, 0x1000, 0x2000, 0x10000);
        Set16(block + pb::State, 0);
        ax::AdvanceVoice(g_base, block); // stopped: left as it is
        EXPECT(Get32(block + pb::CurrentAddress) == 0x1000);

        // A list: the studio, the voices (two linked blocks, the second stopped), an aux bus, the final mix, the end.
        const uint32_t list = 0x80110000, second = 0x80100200;
        Voice(block, ax::Pcm16, 0x1000, 0x2000, 0x10000);
        Voice(second, ax::Pcm16, 0x3000, 0x4000, 0x10000);
        Set16(second + pb::State, 0);
        Set32(block + pb::Next, second & 0x1FFFFFFF);
        const uint16_t words[] = { 0, 0x0012, 0x0000, 4, 0x0010, 0x0000, 5, 0x8000, 0, 0, 0, 0, 11, 0x8000, 0, 0,
            0x0013, 0x4000, 14 };
        for (size_t i = 0; i < sizeof(words) / 2; i++)
            Set16(list + uint32_t(2 * i), words[i]);
        ax::Frame frame = ax::RunCommandList(g_base, list & 0x1FFFFFFF);
        EXPECT(frame.complete && frame.voices == 2 && frame.playing == 1 && frame.output == 0x80134000);
        EXPECT(Get32(block + pb::CurrentAddress) == 0x1000 + 96 && Get32(second + pb::CurrentAddress) == 0x3000);
        Set16(list + 6, 99); // a command not known: the list stops there
        frame = ax::RunCommandList(g_base, list);
        EXPECT(!frame.complete && frame.unknownCommand == 99 && frame.voices == 0);
        Set16(list + 6, 4);
        Set32(block + pb::Next, block & 0x1FFFFFFF); // a block linked to itself: the list stops, the frame ends
        frame = ax::RunCommandList(g_base, list);
        EXPECT(frame.complete && frame.voices == 512);
    }

    // One processor between host threads: each runs only when handed it, in the order the holder chooses; events run
    // on the holder, at its safe points or while it waits idle.
    void ProcessorHandOver()
    {
        // As in the host, it outlives every thread that waits on it: the second one waits for good at the end.
        static wii::Processor& processor = *new wii::Processor;
        wii::Processor::Thread* main = processor.Start();
        std::vector<int> order;
        wii::Processor::Thread* second = processor.Spawn(1 << 20, [&] {
            order.push_back(2); // only the holder runs: no lock needed around `order`
            processor.HandTo(main);
            order.push_back(4);
            processor.HandTo(main);
        });
        EXPECT(processor.Current() == main);
        order.push_back(1);
        processor.HandTo(second);
        order.push_back(3);
        processor.HandTo(second);
        order.push_back(5);
        EXPECT((order == std::vector<int>{ 1, 2, 3, 4, 5 }));
        processor.HandTo(main); // to itself: returns at once
        EXPECT(processor.Current() == main);

        // Events: posted from elsewhere, run here, in order; one may post another.
        const std::thread::id holder = std::this_thread::get_id();
        std::vector<int> events;
        std::atomic<bool> onHolder{ true };
        EXPECT(!processor.RunPending());
        EXPECT(!processor.WaitForEvent(std::chrono::milliseconds(20))); // nothing will come
        std::thread poster([&] {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            processor.Post([&] {
                onHolder = onHolder && std::this_thread::get_id() == holder;
                events.push_back(1);
                processor.Post([&] { events.push_back(2); });
            });
        });
        EXPECT(processor.WaitForEvent(std::chrono::seconds(10)));
        poster.join();
        EXPECT(onHolder && (events == std::vector<int>{ 1, 2 }));
        processor.Post([&] { events.push_back(3); });
        EXPECT(processor.RunPending() && events.size() == 3 && !processor.RunPending());
    }

    // The host's IOS: paths to devices, descriptors, a device's own answers.
    void IosDevices()
    {
        struct Counter : wii::ios::Device
        {
            int* closes;
            explicit Counter(int* c) : closes(c) {}
            int32_t Close() override
            {
                ++*closes;
                return wii::ios::Ok;
            }
            int32_t Ioctl(uint32_t command, wii::ios::Buffer in, wii::ios::Buffer out) override
            {
                return command == 1 ? int32_t(in.size + out.size) : Pending;
            }
        };
        struct Refuses : wii::ios::Device
        {
            int32_t Open(uint32_t) override { return wii::ios::NoAccess; }
        };
        int closes = 0;
        wii::ios::Ios ios;
        ios.Serve("/dev/counter", [&] { return std::make_unique<Counter>(&closes); });
        ios.Serve("/dev/refuses", [] { return std::make_unique<Refuses>(); });
        EXPECT(ios.Open("/dev/none", 0) == wii::ios::NotFound);
        EXPECT(ios.Open("/dev/refuses", 0) == wii::ios::NoAccess);
        const int32_t first = ios.Open("/dev/counter", 0), second = ios.Open("/dev/counter", 0);
        EXPECT(first == 0 && second == 1 && ios.PathOf(second) == "/dev/counter" && ios.PathOf(5).empty());
        EXPECT(ios.Ioctl(first, 1, { 0x80000000, 3 }, { 0x80000100, 4 }) == 7);
        EXPECT(ios.Ioctl(first, 2, {}, {}) == wii::ios::Device::Pending);
        EXPECT(ios.Read(first, {}) == wii::ios::Invalid && ios.Ioctlv(first, 0, {}, {}) == wii::ios::Invalid);
        EXPECT(ios.Close(first) == wii::ios::Ok && closes == 1 && ios.Close(first) == wii::ios::Invalid);
        EXPECT(ios.Open("/dev/counter", 0) == first); // the lowest free descriptor again
        for (int i = 2; i < wii::ios::Ios::MaxDescriptors; i++)
            EXPECT(ios.Open("/dev/counter", 0) == i);
        EXPECT(ios.Open("/dev/counter", 0) == wii::ios::NoDescriptor);
        EXPECT(ios.Ioctl(-1, 1, {}, {}) == wii::ios::Invalid && ios.Ioctl(99, 1, {}, {}) == wii::ios::Invalid);

        // The NAND's files: a path outside /dev/ no device serves, the file's own open answering.
        wii::ios::Ios files;
        std::vector<std::string> asked;
        files.ServeFiles([&](const std::string& path) -> std::unique_ptr<wii::ios::Device> {
            asked.push_back(path);
            if (path == "/shared2/missing")
                return std::make_unique<Refuses>();
            return std::make_unique<Counter>(&closes);
        });
        EXPECT(files.Open("/dev/none", 0) == wii::ios::NotFound && asked.empty());
        EXPECT(files.Open("/shared2/missing", 1) == wii::ios::NoAccess);
        EXPECT(
            files.Open("/title/00010000/data/save.bin", 3) == 0 && files.PathOf(0) == "/title/00010000/data/save.bin");
        EXPECT(asked.size() == 2);
    }

    // The NAND in a scratch folder: its paths, folders and files, as ISFS answers them.
    void NandFiles(const std::filesystem::path& root)
    {
        namespace nand = wii::nand;
        std::error_code error;
        std::filesystem::remove_all(root, error);
        nand::FileSystem fs(root);
        EXPECT(fs.CreateRoot());
        std::filesystem::path host;
        for (const char* refused : { "", "title", "/a/../b", "/a//b", "/a/", "/thirteenchars", "/a\\b", "/." })
            EXPECT(!fs.HostPath(refused, host));
        EXPECT(fs.HostPath("/", host) && host == root && fs.HostPath("/twelve_chars", host));

        EXPECT(fs.CreateFolder("/title") == nand::Ok && fs.CreateFolder("/title") == nand::AlreadyExists);
        EXPECT(fs.CreateFolder("/none/sub") == nand::NotFound && fs.CreateFile("/none/a") == nand::NotFound);
        EXPECT(fs.CreateFolder("/title/sub") == nand::Ok && fs.CreateFile("/title/a.bin") == nand::Ok);
        EXPECT(fs.CreateFile("/title/a.bin") == nand::AlreadyExists && fs.CreateFolder("/") == nand::Invalid);

        std::unique_ptr<nand::File> file;
        EXPECT(fs.Open("/title/missing", 1, file) == nand::NotFound && fs.Open("/title/sub", 1, file) == nand::Invalid);
        EXPECT(fs.Open("/title/a.bin", 3, file) == nand::Ok);
        const uint8_t data[5] = { 1, 2, 3, 4, 5 };
        uint8_t back[8] = {};
        EXPECT(file->Write(data, 5) == 5 && file->Size() == 5 && file->Position() == 5);
        EXPECT(file->Seek(0, 0) == 0 && file->Read(back, 8) == 5 && memcmp(back, data, 5) == 0);
        EXPECT(file->Seek(6, 0) == nand::Invalid && file->Seek(-1, 2) == 4 && file->Seek(-2, 1) == 2);
        EXPECT(file->Seek(0, 3) == nand::Invalid && file->Read(back, 1) == 1 && back[0] == 3);
        file.reset();
        EXPECT(fs.Open("/title/a.bin", 1, file) == nand::Ok && file->Write(data, 1) == nand::NoAccess);
        file.reset();
        EXPECT(fs.Open("/title/a.bin", 2, file) == nand::Ok && file->Read(back, 1) == nand::NoAccess);
        file.reset();
        EXPECT(fs.Open("/title/a.bin", 4, file) == nand::Invalid);

        std::vector<std::string> names;
        EXPECT(fs.ReadFolder("/title", names) == nand::Ok && (names == std::vector<std::string>{ "a.bin", "sub" }));
        EXPECT(fs.ReadFolder("/title/a.bin", names) == nand::Invalid && fs.ReadFolder("/x", names) == nand::NotFound);
        uint32_t clusters = 0, entries = 0;
        EXPECT(fs.Usage("/title", clusters, entries) == nand::Ok && clusters == 1 && entries == 3);

        nand::Attributes attributes;
        EXPECT(fs.GetAttributes("/title/sub", attributes) == nand::Ok && attributes.folder);
        EXPECT(fs.GetAttributes("/title/a.bin", attributes) == nand::Ok && !attributes.folder);
        EXPECT(fs.GetAttributes("/title/b.bin", attributes) == nand::NotFound);

        EXPECT(fs.Delete("/title") == nand::NotEmpty && fs.Delete("/title/none") == nand::NotFound);
        EXPECT(fs.Rename("/title/a.bin", "/title/b.bin") == nand::Ok && !fs.Exists("/title/a.bin"));
        EXPECT(fs.Exists("/title/b.bin") && fs.Rename("/title/b.bin", "/title/sub") == nand::Invalid);
        EXPECT(fs.Put("/shared2/sys/c.bin", { 9, 9 }) && fs.CreateFile("/title/d.bin") == nand::Ok);
        EXPECT(fs.Rename("/title/d.bin", "/title/b.bin") == nand::Ok); // replaces what was there
        EXPECT(fs.Open("/title/b.bin", 1, file) == nand::Ok && file->Size() == 0);
        file.reset();
        EXPECT(fs.Open("/shared2/sys/c.bin", 1, file) == nand::Ok && file->Size() == 2);
        file.reset();
        EXPECT(fs.Delete("/title/b.bin") == nand::Ok && fs.Delete("/title/sub") == nand::Ok);
        EXPECT(fs.ReadFolder("/title", names) == nand::Ok && names.empty());
        std::filesystem::remove_all(root, error);
    }

    // SYSCONF as the SDK's settings library reads it, and setting.txt under its key.
    void ConsoleSettingsFiles()
    {
        wii::ConsoleSettings settings;
        settings.language = 3;
        const std::vector<uint8_t> file = wii::MakeSysconf(settings);
        EXPECT(file.size() == 0x4000 && memcmp(file.data(), "SCv0", 4) == 0 && memcmp(&file[0x3FFC], "SCed", 4) == 0);
        auto get16 = [&](size_t at) { return size_t(file[at]) << 8 | file[at + 1]; };
        const size_t count = get16(4);
        EXPECT(count == 15 && get16(6) == 6 + 2 * (count + 1));
        struct Item
        {
            int type;
            std::vector<uint8_t> value;
        };
        std::vector<std::pair<std::string, Item>> items;
        bool laidOut = true;
        for (size_t i = 0; i < count; i++)
        {
            size_t at = get16(6 + 2 * i);
            const int type = file[at] >> 5;
            const size_t nameLength = (file[at] & 0x1F) + 1u;
            const std::string name(reinterpret_cast<const char*>(&file[at + 1]), nameLength);
            at += 1 + nameLength;
            size_t size = type == 3 ? 1 : type == 5 ? 4 : 0;
            if (type == 2)
                size = file[at++] + 1u;
            else if (type == 1)
            {
                size = get16(at) + 1;
                at += 2;
            }
            items.push_back(
                { name, { type, std::vector<uint8_t>(file.begin() + long(at), file.begin() + long(at + size)) } });
            laidOut = laidOut && at + size == get16(6 + 2 * (i + 1));
        }
        EXPECT(laidOut && get16(6 + 2 * count) <= 0x4000 - 4 - 76); // the SDK's index goes after the items
        auto find = [&](const char* name) -> const Item* {
            for (const auto& item : items)
                if (item.first == name)
                    return &item.second;
            return nullptr;
        };
        const Item* language = find("IPL.LNG");
        const Item* ratio = find("IPL.AR");
        const Item* remotes = find("BT.DINF");
        const Item* idle = find("IPL.IDL");
        const Item* bias = find("IPL.CB");
        EXPECT(language != nullptr && language->type == 3 && language->value == std::vector<uint8_t>{ 3 });
        EXPECT(ratio != nullptr && ratio->value == std::vector<uint8_t>{ 1 });
        EXPECT(remotes != nullptr && remotes->type == 1 && remotes->value.size() == 0x461);
        EXPECT(idle != nullptr && idle->type == 2 && idle->value.size() == 2);
        EXPECT(bias != nullptr && bias->type == 5 && bias->value.size() == 4);

        const std::vector<uint8_t> europe = wii::MakeSettingText('P');
        EXPECT(europe.size() == 0x100 && europe[0] == ('A' ^ 0xFA) &&
               europe[1] == ('R' ^ 0xF4)); // the key's low byte, rotated
        const std::string text = wii::DecryptSettingText(europe);
        EXPECT(text.rfind("AREA=EUR\r\n", 0) == 0 && text.find("VIDEO=PAL\r\n") != std::string::npos);
        EXPECT(text.find("GAME=EU\r\n") != std::string::npos);
        EXPECT(wii::DecryptSettingText(wii::MakeSettingText('E')).find("AREA=USA\r\n") == 0);
        EXPECT(wii::DecryptSettingText(wii::MakeSettingText('J')).find("GAME=JP\r\n") != std::string::npos);
    }
} // namespace

int main(int argc, char** argv)
{
    const std::filesystem::path scratch =
        argc > 1 ? std::filesystem::path(argv[1]) : std::filesystem::temp_directory_path();
    ProcessorHandOver();
    IosDevices();
    NandFiles(scratch / "arkeyan-wii-nand-test");
    ConsoleSettingsFiles();
    AddressSpace();
    if (g_base != nullptr)
    {
        Elf();
        Dol();
        BootInformation();
        AxFrames();
    }
    printf("wii test: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
