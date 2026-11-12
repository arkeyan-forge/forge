#pragma once

#include <atomic>
#include <cstdint>

// Which blocks of the game's TLSF heaps were allocated since their pool was last made (heap_guard.cpp). SuperChargers
// makes its level heap again on the same memory at every zone start (tlsf_add_pool: the whole pool one free block);
// an object from the menus that is released after that hands the new pool a block of the old one, whose header (its
// "previous block free" bit, its previous-block pointer) describes a heap that no longer exists, and the merge that
// follows corrupts the free lists (games/superchargers/docs/status.md, the race start). One bit per 4 bytes of guest
// address (a block's user pointer is 4-aligned): set when the allocator returns the pointer, cleared by its free and by
// the pool's remaking. A free whose bit is clear is from an earlier pool: dropped (the memory was the pool's already).
// Thread-safe (atomic bit operations); no allocation.
namespace alchemy
{
    class LiveBlocks
    {
    public:
        // bits: (end >> 2) / 64 zeroed words, covering guest addresses [0, end).
        LiveBlocks(std::atomic<uint64_t>* bits, uint64_t end) : m_bits(bits), m_end(end) {}

        // A pool made over [memory, memory + bytes): none of its blocks is live any more.
        void PoolMade(uint32_t memory, uint32_t bytes)
        {
            uint64_t first = Granule(memory), last = Granule(uint64_t(memory) + bytes);
            if (last > m_end >> 2)
                last = m_end >> 2;
            while (first < last && (first & 63) != 0)
                Clear(first++);
            for (; first + 64 <= last; first += 64)
                m_bits[first >> 6].store(0, std::memory_order_relaxed);
            while (first < last)
                Clear(first++);
        }

        void Allocated(uint32_t pointer)
        {
            if (Covers(pointer))
                m_bits[Granule(pointer) >> 6].fetch_or(Bit(pointer), std::memory_order_relaxed);
        }

        // True when the block was allocated from the pool as it is now (its bit is cleared: it is freed); false for a
        // block of an earlier pool, whose free must not reach the allocator. Outside the covered range: true.
        bool Freed(uint32_t pointer)
        {
            if (!Covers(pointer))
                return true;
            return (m_bits[Granule(pointer) >> 6].fetch_and(~Bit(pointer), std::memory_order_relaxed) & Bit(pointer)) !=
                   0;
        }

    private:
        static uint64_t Granule(uint64_t address) { return address >> 2; }
        static uint64_t Bit(uint32_t pointer) { return uint64_t(1) << (Granule(pointer) & 63); }
        bool Covers(uint32_t pointer) const { return pointer != 0 && uint64_t(pointer) < m_end; }
        void Clear(uint64_t granule)
        {
            m_bits[granule >> 6].fetch_and(~(uint64_t(1) << (granule & 63)), std::memory_order_relaxed);
        }

        std::atomic<uint64_t>* m_bits;
        uint64_t m_end;
    };
}
