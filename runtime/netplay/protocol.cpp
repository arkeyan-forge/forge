#include "netplay/protocol.h"
#include "netplay/chacha20.h"
#include <algorithm>
#include <bit>

namespace netplay
{
    namespace
    {
        constexpr std::array<uint8_t, 8> Magic{ 'S', 'K', 'Y', 'N', 'E', 'T', '0', '1' };
        uint64_t Read(std::span<const uint8_t> bytes)
        {
            uint64_t result = 0;
            for (size_t i = 0; i < bytes.size(); ++i)
                result |= uint64_t(bytes[i]) << (8 * i);
            return result;
        }
        struct Writer
        {
            Datagram& data;
            size_t at = 0;
            void Number(uint64_t value, size_t count)
            {
                for (size_t i = 0; i < count; ++i)
                    data.bytes[at++] = uint8_t(value >> (8 * i));
            }
            template <typename T, size_t N>
            void Array(const std::array<T, N>& bytes)
            {
                for (const auto byte : bytes)
                    data.bytes[at++] = uint8_t(byte);
            }
        };
        struct Reader
        {
            std::span<const uint8_t> bytes;
            size_t at = 0;
            uint64_t Number(size_t count)
            {
                const auto result = Read(bytes.subspan(at, count));
                at += count;
                return result;
            }
            template <typename T, size_t N>
            void Array(std::array<T, N>& result)
            {
                for (auto& byte : result)
                    byte = T(bytes[at++]);
            }
        };
        size_t PayloadSize(PacketType type, uint8_t count, bool dump = false, uint16_t chunkSize = 0)
        {
            switch (type)
            {
            case PacketType::Hello: return 232;
            case PacketType::Ping:
            case PacketType::Pong:
            case PacketType::Ready: return 8;
            case PacketType::Frames: return count <= 8 ? 26 + size_t(count) * 29 : 0;
            case PacketType::End: return 25;
            case PacketType::Event: return 26 + (dump ? 1024 : 0);
            case PacketType::BlockList:
            case PacketType::BlockData: return chunkSize && chunkSize <= RepairChunkBytes ? 18 + chunkSize : 0;
            case PacketType::BlockRequest: return 21;
            case PacketType::RepairResult: return 17;
            }
            return 0;
        }
        bool ValidChunk(PacketType type, const RepairChunk& chunk)
        {
            const uint32_t limit = type == PacketType::BlockList ? MaxBlockListBytes : MaxBlockDataBytes;
            const uint64_t offset = uint64_t(chunk.index) * RepairChunkBytes;
            return chunk.total && chunk.total <= limit && offset < chunk.total &&
                   chunk.size == std::min(uint64_t(RepairChunkBytes), chunk.total - offset);
        }
        bool ValidText(std::span<const char> text, bool hex = false)
        {
            bool padding = false, any = false;
            for (const auto c : text)
            {
                if (c == 0)
                    padding = true;
                else
                {
                    if (padding || c < 32 || c > 126 || (hex && !((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))))
                        return false;
                    any = true;
                }
            }
            return any && (!hex || !padding);
        }
    }

    Key Tag(std::span<const uint8_t> bytes, const Key& key)
    {
        uint64_t a = 0x736f6d6570736575ull ^ Read(std::span(key).first<8>());
        uint64_t b = 0x646f72616e646f6dull ^ Read(std::span(key).last<8>()) ^ 0xee;
        uint64_t c = 0x6c7967656e657261ull ^ Read(std::span(key).first<8>());
        uint64_t d = 0x7465646279746573ull ^ Read(std::span(key).last<8>());
        const auto rounds = [&](unsigned count) {
            for (unsigned i = 0; i < count; ++i)
            {
                a += b;
                b = std::rotl(b, 13) ^ a;
                a = std::rotl(a, 32);
                c += d;
                d = std::rotl(d, 16) ^ c;
                a += d;
                d = std::rotl(d, 21) ^ a;
                c += b;
                b = std::rotl(b, 17) ^ c;
                c = std::rotl(c, 32);
            }
        };
        const auto absorb = [&](uint64_t word) {
            d ^= word;
            rounds(2);
            a ^= word;
        };
        size_t at = 0;
        for (; at + 8 <= bytes.size(); at += 8)
            absorb(Read(bytes.subspan(at, 8)));
        absorb(Read(bytes.subspan(at)) | (uint64_t(bytes.size() & 255) << 56));
        c ^= 0xee;
        rounds(4);
        const uint64_t low = a ^ b ^ c ^ d;
        b ^= 0xdd;
        rounds(4);
        const uint64_t high = a ^ b ^ c ^ d;
        Key result{};
        for (size_t i = 0; i < 8; ++i)
        {
            result[i] = uint8_t(low >> (8 * i));
            result[8 + i] = uint8_t(high >> (8 * i));
        }
        return result;
    }

