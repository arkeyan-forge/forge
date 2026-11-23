// A real portal and virtual seats as one portal (portal_backend.h, MixedPortal), and the fake real portal of the tests.
#include "host/portal_backend.h"
#include "env.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace
{
    // Second byte of a block answer: the block count in the high nibble, the slot in the low one; 0x01 is a failure.
    constexpr uint8_t Failure = 0x01;
    constexpr uint8_t OneBlock = 0x10;

    // The mixed portal's own block commands (the copies): asked again when unanswered, given up after a few tries.
    constexpr std::chrono::milliseconds Retry{ 250 };
    constexpr int Tries = 4;

    bool IsSectorTrailer(size_t block) { return block % 4 == 3; }

    uint32_t StatusOf(const uint8_t* report)
    {
        return uint32_t(report[1] | report[2] << 8 | report[3] << 16 | uint32_t(report[4]) << 24);
    }
}

MixedPortal::MixedPortal(
    std::unique_ptr<PortalBackend> real, EmulatedPortal& virtualPart, std::chrono::milliseconds holdTime)
    : m_real(std::move(real)), m_virtual(virtualPart), m_holdTime(holdTime)
{
    for (int i = 0; i < Slots; i++)
        m_gameSlot[i] = m_realSlot[i] = -1;
}

bool MixedPortal::IsRealSlot(int slot)
{
    std::lock_guard lock(m_mutex);
    return slot >= 0 && slot < Slots && m_realSlot[slot] >= 0;
}

int MixedPortal::RealSlotOf(int slot)
{
    std::lock_guard lock(m_mutex);
    return slot >= 0 && slot < Slots ? m_realSlot[slot] : -1;
}

bool MixedPortal::IsHeld(int slot)
{
    std::lock_guard lock(m_mutex);
    return slot >= 0 && slot < Slots && m_toys[slot].held;
}

bool MixedPortal::Write(const uint8_t* command, size_t size)
{
    if (size == 0)
        return true;
    if ((command[0] == 'Q' || command[0] == 'W') && size >= 2)
    {
        const int slot = command[1] & 0x0F;
        std::unique_lock lock(m_mutex);
        if (m_toys[slot].copy)
            return AnswerCopy(slot, command, size);
        if (const int real = m_realSlot[slot]; real >= 0)
        {
            // The real portal's toy: its own slot number.
            uint8_t translated[PortalReportSize] = {};
            memcpy(translated, command, std::min(size, sizeof(translated)));
            translated[1] = uint8_t((command[1] & 0xF0) | real);
            lock.unlock();
            return m_real->Write(translated, size);
        }
        if (m_toys[slot].held)
            return true; // its toy is not back yet: no answer, the title asks again (never a write to an unknown toy)
        Report answer{};
        answer[0] = command[0];
        answer[1] = Failure;
        answer[2] = size > 2 ? command[2] : 0;
        if (m_virtual.Holds(slot) && m_virtual.Write(command, size))
            m_virtual.Read(answer.data(), answer.size()); // inactive: its answers only
        m_answers.push_back(answer);
        return true;
    }
    if (command[0] == 'R')
    {
        std::lock_guard lock(m_mutex);
        HoldRealToys();
    }
    return m_real->Write(command, size);
}

bool MixedPortal::AnswerCopy(int slot, const uint8_t* command, size_t size)
{
    Toy& toy = m_toys[slot];
    Report answer{};
    answer[0] = command[0];
    answer[1] = Failure;
    const size_t block = size > 2 ? command[2] : FigureBlockCount;
    answer[2] = uint8_t(block);
    if (block < FigureBlockCount && command[0] == 'Q')
    {
        memcpy(&answer[3], toy.image.data() + block * FigureBlockSize, FigureBlockSize);
        answer[1] = uint8_t(OneBlock | slot);
    }
    else if (block < FigureBlockCount && block != 0 && !IsSectorTrailer(block) && size >= 3 + FigureBlockSize)
    {
        memcpy(toy.image.data() + block * FigureBlockSize, command + 3, FigureBlockSize);
        m_copies->Saved(toy.image);
        const bool queued = std::any_of(m_writes.begin(), m_writes.end(),
            [&](const ToyWrite& write) { return write.game == slot && write.block == int(block); });
        if (!queued)
            m_writes.push_back({ slot, int(block) });
        answer[1] = uint8_t(OneBlock | slot);
    }
    m_answers.push_back(answer);
    return true;
}

