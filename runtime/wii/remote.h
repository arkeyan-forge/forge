#pragma once

// The Wii remotes the game reads (docs/runtime/wii/remotes.md): remote 1 is always connected and its buttons come from
// the host's input (SetHostButtons) and from a script for the tests (SKY_WII_PRESS). The remotes' library above the
// Bluetooth stack (WPADProbe, KPADRead) is answered by the host, so the radio and the remotes' reports are never made.
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace wii::remote
{
    // WPAD's button bits, as KPADStatus holds them.
    namespace button
    {
        constexpr uint32_t Left = 0x0001, Right = 0x0002, Down = 0x0004, Up = 0x0008, Plus = 0x0010;
        constexpr uint32_t Two = 0x0100, One = 0x0200, B = 0x0400, A = 0x0800, Minus = 0x1000, Home = 0x8000;
    }

    // One scripted press: `buttons` held from read `first` for `count` reads.
    struct Press
    {
        uint32_t buttons = 0, first = 0, count = 0;
    };
    // "A@120+10,B@300": A held for reads 120-129, B for read 300 alone (each name: A B 1 2 + - H U D L R).
    bool ParseScript(const std::string& text, std::vector<Press>& out, std::string& error);

    constexpr uint32_t Remotes = 4;
    constexpr uint32_t StatusSize = 0xF0; // KPADStatus in this SDK

    class Remote
    {
    public:
        // The host's buttons, from its input thread.
        void SetHostButtons(uint32_t buttons) { host.store(buttons, std::memory_order_relaxed); }
        void SetScript(std::vector<Press> presses) { script = std::move(presses); }

        // KPADRead(remote 1, status, length): one sample, big-endian, into `status` (StatusSize bytes); the read
        // after it is the next in the script's count.
        void Sample(uint8_t* status);
        uint32_t Reads() const { return reads; }

    private:
        std::atomic<uint32_t> host{ 0 };
        std::vector<Press> script;
        uint32_t reads = 0, held = 0;
    };

    Remote& First(); // remote 1's, the process's
}
