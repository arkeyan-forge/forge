// A HID portal command as a USB control SET_REPORT (macOS and Linux, host/portal_hid.cpp), the ways a command can be
// sent and the order they are tried in, and the device behind SDL's macOS path. No IOKit or libusb here, so it is tested
// on every host (portal_hid_report_test.cpp).
#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <utility>

namespace portal_hid
{
    constexpr size_t CommandSize = 32;

    // HID class SET_REPORT: host to device, class, to the interface (0x21), request 0x09, value = report type
    // (2, output) << 8 | report id (0), index = the interface. The data is the command without the report id byte.
    struct SetReport
    {
        uint8_t requestType = 0x21;
        uint8_t request = 0x09;
        uint16_t value = 0x0200;
        uint16_t index = 0;
        uint8_t data[CommandSize]{};
    };

    // How a command reaches the portal (the comment above WriteMethod's use in host/portal_hid.cpp says why).
    enum class WriteMethod
    {
        OutputReport, // HidD_SetOutputReport (Windows)
        ControlReport, // SET_REPORT on the default pipe: IOUSBDeviceInterface (macOS), libusb (Linux)
        HidWrite, // SDL_hid_write
    };

    inline const char* MethodName(WriteMethod method)
    {
        switch (method)
        {
        case WriteMethod::OutputReport: return "output-report";
        case WriteMethod::ControlReport: return "usb-control";
        default: return "hid-write";
        }
    }

    // Each system's methods, in the order a new portal tries them. Linux: hidapi's write first (the Wii Swap Force
    // portal takes it), then the control request, which detaches the kernel's HID driver from the portal's interface.
    constexpr WriteMethod WindowsMethods[] = { WriteMethod::OutputReport, WriteMethod::HidWrite };
    constexpr WriteMethod MacMethods[] = { WriteMethod::ControlReport, WriteMethod::HidWrite };
    constexpr WriteMethod LinuxMethods[] = { WriteMethod::HidWrite, WriteMethod::ControlReport };

    // The order of one write: the method that worked last, then the others in the system's order.
    template <size_t N>
    inline std::array<WriteMethod, N> TryOrder(const WriteMethod (&methods)[N], WriteMethod last)
    {
        std::array<WriteMethod, N> order{};
        std::copy(std::begin(methods), std::end(methods), order.begin());
        if (auto found = std::find(order.begin(), order.end(), last); found != order.end())
            std::rotate(order.begin(), found, found + 1);
        return order;
    }

    // The methods one command tries, in order. A portal just opened tries the system's methods in their order. Once a
    // method has worked on it, a failure retries that method and never moves to another: on Windows a Wii Traptanium
    // portal's SET_REPORT failed once as its speaker started, the fallback (hidapi's write, the interrupt OUT endpoint
    // that carries the speaker's sound) went through, and every later command went there unanswered: the portal froze.
    // When the retries fail too, the portal is closed and its reopen starts over from the first method.
    // `speakerAvoidsHidWrite`: a portal with a speaker never takes a command by hidapi's write (Windows).
    template <size_t N>
    class MethodChoice
    {
    public:
        static constexpr size_t Retries = 2;
        static constexpr size_t MaxTries = N + Retries;

        struct Order
        {
            std::array<WriteMethod, MaxTries> methods{};
            size_t count = 0;
            const WriteMethod* begin() const { return methods.data(); }
            const WriteMethod* end() const { return methods.data() + count; }
        };

        constexpr MethodChoice(const WriteMethod (&methods)[N], bool speakerAvoidsHidWrite)
            : m_methods(&methods), m_last(methods[0]), m_speakerAvoidsHidWrite(speakerAvoidsHidWrite)
        {}

        // The portal (re)opened: nothing proven. Its speaker is kept: a reopen is most likely the same portal, and a
        // new one says its model in its first 'R' answer.
        void Opened()
        {
            m_last = (*m_methods)[0];
            m_proven = false;
        }

        void Worked(WriteMethod method)
        {
            m_last = method;
            m_proven = true;
        }

        void SetSpeaker(bool speaker) { m_speaker = speaker; }
        bool Speaker() const { return m_speaker; }