void MixedPortal::StartReading(int real)
{
    m_reading[real] = {};
    m_reading[real].state = Reading::Reading;
}

void MixedPortal::Placed(int real, Clock::time_point now)
{
    ReadState& reading = m_reading[real];
    const RealCopies::Plan plan = m_copies->Placed(reading.image);
    if (plan.kind == RealCopies::Plan::Kind::Wait)
    {
        reading.state = Reading::Waiting;
        reading.asked = now;
        return;
    }
    const int game = FreeGameSlot(real, 0);
    if (game < 0)
    {
        fprintf(stderr, "[portal] mixed: no free slot for the real portal's toy in slot %d\n", real);
        reading.state = Reading::Stuck;
        if (plan.kind == RealCopies::Plan::Kind::Play)
            m_copies->Lifted(plan.image);
        return;
    }
    Map(real, game);
    Toy& toy = m_toys[game];
    toy = {};
    toy.arrived = true;
    if (plan.kind == RealCopies::Plan::Kind::Play)
    {
        // Held by a reset like a toy the game has read: its serial is known.
        toy.known = toy.copy = true;
        memcpy(toy.serial, reading.image.data(), sizeof(toy.serial));
        toy.image = plan.image;
        for (int block = 0; block < int(FigureBlockCount); block++)
        {
            if ((plan.write >> block & 1) != 0)
                m_writes.push_back({ game, block });
        }
    }
    reading = {};
}

void MixedPortal::Pump(Clock::time_point now)
{
    if (m_copies == nullptr)
        return;
    for (int real = 0; real < Slots; real++)
    {
        if (m_reading[real].state == Reading::Waiting && now - m_reading[real].asked >= Retry)
            Placed(real, now);
    }
    using Kind = Operation::Kind;
    // A toy being read first: the game does not see it until then.
    for (int real = 0; real < Slots && m_operation.kind == Kind::None; real++)
    {
        if (m_reading[real].state == Reading::Reading)
        {
            m_operation = {};
            m_operation.kind = Kind::Read;
            m_operation.real = real;
            m_operation.block = m_reading[real].block;
        }
    }
    while (m_operation.kind == Kind::None && !m_writes.empty())
    {
        const ToyWrite write = m_writes.front();
        m_writes.pop_front();
        if (!m_toys[write.game].copy)
            continue;
        m_operation = {};
        m_operation.kind = Kind::Write;
        m_operation.game = write.game;
        m_operation.block = write.block;
        memcpy(m_operation.data, m_toys[write.game].image.data() + write.block * FigureBlockSize, FigureBlockSize);
    }
    if (m_operation.kind == Kind::None || (m_operation.sent && now - m_operation.asked < Retry))
        return;
    if (m_operation.tries >= Tries)
    {
        EndOperation(false);
        return;
    }
    const int real = m_operation.kind == Kind::Read ? m_operation.real : m_realSlot[m_operation.game];
    if (real < 0)
    {
        m_operation.sent = false; // held after a reset: sent once the toy is back
        return;
    }
    uint8_t command[3 + FigureBlockSize] = { m_operation.kind == Kind::Write ? uint8_t('W') : uint8_t('Q'),
        uint8_t(OneBlock | real), uint8_t(m_operation.block) };
    size_t size = 3;
    if (m_operation.kind == Kind::Write)
    {
        memcpy(command + 3, m_operation.data, FigureBlockSize);
        size = sizeof(command);
    }
    m_operation.sent = true;
    m_operation.asked = now;
    m_operation.tries++;
    m_real->Write(command, size);
}

