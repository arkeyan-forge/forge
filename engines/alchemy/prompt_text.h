#pragma once

// The texts that name the Start button in words (engines/alchemy/prompt_text.cpp): the word to show as the button's picture.
// Pure text, no guest memory (engines/alchemy/tests/prompt_text_test.cpp).
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace alchemy::prompt
{
    // What stands for the Start button in the text (no game text uses a control character but CR and LF).
    constexpr char StartCharacter = 1;
    // The longest text looked at: the prompts are 11 to 20 bytes; anything longer is not one.
    constexpr size_t LongestPrompt = 40;

    // The press-Start phrases of every disc language (Swap Force English and French, SuperChargers English and German,
    // Imaginators French and Dutch), in lower case: the text ends with one of them, at the start of the text or after a
    // space. The games write them in capitals ("PRESS START", "DRÜCKE START") or not ("Appuie sur Start", "Druk op
    // Start"); script names that end with the word ("On Start", "Fog Start", "Player Start") are not prompts and stay.
    constexpr std::string_view Phrases[] = { "press start", "appuyer sur start", "appuie sur start", "druk op start",
        "drücke start" };

    // Lower case for ASCII and for the UTF-8 capitals of Latin-1 (À-Þ but ×, written C3 80-9E: Ü is C3 9C, ü C3 BC).
    inline std::string Lower(std::string_view text)
    {
        std::string out(text);
        for (size_t i = 0; i < out.size(); ++i)
        {
            unsigned char c = static_cast<unsigned char>(out[i]);
            if (c >= 'A' && c <= 'Z')
                out[i] = char(c + 32);
            else if (c == 0xC3 && i + 1 < out.size())
            {
                unsigned char next = static_cast<unsigned char>(out[i + 1]);
                if (next >= 0x80 && next <= 0x9E && next != 0x97)
                    out[i + 1] = char(next + 0x20);
                ++i;
            }
        }
        return out;
    }

    // Where the word START begins when `text` is a press-Start prompt, else nothing.
    inline std::optional<size_t> StartWord(std::string_view text)
    {
        if (text.size() > LongestPrompt)
            return std::nullopt;
        std::string lower = Lower(text);
        for (std::string_view phrase : Phrases)
        {
            if (!std::string_view(lower).ends_with(phrase))
                continue;
            size_t at = lower.size() - phrase.size();
            if (at == 0 || lower[at - 1] == ' ')
                return lower.size() - 5; // "start"
        }
        return std::nullopt;
    }

    // The text with START replaced by StartCharacter, or nothing when it does not name the button.
    inline std::optional<std::string> WithStartPicture(std::string_view text)
    {
        std::optional<size_t> at = StartWord(text);
        if (!at)
            return std::nullopt;
        return std::string(text.substr(0, *at)) + StartCharacter;
    }
}
