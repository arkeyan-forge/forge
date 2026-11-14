#pragma once
// Where the translated code sits in guest memory: the four bounds XenonRecomp writes to ppc_config.h (PPC_IMAGE_BASE,
// PPC_IMAGE_SIZE, PPC_CODE_BASE, PPC_CODE_SIZE). The function table follows the image: 8 bytes per guest code word.
//
// With ARKEYAN_LAYOUT_RECORD (CMake option, #72), the runtime does not compile those bounds in: LoadXex checks the
// build's record (g_guestLayoutRecord) against the parsed executable, then activates it before any import, thunk or
// guest thread, and runtime/layout/ppc_config.h makes the four names read the active layout. Translated code keeps
// its constants, so both must select the same table entries.

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

struct GuestLayout
{
    uint32_t imageBase{};
    uint32_t imageSize{};
    uint32_t codeBase{};
    uint32_t codeSize{};

    bool operator==(const GuestLayout&) const = default;
};

// Bumped when the record's fields or the lookup contract change.
inline constexpr uint32_t GuestLayoutVersion = 1;

struct GuestLayoutRecord
{
    uint32_t version{};
    GuestLayout layout;
    char imageSha256[65]{}; // lowercase hex of the image the code was translated from; empty: not recorded
};

// The build's record: guest_layout_record.cpp, from the generated headers (Arkeyan-Builder writes its own at install).
extern const GuestLayoutRecord g_guestLayoutRecord;

// The image and the table must stay clear of the heaps and SystemAlloc below 0x80000000 and of the physical-memory
// windows from 0xA0000000 (runtime/memory.cpp).
inline constexpr uint64_t GuestImageFloor = 0x80000000ull;
inline constexpr uint64_t GuestImageCeiling = 0xA0000000ull;

struct GuestSection
{
    uint32_t base;
    uint32_t size;
    bool code;
};

// The layout of a parsed executable, code bounds as Recompiler::Recompile computes them: the envelope of every code
// section, the gaps between them included. No code section: an empty code range.
GuestLayout ParsedGuestLayout(uint32_t imageBase, uint32_t imageSize, std::span<const GuestSection> sections);

// Empty if `record` describes the parsed executable and fits guest memory; else why not, for the log.
// `imageSha256` is the image's hash, needed only when the record has one.
std::string CheckGuestLayout(const GuestLayoutRecord& record, const GuestLayout& parsed, std::string_view imageSha256);

// A function start the table can hold: in the code range and word aligned.
inline bool InGuestCode(const GuestLayout& layout, uint64_t address)
{
    return address >= layout.codeBase && address < uint64_t(layout.codeBase) + layout.codeSize && address % 4 == 0;
}

namespace guest_layout_detail
{
    extern GuestLayout g_active; // only ActivateGuestLayout writes it (and the layout test)
}

// The layout the runtime runs with; all zero (an empty code range) until ActivateGuestLayout.
inline const GuestLayout& ActiveGuestLayout() { return guest_layout_detail::g_active; }

// Sets the active layout once. False (nothing changes) if another layout is already active.
bool ActivateGuestLayout(const GuestLayout& layout);
