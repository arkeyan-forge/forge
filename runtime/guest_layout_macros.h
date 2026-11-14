#pragma once
// The four layout names of ppc_config.h, read from the active layout (guest_layout.h) instead of compiled in. Included
// after the generated ppc_config.h, by runtime/layout/ppc_config.h (ARKEYAN_LAYOUT_RECORD) and the layout test. The
// type stays the generated one's (unsigned long long), so PPC_LOOKUP_FUNC computes exactly what translated code does.
#include "guest_layout.h"

#undef PPC_IMAGE_BASE
#undef PPC_IMAGE_SIZE
#undef PPC_CODE_BASE
#undef PPC_CODE_SIZE
#define PPC_IMAGE_BASE ((unsigned long long)ActiveGuestLayout().imageBase)
#define PPC_IMAGE_SIZE ((unsigned long long)ActiveGuestLayout().imageSize)
#define PPC_CODE_BASE ((unsigned long long)ActiveGuestLayout().codeBase)
#define PPC_CODE_SIZE ((unsigned long long)ActiveGuestLayout().codeSize)
