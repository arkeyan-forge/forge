#pragma once

#include <cstdint>
#include <string_view>

// Skipping a spoken line of a cutscene the game does not let the player skip [Alchemy] [TrapTeam]
// (docs/engines/alchemy/dialogue_skip.md). A cutscene holds the player while its steps run on its own clock; a line is a
// voice it starts on the way. A press of the game's skip button stops the line's voice and runs the game's logic fast
// until the cutscene starts its next line or ends, so every step of the cutscene still runs, only sooner.
namespace alchemy
{
    // A tfbSoundInfoImpl's properties word (game::SoundProperties) holds its category in bits 1-5, an index into the
    // game's list "Effects/", ..., "Voice/" 12, ..., "Music" 28, the same in every game.
    inline uint32_t SoundCategory(uint32_t properties) { return (properties >> 1) & 31; }

    // A character's line: "Voice/", the Skylanders', enemies' and NPCs' "Dialog", the bare "Voice/Enemy/" and
    // "Voice/NPC/", and the announcer's. The emotes (grunts, laughs) and the Skylanders' interface voices are not lines.
    inline bool IsSpokenLine(uint32_t category)
    {
        switch (category)
        {
        case 12: // Voice/
        case 15: // Voice/Champion/Dialog
        case 16: // Voice/Enemy/
        case 18: // Voice/Enemy/Dialog
        case 19: // Voice/NPC/
        case 21: // Voice/NPC/Dialog
        case 22: // Voice/Announcer/
        case 23: // Voice/Announcer/Commentary
        case 24: // Voice/Announcer/Instruction
            return true;
        default: return false;
        }
    }

    // The cutscene system's script variables (ValueInfo, the integer at +0x20, the name at +8): 1 while a cutscene holds
    // the player, 1 once the game skips the cutscene itself, and the flags a cutscene writes as it starts to say whether
    // the game's own skip may end it.
    constexpr uint32_t ValueName = 8, ValueWord = 0x20;
    constexpr std::string_view CutsceneVariable = "cutscene hid player ui";
    constexpr std::string_view SkippingVariable = "cs is skipping";
    constexpr std::string_view SkippableVariables[] = { "skippable", "cs skippable" };

    // Which of the skippable flags belong to the cutscene that holds the player: the ones written around the moment it
    // took hold. A cutscene the game lets the player skip has one of them at 1; lines inside a cutscene write their own
    // later, and the previous cutscene's end writes its own just before.
    class SkippableFlags
    {
    public:
        static constexpr int Size = 16;
        static constexpr double Window = 0.5; // seconds around the hold's start

        void Written(uint32_t record, double now)
        {
            int slot = 0;
            for (int i = 0; i < Size; i++)
            {
                if (m_record[i] == record)
                {
                    slot = i;
                    break;
                }
                if (m_time[i] < m_time[slot])
                    slot = i;
            }
            m_record[slot] = record;
            m_time[slot] = now;
        }

        void HoldStarted(double now) { m_holdStart = now; }

        // The i-th flag of the holding cutscene (0 if that slot is not one).
        uint32_t Flag(int i) const
        {
            bool near = m_time[i] >= m_holdStart - Window && m_time[i] <= m_holdStart + Window;
            return near ? m_record[i] : 0;
        }

    private:
        uint32_t m_record[Size] = {};
        double m_time[Size] = {};
        double m_holdStart = -1e9;
    };

    class DialogueSkip
    {
    public:
        // A line must have spoken this long before a press closes it, so one press closes one line.
        static constexpr double MinSeconds = 0.5;
        // After a press, the time left to the game to start its own skip.
        static constexpr double GameSeconds = 0.15;
        // The longest a fast run lasts if no line comes (a cutscene that ends on a long camera move).
        static constexpr double MaxFastSeconds = 8.0;
        // How much faster the game's logic runs during a fast run.
        static constexpr float FastScale = 16.0f;

        // A sound not updated for this long has ended (the game updates every playing sound each frame).
        static constexpr double GoneSeconds = 0.25;

        // The game updated `sound`, a line, playing as `playback`. A sound or playback not seen just before is a new
        // line. True if that ends a fast run.
        bool Played(uint32_t sound, uint32_t playback, double now)
        {
            int slot = 0;
            for (int i = 0; i < SeenSize; i++)
            {
                if (m_seen[i].sound == sound)
                {
                    slot = i;
                    break;
                }
                if (m_seen[i].time < m_seen[slot].time)
                    slot = i;
            }
            Seen& seen = m_seen[slot];
            const bool started = seen.sound != sound || seen.playback != playback || now - seen.time > GoneSeconds;
            seen = { sound, playback, now };
            if (sound == m_line)
                m_lineSeen = now;
            if (!started)
                return false;
            m_line = sound;
            m_lineStart = m_lineSeen = now;
            m_pending = -1;
            const bool ended = m_fast;
            m_fast = false;
            return ended;
        }

        // A sound stopped (its line ended by itself, or the press stopped it).
        void Stopped(uint32_t sound)
        {
            if (sound == m_line)
                m_line = 0;
        }

        // A press of the skip button; `held` a cutscene holds the player, `skippable` the game's own skip may end it.
        void Press(double now, bool held, bool skippable)
        {
            if (held && !skippable && !m_fast && Speaking(now) && now - m_lineStart >= MinSeconds)
                m_pending = now;
        }

        // Once per check: the line to stop now (0: none), and whether the logic runs fast.
        struct Step
        {
            uint32_t stop;
            bool fast;
        };

        Step Update(double now, bool held, bool gameSkips)
        {
            Step step{ 0, m_fast };
            if (!held || gameSkips)
            {
                m_pending = -1;
                m_fast = step.fast = false;
                return step;
            }
            if (m_fast && now - m_fastStart > MaxFastSeconds)
                m_fast = step.fast = false;
            if (m_pending >= 0 && now - m_pending >= GameSeconds)
            {
                m_pending = -1;
                if (Speaking(now))
                {
                    step.stop = m_line;
                    m_line = 0;
                    m_fast = step.fast = true;
                    m_fastStart = now;
                }
            }
            return step;
        }

        bool Fast() const { return m_fast; }

    private:
        static constexpr int SeenSize = 32;
        struct Seen
        {
            uint32_t sound, playback;
            double time;
        };

        bool Speaking(double now) const { return m_line != 0 && now - m_lineSeen <= GoneSeconds; }

        Seen m_seen[SeenSize] = {};
        uint32_t m_line = 0;
        double m_lineStart = 0, m_lineSeen = 0, m_pending = -1, m_fastStart = 0;
        bool m_fast = false;
    };
}
