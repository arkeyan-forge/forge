// The layout this build's translated code was generated for, from its generated ppc_config.h (ARKEYAN_LAYOUT_RECORD).
// No image hash: a normal build does not know which executable produced ppc/. Arkeyan-Builder writes its own record at
// install, with the hash.
#define ARKEYAN_LAYOUT_CONSTANTS
#include "guest_layout.h"
#include <ppc_config.h>

static_assert(PPC_IMAGE_BASE + PPC_IMAGE_SIZE <= 0x100000000ull && PPC_CODE_BASE + PPC_CODE_SIZE <= 0x100000000ull);

const GuestLayoutRecord g_guestLayoutRecord = { GuestLayoutVersion,
    { uint32_t(PPC_IMAGE_BASE), uint32_t(PPC_IMAGE_SIZE), uint32_t(PPC_CODE_BASE), uint32_t(PPC_CODE_SIZE) }, "" };
