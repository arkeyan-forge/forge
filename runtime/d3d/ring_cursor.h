#pragma once

#include <cstdint>

// Where d3d::ScanRing reads the PM4 stream from (d3d/ring.cpp). The device reports the stream's last written word; a
// move the scan cannot follow (backwards, or further than MaxSpan) is a jump, and the packets before it are not read.
// A jump is often a detour: after D3DDevice_EndCommandBuffer the write position holds a placeholder (SSA: 0xE0010000)
// until the next write reserves ring space again, and the ring then goes on from where the recording began. Reading
// from the new position would drop the packets the next draw wrote in the ring, its blend, colour, depth and stencil
// state among them, so that draw used the one before's (SSA records about 65 small command buffers a frame while a level
// fades in: black or white shapes for one frame, issue #39). The cursor keeps the position the stream left at a jump
// and, when the stream comes back ahead of it, reads from there.
class RingCursor
{
public:
    static constexpr uint32_t MaxSpan = 0x400000;

    // Packets to read: the words after `from` up to `to` included; none when `from` is 0.
    struct Span
    {
        uint32_t from = 0, to = 0;
    };

    // The device's write position now. `recording`: the stream is a command buffer being recorded (the ring position
    // left at its start stays kept through it). `jumped` is set when packets may have been lost.
    Span Scan(uint32_t cursor, bool recording, bool& jumped)
    {
        jumped = false;
        const uint32_t from = m_cursor;
        m_cursor = cursor;
        if (from == 0 || cursor == from)
            return {};
        if (cursor > from && cursor - from <= MaxSpan)
        {
            if (!recording)
                m_left = 0; // following the ring again
            m_followed = true;
            return { from, cursor };
        }
        if (m_left != 0 && cursor > m_left && cursor - m_left <= MaxSpan)
        {
            const Span span{ m_left, cursor };
            m_left = 0;
            m_followed = true;
            return span;
        }
        if (m_followed && m_left == 0)
            m_left = from;
        m_followed = false;
        jumped = true;
        return {};
    }

    // The write position moved where the scan cannot see it go (another ring segment, into or out of a command buffer):
    // reading continues from `cursor`.
    void Resync(uint32_t cursor)
    {
        m_cursor = cursor;
        m_followed = true;
    }

private:
    uint32_t m_cursor = 0;   // the last position read up to, or jumped to
    uint32_t m_left = 0;     // the position the stream left at a jump, 0 if none
    bool m_followed = false; // m_cursor was read up to or resynced to, not jumped to
};