void MixedPortal::EndOperation(bool done)
{
    using Kind = Operation::Kind;
    const Operation operation = m_operation;
    m_operation = {};
    if (operation.kind == Kind::Read)
    {
        // Not read whole (a worn tag?): the game reads it itself, as without copies.
        fprintf(stderr,
            "[portal] mixed: the real portal's toy in slot %d could not be read whole, played without a copy\n",
            operation.real);
        m_reading[operation.real] = {};
        if (const int game = FreeGameSlot(operation.real, 0); game >= 0)
        {
            Map(operation.real, game);
            m_toys[game] = {};
            m_toys[game].arrived = true;
        }
        else
            m_reading[operation.real].state = Reading::Stuck;
        return;
    }
    if (operation.kind == Kind::Write && done)
    {
        m_operation = operation;
        m_operation.kind = Kind::Check;
        m_operation.sent = false;
        m_operation.tries = 0;
        return;
    }
    if (operation.kind == Kind::None || !m_toys[operation.game].copy)
        return;
    FigureData written = m_toys[operation.game].image;
    memcpy(written.data() + operation.block * FigureBlockSize, operation.data, FigureBlockSize);
    if (!done)
        fprintf(stderr, "[portal] mixed: block %d of the game's slot %d did not reach the toy; kept in its copy\n",
            operation.block, operation.game);
    m_copies->ToyWrote(written, operation.block, done);
}

bool MixedPortal::TakeToyAnswer(const uint8_t* report, size_t size, Clock::time_point now)
{
    using Kind = Operation::Kind;
    if (m_copies == nullptr || size < 3)
        return false;
    const bool failed = report[1] == Failure;
    const int real = report[1] & 0x0F;
    Operation& operation = m_operation;
    const int operationReal = operation.kind == Kind::Read   ? operation.real
                              : operation.kind != Kind::None ? m_realSlot[operation.game]
                                                             : -1;
    const uint8_t letter = operation.kind == Kind::Write ? 'W' : 'Q';
    if (operation.kind != Kind::None && operation.sent && report[0] == letter && report[2] == operation.block &&
        (failed || real == operationReal))
    {
        const bool block = size >= 3 + FigureBlockSize;
        if (failed || (operation.kind != Kind::Write && !block))
        {
            operation.sent = false; // asked again at once, until the tries run out
            return true;
        }
        if (operation.kind == Kind::Read)
        {
            ReadState& reading = m_reading[operation.real];
            memcpy(reading.image.data() + operation.block * FigureBlockSize, report + 3, FigureBlockSize);
            m_operation = {};
            if (++reading.block == int(FigureBlockCount))
                Placed(real, now);
        }
        else if (operation.kind == Kind::Write)
            EndOperation(true);
        else
            EndOperation(memcmp(report + 3, operation.data, FigureBlockSize) == 0);
        return true;
    }
    // A late answer to a command sent again, or a toy the game does not talk to itself: never the game's.
    if (failed)
        return false;
    if (m_reading[real].state != Reading::None)
        return true;
    const int game = m_gameSlot[real];
    return game >= 0 && m_toys[game].copy;
}

void MixedPortal::Release(int game)
{
    using Kind = Operation::Kind;
    Toy& toy = m_toys[game];
    if (toy.copy)
    {
        if ((m_operation.kind == Kind::Write || m_operation.kind == Kind::Check) && m_operation.game == game)
        {
            // Sent, maybe written: the next placement compares that block again.
            if (m_operation.sent || m_operation.kind == Kind::Check)
                EndOperation(false);
            m_operation = {};
        }
        m_writes.erase(
            std::remove_if(m_writes.begin(), m_writes.end(), [&](const ToyWrite& write) { return write.game == game; }),
            m_writes.end());
        m_copies->Lifted(toy.image);
    }
    toy = {};
}

