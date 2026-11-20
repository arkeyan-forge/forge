#pragma once

#include "netplay/inputs.h"
#include <array>
#include <cstdint>
#include <span>
#include <string_view>

namespace netplay
{
    inline constexpr uint16_t ProtocolVersion = 1;
    using Key = std::array<uint8_t, 16>;
    using Nonce = std::array<uint8_t, 16>;
    enum class PacketType : uint8_t
    {
        Hello = 1,
        Ping,
        Pong,
        Ready,
        Frames,
        End,
        Event
    };
    enum class EndReason : uint8_t
    {
        None,
        Protocol,
        Game,
        Image,
        Runtime,
        Platform,
        Fps,
        Seats,
        Timeout,
        Desync,
        Quit,
        Transport,
        Invalid
    };
    const char* ReasonText(EndReason reason);
    struct Hello
    {
        std::array<char, 64> game{}, image{};
        std::array<char, 32> runtime{}, platform{};
        uint32_t fps = 30;
        uint16_t protocol = ProtocolVersion;
        Nonce nonce{}, echo{};
        bool crossPlatform = false, host = false;
    };
    Hello MakeHello(std::string_view game, std::string_view image, std::string_view runtime, std::string_view platform,
        uint32_t fps, bool crossPlatform, bool host);
    EndReason CompareHello(const Hello& local, const Hello& remote);
    inline constexpr size_t MaxFrameEvents = 32;
    struct PortalEvent
    {
        uint64_t sequence = 0, frame = 0, figure = 0;
        uint8_t slot = 0;
        bool hasDump = false, last = false;
        std::array<uint8_t, 1024> dump{};
    };
    struct EventBatch
    {
        std::array<PortalEvent, MaxFrameEvents> events{};
        size_t count = 0;
    };
    struct FrameEvents
    {
        std::array<EventBatch, 2> players{};
    };
    inline bool SeatAllowed(bool host, uint8_t player) { return player <= 2 && (host ? player != 2 : player == 2); }
    struct FramePad
    {
        uint64_t frame = UINT64_MAX;
        Pad pad{};
        uint64_t events = 0;
    };
    struct Packet
    {
        PacketType type = PacketType::Hello;
        uint64_t sequence = 0, ack = 0, value = 0, hashFrame = 0, hash = 0;
        Hello hello{};
        std::array<FramePad, 8> pads{};
        uint8_t count = 0;
        uint64_t eventAck = 0;
        PortalEvent event{};
        EndReason reason = EndReason::None;
    };
    struct Datagram
    {
        std::array<uint8_t, 1200> bytes{};
        uint16_t size = 0;
        std::span<const uint8_t> View() const { return { bytes.data(), size }; }
    };
    Key Tag(std::span<const uint8_t> bytes, const Key& key);
    Key PasswordKey(std::string_view password);
    Key SessionKey(const Key& password, const Nonce& host, const Nonce& join);
    Key HelloKey(const Key& password, const Hello& hello);
    bool DecodeHello(std::span<const uint8_t> bytes, const Key& password, Packet& packet, uint64_t& dropped);
    bool EncodePacket(const Packet& packet, const Key& key, Datagram& bytes);
    bool DecodePacket(std::span<const uint8_t> bytes, const Key& key, Packet& packet, uint64_t& dropped);
    uint32_t ChooseDelay(uint64_t roundTripMs, uint32_t fps);
}
