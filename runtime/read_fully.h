#pragma once

// A file read that a passing failure does not cut short (issue #26). The guest takes a short read for the end of the
// file: a movie whose data stops there shows empty pictures (plain green: all-zero YUV) and the game waits for it
// forever. A host read can fail for a moment: Windows refuses a big read while memory is short
// (ERROR_NO_SYSTEM_RESOURCES, ERROR_WORKING_SET_QUOTA), and the kernel cannot write a page the write watch protects
// (ERROR_NOACCESS, EFAULT; write_watch.h's HostWrite keeps them writable during a read).
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <thread>

namespace readfully
{
    // A failed piece is tried again at most this many times in a row, each time half as big down to MinPiece, after a
    // pause of as many milliseconds as tries so far (36 ms in all): time for a shortage of memory to pass.
    constexpr uint32_t MaxRetries = 8;
    constexpr uint32_t MinPiece = 64 * 1024;

    struct Result
    {
        uint32_t bytes = 0;    // read into the buffer from its start
        uint32_t failures = 0; // failed attempts, retried or not
        int error = 0;         // the last failure's `read` error, 0 for none
    };

    // Reads `size` bytes at `offset` into `buffer` until they are all read, the file ends (a read of 0) or a piece still
    // fails after MaxRetries. `read(buffer, size, offset)`: bytes read, 0 at the end of the file, -1 on failure;
    // `error()`: the failure's system error.
    template <class ReadFunction, class ErrorFunction>
    Result Read(uint8_t* buffer, uint32_t size, uint64_t offset, ReadFunction&& read, ErrorFunction&& error)
    {
        Result result;
        uint32_t piece = size, retries = 0;
        while (result.bytes < size)
        {
            const uint32_t want = std::min(piece, size - result.bytes);
            const int64_t got = read(buffer + result.bytes, want, offset + result.bytes);
            if (got > 0)
            {
                result.bytes += uint32_t(std::min<int64_t>(got, want));
                retries = 0;
                continue;
            }
            if (got == 0)
                break;
            result.failures++;
            result.error = error();
            if (++retries > MaxRetries)
                break;
            piece = std::max(MinPiece, want / 2);
            std::this_thread::sleep_for(std::chrono::milliseconds(retries));
        }
        return result;
    }
}
