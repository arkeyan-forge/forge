// readfully::Read (read_fully.h) with a scripted file: a whole read, short reads continued, the end of the file, a
// failure retried in smaller pieces until it reads in full, and one that persists past MaxRetries (issue #26).
// usage: ArkeyanReadFullyTest
#include "read_fully.h"
#include <cstdio>
#include <cstring>
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

    // A file of `size` bytes (byte i = i * 13 + 5) whose reads return at most `shortest` bytes, and whose first `fail`
    // reads, or every read bigger than `failAbove`, fail with error 1450 (ERROR_NO_SYSTEM_RESOURCES).
    struct ScriptedFile
    {
        uint32_t size = 0, shortest = ~0u, failAbove = ~0u;
        int fail = 0;
        std::vector<uint32_t> asked{}; // the size of every read

        int64_t operator()(uint8_t* into, uint32_t bytes, uint64_t at)
        {
            asked.push_back(bytes);
            if (fail > 0 || bytes > failAbove)
            {
                fail--;
                return -1;
            }
            if (at >= size)
                return 0;
            uint32_t count = uint32_t(std::min<uint64_t>({ bytes, shortest, size - at }));
            for (uint32_t i = 0; i < count; i++)
                into[i] = uint8_t((at + i) * 13 + 5);
            return count;
        }
    };

    bool Matches(const std::vector<uint8_t>& buffer, uint32_t bytes, uint64_t offset)
    {
        for (uint32_t i = 0; i < bytes; i++)
            if (buffer[i] != uint8_t((offset + i) * 13 + 5))
                return false;
        return true;
    }

    readfully::Result Run(ScriptedFile& file, std::vector<uint8_t>& buffer, uint64_t offset)
    {
        return readfully::Read(
            buffer.data(), uint32_t(buffer.size()), offset,
            [&](uint8_t* into, uint32_t bytes, uint64_t at) { return file(into, bytes, at); }, [] { return 1450; });
    }
}

int main()
{
    constexpr uint32_t Size = 1u << 20;
    {
        ScriptedFile file{ .size = Size };
        std::vector<uint8_t> buffer(256 * 1024);
        readfully::Result result = Run(file, buffer, 4096);
        CHECK(result.bytes == buffer.size() && result.failures == 0 && result.error == 0);
        CHECK(file.asked.size() == 1 && Matches(buffer, result.bytes, 4096));
    }
    // Short reads (a pipe, a network share) are continued where they stopped.
    {
        ScriptedFile file{ .size = Size, .shortest = 1000 };
        std::vector<uint8_t> buffer(10000);
        readfully::Result result = Run(file, buffer, 7);
        CHECK(result.bytes == 10000 && result.failures == 0 && file.asked.size() == 10);
        CHECK(Matches(buffer, result.bytes, 7));
    }
    // The end of the file stops the read: the bytes before it.
    {
        ScriptedFile file{ .size = Size };
        std::vector<uint8_t> buffer(4096);
        readfully::Result result = Run(file, buffer, Size - 100);
        CHECK(result.bytes == 100 && result.failures == 0 && Matches(buffer, 100, Size - 100));
    }
    // A read that fails twice (memory short for a moment) reads in full on the third attempt, in smaller pieces.
    {
        ScriptedFile file{ .size = Size, .fail = 2 };
        std::vector<uint8_t> buffer(512 * 1024);
        readfully::Result result = Run(file, buffer, 0);
        CHECK(result.bytes == buffer.size() && result.failures == 2 && result.error == 1450);
        CHECK(file.asked.size() >= 3 && file.asked[0] == 512 * 1024 && file.asked[1] == 256 * 1024 &&
              file.asked[2] == 128 * 1024);
        CHECK(Matches(buffer, result.bytes, 0));
    }
    // A size that always fails above 64 KB reads in 64 KB pieces after halving down to them.
    {
        ScriptedFile file{ .size = Size, .failAbove = readfully::MinPiece };
        std::vector<uint8_t> buffer(1u << 20);
        readfully::Result result = Run(file, buffer, 0);
        CHECK(result.bytes == buffer.size() && result.failures == 4 && Matches(buffer, result.bytes, 0));
    }
    // A failure that persists: MaxRetries more attempts, then the bytes read so far (none).
    {
        ScriptedFile file{ .size = Size, .fail = 1000 };
        std::vector<uint8_t> buffer(4096);
        readfully::Result result = Run(file, buffer, 0);
        CHECK(result.bytes == 0 && result.failures == readfully::MaxRetries + 1 && result.error == 1450);
        CHECK(file.asked.size() == readfully::MaxRetries + 1);
    }

    if (g_failures == 0)
        printf("read fully test: all checks passed\n");
    return g_failures == 0 ? 0 : 1;
}