    Key PasswordKey(std::string_view password)
    {
        constexpr Key Domain{ 'S', 'K', 'Y', 'N', 'E', 'T', '-', 'P', 'A', 'S', 'S', '-', '0', '0', '0', '1' };
        return Tag({ reinterpret_cast<const uint8_t*>(password.data()), password.size() }, Domain);
    }

    Key SessionKey(const Key& password, const Nonce& host, const Nonce& join)
    {
        std::array<uint8_t, 32> nonces{};
        std::copy(host.begin(), host.end(), nonces.begin());
        std::copy(join.begin(), join.end(), nonces.begin() + 16);
        return Tag(nonces, password);
    }

    Key HelloKey(const Key& password, const Hello& hello)
    {
        return hello.host ? SessionKey(password, hello.nonce, hello.echo)
                          : SessionKey(password, hello.echo, hello.nonce);
    }

    bool DecodeHello(
        std::span<const uint8_t> bytes, const Key& password, Packet& packet, uint64_t& dropped, uint64_t* rejected)
    {
        if (bytes.size() != 280 || bytes[10] != uint8_t(PacketType::Hello))
        {
            ++dropped;
            return false;
        }
        // Only fixed, bounded nonce fields are inspected before authentication; none changes session state.
        Hello hello;
        std::copy_n(bytes.begin() + 228, 16, hello.nonce.begin());
        std::copy_n(bytes.begin() + 244, 16, hello.echo.begin());
        hello.host = bytes[261] != 0;
        return DecodePacket(bytes, HelloKey(password, hello), packet, dropped, rejected);
    }

    Hello MakeHello(std::string_view game, std::string_view image, std::string_view runtime, std::string_view platform,
        uint32_t fps, bool crossPlatform, bool host)
    {
        Hello hello;
        const auto copy = [](auto& target, std::string_view source) {
            std::copy_n(source.begin(), std::min(source.size(), target.size()), target.begin());
        };
        copy(hello.game, game);
        copy(hello.image, image);
        copy(hello.runtime, runtime);
        copy(hello.platform, platform);
        hello.fps = fps;
        hello.crossPlatform = crossPlatform;
        hello.host = host;
        return hello;
    }

    EndReason CompareHello(const Hello& local, const Hello& remote)
    {
        if (local.protocol != remote.protocol)
            return EndReason::Protocol;
        if (local.game != remote.game)
            return EndReason::Game;
        if (local.image != remote.image)
            return EndReason::Image;
        if (local.runtime != remote.runtime)
            return EndReason::Runtime;
        if (local.platform != remote.platform && !(local.crossPlatform && remote.crossPlatform))
            return EndReason::Platform;
        if (local.fps != remote.fps)
            return EndReason::Fps;
        return EndReason::None;
    }

    const char* ReasonText(EndReason reason)
    {
        switch (reason)
        {
        case EndReason::None: return "none";
        case EndReason::Protocol: return "protocol version differs";
        case EndReason::Game: return "game id differs";
        case EndReason::Image: return "image SHA-256 differs";
        case EndReason::Runtime: return "runtime version differs";
        case EndReason::Platform: return "platform differs";
        case EndReason::Fps: return "fps limit differs";
        case EndReason::Seats: return "player seats differ";
        case EndReason::Timeout: return "other player timed out";
        case EndReason::Desync: return "state hashes differ";
        case EndReason::Quit: return "other player quit";
        case EndReason::Transport: return "UDP transport failed";
        case EndReason::Invalid: return "invalid session data";
        }
        return "invalid session data";
    }

