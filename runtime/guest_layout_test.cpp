// The layout record (#72, guest_layout.h) against the constant path, on invented layouts (no game code, no disc):
// parsed code envelopes, the record checks, PPC_LOOKUP_FUNC's entries, the code-range guard, the table's read-only
// granules, the image queries and thunk slots, each equal with the four bounds compiled in and read from the active
// layout. Prints the lookup loop's cost both ways.
//   ./ArkeyanGuestLayoutTest             Exit code 0 = pass.
#include "guest_layout.h"
#include "thunk_slot.h"
#include <chrono>
#include <cstdio>
#include <set>
#include <type_traits>
#include <vector>

// A generated ppc_config.h with no policy macros; the bounds are set per layout below.
#define PPC_CONFIG_H_INCLUDED
#include <ppc_context.h>

namespace
{
    int g_checks = 0, g_failures = 0;

#define CHECK(condition)                                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        g_checks++;                                                                                                    \
        if (!(condition))                                                                                              \
        {                                                                                                              \
            g_failures++;                                                                                              \
            fprintf(stderr, "FAILED %s:%d: %s\n", __FILE__, __LINE__, #condition);                                     \
        }                                                                                                              \
    } while (0)

    // A host address only used for arithmetic: nothing is read or written there.
    uint8_t* const Base = reinterpret_cast<uint8_t*>(uintptr_t(0x100000000000ull));

    // The table's read-only part, as loader.cpp's TableBegin/TableEnd compute it from two entries.
    constexpr uintptr_t TableGranule = 0x10000;
    uintptr_t ReadOnlyBegin(uintptr_t codeBaseEntry)
    {
        return (codeBaseEntry + TableGranule - 1) & ~(TableGranule - 1);
    }
    uintptr_t ReadOnlyEnd(uintptr_t codeEndEntry) { return codeEndEntry & ~(TableGranule - 1); }

    struct Constant
    {
        uintptr_t (*entry)(uint32_t address);
        bool (*inCode)(uint32_t address); // indirect_call_guard.cpp's GuestCodeAddress
        bool (*inImage)(uint32_t address); // kernel/memory.cpp's image queries
        uint32_t imagePages;
        uint32_t codeBase, codeEnd;
    };

// One invented layout with its bounds compiled in, as a generated ppc_config.h has them.
#define CONSTANT_LAYOUT(name)                                                                                          \
    static_assert(std::is_same_v<decltype(PPC_IMAGE_BASE), unsigned long long>);                                       \
    const Constant name = { [](uint32_t address) {                                                                     \
                               return reinterpret_cast<uintptr_t>(&PPC_LOOKUP_FUNC(Base, address));                    \
                           },                                                                                          \
        [](uint32_t address) {                                                                                         \
            return __builtin_rotateright32(address - uint32_t(PPC_CODE_BASE), 2) < uint32_t(PPC_CODE_SIZE / 4);        \
        },                                                                                                             \
        [](uint32_t address) { return address >= PPC_IMAGE_BASE && address < PPC_IMAGE_BASE + PPC_IMAGE_SIZE; },       \
        uint32_t(PPC_IMAGE_SIZE / 0x1000), uint32_t(PPC_CODE_BASE), uint32_t(PPC_CODE_BASE + PPC_CODE_SIZE) }

// One code section.
#define PPC_IMAGE_BASE 0x82000000ull
#define PPC_IMAGE_SIZE 0x400000ull
#define PPC_CODE_BASE 0x82010000ull
#define PPC_CODE_SIZE 0x200000ull
    CONSTANT_LAYOUT(OneSection);
    uintptr_t OneSectionEntry(uint32_t address) { return reinterpret_cast<uintptr_t>(&PPC_LOOKUP_FUNC(Base, address)); }
    const GuestLayout OneSectionLayout = { 0x82000000, 0x400000, 0x82010000, 0x200000 };
#undef PPC_IMAGE_BASE
#undef PPC_IMAGE_SIZE
#undef PPC_CODE_BASE
#undef PPC_CODE_SIZE

// Three code sections with data and padding between them: the code range is their envelope.
#define PPC_IMAGE_BASE 0x88000000ull
#define PPC_IMAGE_SIZE 0x1000000ull
#define PPC_CODE_BASE 0x88010000ull
#define PPC_CODE_SIZE 0x4F0000ull
    CONSTANT_LAYOUT(Gaps);
    const GuestSection GapsSections[] = {
        { 0x88200000, 0x300000, true }, // listed out of order: the envelope does not depend on it
        { 0x88000000, 0x10000, false },
        { 0x88010000, 0x100000, true },
        { 0x88110000, 0x8000, false },
        { 0x88118000, 0x7FF0, true },
        { 0x88500000, 0x20000, false },
    };
    const GuestLayout GapsLayout = { 0x88000000, 0x1000000, 0x88010000, 0x4F0000 };
#undef PPC_IMAGE_BASE
#undef PPC_IMAGE_SIZE
#undef PPC_CODE_BASE
#undef PPC_CODE_SIZE

// The table ends 8 bytes short of the physical windows; code not on a granule boundary.
#define PPC_IMAGE_BASE 0x88000000ull
#define PPC_IMAGE_SIZE 0x8000000ull
#define PPC_CODE_BASE 0x88000004ull
#define PPC_CODE_SIZE 0x7FFFFFCull
    CONSTANT_LAYOUT(Ceiling);
    const GuestLayout CeilingLayout = { 0x88000000, 0x8000000, 0x88000004, 0x7FFFFFC };
#undef PPC_IMAGE_BASE
#undef PPC_IMAGE_SIZE
#undef PPC_CODE_BASE
#undef PPC_CODE_SIZE
}

