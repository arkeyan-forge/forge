#pragma once

#include "host/input.h"
#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace netplay
{
    struct Pad
    {
        GamepadState state{};
        bool connected = false;
    };
    using InputFrame = std::array<Pad, MaxUsers>;
    inline constexpr size_t InputFrameBytes = MaxUsers * 13;
    inline constexpr uint64_t MaxReplayFrames = 10000000;
    inline constexpr size_t MaxReplayBytes = 512 * 1024 * 1024;

    struct Replay
    {
        std::string gameId;
        std::vector<InputFrame> frames;
    };

    std::vector<uint8_t> InputHeader(const std::string& gameId, uint64_t frames);
    std::array<uint8_t, InputFrameBytes> EncodeFrame(const InputFrame& frame);
    bool ParseReplay(std::span<const uint8_t> bytes, Replay& replay, std::string& error);
    InputFrame ReplayFrame(const Replay& replay, uint64_t index);

    class InputLatch
    {
    public:
        void Latch(const InputFrame& frame);
        Pad Get(int user, uint32_t& packet) const;

    private:
        InputFrame m_frame{};
        std::array<uint32_t, MaxUsers> m_packets{};
    };

    bool LatchedConnected(int user);
    bool ReadPad(int user, GamepadState& state, uint32_t& packet);
}