    namespace
    {
        // The payload between the sequence numbers and the tag, XORed with the session's keystream: a 32-byte key made
        // from the session key, the nonce from the sender's side and the packet's sequence (never reused in a session).
        void Encipher(std::span<uint8_t> payload, const Key& key, Cipher cipher, uint64_t sequence)
        {
            CipherKey cipherKey{};
            for (uint8_t half = 0; half < 2; ++half)
            {
                const std::array<uint8_t, 8> label{ 'c', 'i', 'p', 'h', 'e', 'r', '-', uint8_t('0' + half) };
                const Key part = Tag(label, key);
                std::copy(part.begin(), part.end(), cipherKey.begin() + half * 16);
            }
            CipherNonce nonce{};
            nonce[0] = cipher == Cipher::FromHost ? 1 : 2;
            for (int i = 0; i < 8; ++i)
                nonce[4 + i] = uint8_t(sequence >> (8 * i));
            ChaCha20Xor(cipherKey, nonce, 1, payload);
        }
    }

    bool EncodePacket(const Packet& packet, const Key& key, Datagram& bytes, Cipher cipher)
    {
        const size_t payload = PayloadSize(packet.type, packet.count, packet.event.hasDump, packet.chunk.size);
        bytes = {};
        if ((packet.type == PacketType::BlockList || packet.type == PacketType::BlockData) &&
            !ValidChunk(packet.type, packet.chunk))
            return false;
        if (packet.type == PacketType::BlockRequest &&
            (packet.stream < RepairStream::List || packet.stream > RepairStream::Data || !packet.value ||
                packet.chunk.index >= (MaxBlockListBytes + RepairChunkBytes - 1) / RepairChunkBytes))
            return false;
        if (packet.type == PacketType::RepairResult &&
            (packet.phase < RepairPhase::Verify || packet.phase > RepairPhase::Ack))
            return false;
        if (!payload || (packet.type == PacketType::Event && (packet.event.slot >= 16 || !packet.event.sequence ||
                                                                 (packet.event.hasDump && !packet.event.figure))))
            return false;
        Writer out{ bytes };
        out.Array(Magic);
        out.Number(ProtocolVersion, 2);
        out.Number(uint8_t(packet.type), 1);
        out.Number(0, 1);
        out.Number(32 + payload + 16, 2);
        out.Number(0, 2);
        out.Number(packet.sequence, 8);
        out.Number(packet.ack, 8);
        switch (packet.type)
        {
        case PacketType::BlockList:
        case PacketType::BlockData:
            out.Number(packet.hashFrame, 8);
            out.Number(packet.chunk.total, 4);
            out.Number(packet.chunk.index, 4);
            out.Number(packet.chunk.size, 2);
            for (size_t i = 0; i < packet.chunk.size; ++i)
                out.Number(packet.chunk.bytes[i], 1);
            break;
        case PacketType::BlockRequest:
            out.Number(packet.hashFrame, 8);
            out.Number(uint8_t(packet.stream), 1);
            out.Number(packet.chunk.index, 4);
            out.Number(packet.value, 8);
            break;
        case PacketType::RepairResult:
            out.Number(packet.hashFrame, 8);
            out.Number(uint8_t(packet.phase), 1);
            out.Number(packet.hash, 8);
            break;
        case PacketType::Hello:
            out.Array(packet.hello.game);
            out.Array(packet.hello.image);
            out.Array(packet.hello.runtime);
            out.Array(packet.hello.platform);
            out.Number(packet.hello.fps, 4);
            out.Array(packet.hello.nonce);
            out.Array(packet.hello.echo);
            out.Number(packet.hello.crossPlatform, 1);
            out.Number(packet.hello.host, 1);
            out.Number(packet.hello.protocol, 2);
            break;
        case PacketType::Ping:
        case PacketType::Pong:
        case PacketType::Ready: out.Number(packet.value, 8); break;
        case PacketType::Frames:
            out.Number(packet.count, 1);
            for (size_t i = 0; i < packet.count; ++i)
            {
                const auto& pad = packet.pads[i].pad;
                const auto& state = pad.state;
                out.Number(packet.pads[i].frame, 8);
                out.Number(packet.pads[i].events, 8);
                out.Number(pad.connected, 1);
                out.Number(state.buttons, 2);
                out.Number(state.leftTrigger, 1);
                out.Number(state.rightTrigger, 1);
                out.Number(uint16_t(state.thumbLX), 2);
                out.Number(uint16_t(state.thumbLY), 2);
                out.Number(uint16_t(state.thumbRX), 2);
                out.Number(uint16_t(state.thumbRY), 2);
            }
            out.Number(packet.hashFrame, 8);
            out.Number(packet.hash, 8);
            out.Number(packet.eventAck, 8);
            out.Number(packet.menuOpen, 1);
            break;
        case PacketType::Event:
            out.Number(packet.event.sequence, 8);
            out.Number(packet.event.frame, 8);
            out.Number(packet.event.figure, 8);
            out.Number(packet.event.slot, 1);
            out.Number((packet.event.hasDump ? 1 : 0) | (packet.event.last ? 2 : 0), 1);
            if (packet.event.hasDump)
                out.Array(packet.event.dump);
            break;
        case PacketType::End:
            out.Number(uint8_t(packet.reason), 1);
            out.Number(packet.hashFrame, 8);
            out.Number(packet.hash, 8);
            out.Number(packet.value, 8);
            break;
        }
        if (cipher != Cipher::None && packet.type != PacketType::Hello)
            Encipher(std::span(bytes.bytes).subspan(32, out.at - 32), key, cipher, packet.sequence);
        out.Array(Tag(std::span(bytes.bytes).first(out.at), key));
        bytes.size = uint16_t(out.at);
        return true;
    }

