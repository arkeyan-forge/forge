// The read-backs kept by request (gpu/readback_queue.h): online play takes the one asked three requests before, waited
// for, so two computers' games read the same frame's depth whatever their GPU's speed.
#include "gpu/readback_queue.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

namespace
{
    int g_failures = 0;
    void Check(bool condition, const char* what)
    {
        if (!condition)
        {
            fprintf(stderr, "FAIL: %s\n", what);
            ++g_failures;
        }
    }
}

int main()
{
    using namespace std::chrono_literals;
    gpu::ReadbackQueue queue;
    std::vector<uint32_t> data;

    queue.Finish(1, { 11 });
    queue.Finish(2, { 22 });
    Check(queue.Take(1, data, 0ms) && data == std::vector<uint32_t>{ 11 }, "the one asked, not the latest");
    Check(!queue.Take(1, data, 0ms), "taken once");
    Check(queue.Take(2, data, 0ms) && data[0] == 22, "the next one");

    // Not finished yet: the taker waits for the GPU thread.
    std::thread gpuThread([&] {
        std::this_thread::sleep_for(30ms);
        queue.Finish(3, { 33 });
    });
    const auto start = std::chrono::steady_clock::now();
    Check(queue.Take(3, data, 2000ms) && data[0] == 33, "waits for its read-back");
    Check(std::chrono::steady_clock::now() - start >= 25ms, "it really waited");
    gpuThread.join();

    // A request the GPU thread could not record (device lost, slot busy) is answered empty, so nobody waits for ever.
    queue.Drop(4);
    Check(!queue.Take(4, data, 2000ms), "a dropped request gives nothing, at once");

    // A request nobody takes does not pile up.
    for (uint64_t id = 5; id < 100; ++id)
        queue.Finish(id, { uint32_t(id) });
    Check(queue.Size() <= gpu::ReadbackQueue::Kept, "old read-backs are forgotten");
    Check(queue.Take(99, data, 0ms) && data[0] == 99, "the newest kept");
    Check(!queue.Take(5, data, 0ms), "the oldest forgotten");

    // The time limit: a read-back that never comes returns false instead of hanging the game.
    Check(!queue.Take(1000, data, 20ms), "gives up after the time limit");

    if (g_failures == 0)
        printf("ArkeyanReadbackQueueTest: all passed\n");
    return g_failures == 0 ? 0 : 1;
}
