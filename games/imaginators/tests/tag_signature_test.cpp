// The port's rule for Imaginators toys without a factory signature (games/imaginators/tag_signature.h): only a missing
// signature is taken as signed; any byte of blocks 2 and 4 makes it the game's own check.
#include "tag_signature.h"
#include <array>
#include <cstdio>

namespace
{
    int g_checks, g_failures;
    void Check(bool ok, const char* what)
    {
        g_checks++;
        if (!ok)
            g_failures++, fprintf(stderr, "FAIL: %s\n", what);
    }
}

int main()
{
    using namespace imaginators;
    // The verifier's input: blocks 2, 4, 34, 62, 0, 1.
    std::array<uint8_t, 96> data{};
    data[64] = 0x12; // block 0, the serial: identity is not the signature
    data[80] = 0xA8; // block 1
    Check(SignatureMissing(data.data()), "a tag made here has no signature");
    data[32] = 0x5A; // ciphertext an older tool left in block 34
    Check(SignatureMissing(data.data()), "blocks 34 and 62 alone are no signature");
    for (uint32_t at : { 0u, 15u, 16u, 31u })
    {
        auto signed_ = data;
        signed_[at] = 1;
        Check(!SignatureMissing(signed_.data()), "any byte of blocks 2 and 4 counts");
    }
    printf("tag signature test: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
