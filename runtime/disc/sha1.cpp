#include "disc/sha1.h"
#include <cstring>

namespace disc
{
    namespace
    {
        uint32_t Rotate(uint32_t value, int bits) { return (value << bits) | (value >> (32 - bits)); }
    }

    Sha1::Sha1() : m_state{ 0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u } {}

    void Sha1::Block(const uint8_t* block)
    {
        uint32_t w[80];
        for (int i = 0; i < 16; i++)
            w[i] = uint32_t(block[i * 4]) << 24 | uint32_t(block[i * 4 + 1]) << 16 | uint32_t(block[i * 4 + 2]) << 8 |
                   block[i * 4 + 3];
        for (int i = 16; i < 80; i++)
            w[i] = Rotate(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

        uint32_t a = m_state[0], b = m_state[1], c = m_state[2], d = m_state[3], e = m_state[4];
        for (int i = 0; i < 80; i++)
        {
            uint32_t f, k;
            if (i < 20)
                f = (b & c) | (~b & d), k = 0x5A827999u;
            else if (i < 40)
                f = b ^ c ^ d, k = 0x6ED9EBA1u;
            else if (i < 60)
                f = (b & c) | (b & d) | (c & d), k = 0x8F1BBCDCu;
            else
                f = b ^ c ^ d, k = 0xCA62C1D6u;
            const uint32_t next = Rotate(a, 5) + f + e + k + w[i];
            e = d;
            d = c;
            c = Rotate(b, 30);
            b = a;
            a = next;
        }
        m_state[0] += a;
        m_state[1] += b;
        m_state[2] += c;
        m_state[3] += d;
        m_state[4] += e;
    }

    void Sha1::Update(const void* data, size_t size)
    {
        const uint8_t* bytes = static_cast<const uint8_t*>(data);
        m_length += size;
        if (m_buffered != 0)
        {
            const size_t take = std::min(size, sizeof(m_buffer) - m_buffered);
            memcpy(m_buffer + m_buffered, bytes, take);
            m_buffered += take;
            bytes += take;
            size -= take;
            if (m_buffered < sizeof(m_buffer))
                return;
            Block(m_buffer);
            m_buffered = 0;
        }
        for (; size >= 64; bytes += 64, size -= 64)
            Block(bytes);
        memcpy(m_buffer, bytes, size);
        m_buffered = size;
    }

    Sha1Digest Sha1::Finish()
    {
        const uint64_t bits = m_length * 8;
        const uint8_t one = 0x80;
        Update(&one, 1);
        const uint8_t zero = 0;
        while (m_buffered != 56)
            Update(&zero, 1);
        uint8_t length[8];
        for (int i = 0; i < 8; i++)
            length[i] = uint8_t(bits >> (56 - 8 * i));
        Update(length, 8);
        Sha1Digest digest;
        for (int i = 0; i < 20; i++)
            digest[i] = uint8_t(m_state[i / 4] >> (24 - 8 * (i % 4)));
        return digest;
    }

    Sha1Digest Sha1::Of(const void* data, size_t size)
    {
        Sha1 sha;
        sha.Update(data, size);
        return sha.Finish();
    }
}
