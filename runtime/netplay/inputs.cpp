#include "netplay/inputs.h"
#include <algorithm>
#include <bit>

namespace netplay
{
    namespace
    {
        constexpr std::array<uint8_t, 8> Magic = { 'S', 'K', 'Y', 'P', 'A', 'D', '0', '1' };
        void Put(std::span<uint8_t> bytes, size_t offset, uint64_t value, size_t size)
        {
            for (size_t i = 0; i < size; ++i)
                bytes[offset + i] = uint8_t(value >> (i * 8));
        }
        uint64_t Get(std::span<const uint8_t> bytes, size_t offset, size_t size)
        {
            uint64_t value = 0;
            for (size_t i = 0; i < size; ++i)
                value |= uint64_t(bytes[offset + i]) << (i * 8);
            return value;
        }
        bool Equal(const Pad& a, const Pad& b)
        {
            const auto& x = a.state;
            const auto& y = b.state;
            return a.connected == b.connected && x.buttons == y.buttons && x.leftTrigger == y.leftTrigger &&
                   x.rightTrigger == y.rightTrigger && x.thumbLX == y.thumbLX && x.thumbLY == y.thumbLY &&
                   x.thumbRX == y.thumbRX && x.thumbRY == y.thumbRY;
        }
    }

    std::vector<uint8_t> InputHeader(const std::string& gameId, uint64_t frames)
    {
        std::vector<uint8_t> bytes(28 + gameId.size());
        std::copy(Magic.begin(), Magic.end(), bytes.begin());
        Put(bytes, 8, 1, 4);
        Put(bytes, 12, gameId.size(), 4);
        Put(bytes, 16, frames, 8);
        Put(bytes, 24, MaxUsers, 4);
        std::copy(gameId.begin(), gameId.end(), bytes.begin() + 28);
        return bytes;
    }

    std::array<uint8_t, InputFrameBytes> EncodeFrame(const InputFrame& frame)
    {
        std::array<uint8_t, InputFrameBytes> bytes{};
        for (size_t i = 0; i < frame.size(); ++i)
        {
            const auto& pad = frame[i];
            const auto& s = pad.state;
            const size_t at = i * 13;
            bytes[at] = pad.connected;
            Put(bytes, at + 1, s.buttons, 2);
            bytes[at + 3] = s.leftTrigger;
            bytes[at + 4] = s.rightTrigger;
            Put(bytes, at + 5, uint16_t(s.thumbLX), 2);
            Put(bytes, at + 7, uint16_t(s.thumbLY), 2);
            Put(bytes, at + 9, uint16_t(s.thumbRX), 2);
            Put(bytes, at + 11, uint16_t(s.thumbRY), 2);
        }
        return bytes;
    }

    bool ParseReplay(std::span<const uint8_t> bytes, Replay& replay, std::string& error)
    {
        replay = {};
        const auto fail = [&](const char* message) {
            error = message;
            return false;
        };
        if (bytes.size() < 28)
            return fail("truncated header");
        if (bytes.size() > MaxReplayBytes)
            return fail("oversize replay");
        if (!std::equal(Magic.begin(), Magic.end(), bytes.begin()))
            return fail("bad magic");
        if (Get(bytes, 8, 4) != 1)
            return fail("unsupported version");
        const uint64_t idSize = Get(bytes, 12, 4), count = Get(bytes, 16, 8);
        if (idSize == 0 || idSize > 64 || count > MaxReplayFrames || Get(bytes, 24, 4) != MaxUsers)
            return fail("invalid game id, user count or oversize frame count");
        const uint64_t headerSize = 28 + idSize;
        if (headerSize + count * InputFrameBytes != bytes.size())
            return fail("truncated replay or trailing bytes");
        for (size_t i = 28; i < headerSize; ++i)
            if (!((bytes[i] >= 'a' && bytes[i] <= 'z') || (bytes[i] >= '0' && bytes[i] <= '9') || bytes[i] == '_'))
                return fail("invalid game id");
        for (size_t at = size_t(headerSize); at < bytes.size(); at += 13)
            if (bytes[at] > 1)
                return fail("invalid connection flag");
        replay.gameId.assign(reinterpret_cast<const char*>(bytes.data() + 28), size_t(idSize));
        replay.frames.resize(size_t(count));
        for (size_t i = 0; i < count; ++i)
            for (size_t user = 0; user < MaxUsers; ++user)
            {
                const size_t at = size_t(headerSize) + i * InputFrameBytes + user * 13;
                auto& pad = replay.frames[i][user];
                pad.connected = bytes[at] != 0;
                auto& s = pad.state;
                s.buttons = uint16_t(Get(bytes, at + 1, 2));
                s.leftTrigger = bytes[at + 3];
                s.rightTrigger = bytes[at + 4];
                s.thumbLX = std::bit_cast<int16_t>(uint16_t(Get(bytes, at + 5, 2)));
                s.thumbLY = std::bit_cast<int16_t>(uint16_t(Get(bytes, at + 7, 2)));
                s.thumbRX = std::bit_cast<int16_t>(uint16_t(Get(bytes, at + 9, 2)));
                s.thumbRY = std::bit_cast<int16_t>(uint16_t(Get(bytes, at + 11, 2)));
            }
        error.clear();
        return true;
    }

    InputFrame ReplayFrame(const Replay& replay, uint64_t index)
    {
        if (index < replay.frames.size())
            return replay.frames[size_t(index)];
        InputFrame neutral{};
        if (!replay.frames.empty())
            for (size_t user = 0; user < MaxUsers; ++user)
                neutral[user].connected = replay.frames.back()[user].connected;
        return neutral;
    }

    void InputLatch::Latch(const InputFrame& frame)
    {
        for (size_t user = 0; user < MaxUsers; ++user)
            if (!Equal(frame[user], m_frame[user]))
                ++m_packets[user];
        m_frame = frame;
    }

    Pad InputLatch::Get(int user, uint32_t& packet) const
    {
        packet = 0;
        if (user < 0 || user >= MaxUsers)
            return {};
        packet = m_packets[size_t(user)];
        return m_frame[size_t(user)];
    }
}
