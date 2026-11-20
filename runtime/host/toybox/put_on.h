#pragma once

// Whether a seat takes a card (Model::PutOn): one rule for the refusal toast and for the seats a drag greys out, so the
// two never disagree. Plain C++, so a test can drive it (ArkeyanToyboxPutOnTest).
#include <span>

namespace toybox::puton
{
    enum class Refusal
    {
        None,
        RealPortal, // the real portal plays: the virtual one cannot put cards on
        Unknown,    // no such card in the collection, or no such seat
        RealFigure, // a real figure sits on the seat
        Busy,       // another program uses a half
        CannotRead, // the game reads no such toy
        WrongSeat,  // the game reads it, on another seat
    };

    // One half of the card (a swapper has two): whether another program uses it, whether the game reads that toy at all,
    // and whether this seat takes its kind.
    struct Half
    {
        bool busy = false;
        bool gameReads = true;
        bool seatTakes = true;
    };

    // In PutOn's order: the first refusal found.
    inline Refusal Check(bool realPortal, bool known, bool seatPhysical, std::span<const Half> halves)
    {
        if (realPortal)
            return Refusal::RealPortal;
        if (!known)
            return Refusal::Unknown;
        if (seatPhysical)
            return Refusal::RealFigure;
        for (const Half& half : halves)
        {
            if (half.busy)
                return Refusal::Busy;
            if (!half.gameReads)
                return Refusal::CannotRead;
            if (!half.seatTakes)
                return Refusal::WrongSeat;
        }
        return Refusal::None;
    }
}