    bool DecodePacket(std::span<const uint8_t> sealed, const Key& key, Packet& packet, uint64_t& dropped,
        uint64_t* rejected, Cipher cipher)
    {
        const auto fail = [&] {
            ++dropped;
            return false;
        };
        if (sealed.size() < 48 || sealed.size() > Datagram{}.bytes.size() ||
            !std::equal(Magic.begin(), Magic.end(), sealed.begin()))
            return fail();
        if (Read(sealed.subspan(8, 2)) != ProtocolVersion || sealed[11] || sealed[14] || sealed[15] ||
            Read(sealed.subspan(12, 2)) != sealed.size())
            return fail();
        // Authenticated before anything inside is read: the payload's own lengths are encrypted.
        const auto tag = Tag(sealed.first(sealed.size() - 16), key);
        uint8_t difference = 0;
        for (size_t i = 0; i < tag.size(); ++i)
            difference |= tag[i] ^ sealed[sealed.size() - 16 + i];
        if (difference)
        {
            if (rejected)
                ++*rejected;
            return fail();
        }
        Datagram plain{};
        std::copy(sealed.begin(), sealed.end(), plain.bytes.begin());
        const auto bytes = std::span<const uint8_t>(plain.bytes).first(sealed.size());
        if (cipher != Cipher::None && PacketType(sealed[10]) != PacketType::Hello)
            Encipher(std::span(plain.bytes).subspan(32, sealed.size() - 48), key, cipher, Read(sealed.subspan(16, 8)));
        const auto type = PacketType(bytes[10]);
        if (type == PacketType::Frames && bytes.size() < 74)
            return fail();
        const uint8_t count = type == PacketType::Frames ? bytes[32] : 0;
        if (type == PacketType::Event && (bytes.size() < 74 || bytes[57] > 3))
            return fail();
        const bool chunk = type == PacketType::BlockList || type == PacketType::BlockData;
        if (chunk && bytes.size() < 67)
            return fail();
        const auto chunkSize = chunk ? uint16_t(Read(bytes.subspan(48, 2))) : uint16_t(0);
        const size_t payload = PayloadSize(type, count, type == PacketType::Event && (bytes[57] & 1), chunkSize);
        if (!payload || bytes.size() != 48 + payload)
            return fail();
        Packet result{};
        result.type = type;
        Reader in{ bytes, 16 };
        result.sequence = in.Number(8);
        result.ack = in.Number(8);
        switch (type)
        {
        case PacketType::BlockList:
        case PacketType::BlockData:
            result.hashFrame = in.Number(8);
            result.chunk.total = uint32_t(in.Number(4));
            result.chunk.index = uint32_t(in.Number(4));
            result.chunk.size = uint16_t(in.Number(2));
            if (!ValidChunk(type, result.chunk))
                return fail();
            for (size_t i = 0; i < result.chunk.size; ++i)
                result.chunk.bytes[i] = uint8_t(in.Number(1));
            break;
        case PacketType::BlockRequest:
            result.hashFrame = in.Number(8);
            result.stream = RepairStream(in.Number(1));
            result.chunk.index = uint32_t(in.Number(4));
            result.value = in.Number(8);
            if (result.stream < RepairStream::List || result.stream > RepairStream::Data || !result.value ||
                result.chunk.index >= (MaxBlockListBytes + RepairChunkBytes - 1) / RepairChunkBytes)
                return fail();
            break;
        case PacketType::RepairResult:
            result.hashFrame = in.Number(8);
            result.phase = RepairPhase(in.Number(1));
            result.hash = in.Number(8);
            if (result.phase < RepairPhase::Verify || result.phase > RepairPhase::Ack)
                return fail();
            break;
        case PacketType::Hello:
            in.Array(result.hello.game);
            in.Array(result.hello.image);
            in.Array(result.hello.runtime);
            in.Array(result.hello.platform);
            result.hello.fps = uint32_t(in.Number(4));
            in.Array(result.hello.nonce);
            in.Array(result.hello.echo);
            if (bytes[in.at] > 1 || bytes[in.at + 1] > 1)
                return fail();
            result.hello.crossPlatform = in.Number(1) != 0;
            result.hello.host = in.Number(1) != 0;
            result.hello.protocol = uint16_t(in.Number(2));
            if (!ValidText(result.hello.game) || !ValidText(result.hello.image, true) ||
                !ValidText(result.hello.runtime) || !ValidText(result.hello.platform) ||
                (result.hello.fps != 30 && result.hello.fps != 60))
                return fail();
            break;
        case PacketType::Ping:
        case PacketType::Pong:
        case PacketType::Ready: result.value = in.Number(8); break;
        case PacketType::Frames:
            result.count = uint8_t(in.Number(1));
            for (size_t i = 0; i < result.count; ++i)
            {
                auto& pad = result.pads[i].pad;
                auto& state = pad.state;
                result.pads[i].frame = in.Number(8);
                result.pads[i].events = in.Number(8);
                if (bytes[in.at] > 1)
                    return fail();
                pad.connected = in.Number(1) != 0;
                state.buttons = uint16_t(in.Number(2));
                state.leftTrigger = uint8_t(in.Number(1));
                state.rightTrigger = uint8_t(in.Number(1));
                state.thumbLX = std::bit_cast<int16_t>(uint16_t(in.Number(2)));
                state.thumbLY = std::bit_cast<int16_t>(uint16_t(in.Number(2)));
                state.thumbRX = std::bit_cast<int16_t>(uint16_t(in.Number(2)));
                state.thumbRY = std::bit_cast<int16_t>(uint16_t(in.Number(2)));
            }
            result.hashFrame = in.Number(8);
            result.hash = in.Number(8);
            result.eventAck = in.Number(8);
            if (bytes[in.at] > 1)
                return fail();
            result.menuOpen = in.Number(1) != 0;
            break;
        case PacketType::Event:
            result.event.sequence = in.Number(8);
            result.event.frame = in.Number(8);
            result.event.figure = in.Number(8);
            result.event.slot = uint8_t(in.Number(1));
            result.event.hasDump = (bytes[in.at] & 1) != 0;
            result.event.last = (in.Number(1) & 2) != 0;
            if (result.event.slot >= 16 || !result.event.sequence || (result.event.hasDump && !result.event.figure))
                return fail();
            if (result.event.hasDump)
                in.Array(result.event.dump);
            break;
        case PacketType::End:
            result.reason = EndReason(in.Number(1));
            if (result.reason <= EndReason::None || result.reason > EndReason::Invalid)
                return fail();
            result.hashFrame = in.Number(8);
            result.hash = in.Number(8);
            result.value = in.Number(8);
            break;
        }
        packet = result;
        return true;
    }

    uint32_t ChooseDelay(uint64_t roundTripMs, uint32_t fps)
    {
        const uint64_t bounded = std::min(roundTripMs, uint64_t(16000));
        return uint32_t(std::clamp((bounded * fps + 1999) / 2000 + 1, uint64_t(1), uint64_t(8)));
    }
}
