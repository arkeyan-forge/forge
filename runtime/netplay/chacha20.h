#pragma once

// ChaCha20 (RFC 8439): the stream cipher of the session's packets (netplay/protocol.h); our own code, checked against
// the RFC's test vectors (protocol_test.cpp).

#include <array>
#include <cstdint>
#include <span>

namespace netplay
{
    using CipherKey = std::array<uint8_t, 32>;
    using CipherNonce = std::array<uint8_t, 12>;
    std::array<uint8_t, 64> ChaCha20Block(const CipherKey& key, const CipherNonce& nonce, uint32_t counter);
    // Refuse counter exhaustion before changing any bytes.
    bool ChaCha20Xor(const CipherKey& key, const CipherNonce& nonce, uint32_t counter, std::span<uint8_t> bytes);
}
