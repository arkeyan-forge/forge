#pragma once

// SHA-1 (FIPS 180-4): a Wii disc's hash tree (H0 to H3) and its title metadata use it to prove the partition's data.
#include <array>
#include <cstddef>
#include <cstdint>

namespace disc
{
    using Sha1Digest = std::array<uint8_t, 20>;

    class Sha1
    {
    public:
        Sha1();
        void Update(const void* data, size_t size);
        Sha1Digest Finish();

        static Sha1Digest Of(const void* data, size_t size);

    private:
        void Block(const uint8_t* block);

        uint32_t m_state[5];
        uint8_t m_buffer[64];
        size_t m_buffered = 0;
        uint64_t m_length = 0;
    };
}
