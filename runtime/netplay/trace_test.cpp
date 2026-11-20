#include "netplay/trace.h"
#include "netplay/trace_macros.h"
#include "netplay/state_hash.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

namespace netplay
{
    uint32_t ProbeFrame() { return 7; }
}

namespace
{
    void Check(bool condition, const char* message)
    {
        if (!condition)
        {
            fprintf(stderr, "FAIL: %s\n", message);
            fflush(stderr);
            std::_Exit(1);
        }
    }

    uint64_t ReadLE(FILE* file, unsigned size)
    {
        uint64_t value = 0;
        for (unsigned i = 0; i < size; ++i)
        {
            const int byte = fgetc(file);
            Check(byte != EOF, "complete trace output");
            value |= uint64_t(byte) << (8 * i);
        }
        return value;
    }

    uint64_t Replay(uint32_t hostValue)
    {
        using File = std::unique_ptr<FILE, decltype(&fclose)>;
        File records(tmpfile(), fclose), frames(tmpfile(), fclose);
        Check(records && frames, "scratch streams");
        netplay::Trace trace(records.get(), frames.get(), 1, 1);
        alignas(8) std::array<uint8_t, 8192> memory{};
        auto* base = memory.data();
        netplay::Store<uint32_t>(base, 16, 0x12345678, &trace);
        Check(netplay::Load<uint32_t>(base, 16, 0, &trace, 0) == 0x12345678, "guest load value");
        Check(trace.Count() == 0, "guest stores do not become host loads");
        Check(base[16] == 0x12 && base[19] == 0x78, "guest memory is big endian");
        const uint32_t raw = __builtin_bswap32(hostValue);
        std::memcpy(base + 16, &raw, sizeof(raw));
        Check(netplay::Load<uint32_t>(base, 16, 0x1234, &trace, 0) == hostValue, "host write read");
        Check(trace.Count() == 1, "host write recorded");
        netplay::Load<uint32_t>(base, 16, 0x1234, &trace, 0);
        Check(trace.Count() == 1, "unchanged host value recorded once");
        const uint64_t hash = trace.Hash();
        Check(hash == netplay::Mix(netplay::Mix(netplay::Mix(netplay::HashSeed, 16), 4), hostValue),
            "ordered address, width and value hash");
        trace.EndFrame(0);
        netplay::Store<uint64_t>(base, 4088, 0x1020304050607080, &trace);
        Check(netplay::Load<uint64_t>(base, 4088, 0, &trace, 1) == 0x1020304050607080, "64-bit shadow stores");
        const uint8_t hostByte = 0xAB;
        std::memcpy(base + 4095, &hostByte, 1);
        Check(netplay::Load<uint16_t>(base, 4094, 0xABCDEF01, &trace, 1) == 0x70AB, "overlapping load compares bytes");
        netplay::Load<uint8_t>(base, 4095, 0, &trace, 1);
        Check(trace.Count() == 1, "overlapping host bytes updated in shadow");
        trace.EndFrame(1);
        base[24] = 0xEE;
        netplay::Load<uint8_t>(base, 24, 0, &trace, 2);
        trace.EndFrame(2);
        Check(fseek(records.get(), 0, SEEK_SET) == 0, "rewind the trace file");
        std::array<char, 8> magic{};
        Check(fread(magic.data(), 1, magic.size(), records.get()) == magic.size() &&
                  std::memcmp(magic.data(), "SKYTRC01", 8) == 0,
            "versioned header");
        Check(ReadLE(records.get(), 4) == 1, "format version");
        Check(ReadLE(records.get(), 8) == 2, "sequence includes filtered records");
        Check(ReadLE(records.get(), 4) == 1, "frame range includes boundary");
        Check(ReadLE(records.get(), 4) == 4094 && ReadLE(records.get(), 1) == 2, "address and size");
        Check(ReadLE(records.get(), 8) == 0x70AB && ReadLE(records.get(), 8) == 0x7080, "loaded and previous values");
        Check(ReadLE(records.get(), 4) == 0xABCDEF01, "link register");
        Check(fgetc(records.get()) == EOF, "records outside range filtered");
        Check(fseek(frames.get(), 0, SEEK_SET) == 0, "rewind the trace file");
        for (unsigned frame = 0; frame < 3; ++frame)
        {
            unsigned number = 0;
            unsigned long long count = 0, chain = 0;
            Check(fscanf(frames.get(), "frame %u %llu %llx\n", &number, &count, &chain) == 3 && number == frame &&
                      count == 1,
                "every frame summarized despite filtering");
            if (frame == 0)
                Check(chain == hash, "frame chain written");
        }
        const uint64_t zero = 0;
        trace.Observe(0xFFFFFFF8, &zero, 8, 0, 3);
        Check(trace.Count() == 0, "untouched shadow pages read as zero at the top of guest memory");
        const std::array<uint8_t, 4> crossing{ 1, 2, 3, 4 };
        trace.Remember(65534, crossing.data(), crossing.size());
        trace.Observe(65534, crossing.data(), crossing.size(), 0, 3);
        Check(trace.Count() == 0, "shadow copies cross commit granules");
        trace.Finish(3);
        trace.Observe(0, crossing.data(), crossing.size(), 0, 4);
        trace.EndFrame(4);
        Check(trace.Count() == 0, "finalization is idempotent and stops output");
        Check(netplay::Load<uint32_t>(base, 16, 0, nullptr, 0) == hostValue, "inactive trace reads memory");
        netplay::Store<uint8_t>(base, 0, 7, nullptr);
        Check(base[0] == 7, "inactive trace writes memory");
        return hash;
    }
}

int main(int argc, char** argv)
{
    alignas(8) std::array<uint8_t, 16> memory{};
    auto* base = memory.data();
    struct
    {
        uint64_t lr = 0x12345678;
    } ctx;
    if (argc == 2 && std::string_view(argv[1]) == "--environment")
    {
        base[0] = 0xAB;
        Check(PPC_LOAD_U8(0) == 0xAB, "macro host load");
        printf("trace=%d\n", netplay::g_trace != nullptr);
        netplay::FinishTrace(netplay::ProbeFrame());
        return 0;
    }
    PPC_STORE_U8(0, 0x12);
    PPC_STORE_U16(2, 0x3456);
    PPC_STORE_U32(4, 0x789ABCDE);
    PPC_STORE_U64(8, 0x123456789ABCDEF0);
    Check(PPC_LOAD_U8(0) == 0x12 && PPC_LOAD_U16(2) == 0x3456 && PPC_LOAD_U32(4) == 0x789ABCDE &&
              PPC_LOAD_U64(8) == 0x123456789ABCDEF0,
        "every macro width round trips");
    Check(Replay(0x11223344) != Replay(0x11223345), "one host value changes the chain");
    uint32_t first = 0, last = 0;
    Check(netplay::ParseTraceFrames("0-4294967295", first, last) && first == 0 && last == UINT32_MAX,
        "inclusive frame bounds");
    for (const char* text : { "", "1", "2-1", "-1-2", "1-2x", "1-4294967296", " 1-2", "1-2-3" })
        Check(!netplay::ParseTraceFrames(text, first, last), "malformed frame range refused");
    puts("trace: tests passed");
}