// From here on the four names read the active layout, as runtime sources see them with ARKEYAN_LAYOUT_RECORD.
#include "guest_layout_macros.h"
static_assert(std::is_same_v<decltype(PPC_IMAGE_BASE), unsigned long long>);

namespace
{
    void Use(const GuestLayout& layout) { guest_layout_detail::g_active = layout; }

    uintptr_t Entry(uint32_t address) { return reinterpret_cast<uintptr_t>(&PPC_LOOKUP_FUNC(Base, address)); }
    bool InCode(uint32_t address)
    {
        return __builtin_rotateright32(address - uint32_t(PPC_CODE_BASE), 2) < uint32_t(PPC_CODE_SIZE / 4);
    }
    bool InImage(uint32_t address) { return address >= PPC_IMAGE_BASE && address < PPC_IMAGE_BASE + PPC_IMAGE_SIZE; }

    GuestLayoutRecord Record(const GuestLayout& layout, const char* sha256 = "")
    {
        GuestLayoutRecord record{ GuestLayoutVersion, layout, {} };
        snprintf(record.imageSha256, sizeof(record.imageSha256), "%s", sha256);
        return record;
    }

    // Every table entry, guard answer and image query the runtime can ask for is the constant one.
    void SameAsConstant(const GuestLayout& layout, const Constant& constant)
    {
        Use(layout);
        CHECK(constant.codeBase == layout.codeBase && constant.codeEnd == layout.codeBase + layout.codeSize);
        std::vector<uint32_t> addresses = { 0, 1, 4, layout.imageBase - 4, layout.imageBase, layout.codeBase - 4,
            layout.codeBase - 1, layout.codeBase, layout.codeBase + 2, layout.codeBase + 4, constant.codeEnd - 4,
            constant.codeEnd - 2, constant.codeEnd, constant.codeEnd + 4, layout.imageBase + layout.imageSize - 4,
            layout.imageBase + layout.imageSize, 0xFFFFFFFC };
        for (uint64_t address = layout.codeBase; address < constant.codeEnd; address += 0x1234 * 4 + 2)
            addresses.push_back(uint32_t(address));
        bool same = true;
        for (uint32_t address : addresses)
        {
            same = same && Entry(address) == constant.entry(address) && InCode(address) == constant.inCode(address) &&
                   InImage(address) == constant.inImage(address);
            if (InCode(address) != InGuestCode(layout, address))
                same = false;
        }
        CHECK(same);
        CHECK(uint32_t(PPC_IMAGE_SIZE / 0x1000) == constant.imagePages);

        // The read-only granules: the same, inside the table, below the physical windows.
        const uintptr_t begin = ReadOnlyBegin(Entry(uint32_t(PPC_CODE_BASE)));
        const uintptr_t end = ReadOnlyEnd(Entry(uint32_t(PPC_CODE_BASE + PPC_CODE_SIZE)));
        CHECK(begin == ReadOnlyBegin(constant.entry(constant.codeBase)) &&
              end == ReadOnlyEnd(constant.entry(constant.codeEnd)));
        CHECK(begin >= Entry(layout.codeBase) && end <= Entry(constant.codeEnd) && begin % TableGranule == 0);
        CHECK(Entry(constant.codeEnd) <= reinterpret_cast<uintptr_t>(Base) + GuestImageCeiling);
        CHECK(Entry(layout.codeBase) == reinterpret_cast<uintptr_t>(Base) + layout.imageBase + layout.imageSize);
    }

