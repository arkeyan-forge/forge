#include "platform/jmpbuf.h"
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <thread>

namespace
{
    void Check(bool condition, const char* message)
    {
        if (!condition)
        {
            fprintf(stderr, "FAIL: %s\n", message);
            std::exit(1);
        }
    }

    void RoundTrip(uint32_t guestAddress)
    {
        switch (setjmp(*ppc_setjmp_buf(guestAddress)))
        {
        case 0:
            // Growth must not invalidate the saved environment's address.
            for (uint32_t i = 1; i <= 4096; ++i)
                ppc_setjmp_buf(guestAddress + i);
            longjmp(*ppc_longjmp_buf(guestAddress), 17);
        case 17: break;
        default: Check(false, "longjmp preserves its return value");
        }

        switch (setjmp(*ppc_setjmp_buf(guestAddress)))
        {
        case 0: longjmp(*ppc_longjmp_buf(guestAddress), 0);
        case 1: break;
        default: Check(false, "longjmp with zero returns one");
        }
    }
}

int main(int argc, char** argv)
{
    constexpr uint32_t GuestAddress = 64;
    if (argc == 2 && std::string_view(argv[1]) == "missing")
    {
        ppc_longjmp_buf(GuestAddress);
        Check(false, "an unset longjmp buffer must abort");
    }

    alignas(jmp_buf) std::array<uint8_t, GuestAddress + sizeof(jmp_buf) + 64> guestMemory;
    for (size_t i = 0; i < guestMemory.size(); ++i)
        guestMemory[i] = uint8_t(i * 37 + 11);
    const auto originalMemory = guestMemory;
    auto* const first = ppc_setjmp_buf(GuestAddress);
    Check(first == ppc_setjmp_buf(GuestAddress), "same thread and guest address reuse the buffer");
    Check(first == ppc_longjmp_buf(GuestAddress), "longjmp finds the setjmp buffer");
    RoundTrip(GuestAddress);
    Check(first == ppc_setjmp_buf(GuestAddress), "buffer address survives storage growth");
    Check(first != ppc_setjmp_buf(GuestAddress + 1), "different guest addresses have different buffers");

    std::array<jmp_buf*, 2> threadBuffers{};
    std::atomic<unsigned> ready{ 0 };
    std::atomic<bool> release{ false };
    auto body = [&](size_t index) {
        threadBuffers[index] = ppc_setjmp_buf(GuestAddress);
        RoundTrip(GuestAddress);
        Check(threadBuffers[index] == ppc_longjmp_buf(GuestAddress), "thread retains its own buffer");
        ready.fetch_add(1);
        while (!release.load())
            std::this_thread::yield();
    };
    std::thread firstThread(body, 0);
    std::thread secondThread(body, 1);
    while (ready.load() != threadBuffers.size())
        std::this_thread::yield();
    // Keep both threads alive so allocator reuse cannot hide a shared buffer.
    Check(threadBuffers[0] != threadBuffers[1], "two threads isolate the same guest address");
    Check(threadBuffers[0] != first && threadBuffers[1] != first, "workers isolate the main thread's buffer");
    release.store(true);
    firstThread.join();
    secondThread.join();

    Check(guestMemory == originalMemory, "guest memory remains byte-identical");
    printf("ArkeyanJmpBufTest: all passed\n");
    return 0;
}