void MixedPortal::HoldRealToys()
{
    for (int game = 0; game < Slots; game++)
    {
        const int real = m_realSlot[game];
        if (real < 0)
            continue;
        m_gameSlot[real] = m_realSlot[game] = -1;
        // A toy the game never read is not known by its serial: it simply arrives again.
        m_toys[game].held = m_toys[game].known;
        if (!m_toys[game].known)
            m_toys[game] = {};
    }
    for (bool& identifying : m_identifying)
        identifying = false; // the real portal numbers its toys anew
    for (ReadState& reading : m_reading)
        reading = {};
    if (m_operation.kind == Operation::Kind::Read)
        m_operation = {};
    m_operation.sent = false; // a write is sent again once its toy is back
    m_holdUntil = Clock::now() + m_holdTime;
}

void MixedPortal::Map(int real, int game)
{
    m_gameSlot[real] = game;
    m_realSlot[game] = real;
    fprintf(stderr, "[portal] mixed: the real portal's slot %d is the game's slot %d\n", real, game);
}

int MixedPortal::FreeGameSlot(int wanted, uint32_t virtualBits)
{
    // Its own slot when the game's is free, else the first free one.
    auto free = [&](int g) {
        return m_realSlot[g] < 0 && !m_toys[g].held && (virtualBits >> (g * 2) & 3) == 0 && !m_virtual.Holds(g);
    };
    if (free(wanted))
        return wanted;
    for (int g = 0; g < Slots; g++)
    {
        if (free(g))
            return g;
    }
    return -1;
}

void MixedPortal::MergeStatus(uint8_t* report)
{
    const auto now = Clock::now();
    const uint32_t real = StatusOf(report);
    const uint32_t virtualBits = m_virtual.TakeStatus();
    const bool holding = std::any_of(std::begin(m_toys), std::end(m_toys), [](const Toy& toy) { return toy.held; });
    uint32_t merged = virtualBits;
    for (int slot = 0; slot < Slots; slot++)
    {
        uint32_t bits = real >> (slot * 2) & 3;
        if (const int game = m_gameSlot[slot]; game >= 0)
        {
            if (m_toys[game].arrived && (bits & 1) != 0)
                bits |= 2;
            m_toys[game].arrived = false;
            merged |= bits << (game * 2);
            if ((bits & 1) == 0)
            {
                // Gone, and said so in this report: the slot is free again.
                m_gameSlot[slot] = m_realSlot[game] = -1;
                Release(game);
            }
            continue;
        }
        if ((bits & 1) == 0)
        {
            m_identifying[slot] = false;
            if (m_operation.kind == Operation::Kind::Read && m_operation.real == slot)
                m_operation = {};
            m_reading[slot] = {};
            continue;
        }
        if (holding)
        {
            // A toy while the game's toys are held: who it is first (its block 0, asked again after 250 ms).
            if (!m_identifying[slot] || now - m_asked[slot] > std::chrono::milliseconds(250))
            {
                m_identifying[slot] = true;
                m_asked[slot] = now;
                const uint8_t query[] = { 'Q', uint8_t(0x10 | slot), 0 };
                m_real->Write(query, sizeof(query));
            }
            continue;
        }
        if (m_copies != nullptr)
        {
            // Read whole first; the game is told it arrived after.
            if (m_reading[slot].state == Reading::None)
                StartReading(slot);
            continue;
        }
        const int game = FreeGameSlot(slot, virtualBits);
        if (game < 0)
        {
            fprintf(stderr, "[portal] mixed: no free slot for the real portal's toy in slot %d\n", slot);
            continue;
        }
        Map(slot, game);
        merged |= bits << (game * 2);
    }
    for (int game = 0; game < Slots; game++)
    {
        if (!m_toys[game].held)
            continue;
        if (now < m_holdUntil)
        {
            merged |= 1u << (game * 2); // still there for the game, unchanged
            continue;
        }
        fprintf(stderr, "[portal] mixed: the game's slot %d: its toy did not come back after the reset\n", game);
        merged |= 2u << (game * 2);
        Release(game);
    }
    uint32_t reserved = 0;
    for (int g = 0; g < Slots; g++)
    {
        if (m_realSlot[g] >= 0 || m_toys[g].held)
            reserved |= 1u << g;
    }
    m_virtual.SetReserved(reserved);
    report[1] = uint8_t(merged);
    report[2] = uint8_t(merged >> 8);
    report[3] = uint8_t(merged >> 16);
    report[4] = uint8_t(merged >> 24);
}

