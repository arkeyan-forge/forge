#pragma once

#include <cstdint>

// Imaginators' factory signature and the port's rule for toys made without one (the owner's decision, 2026-11-04): the game
// takes them as signed. A signature that is there but wrong stays refused.
namespace imaginators
{
    // The verifier's input starts with the signature: blocks 2 and 4 (32 bytes), then 34 and 62, then blocks 0 and 1.
    constexpr uint32_t SignatureFirstHalf = 32;

    // True when the signature's first half (blocks 2 and 4, in the plain sectors) is all zero: a toy made here. Blocks 34
    // and 62 sit inside the encrypted areas, so a tag made by older tools may hold ciphertext there: they do not count.
    inline bool SignatureMissing(const uint8_t* signature)
    {
        for (uint32_t i = 0; i < SignatureFirstHalf; i++)
            if (signature[i] != 0)
                return false;
        return true;
    }
}
