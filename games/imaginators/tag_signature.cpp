// Imaginators: a crystal made in the Toy Box has no factory signature; the game would call it corrupted. The hook makes the
// portal library's signature check pass for such a toy only (games/imaginators/docs/toybox.md, "Signature").
#include "tag_signature.h"
#include "game_hooks.h"
#include "memory.h"
#include <cstdio>
#include <ppc_config.h>
#include <ppc_context.h>

#ifdef GAME_HOOK_Portal_TagSignatureVerify
PPC_FUNC_IMPL(GAME_IMP(Portal_TagSignatureVerify));

PPC_FUNC(GAME_HOOK(Portal_TagSignatureVerify))
{
    uint8_t signature[imaginators::SignatureFirstHalf];
    for (uint32_t i = 0; i < sizeof(signature); i++)
        signature[i] = g_memory.Read<uint8_t>(ctx.r4.u32 + i);
    if (!imaginators::SignatureMissing(signature))
    {
        GAME_IMP(Portal_TagSignatureVerify)(ctx, base);
        return;
    }
    static bool told = false;
    if (!told)
        fprintf(stderr, "[portal] a toy without its factory signature is taken as signed (virtual)\n");
    told = true;
    ctx.r3.u64 = 0; // verified
}
#endif