bool MixedPortal::Identify(const uint8_t* report, size_t size)
{
    if (size < 3 + 4 || report[0] != 'Q' || (report[1] & 0xF0) == 0 || report[2] != 0)
        return false;
    const int real = report[1] & 0x0F;
    if (!m_identifying[real] || m_gameSlot[real] >= 0)
        return false;
    m_identifying[real] = false;
    const uint8_t* serial = report + 3;
    for (int game = 0; game < Slots; game++)
    {
        Toy& toy = m_toys[game];
        if (toy.held && memcmp(toy.serial, serial, sizeof(toy.serial)) == 0)
        {
            toy.held = false;
            m_gameSlot[real] = game;
            m_realSlot[game] = real;
            fprintf(stderr, "[portal] mixed: the real portal's slot %d is the game's slot %d again (the same toy)\n",
                real, game);
            return true;
        }
    }
    if (m_copies != nullptr)
    {
        StartReading(real);
        return true;
    }
    const int game = FreeGameSlot(real, 0);
    if (game < 0)
        return true;
    Map(real, game);
    m_toys[game].arrived = true;
    return true;
}

size_t MixedPortal::Read(uint8_t* report, size_t capacity)
{
    // The answers to the mixed portal's own commands are taken here and the next report read in their place.
    for (int taken = 0; taken < 16; taken++)
    {
        {
            std::lock_guard lock(m_mutex);
            Pump(Clock::now());
            if (!m_answers.empty())
            {
                const size_t size = std::min(capacity, m_answers.front().size());
                memcpy(report, m_answers.front().data(), size);
                m_answers.pop_front();
                return size;
            }
        }
        const size_t size = m_real->Read(report, capacity);
        if (size == 0)
            return 0;
        std::lock_guard lock(m_mutex);
        if (report[0] == 'S' && size >= 5)
            MergeStatus(report);
        else if ((report[0] == 'Q' || report[0] == 'W') && size >= 2)
        {
            if ((report[1] & 0xF0) != 0 && Identify(report, size))
                return 0; // asked here, not by the game
            if (TakeToyAnswer(report, size, Clock::now()))
                continue;
            // A block of a real toy: the game's slot number. Block 0's serial names the toy after a reset.
            const int game = (report[1] & 0xF0) != 0 ? m_gameSlot[report[1] & 0x0F] : -1;
            if (game >= 0)
            {
                report[1] = uint8_t((report[1] & 0xF0) | game);
                if (report[0] == 'Q' && size >= 7 && report[2] == 0)
                {
                    m_toys[game].known = true;
                    memcpy(m_toys[game].serial, report + 3, sizeof(m_toys[game].serial));
                }
            }
        }
        return size;
    }
    return 0;
}