    void ParsedLayouts()
    {
        CHECK(ParsedGuestLayout(0x88000000, 0x1000000, GapsSections) == GapsLayout);
        const GuestSection one[] = { { 0x82000000, 0x10000, false }, { 0x82010000, 0x200000, true } };
        CHECK(ParsedGuestLayout(0x82000000, 0x400000, one) == OneSectionLayout);
        const GuestSection data[] = { { 0x82000000, 0x10000, false } };
        CHECK(ParsedGuestLayout(0x82000000, 0x400000, data).codeSize == 0);
    }

    void Checks()
    {
        for (const GuestLayout& layout : { OneSectionLayout, GapsLayout, CeilingLayout })
            CHECK(CheckGuestLayout(Record(layout), layout, "").empty());

        // The executable is not the one the code was translated from.
        auto mismatch = [](GuestLayout parsed) { return !CheckGuestLayout(Record(GapsLayout), parsed, "").empty(); };
        GuestLayout other = GapsLayout;
        other.imageBase += 0x10000;
        CHECK(mismatch(other));
        other = GapsLayout;
        other.imageSize += 0x1000;
        CHECK(mismatch(other));
        other = GapsLayout;
        other.codeBase += 4;
        CHECK(mismatch(other));
        other = GapsLayout;
        other.codeSize -= 4; // the first section only, or a lost last section
        CHECK(mismatch(other));
        GuestLayoutRecord record = Record(GapsLayout);
        record.version = GuestLayoutVersion + 1;
        CHECK(!CheckGuestLayout(record, GapsLayout, "").empty());

        // Identity: checked when recorded (an equal layout from another release is refused).
        const char* hash = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
        CHECK(CheckGuestLayout(Record(GapsLayout, hash), GapsLayout, hash).empty());
        CHECK(!CheckGuestLayout(Record(GapsLayout, hash), GapsLayout, "").empty());
        CHECK(CheckGuestLayout(Record(GapsLayout, hash), GapsLayout,
                  "1123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef")
                  .find("hash") != std::string::npos);

        // Layouts no build can run, even when record and executable agree.
        auto refused = [](const GuestLayout& layout) { return !CheckGuestLayout(Record(layout), layout, "").empty(); };
        CHECK(refused({ 0x82000000, 0x400000, 0, 0 }));                     // no code
        CHECK(refused({ 0x82000000, 0, 0x82000000, 0x100 }));               // empty image
        CHECK(refused({ 0x82000000, 0x400000, 0x82000002, 0x100 }));        // misaligned code
        CHECK(refused({ 0x82000000, 0x400000, 0x82010000, 0x102 }));        // misaligned code end
        CHECK(refused({ 0x82000000, 0x400000, 0x81FF0000, 0x100 }));        // code before the image
        CHECK(refused({ 0x82000000, 0x400000, 0x823F0000, 0x20000 }));      // code past the image
        CHECK(refused({ 0x70000000, 0x400000, 0x70010000, 0x100 }));        // over SystemAlloc
        CHECK(refused({ 0x88000000, 0x8010000, 0x88000000, 0x8000000 }));   // table into the physical windows
        CHECK(refused({ 0xFFFF0000, 0x20000, 0xFFFF0000, 0x10000 }));       // past 4 GB
        CHECK(refused({ 0x9FFF0000, 0x10000, 0x9FFF0000, 0x10000 }));       // the table alone crosses the ceiling
    }

