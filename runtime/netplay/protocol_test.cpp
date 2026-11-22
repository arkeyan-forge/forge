#include "netplay/protocol.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <random>

namespace
{
    void Check(bool value, const char* message)
    {
        if (!value)
        {
            fprintf(stderr, "FAIL: %s\n", message);
            std::exit(1); // NOLINT(concurrency-mt-unsafe): one thread
        }
    }
}

int main()
{
    using namespace netplay;
    Key vectorKey{};
    for (size_t i = 0; i < vectorKey.size(); ++i)
        vectorKey[i] = uint8_t(i);
    constexpr Key EmptyTag{ 0xa3, 0x81, 0x7f, 0x04, 0xba, 0x25, 0xa8, 0xe6, 0x6d, 0xf6, 0x72, 0x14, 0xc7, 0x55, 0x02,
        0x93 };
    Check(Tag({}, vectorKey) == EmptyTag, "SipHash-2-4-128 empty message vector");
    const auto key = PasswordKey("room");
    Check(ProtocolVersion == 2, "repair requires protocol version two");
    for (auto type :
        { PacketType::BlockList, PacketType::BlockData, PacketType::BlockRequest, PacketType::RepairResult })
    {
        Packet repair;
        repair.type = type;
        repair.hashFrame = 300;
        repair.hash = 0x12345678;
        repair.value = 0x8000000000000001ull;
        repair.stream = RepairStream::Data;
        repair.phase = RepairPhase::Verify;
        repair.chunk.total = 2049;
        repair.chunk.index = 1;
        repair.chunk.size = 1024;
        repair.chunk.bytes.fill(0x73);
        Datagram wire;
        Packet result;
        uint64_t rejected = 0;
        Check(EncodePacket(repair, key, wire) && wire.size <= 1200 && DecodePacket(wire.View(), key, result, rejected),
            "repair packet round trip");
        Check(result.type == type && result.hashFrame == 300, "repair checkpoint survives encoding");
        if (type == PacketType::BlockList || type == PacketType::BlockData)
            Check(result.chunk.total == 2049 && result.chunk.index == 1 && result.chunk.size == 1024 &&
                      result.chunk.bytes == repair.chunk.bytes,
                "repair chunk fields and bytes");
        if (type == PacketType::BlockRequest)
            Check(result.stream == repair.stream && result.chunk.index == 1 && result.value == repair.value,
                "missing chunk bitmap round trip");
        if (type == PacketType::RepairResult)
            Check(result.phase == repair.phase && result.hash == repair.hash, "repair verification total round trip");
        for (size_t length = 0; length < wire.size; ++length)
            Check(!DecodePacket(wire.View().first(length), key, result, rejected),
                "all truncated repair packets refused");
        auto oversized = wire;
        ++oversized.size;
        oversized.bytes[12] = uint8_t(oversized.size);
        oversized.bytes[13] = uint8_t(oversized.size >> 8);
        const auto oversizedTag = Tag(oversized.View().first(oversized.size - 16), key);
        std::copy(oversizedTag.begin(), oversizedTag.end(), oversized.bytes.begin() + oversized.size - 16);
        Check(!DecodePacket(oversized.View(), key, result, rejected), "authenticated oversized repair packet refused");
        if (type == PacketType::BlockList || type == PacketType::BlockData)
        {
            repair.chunk.size = 1025;
            Check(!EncodePacket(repair, key, wire), "oversized repair chunk cannot be encoded");
            repair.chunk.size = 1024;
            repair.chunk.index = 2;
            Check(!EncodePacket(repair, key, wire), "incorrect final chunk size refused");
        }
    }
    Packet packet{};
    packet.type = PacketType::Hello;
    packet.hello = MakeHello("test", std::string(64, 'a'), "test-runtime", "test-cpu", 30, false, true);
    packet.hello.nonce[0] = 37;
    Datagram bytes{};
    Packet decoded{};
    uint64_t dropped = 0;
    EncodePacket(packet, key, bytes);
    Check(DecodePacket(bytes.View(), key, decoded, dropped), "hello round trip");
    Check(CompareHello(packet.hello, decoded.hello) == EndReason::None, "same identity");
    EncodePacket(packet, HelloKey(key, packet.hello), bytes);
    Check(DecodeHello(bytes.View(), key, decoded, dropped), "nonce-bound hello tag");
    Check(!DecodeHello(bytes.View(), PasswordKey("wrong"), decoded, dropped), "hello password");
    const auto mismatch = [&](auto change, EndReason reason) {
        auto hello = packet.hello;
        change(hello);
        Check(CompareHello(packet.hello, hello) == reason, "identity refusal");
    };
    mismatch([](Hello& h) { ++h.protocol; }, EndReason::Protocol);
    mismatch([](Hello& h) { h.game[0] ^= 1; }, EndReason::Game);
    mismatch([](Hello& h) { h.image[0] ^= 1; }, EndReason::Image);
    mismatch([](Hello& h) { h.runtime[0] ^= 1; }, EndReason::Runtime);
    mismatch([](Hello& h) { h.platform[0] ^= 1; }, EndReason::Platform);
    mismatch([](Hello& h) { h.fps = 60; }, EndReason::Fps);
    auto cross = packet.hello;
    cross.platform[0] ^= 1;
    cross.crossPlatform = true;
    Check(CompareHello(packet.hello, cross) == EndReason::Platform, "both must allow cross platform");
    auto allowed = packet.hello;
    allowed.crossPlatform = true;
    Check(CompareHello(allowed, cross) == EndReason::None, "explicit cross platform");
    for (const auto type :
        { PacketType::Ping, PacketType::Pong, PacketType::Ready, PacketType::Frames, PacketType::End })
    {
        packet.type = type;
        packet.sequence = 987;
        packet.ack = 63;
        packet.value = 4;
        packet.count = 8;
        packet.hashFrame = 300;
        packet.hash = 1234567;
        packet.reason = EndReason::Desync;
        for (size_t i = 0; i < packet.pads.size(); ++i)
        {
            packet.pads[i].frame = 70 + i;
            packet.pads[i].pad = { { uint16_t(i), 19, 207, -32768, 32767, -12, 38 }, true };
        }
        EncodePacket(packet, key, bytes);
        Check(DecodePacket(bytes.View(), key, decoded, dropped), "packet round trip");
        Check(decoded.type == type && decoded.sequence == 987 && decoded.ack == 63, "packet header");
        if (type == PacketType::Frames)
            Check(decoded.count == 8 && decoded.pads[7].frame == 77 && decoded.pads[7].pad.state.thumbLX == -32768 &&
                      decoded.hashFrame == 300 && decoded.hash == 1234567,
                "pad and hash fields");
    }
    packet.type = PacketType::Frames;
    for (bool open : { false, true, false })
    {
        packet.menuOpen = open;
        Check(EncodePacket(packet, key, bytes) && DecodePacket(bytes.View(), key, decoded, dropped) &&
                  decoded.menuOpen == open,
            "menu flag round trip, independent of pads");
    }
    const auto savedPacket = packet;
    packet = {};
    packet.type = PacketType::Event;
    packet.event.sequence = 17;
    packet.event.frame = 42;
    packet.event.slot = 3;
    packet.event.figure = 123;
    packet.event.hasDump = true;
    packet.event.last = true;
    for (size_t i = 0; i < packet.event.dump.size(); ++i)
        packet.event.dump[i] = uint8_t(i * 13);
    Check(EncodePacket(packet, key, bytes) && DecodePacket(bytes.View(), key, decoded, dropped),
        "figure event round trip");
    Check(decoded.event.dump == packet.event.dump && decoded.event.figure == 123 && decoded.event.slot == 3 &&
              decoded.event.sequence == 17 && decoded.event.frame == 42 && decoded.event.last,
        "full event fields");
    packet.event.hasDump = false;
    Check(EncodePacket(packet, key, bytes) && bytes.size < 128 && DecodePacket(bytes.View(), key, decoded, dropped),
        "known figure uses only its id");
    packet.event.figure = 0;
    Check(
        EncodePacket(packet, key, bytes) && DecodePacket(bytes.View(), key, decoded, dropped) && !decoded.event.figure,
        "removal event");
    packet.event.slot = 16;
    Check(!EncodePacket(packet, key, bytes), "invalid event slot refused");
    packet.event.slot = 0;
    EncodePacket(packet, key, bytes);
    packet = savedPacket;
    EncodePacket(packet, key, bytes);
    Check(!DecodePacket(bytes.View(), PasswordKey("wrong"), decoded, dropped), "wrong password tag");
    std::mt19937 random(9281);
    const auto valid = bytes;
    for (unsigned i = 0; i < 100000; ++i)
    {
        bytes = valid;
        if ((i & 1) == 0)
        {
            bytes.size = uint16_t(random() % (bytes.bytes.size() + 1));
            for (auto& byte : bytes.bytes)
                byte = uint8_t(random());
        }
        else
        {
            const size_t at = random() % bytes.size;
            bytes.bytes[at] ^= uint8_t(1u << (random() % 8));
        }
        const uint64_t before = dropped;
        Check(!DecodePacket(bytes.View(), key, decoded, dropped) && dropped == before + 1, "fuzz dropped and counted");
    }
    bytes = valid;
    bytes.bytes[8] ^= 1;
    Check(!DecodePacket(bytes.View(), key, decoded, dropped), "wrong version");
    bytes = valid;
    --bytes.size;
    Check(!DecodePacket(bytes.View(), key, decoded, dropped), "wrong size");
    packet.type = PacketType::Hello;
    EncodePacket(packet, HelloKey(key, packet.hello), bytes);
    for (size_t size = 0; size < bytes.size; ++size)
        Check(!DecodeHello(bytes.View().first(size), key, decoded, dropped), "every truncated hello");
    packet.type = PacketType::Frames;
    packet.count = 8;
    EncodePacket(packet, key, bytes);
    for (size_t size = 0; size < bytes.size; ++size)
        Check(!DecodePacket(bytes.View().first(size), key, decoded, dropped), "every truncated frame packet");
    bytes.bytes[49] = 2;
    const auto tag = Tag(bytes.View().first(bytes.size - 16), key);
    std::copy(tag.begin(), tag.end(), bytes.bytes.begin() + bytes.size - 16);
    Check(!DecodePacket(bytes.View(), key, decoded, dropped), "authenticated invalid connection flag");
    packet.type = PacketType::Frames;
    packet.count = 9;
    Check(!EncodePacket(packet, key, bytes), "oversize frame history");
    Check(
        ChooseDelay(0, 30) == 1 && ChooseDelay(67, 30) == 3 && ChooseDelay(10000, 60) == 8 && ChooseDelay(34, 60) == 3,
        "round trip delay");
    Check(
        SessionKey(key, packet.hello.nonce, {}) != SessionKey(key, {}, packet.hello.nonce), "nonce order binds seats");
    std::puts("protocol: round trips, identity refusals, 100000 malformed packets passed");
}