namespace
{
    // An emulated portal dressed as hardware: what the mixed portal is tested against. "<dump>@<on>-<off>" puts that toy
    // on that many seconds after the game first reads the portal and lifts it at <off> (a player placing and lifting real
    // figures in play; both are optional). Like the portals in the players' logs (issue #20), it sends status reports
    // from the start, before the first reset, and numbers its toys anew from slot 0 on each reset, each one arriving again. SKY_PORTAL_FAKE_REAL_ID ("02 0A 05 08")
    // is the id it answers, else the game's own Xbox 360 portal's.
    class FakeRealPortal final : public PortalBackend
    {
    public:
        explicit FakeRealPortal(const std::string& dumps)
        {
            m_model = EmulatedPortalModel(ARKEYAN_GAME_ID);
            if (const char* id = skyenv::Get("SKY_PORTAL_FAKE_REAL_ID"))
            {
                unsigned bytes[4] = {};
                const int count = sscanf(id, "%x %x %x %x", &bytes[0], &bytes[1], &bytes[2], &bytes[3]);
                if (count == 2 || count == 4)
                {
                    for (int i = 0; i < count; i++)
                        m_model.id[i] = uint8_t(bytes[i]);
                    m_model.idLength = size_t(count);
                    const PortalModel* known = FindPortalModel(m_model.id, m_model.idLength);
                    m_model.name = known != nullptr ? known->name : "unknown portal";
                }
            }
            m_portal.SetModel(m_model);
            m_portal.SetStatusInterval(Interval);
            for (size_t begin = 0; begin <= dumps.size();)
            {
                const size_t end = std::min(dumps.find(',', begin), dumps.size());
                std::string path = dumps.substr(begin, end - begin);
                Later toy{};
                if (const size_t at = path.rfind('@'); at != std::string::npos)
                {
                    const std::string times = path.substr(at + 1);
                    toy.on = atof(times.c_str());
                    if (const size_t dash = times.find('-'); dash != std::string::npos)
                        toy.off = atof(times.c_str() + dash + 1);
                    path.resize(at);
                }
                toy.path = path;
                if (!path.empty() && LoadFigure(path, toy.data))
                    m_later.push_back(toy);
                else if (!path.empty())
                    fprintf(stderr, "[portal] fake real portal: cannot read %s\n", path.c_str());
                begin = end + 1;
            }
        }
        const char* Name() const override { return "fake-real"; }
        bool IsConnected() override { return true; }
        bool Write(const uint8_t* command, size_t size) override
        {
            if (size > 0 && command[0] == 'R')
            {
                m_portal.Renumber();
                m_reset = true;
            }
            return m_portal.Write(command, size);
        }
        size_t Read(uint8_t* report, size_t capacity) override
        {
            const auto now = std::chrono::steady_clock::now();
            if (m_start == std::chrono::steady_clock::time_point{})
                m_start = now;
            const double seconds = std::chrono::duration<double>(now - m_start).count();
            for (Later& toy : m_later)
            {
                if (toy.slot == Waiting && seconds >= toy.on)
                {
                    toy.slot = m_portal.Place(toy.data, {});
                    fprintf(stderr, "[portal] fake real portal: %s put on, slot %d\n", toy.path.c_str(), toy.slot);
                }
                if (toy.slot >= 0 && toy.off > 0 && seconds >= toy.off)
                {
                    // Its slot now, after the resets that renumbered it.
                    const int slot = m_portal.SlotOfSerial(toy.data.data());
                    m_portal.Remove(slot);
                    fprintf(stderr, "[portal] fake real portal: %s lifted, slot %d\n", toy.path.c_str(), slot);
                    toy.slot = Lifted;
                }
            }
            if (const size_t size = m_portal.Read(report, capacity); size > 0 || m_portal.IsActive() || m_reset)
                return size;
            // Just plugged in, never reset: status reports all the same, as the hardware sends.
            if (now - m_lastInactive < Interval)
                return 0;
            m_lastInactive = now;
            const uint8_t status = 'S';
            m_portal.Write(&status, 1);
            return m_portal.Read(report, capacity);
        }

    private:
        static constexpr std::chrono::milliseconds Interval{ 20 }; // a real portal's pace
        static constexpr int Waiting = -1, Lifted = -2;
        struct Later
        {
            double on = 0, off = 0;
            std::string path;
            FigureData data{};
            int slot = Waiting;
        };
        PortalModel m_model;
        EmulatedPortal m_portal;
        std::vector<Later> m_later;
        std::chrono::steady_clock::time_point m_start{}, m_lastInactive{};
        bool m_reset = false;
    };
}

std::unique_ptr<PortalBackend> CreateFakeRealPortal(const std::string& dumps)
{
    return std::make_unique<FakeRealPortal>(dumps);
}