        Order Next() const
        {
            Order order;
            if (m_proven && Allowed(m_last))
            {
                for (size_t i = 0; i <= Retries; i++)
                    order.methods[order.count++] = m_last;
                return order;
            }
            for (WriteMethod method : TryOrder(*m_methods, m_last))
                if (Allowed(method))
                    order.methods[order.count++] = method;
            return order;
        }

    private:
        bool Allowed(WriteMethod method) const
        {
            return !(m_speaker && m_speakerAvoidsHidWrite && method == WriteMethod::HidWrite);
        }

        const WriteMethod (*m_methods)[N];
        WriteMethod m_last;
        bool m_proven = false, m_speaker = false;
        bool m_speakerAvoidsHidWrite;
    };

    // 16-bit samples in one write of the speaker's sound. Windows' HID driver sends reports of the descriptor's output
    // length (the report id byte, then the data): the Wii Traptanium portal's 33 bytes carry 16 samples, and a longer
    // write is not split into reports. Elsewhere (`outputReportLength` 0) hidapi's write goes to the OUT endpoint as it
    // is: 64 bytes, 32 samples.
    constexpr size_t MaxSoundSamples = 32;
    constexpr size_t SoundSamplesPerWrite(size_t outputReportLength)
    {
        return outputReportLength < 3 ? MaxSoundSamples : std::min((outputReportLength - 1) / 2, MaxSoundSamples);
    }

    // The speaker's writes on one open portal: how many samples each carries, and when the sound goes to the computer.
    // A block the portal refuses or does not take in time is dropped and the next one tried: on Windows a Wii
    // Traptanium portal left one 33-byte write pending for hidapi's whole second and, refused for good, the trap's
    // voice played on the computer for the rest of the session (issue #60). A run of failures in the first framing
    // tries the other (one 65-byte write of 32 samples, as before 0.9.9) unless the first has worked; a run in the
    // last leaves the sound to the computer until the portal is reopened.
    class SpeakerWrites
    {
    public:
        static constexpr size_t FailuresInARow = 10;
        enum class Next
        {
            Retry, // drop this block, send the next
            NewFraming, // Samples() changed
            Computer, // Refused()
        };

        // `outputReportLength` as in SoundSamplesPerWrite.
        void Opened(size_t outputReportLength)
        {
            m_framings[0] = SoundSamplesPerWrite(outputReportLength);
            m_count = m_framings[0] == MaxSoundSamples ? 1 : 2;
            m_framing = m_failures = 0;
            m_proven = m_refused = false;
        }
        size_t Samples() const { return m_framings[m_framing]; }
        bool Refused() const { return m_refused; }
        // The speaker turned on (`M 01`).
        void Started() { m_failures = 0; }
        // True for the first write of this framing that went through.
        bool Worked()
        {
            m_failures = 0;
            return !std::exchange(m_proven, true);
        }
        Next Failed()
        {
            if (++m_failures < FailuresInARow)
                return Next::Retry;
            m_failures = 0;
            if (!m_proven && m_framing + 1 < m_count)
            {
                m_framing++;
                return Next::NewFraming;
            }
            m_refused = true;
            return Next::Computer;
        }

    private:
        size_t m_framings[2] = { MaxSoundSamples, MaxSoundSamples };
        size_t m_count = 1, m_framing = 0, m_failures = 0;
        bool m_proven = false, m_refused = false;
    };

    // `command` starts at the letter; zero-padded to 32 bytes, cut beyond.
    inline SetReport MakeSetReport(const uint8_t* command, size_t size, int interface)
    {
        SetReport report;
        report.index = uint16_t(std::max(interface, 0));
        memcpy(report.data, command, std::min(size, CommandSize));
        return report;
    }

    // SDL's hidapi names a macOS device "DevSrvsID:<registry entry id>"; 0 when `path` is not such a name.
    inline uint64_t MacRegistryEntryId(const char* path)
    {
        constexpr char Prefix[] = "DevSrvsID:";
        if (path == nullptr || strncmp(path, Prefix, sizeof(Prefix) - 1) != 0)
            return 0;
        const char* digits = path + sizeof(Prefix) - 1;
        if (*digits < '0' || *digits > '9')
            return 0;
        char* end = nullptr;
        unsigned long long id = strtoull(digits, &end, 10);
        return *end == '\0' ? uint64_t(id) : 0;
    }
}