    void Activation()
    {
        Use({});
        CHECK(!InCode(0x82010000) && !InCode(0)); // nothing active: no address is code
        CHECK(ActivateGuestLayout(OneSectionLayout) && ActivateGuestLayout(OneSectionLayout));
        CHECK(!ActivateGuestLayout(GapsLayout) && ActiveGuestLayout() == OneSectionLayout);
        CHECK(InCode(0x82010000) && !InCode(0x82010000 + 0x200000));
    }

    // CreateHostThunk's search (two tries below the last thunk) from `codeEnd` down to `codeBase`, until none is left.
    template <typename IsPadding>
    std::vector<uint32_t> ThunkSequence(uint32_t codeBase, uint32_t codeEnd, IsPadding isPadding)
    {
        std::vector<uint32_t> thunks;
        for (uint32_t next = codeEnd;;)
        {
            uint32_t thunk = FindThunkSlot(next, codeBase, 16, isPadding);
            if (thunk == 0)
                thunk = FindThunkSlot(next, codeBase, 1, isPadding);
            if (thunk == 0)
                return thunks;
            thunks.push_back(thunk);
            next = thunk;
        }
    }

    // Thunks with the active bounds: the same slots as with the constants, only in padding inside the envelope, going
    // down, then exhaustion.
    void Thunks()
    {
        Use(GapsLayout);
        std::set<uint32_t> padding, taken;
        for (uint32_t gap : { 0x88110000u, 0x88118000u + 0x7FF0u })
            for (uint32_t address = gap; address < gap + 0x100; address += 4)
                padding.insert(address);
        padding.insert(0x88200000 + 0x1000); // a lone zero word inside a section
        auto isPadding = [&](uint32_t address) { return padding.contains(address) && !taken.contains(address); };

        const std::vector<uint32_t> constant = ThunkSequence(Gaps.codeBase, Gaps.codeEnd, isPadding);
        const std::vector<uint32_t> active =
            ThunkSequence(uint32_t(PPC_CODE_BASE), uint32_t(PPC_CODE_BASE + PPC_CODE_SIZE), isPadding);
        CHECK(!active.empty() && active == constant);
        bool inPadding = true;
        for (size_t i = 0; i < active.size(); i++)
            inPadding =
                inPadding && padding.contains(active[i]) && InCode(active[i]) && (i == 0 || active[i] < active[i - 1]);
        CHECK(inPadding);
        CHECK(active.front() < 0x88200000 && active.back() < 0x88118000); // both gaps used, the 16-word run first

        taken.insert(active.begin(), active.end()); // CreateHostThunk aborts here: no slot below the last thunk
        CHECK(FindThunkSlot(active.back(), uint32_t(PPC_CODE_BASE), 1, isPadding) == 0);
    }

    // The runtime's lookups with the bounds compiled in and read from the active layout.
    void Cost()
    {
        Use(OneSectionLayout);
        constexpr int Rounds = 64;
        auto time = [](auto entry, uintptr_t& sum) {
            auto start = std::chrono::steady_clock::now();
            for (int round = 0; round < Rounds; round++)
                for (uint32_t address = 0x82010000; address < 0x82010000 + 0x200000; address += 4)
                    sum += entry(address ^ uint32_t(sum & 4));
            return std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - start).count();
        };
        uintptr_t constantSum = 0, activeSum = 0;
        const double constantNs = time([](uint32_t address) { return OneSectionEntry(address); }, constantSum);
        const double activeNs = time([](uint32_t address) { return Entry(address); }, activeSum);
        const double lookups = double(Rounds) * (0x200000 / 4);
        CHECK(constantSum == activeSum);
        printf("lookup: %.2f ns compiled in, %.2f ns from the active layout (%.0f lookups each)\n",
            constantNs / lookups, activeNs / lookups, lookups);
    }
}

int main()
{
    ParsedLayouts();
    Checks();
    SameAsConstant(OneSectionLayout, OneSection);
    SameAsConstant(GapsLayout, Gaps);
    SameAsConstant(CeilingLayout, Ceiling);
    Activation();
    Thunks();
    Cost();
    printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
