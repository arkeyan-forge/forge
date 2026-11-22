// How many compilers the compile step runs at once, in units of compiler memory. Measured with the players' compilers
// (clang 22 in llvm-mingw and zig, Apple clang 21) on every game: a game code file needs at most 0.75 GB; the shader
// cache, compiled from its string-literal copy, about 24 times that copy's size (48 MB took 1.2 GB, 189 MB 4.2 GB).
#pragma once

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>

namespace compile_budget
{
    constexpr uint64_t Unit = 1ull << 30;    // one game code file's compiler
    constexpr uint64_t Reserve = 2ull << 30; // left to the system and the launcher

    // The units all compilers may hold together on a machine with this much memory (at least 1).
    unsigned Units(uint64_t memoryBytes);
    // Compilers run at once: one per thread, as many as the units allow.
    unsigned Jobs(unsigned threads, uint64_t memoryBytes);
    // The units one file holds while it compiles, at most total: 1 for game code; the shader cache by its literal
    // copy's size.
    unsigned FileUnits(uintmax_t literalBytes, bool shaderCache, unsigned total);

    // The units the compile threads share. Files take theirs in their order (index 0, 1, ...), so a large file waiting
    // for units is not overtaken by smaller ones. Every index must be taken once, and each Take given back.
    class Pool
    {
    public:
        explicit Pool(unsigned units);
        void Take(size_t index, unsigned units);
        void Give(unsigned units);

    private:
        std::mutex m_mutex;
        std::condition_variable m_changed;
        unsigned m_free;
        size_t m_next = 0;
    };
}
