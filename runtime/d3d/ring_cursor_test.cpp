// Checks where the ring scan reads from (d3d/ring_cursor.h). usage: ArkeyanRingCursorTest
#include "d3d/ring_cursor.h"
#include <cstdio>

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

    bool Reads(RingCursor::Span span, uint32_t from, uint32_t to) { return span.from == from && span.to == to; }
    bool Nothing(RingCursor::Span span) { return span.from == 0; }

    constexpr uint32_t Ring = 0xA1952000;
    constexpr uint32_t Placeholder = 0xE0010000; // what SSA's device holds after EndCommandBuffer
    constexpr uint32_t Buffer = 0xE1752000;      // a command buffer being recorded
}

int main()
{
    bool jumped;

    // Following the ring: each scan reads what was written since the last one.
    {
        RingCursor cursor;
        cursor.Resync(Ring);
        CHECK(Reads(cursor.Scan(Ring + 0x100, false, jumped), Ring, Ring + 0x100) && !jumped);
        CHECK(Nothing(cursor.Scan(Ring + 0x100, false, jumped)) && !jumped);
        CHECK(Reads(cursor.Scan(Ring + 0x180, false, jumped), Ring + 0x100, Ring + 0x180));
    }

    // SSA's command buffer (issue #39): the position is the placeholder before BeginCommandBuffer, the buffer while it
    // records (a kick-off inside reports 4), the placeholder again after EndCommandBuffer, then the ring further on.
    // The draw's packets written in the ring before that scan are read, from where the ring was left.
    {
        RingCursor cursor;
        cursor.Resync(Ring);
        CHECK(Reads(cursor.Scan(Ring + 0x30, false, jumped), Ring, Ring + 0x30));
        CHECK(Nothing(cursor.Scan(Placeholder, false, jumped)) && jumped); // Begin's scan
        CHECK(Nothing(cursor.Scan(4, true, jumped)) && jumped);            // the kick-off inside it
        cursor.Resync(Buffer);                                             // recording
        CHECK(Reads(cursor.Scan(Buffer + 0x40, true, jumped), Buffer, Buffer + 0x40));
        cursor.Resync(Placeholder); // End
        CHECK(Reads(cursor.Scan(Ring + 0x190, false, jumped), Ring + 0x30, Ring + 0x190) && !jumped);
        // Then the ring is followed as before, and the next detour is handled the same way.
        CHECK(Reads(cursor.Scan(Ring + 0x200, false, jumped), Ring + 0x190, Ring + 0x200));
        CHECK(Nothing(cursor.Scan(Placeholder, false, jumped)) && jumped);
        cursor.Resync(Buffer + 0x1000);
        cursor.Resync(Placeholder);
        CHECK(Reads(cursor.Scan(Ring + 0x260, false, jumped), Ring + 0x200, Ring + 0x260));
    }

    // A longer detour (logged at level_027's load, 2026-11-11): the placeholder, the kick-off inside a recording, then a
    // position outside the ring, each scanned twice, then the ring again exactly where it was left, then further on.
    // Nothing between the place left and the new position is skipped, whether the stream comes back on it or past it.
    {
        constexpr uint32_t Elsewhere = 0x60EC5404;
        RingCursor cursor;
        cursor.Resync(Ring);
        CHECK(Reads(cursor.Scan(Ring + 0x118, false, jumped), Ring, Ring + 0x118));
        CHECK(Nothing(cursor.Scan(Placeholder, false, jumped)) && jumped);
        CHECK(Nothing(cursor.Scan(4, true, jumped)) && jumped);
        CHECK(Nothing(cursor.Scan(4, true, jumped)));
        CHECK(Nothing(cursor.Scan(Elsewhere, false, jumped)) && jumped);
        CHECK(Nothing(cursor.Scan(Elsewhere, false, jumped)));
        CHECK(Nothing(cursor.Scan(Ring + 0x118, false, jumped)));
        CHECK(Reads(cursor.Scan(Ring + 0x148, false, jumped), Ring + 0x118, Ring + 0x148) && !jumped);
        CHECK(Nothing(cursor.Scan(Placeholder, false, jumped)) && jumped);
        CHECK(Nothing(cursor.Scan(Elsewhere, false, jumped)) && jumped);
        CHECK(Reads(cursor.Scan(Ring + 0x200, false, jumped), Ring + 0x148, Ring + 0x200) && !jumped);
    }

    // A real jump stays one: back behind the place left (the ring wrapped) or too far ahead of it.
    {
        RingCursor cursor;
        cursor.Resync(Ring + 0x1000);
        CHECK(Reads(cursor.Scan(Ring + 0x1010, false, jumped), Ring + 0x1000, Ring + 0x1010));
        CHECK(Nothing(cursor.Scan(Placeholder, false, jumped)) && jumped);
        CHECK(Nothing(cursor.Scan(Ring + 0x800, false, jumped)) && jumped); // behind: not read
        CHECK(Reads(cursor.Scan(Ring + 0x820, false, jumped), Ring + 0x800, Ring + 0x820));
    }
    {
        RingCursor cursor;
        cursor.Resync(Ring);
        CHECK(Reads(cursor.Scan(Ring + 0x10, false, jumped), Ring, Ring + 0x10));
        CHECK(Nothing(cursor.Scan(Placeholder, false, jumped)) && jumped);
        CHECK(Nothing(cursor.Scan(Ring + 0x10 + RingCursor::MaxSpan + 4, false, jumped)) && jumped);
    }

    // Following the ring again forgets the place left: a later jump ahead is not read from it.
    {
        RingCursor cursor;
        cursor.Resync(Ring);
        CHECK(Reads(cursor.Scan(Ring + 0x10, false, jumped), Ring, Ring + 0x10));
        CHECK(Nothing(cursor.Scan(Placeholder, false, jumped)) && jumped);
        cursor.Resync(Ring + 0x5000); // the next ring segment
        CHECK(Reads(cursor.Scan(Ring + 0x5040, false, jumped), Ring + 0x5000, Ring + 0x5040));
        CHECK(Nothing(cursor.Scan(Placeholder, false, jumped)) && jumped);
        CHECK(Nothing(cursor.Scan(4, false, jumped)) && jumped);
        CHECK(Reads(cursor.Scan(Ring + 0x5100, false, jumped), Ring + 0x5040, Ring + 0x5100));
    }

    // The first scan only learns the position.
    {
        RingCursor cursor;
        CHECK(Nothing(cursor.Scan(Ring, false, jumped)) && !jumped);
        CHECK(Reads(cursor.Scan(Ring + 8, false, jumped), Ring, Ring + 8));
    }

    if (g_failures == 0)
        printf("ring cursor: all checks passed\n");
    return g_failures == 0 ? 0 : 1;
}
