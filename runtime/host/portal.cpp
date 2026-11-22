#include "host/portal.h"
#include "host/portal_async.h"
#include "host/portal_backend.h"
#include "host/portal_identity.h"
#include "host/portal_speaker.h"
#include "host/portal_stall.h"
#include "host/collection/collection.h"
#include "host/collection/datadir.h"
#include "host/figures/figure_tag.h"
#include "kernel/objects.h"
#include "netplay/clock.h"
#include "netplay/session_runtime.h"
#include <filesystem>
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>
#include <vector>
#include "env.h"

namespace
{
    std::mutex g_mutex;
    PortalBackend* g_backend;   // chosen once by PortalInit, kept until exit
    EmulatedPortal* g_emulated; // g_backend when the portal is emulated; the virtual seats of a mixed portal
    MixedPortal* g_mixed;       // g_backend when a real portal is plugged in: its toys and the virtual seats
    // A real portal is there (alone, or mixed with the virtual seats).
    bool Real() { return g_emulated == nullptr || g_mixed != nullptr; }
    const bool g_log = skyenv::Get("SKY_PORTAL_LOG") != nullptr;

    constexpr const char* GameId = ARKEYAN_GAME_ID;

    struct OnlineFigure
    {
        bool host = false;
        uint64_t id = 0;
        std::string path, source;
        bool announced = false;
    };
    std::mutex g_onlineMutex;
    std::vector<OnlineFigure> g_onlineFigures;
    std::array<std::string, PortalSlotCount> g_onlinePlanned;
    std::array<uint8_t, PortalSlotCount> g_onlinePlayers{};
    std::array<std::array<int, 2>, PortalSlotCount> g_onlineSlots{};
    uint64_t g_nextFigure = 0;

    OnlineFigure* OnlineFind(bool host, uint64_t id)
    {
        for (auto& figure : g_onlineFigures)
            if (figure.host == host && figure.id == id)
                return &figure;
        return nullptr;
    }

    std::string OnlinePath(bool host, uint64_t id)
    {
        return datadir::Text(std::filesystem::path(PortalSessionFolder()) / "online-figures" /
                             (std::string(host ? "1-" : "2-") + std::to_string(id) + ".sky"));
    }

    bool InCollection(const std::string& path);

    bool QueueOnlineLocked(const std::vector<PortalFigureChange>& changes)
    {
        if (changes.empty() || changes.size() > PortalSlotCount)
            return false;
        std::array<netplay::PortalEvent, PortalSlotCount> events{};
        const size_t previousSize = g_onlineFigures.size();
        std::vector<std::string> acquired;
        const auto refuse = [&] {
            for (const auto& path : acquired)
                collection::Unlock(path);
            for (size_t index = previousSize; index < g_onlineFigures.size(); ++index)
            {
                std::error_code error;
                std::filesystem::remove(g_onlineFigures[index].path, error);
            }
            g_onlineFigures.resize(previousSize);
            return false;
        };
        uint32_t used = 0;
        size_t count = 0;
        for (const auto& change : changes)
        {
            if (change.slot < 0 || change.slot >= PortalSlotCount || (used >> change.slot & 1) ||
                !netplay::SeatAllowed(netplay::Hosting(), g_onlinePlayers[change.slot]))
                return refuse();
            used |= uint32_t(1) << change.slot;
            auto& event = events[count++];
            event.slot = uint8_t(change.slot);
            if (change.path.empty())
                continue;
            OnlineFigure* known = nullptr;
            for (auto& figure : g_onlineFigures)
                if (figure.host == netplay::Hosting() && figure.source == change.path)
                    known = &figure;
            if (known)
            {
                event.figure = known->id;
                if (!known->announced)
                {
                    event.hasDump = true;
                    if (!LoadFigure(known->path, event.dump))
                        return refuse();
                }
            }
            else
            {
                FigureData data;
                if (g_onlineFigures.size() >= 4096 || !LoadFigure(change.path, data))
                    return refuse();
                figures::MendVirtualToy(data);
                if (InCollection(change.path))
                {
                    const bool mine = collection::GetLock(change.path).mine;
                    if (!collection::Lock(change.path, GameId))
                        return refuse();
                    if (!mine)
                        acquired.push_back(change.path);
                }
                event.figure = ++g_nextFigure;
                event.hasDump = true;
                event.dump = data;
                const auto path = OnlinePath(netplay::Hosting(), event.figure);
                std::error_code error;
                std::filesystem::create_directories(std::filesystem::path(path).parent_path(), error);
                if (error || !SaveFigure(path, data))
                    return refuse();
                g_onlineFigures.push_back({ netplay::Hosting(), event.figure, path, change.path });
            }
        }
        for (size_t index = 0; index < count; ++index)
            for (size_t before = 0; before < index; ++before)
                if (events[index].figure && events[index].figure == events[before].figure)
                    events[index].hasDump = false;
        events[count - 1].last = true;
        if (!netplay::QueuePortalEvents(std::span(events).first(count)))
            return refuse();
        for (size_t index = 0; index < count; ++index)
        {
            const auto& change = changes[index];
            g_onlinePlanned[change.slot] = change.path;
            if (auto* figure = OnlineFind(netplay::Hosting(), events[index].figure))
                figure->announced = true;
        }
        return true;
    }

    bool QueueOnline(const std::vector<PortalFigureChange>& changes)
    {
        std::lock_guard lock(g_onlineMutex);
        return QueueOnlineLocked(changes);
    }

    // The traffic as the title sees it (PortalSeen). Under g_mutex.
    PortalSlotSeen g_seen[PortalSlotCount];

    PortalLightState g_light; // under g_mutex
    uint64_t g_resets = 0;    // the title's `R` commands, under g_mutex

    // A real portal's id, from its answer to 'R' (the speaker's route). Under g_mutex.
    uint8_t g_realId[4];
    size_t g_realIdLength;

    // The "Portal sound" switch.
    std::atomic<bool> g_sound{ true };
    // Answers the runtime gives in place of a real portal (Read returns them first). Under g_mutex.
    std::deque<std::array<uint8_t, 32>> g_answers;
    // Where the speaker's blocks went last (logged when it changes). Under g_mutex.
    SpeakerRoute g_speakerRoute = SpeakerRoute::Off;
    bool g_speakerRouted = false;

    // A real portal's answer to 'R': its model, once per distinct id.
    void ObserveReset(const uint8_t* report, size_t size)
    {
        uint8_t id[4];
        size_t length;
        if (!PortalIdFromReset(report, size, id, length) ||
            (length == g_realIdLength && memcmp(g_realId, id, length) == 0))
            return;
        memcpy(g_realId, id, length);
        g_realIdLength = length;
        fprintf(stderr, "[portal] model: %s\n", DescribePortal(GameId, id, length).c_str());
        std::string reason;
        PortalVerdict verdict = PortalVerdictFor(GameId, id, length, &reason);
        if (verdict == PortalVerdict::Rejected || verdict == PortalVerdict::Warns)
            fprintf(stderr, "[portal] warning: this portal may not work with this game: %s\n", reason.c_str());
    }

    void ObserveReport(const uint8_t* report, size_t size)
    {
        uint64_t now = SDL_GetTicksNS();
        if (Real() && report[0] == 'R')
            ObserveReset(report, size);
        if (size >= 5 && report[0] == 'S')
        {
            uint32_t status = uint32_t(report[1] | report[2] << 8 | report[3] << 16 | uint32_t(report[4]) << 24);
            for (int slot = 0; slot < PortalSlotCount; slot++)
            {
                uint32_t bits = status >> (slot * 2) & 3;
                bool present = (bits & 1) != 0;
                // Arrived, or changed while present (a toy lifted and another placed between two reports).
                if (present && (!g_seen[slot].present || (bits & 2) != 0))
                    g_seen[slot] = { true, now };
                else if (!present && g_seen[slot].present)
                {
                    // A trap leaving the portal ends its voice at once (traps are toy types 210 to 220).
                    const FigureData& data = g_seen[slot].data;
                    unsigned toyType = data[0x10] | data[0x11] << 8;
                    if ((g_seen[slot].blocks >> 1 & 1) != 0 && toyType >= 210 && toyType <= 220)
                        HostSpeakerStream().Flush();
                    g_seen[slot] = {};
                }
            }
        }
        else if (size >= 3 + FigureBlockSize && report[0] == 'Q' && (report[1] & 0xF0) == 0x10)
        {
            int slot = report[1] & 0x0F;
            size_t block = report[2];
            if (block < FigureBlockCount)
            {
                memcpy(g_seen[slot].data.data() + block * FigureBlockSize, report + 3, FigureBlockSize);
                g_seen[slot].blocks |= uint64_t(1) << block;
                g_seen[slot].lastRead = now;
                if (block == 0)
                    g_seen[slot].headRead = now;
            }
        }
    }

    // Blocks the title writes keep the copy current; `C r g b` is the portal's light; `R` a reset (PortalResets).
    void ObserveCommand(const uint8_t* command, size_t size)
    {
        if (size >= 1 && command[0] == 'R')
            g_resets++;
        else if (size >= 1 && command[0] == 'C')
        {
            // Missing bytes are zero: the title sends the whole 32-byte report, the log trims trailing zeros.
            uint8_t colour[3] = {};
            memcpy(colour, command + 1, std::min<size_t>(size - 1, 3));
            bool wasOn = PortalLightOn(g_light);
            g_light.set = true;
            g_light.r = colour[0];
            g_light.g = colour[1];
            g_light.b = colour[2];
            g_light.changed = SDL_GetTicksNS();
            if (g_light.commands++ == 0 || wasOn != PortalLightOn(g_light))
                fprintf(stderr, "[portal] light: #%02X%02X%02X (%s)\n", colour[0], colour[1], colour[2],
                    PortalLightOn(g_light) ? "on" : "off");
        }
        else if (size >= 2 && command[0] == 'M')
        {
            // The speaker on (a new line: the decoder starts over) or off (what is queued plays out).
            if (command[1] != 0)
                HostSpeakerStream().Start();
            else
                HostSpeakerStream().Stop();
        }
        else if (size >= 3 + FigureBlockSize && command[0] == 'W')
        {
            int slot = command[1] & 0x0F;
            size_t block = command[2];
            if (block < FigureBlockCount && (g_seen[slot].blocks >> block & 1) != 0)
                memcpy(g_seen[slot].data.data() + block * FigureBlockSize, command + 3, FigureBlockSize);
        }
    }

    // Figures placed with the F7 hotkey: index in the folder listing and the slot it went to.
    int g_hotkeyFigure = -1;
    int g_hotkeySlot = -1;

    // The collection shared with the launcher and the other games (docs/runtime/host/collection.md): SKY_COLLECTION, SKY_FIGURES,
    // else <user data>/Skylanders/Collection.
    std::string FigureFolder() { return collection::Folder(); }

    // Figures of the collection are locked while on the portal, so that the launcher leaves them alone.
    bool InCollection(const std::string& path) { return path.rfind(collection::Folder() + "/", 0) == 0; }

    std::string FigurePath(const std::string& name)
    {
        bool absolute = !name.empty() && (name[0] == '/' || name[0] == '\\' || (name.size() > 1 && name[1] == ':'));
        return absolute ? name : FigureFolder() + "/" + name;
    }

    // Figure dumps in the folder, sorted by name.
    std::vector<std::string> ListFigures()
    {
        std::vector<std::string> figures;
        std::string folder = FigureFolder();
        int count = 0;
        if (char** names = SDL_GlobDirectory(folder.c_str(), "*", SDL_GLOB_CASEINSENSITIVE, &count))
        {
            for (int i = 0; i < count; i++)
            {
                std::string name = names[i];
                std::string extension = name.size() > 4 ? name.substr(name.size() - 4) : "";
                std::transform(
                    extension.begin(), extension.end(), extension.begin(), [](char c) { return char(tolower(c)); });
                if (extension == ".sky" || extension == ".bin" || extension == ".dmp")
                {
                    std::string path = folder;
                    path += '/';
                    path += name;
                    figures.push_back(std::move(path));
                }
            }
            SDL_free(reinterpret_cast<void*>(names));
        }
        std::sort(figures.begin(), figures.end());
        return figures;
    }

    // The emulated portal's figures from one session to the next: "<slot> <path>" lines in portal.txt in the save
    // folder (SKY_SAVE_DIR, else the user's data folder: LocalState on the Xbox), rewritten at every change.
    bool g_restoring;

    std::string StatePath()
    {
        std::string folder = PortalSessionFolder();
        return folder.empty() ? std::string("portal.txt") : folder + "/portal.txt";
    }

    void SaveState()
    {
        if (netplay::Online() || g_emulated == nullptr || g_restoring ||
            g_mixed != nullptr) // mixed: the virtual seats last one session
            return;
        std::string text;
        for (int slot = 0; slot < EmulatedPortal::SlotCount; slot++)
        {
            std::string path = g_emulated->SlotPath(slot);
            if (!path.empty())
                text += std::to_string(slot) + " " + path + "\n";
        }
        std::string state = StatePath();
        if (!SDL_SaveFile(state.c_str(), text.data(), text.size()))
            fprintf(stderr, "[portal] cannot write %s\n", state.c_str());
    }

    int Place(const std::string& path, int slot);

    void RestoreState()
    {
        std::string state = StatePath();
        size_t size = 0;
        char* text = static_cast<char*>(SDL_LoadFile(state.c_str(), &size));
        if (text == nullptr)
        {
            fprintf(stderr, "[portal] empty at start (no %s yet)\n", state.c_str());
            return;
        }
        std::string lines(text, size);
        SDL_free(text);
        int restored = 0;
        g_restoring = true;
        for (size_t begin = 0; begin < lines.size();)
        {
            size_t end = std::min(lines.find('\n', begin), lines.size());
            std::string line = lines.substr(begin, end - begin);
            size_t space = line.find(' ');
            // Paths of an older build's figure folder lead to their copies in the collection.
            if (space != std::string::npos &&
                Place(collection::MigratedPath(line.substr(space + 1)), atoi(line.c_str())) >= 0)
                restored++;
            begin = end + 1;
        }
        g_restoring = false;
        fprintf(stderr, "[portal] %d figure(s) back on the portal from %s\n", restored, state.c_str());
    }

    // "new:<toy type>[:<variant>]" creates a never-played figure of that toy in the folder (once) and places it; the
    // variant is hexadecimal ("new:100:1206" is a LightCore Jet-Vac).
    void PlaceStartupFigure(const std::string& entry)
    {
        std::string path = FigurePath(entry);
        if (entry.rfind("new:", 0) == 0)
        {
            int toyType = atoi(entry.c_str() + 4);
            size_t colon = entry.find(':', 4);
            unsigned variant =
                colon == std::string::npos ? 0 : unsigned(strtoul(entry.c_str() + colon + 1, nullptr, 16));
            char suffix[16] = "";
            if (variant != 0)
                snprintf(suffix, sizeof(suffix), "_%X", variant);
            path = FigurePath("new_" + std::to_string(toyType) + suffix + ".sky");
            SDL_PathInfo info;
            if (!SDL_GetPathInfo(path.c_str(), &info))
            {
                FigureData data = CreateBlankFigure(uint16_t(toyType), uint16_t(variant));
                SDL_CreateDirectory(FigureFolder().c_str());
                if (!SDL_SaveFile(path.c_str(), data.data(), data.size()))
                    fprintf(stderr, "[portal] cannot create %s\n", path.c_str());
            }
        }
        PortalPlaceFigure(path);
    }
}

void PortalPlaceNextFigure()
{
    std::vector<std::string> figures = ListFigures();
    if (figures.empty())
    {
        fprintf(stderr, "[portal] no figure dumps (.sky/.bin/.dmp) in %s\n", FigureFolder().c_str());
        return;
    }

    g_hotkeyFigure = (g_hotkeyFigure + 1) % int(figures.size());
    if (g_hotkeySlot < 0)
        g_hotkeySlot = PortalPlaceFigure(figures[g_hotkeyFigure]);
    else
        PortalReplaceFigures({ { g_hotkeySlot, figures[g_hotkeyFigure] } });
}

namespace
{
    // F7 swaps the hotkey figure for the next dump of the folder, F8 takes every figure off the portal (PortalSetHotkeys).
    std::atomic<int> g_addKey{ SDL_SCANCODE_F7 }, g_clearKey{ SDL_SCANCODE_F8 };

    bool SDLCALL OnHostEvent(void*, SDL_Event* event)
    {
        if (event->type != SDL_EVENT_KEY_DOWN || event->key.repeat)
            return true;

        if (int(event->key.scancode) == g_addKey)
        {
            PortalPlaceNextFigure();
        }
        else if (int(event->key.scancode) == g_clearKey)
        {
            PortalRemoveAllFigures();
        }
        return true;
    }

    int Place(const std::string& path, int wanted)
    {
        if (netplay::Online())
        {
            std::lock_guard lock(g_onlineMutex);
            int slot = wanted;
            if (slot < 0 || slot >= PortalSlotCount || !g_onlinePlanned[slot].empty())
            {
                slot = -1;
                for (int i = 0; i < PortalSlotCount && slot < 0; ++i)
                    if (netplay::SeatAllowed(netplay::Hosting(), g_onlinePlayers[i]) && g_onlinePlanned[i].empty())
                        slot = i;
            }
            return slot >= 0 && QueueOnlineLocked({ { slot, path } }) ? slot : -1;
        }
        if (g_emulated == nullptr)
            return -1;

        FigureData data;
        if (!LoadFigure(path, data))
        {
            fprintf(stderr, "[portal] %s is not a figure dump (1 KB tag image)\n", path.c_str());
            return -1;
        }
        // A toy made by an older build: the game sees it mended, and the file is when the game first writes it.
        if (figures::MendVirtualToy(data))
            fprintf(stderr, "[portal] %s: the 2016 sector keys of a virtual toy set\n", path.c_str());

        bool locked = InCollection(path);
        if (locked && !collection::Lock(path, GameId))
        {
            fprintf(stderr, "[portal] %s is in use by %s\n", path.c_str(), collection::GetLock(path).game.c_str());
            return -1;
        }
        int slot = g_emulated->Place(data, path, wanted);
        uint16_t toyType = uint16_t(data[0x10] | data[0x11] << 8);
        fprintf(stderr, "[portal] placed %s (toy type %u) in slot %d\n", path.c_str(), toyType, slot);
        if (locked && slot < 0)
            collection::Unlock(path);
        else if (locked)
            collection::Touch(path, GameId);
        SaveState();
        return slot;
    }
}

std::string PortalSessionFolder()
{
    if (const char* saves = skyenv::Get("SKY_SAVE_DIR"))
        return saves;
    if (std::filesystem::path game = datadir::PlayerGame(ARKEYAN_GAME_ID); !game.empty()) // the player profile's seats
        return datadir::Text(game);
    return {};
}

void PortalInit()
{
    netplay::PrepareSessionSetup();
    const char* modeVariable = skyenv::Get("SKY_PORTAL");
    std::string mode = modeVariable != nullptr ? modeVariable : "auto";
    if (netplay::Online())
    {
        mode = "emulated";
        fprintf(stderr, "[portal] online session: real portals are not read; using the emulated portal\n");
        const auto* game = collection::Catalogue::Get().FindGame(GameId);
        for (auto& slots : g_onlineSlots)
            slots.fill(-1);
        int slot = 0;
        bool halves = false;
        if (game)
            for (const auto& seat : game->seats)
                halves |= std::find(seat.accepts.begin(), seat.accepts.end(), collection::Category::SwapTop) !=
                          seat.accepts.end();
        if (game)
            for (size_t index = 0; index < game->seats.size() && index < g_onlineSlots.size(); ++index)
            {
                const auto& seat = game->seats[index];
                const uint8_t player = uint8_t(seat.player);
                for (int half = 0;
                    half < (halves && player && seat.id.starts_with('p') ? 2 : 1) && slot < PortalSlotCount;
                    ++half, ++slot)
                {
                    g_onlinePlayers[slot] = player;
                    g_onlineSlots[index][half] = slot;
                }
            }
    }
    fprintf(stderr, "[portal] mode %s%s\n", mode.c_str(), modeVariable != nullptr ? " (SKY_PORTAL)" : "");
    if (mode == "none")
    {
        fprintf(stderr, "[portal] disabled\n");
        return;
    }

    // Auto mode keeps a real portal only if one is plugged in now; a forced one waits for it to be plugged in.
    // Real mode takes whichever real portal is plugged in, else waits on the first backend available.
    std::unique_ptr<PortalBackend> backend, waiting;
    if (mode == "stall")
        backend = std::make_unique<StallingPortal>();
    if (mode == "fake-real") // tests: a fake real portal (portal_backend.h)
        backend = CreateFakeRealPortal(
            skyenv::Get("SKY_PORTAL_FAKE_REAL") != nullptr ? skyenv::Get("SKY_PORTAL_FAKE_REAL") : "");
    if (mode == "auto" || mode == "real" || mode == "hid")
    {
        backend = CreateHidPortal();
        if (backend != nullptr && (mode == "auto" || mode == "real") && !backend->IsConnected())
        {
            if (mode == "real")
                waiting = std::move(backend);
            backend.reset();
        }
    }
    if (backend == nullptr && (mode == "auto" || mode == "real" || mode == "usb"))
    {
        backend = CreateUsbPortal();
        if (backend == nullptr)
#ifdef _WIN32
            fprintf(stderr, "[portal] usb: neither winusb.dll nor Windows.Gaming.Input.Custom: no Xbox portal\n");
#else
            fprintf(stderr, "[portal] usb: libusb-1.0 not found, no Xbox 360 or Xbox One portal (install the "
                            "libusb-1.0 package; SKY_LIBUSB=<path> names another copy)\n");
#endif
        if (backend != nullptr && (mode == "auto" || (mode == "real" && waiting != nullptr)) && !backend->IsConnected())
            backend.reset();
    }
    if (backend == nullptr)
        backend = std::move(waiting);
    if (backend == nullptr)
    {
        if (mode == "auto")
            fprintf(stderr, "[portal] no real portal plugged in: emulating one\n");
        else if (mode != "emulated")
            fprintf(stderr, "[portal] %s portal unavailable, emulating one\n", mode.c_str());
        g_emulated = new EmulatedPortal;
        g_emulated->SetModel(EmulatedPortalModel(GameId));
        // Trap Team's portal thread reads until a read returns nothing: one status report per 8 ms (below Giants' 12 ms
        // poll) lets it finish and release the lock the main thread waits on.
        g_emulated->SetStatusInterval(std::chrono::milliseconds(8));
        // Online play: the poll that gets a status report is a matter of the game's virtual time, not of this computer's
        // speed (two replays of Giants read them at different polls on the host's clock).
        if (netplay::Enabled())
            g_emulated->SetClock([] { return std::chrono::nanoseconds(int64_t(GuestSystemTime()) * 100); });
        // Tests: another model from the table, by its id ("01 28", "0128"), to see what the game does with it.
        if (const char* wanted = skyenv::Get("SKY_PORTAL_MODEL"))
        {
            uint8_t id[4];
            size_t length = 0;
            for (const char* p = wanted; *p != '\0' && length < 4;)
            {
                if (isspace(uint8_t(*p)))
                {
                    p++;
                    continue;
                }
                char byte[3] = { p[0], p[1], '\0' };
                id[length++] = uint8_t(strtoul(byte, nullptr, 16));
                p += p[1] != '\0' ? 2 : 1;
            }
            if (const PortalModel* model = FindPortalModel(id, length))
                g_emulated->SetModel(*model);
            else
                fprintf(stderr, "[portal] SKY_PORTAL_MODEL=%s is not in the table, kept the default\n", wanted);
        }
        backend.reset(g_emulated);
    }
    else
    {
        // Real portals: the device's I/O never runs on the title's threads (host/portal_async.h).
        bool connected = backend->IsOpen();
        backend = std::make_unique<AsyncPortal>(std::move(backend), connected);
        // With the Toy Box's virtual seats beside its toys (MixedPortal), unless SKY_PORTAL_VIRTUAL=0.
        if (const char* seats = skyenv::Get("SKY_PORTAL_VIRTUAL"); seats == nullptr || std::string(seats) != "0")
        {
            g_emulated = new EmulatedPortal;
            g_mixed = new MixedPortal(std::move(backend), *g_emulated);
            backend.reset(g_mixed);
        }
    }
    g_backend = backend.release();
    fprintf(stderr, "[portal] %s portal\n", g_backend->Name());

    if (g_emulated == nullptr)
        return;

    if (g_mixed != nullptr)
        fprintf(stderr, "[portal] mixed: the real portal's toys and the Toy Box's virtual seats in one portal\n");
    else
        fprintf(stderr, "[portal] model: %s (emulated)\n",
            DescribePortal(GameId, g_emulated->Model().id, g_emulated->Model().idLength).c_str());
    fprintf(stderr, "[portal] figure folder: %s (F7: next figure, F8: remove all)\n", FigureFolder().c_str());
    if (int cleared = collection::ClearStaleLocks())
        fprintf(stderr, "[portal] %d lock(s) left by programs that are gone removed\n", cleared);
    if (std::string legacy = collection::LegacyFolder(); !legacy.empty())
        collection::Migrate(legacy);
    // The figures on the portal stay locked until the game ends.
    atexit([] {
        for (int slot = 0; slot < EmulatedPortal::SlotCount; slot++)
            collection::Unlock(g_emulated->SlotPath(slot));
    });
    // Tests: exactly the figures of SKY_PORTAL_PLACE, whatever the last session left.
    if (const char* list = skyenv::Get("SKY_PORTAL_PLACE"))
    {
        std::string entries = list;
        for (size_t begin = 0; begin <= entries.size();)
        {
            size_t end = std::min(entries.find(',', begin), entries.size());
            if (end > begin)
                PlaceStartupFigure(entries.substr(begin, end - begin));
            begin = end + 1;
        }
    }
    // With a real portal the virtual seats start empty (portal.txt is the virtual portal's). A session restores them
    // too: the launcher writes the same portal.txt, the same figure files, into both games' session folders.
    else if (g_mixed == nullptr)
        RestoreState();
    SDL_AddEventWatch(OnHostEvent, nullptr);
}

PortalLightState PortalLight()
{
    std::lock_guard lock(g_mutex);
    return g_light;
}

uint64_t PortalResets()
{
    std::lock_guard lock(g_mutex);
    return g_resets;
}

bool PortalIsConnected()
{
    std::lock_guard lock(g_mutex);
    return g_backend != nullptr && g_backend->IsConnected();
}

bool PortalWrite(const uint8_t* command, size_t size)
{
    std::lock_guard lock(g_mutex);
    if (g_backend == nullptr)
        return false;
    ObserveCommand(command, size);
    // A real portal without a speaker (any but the Traptanium) may never answer `M`, and Trap Team reads no figure until
    // it is answered: the runtime answers as the speaker would, and the portal is not sent the command.
    if (size >= 2 && command[0] == 'M' && Real() && g_realIdLength != 0 &&
        !PortalModelHasSpeaker(g_realId, g_realIdLength))
    {
        std::array<uint8_t, 32> answer{};
        answer[0] = 'M';
        answer[1] = command[1];
        answer[3] = 0x19; // as the emulated portal answers (host/portal_emulated.cpp)
        g_answers.push_back(answer);
        return true;
    }
    return g_backend->Write(command, size);
}

void PortalSpeakerSound(const uint8_t* block, size_t size)
{
    std::lock_guard lock(g_mutex);
    if (g_backend == nullptr)
        return;
    SpeakerRoute route =
        ChooseSpeakerRoute(g_sound, Real(), g_realId, g_realIdLength, Real() && g_backend->TakesSound());
    if (!g_speakerRouted || route != g_speakerRoute)
    {
        fprintf(stderr, "[portal] speaker: %s\n",
            route == SpeakerRoute::Device ? "the portal plays the sound"
            : route == SpeakerRoute::Host ? "the computer plays the sound"
                                          : "off (Portal sound)");
        if (route != SpeakerRoute::Host)
            HostSpeakerStream().Flush();
        g_speakerRoute = route;
        g_speakerRouted = true;
    }
    if (route == SpeakerRoute::Device)
        g_backend->WriteSound(block, size);
    else if (route == SpeakerRoute::Host)
        HostSpeakerStream().Push(block, size);
}

void PortalSetSound(bool on)
{
    g_sound = on;
    if (!on)
        HostSpeakerStream().Flush();
}

bool PortalGameHasSpeaker()
{
    const PortalModel& model = EmulatedPortalModel(GameId);
    return PortalModelHasSpeaker(model.id, model.idLength);
}

size_t PortalRead(uint8_t* report, size_t capacity)
{
    std::lock_guard lock(g_mutex);
    if (!g_answers.empty() && capacity >= 4)
    {
        const size_t size = std::min(capacity, g_answers.front().size());
        memcpy(report, g_answers.front().data(), size);
        g_answers.pop_front();
        return size;
    }
    size_t size = g_backend != nullptr ? g_backend->Read(report, capacity) : 0;
    ObserveReport(report, size);
    return size;
}

PortalSlotSeen PortalSeen(int slot)
{
    std::lock_guard lock(g_mutex);
    return slot >= 0 && slot < PortalSlotCount ? g_seen[slot] : PortalSlotSeen{};
}

int PortalPlaceFigure(const std::string& path) { return Place(path, -1); }

int PortalPlaceFigureAt(const std::string& path, int slot) { return Place(path, slot); }

bool PortalReplaceFigures(const std::vector<PortalFigureChange>& changes)
{
    if (netplay::Online())
        return QueueOnline(changes);
    std::lock_guard lock(g_mutex);
    if (g_emulated == nullptr)
        return false;

    std::vector<PortalSlots::Replacement> replacements;
    std::vector<std::string> acquired, previous;
    auto refuse = [&]() {
        for (const std::string& path : acquired)
            collection::Unlock(path);
        return false;
    };
    uint32_t used = 0;
    for (const PortalFigureChange& change : changes)
    {
        if (change.slot < 0 || change.slot >= PortalSlotCount || (used >> change.slot & 1) != 0 ||
            PortalSlotIsReal(change.slot))
            return refuse();
        used |= uint32_t(1) << change.slot;
        previous.push_back(g_emulated->SlotPath(change.slot));
        PortalSlots::Replacement replacement{ change.slot, std::nullopt, change.path };
        if (!change.path.empty())
        {
            FigureData data;
            if (!LoadFigure(change.path, data))
                return refuse();
            figures::MendVirtualToy(data);
            if (InCollection(change.path))
            {
                bool mine = collection::GetLock(change.path).mine;
                if (!collection::Lock(change.path, GameId))
                    return refuse();
                if (!mine)
                    acquired.push_back(change.path);
            }
            replacement.data = data;
        }
        replacements.push_back(std::move(replacement));
    }
    if (!g_emulated->Replace(replacements))
        return refuse();

    for (const std::string& path : previous)
    {
        bool kept = std::any_of(
            changes.begin(), changes.end(), [&](const PortalFigureChange& change) { return change.path == path; });
        if (!path.empty() && !kept)
            collection::Unlock(path);
    }
    for (const PortalSlots::Replacement& replacement : replacements)
    {
        if (!replacement.data)
            continue;
        if (InCollection(replacement.path))
            collection::Touch(replacement.path, GameId);
        const FigureData& data = *replacement.data;
        uint16_t toyType = uint16_t(data[0x10] | data[0x11] << 8);
        fprintf(stderr, "[portal] placed %s (toy type %u) in slot %d\n", replacement.path.c_str(), toyType,
            replacement.slot);
    }
    SaveState();
    return true;
}

void PortalRemoveFigure(int slot)
{
    if (netplay::Online())
    {
        QueueOnline({ { slot, {} } });
        return;
    }
    if (g_emulated == nullptr)
        return;
    collection::Unlock(g_emulated->SlotPath(slot));
    g_emulated->Remove(slot);
    SaveState();
}

void PortalRemoveAllFigures()
{
    if (netplay::Online())
    {
        std::vector<PortalFigureChange> changes;
        for (int slot = 0; slot < PortalSlotCount; ++slot)
            if (netplay::SeatAllowed(netplay::Hosting(), g_onlinePlayers[slot]))
                changes.push_back({ slot, {} });
        QueueOnline(changes);
        g_hotkeySlot = -1;
        return;
    }
    if (g_emulated == nullptr)
        return;
    for (int slot = 0; slot < EmulatedPortal::SlotCount; slot++)
    {
        collection::Unlock(g_emulated->SlotPath(slot));
        g_emulated->Remove(slot);
    }
    g_hotkeySlot = -1;
    SaveState();
    fprintf(stderr, "[portal] removed every figure\n");
}

bool PortalIsEmulated() { return g_emulated != nullptr; }

bool PortalSeatWritable(int player)
{
    return !netplay::Online() || netplay::SeatAllowed(netplay::Hosting(), uint8_t(player));
}

int PortalOnlineSlot(int seat, int half)
{
    return seat >= 0 && seat < PortalSlotCount && half >= 0 && half < 2 ? g_onlineSlots[seat][half] : -1;
}

std::array<uint8_t, PortalSlotCount> PortalOnlinePlayers() { return g_onlinePlayers; }

bool PortalPrepareOnline(bool host, const netplay::PortalEvent& event)
{
    if (!event.figure)
        return true;
    std::lock_guard lock(g_onlineMutex);
    if (OnlineFind(host, event.figure))
        return !event.hasDump;
    if (!event.hasDump || g_onlineFigures.size() >= 4096)
        return false;
    const auto path = OnlinePath(host, event.figure);
    std::error_code error;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), error);
    if (error || !SaveFigure(path, event.dump))
        return false;
    g_onlineFigures.push_back({ host, event.figure, path, {} });
    return true;
}

bool PortalApplyOnline(const netplay::FrameEvents& events)
{
    for (const auto& player : events.players)
        if (player.count)
        {
            std::lock_guard onlineLock(g_onlineMutex);
            std::lock_guard portalLock(g_mutex);
            std::vector<PortalSlots::Replacement> replacements;
            replacements.reserve(PortalSlotCount);
            const bool host = &player == &events.players[0];
            for (size_t index = 0; index < player.count; ++index)
            {
                const auto& event = player.events[index];
                PortalSlots::Replacement replacement{ event.slot, std::nullopt, {} };
                if (event.figure)
                {
                    const auto* figure = OnlineFind(host, event.figure);
                    FigureData data;
                    if (!figure || !LoadFigure(figure->path, data))
                        return false;
                    replacement.data = data;
                    replacement.path = figure->path;
                }
                replacements.push_back(std::move(replacement));
                if (event.last)
                {
                    if (!g_emulated || !g_emulated->Replace(replacements))
                        return false;
                    replacements.clear();
                }
            }
            if (!replacements.empty())
                return false;
        }
    return true;
}

void PortalPeerLeft()
{
    std::lock_guard onlineLock(g_onlineMutex);
    std::lock_guard portalLock(g_mutex);
    if (!g_emulated)
        return;
    for (int slot = 0; slot < PortalSlotCount; ++slot)
        if (!netplay::SeatAllowed(netplay::Hosting(), g_onlinePlayers[slot]))
        {
            g_emulated->Remove(slot);
            g_onlinePlanned[slot].clear();
        }
    for (auto& figure : g_onlineFigures)
        if (!figure.source.empty())
        {
            FigureData data;
            if (!LoadFigure(figure.path, data) || !SaveFigure(figure.source, data))
            {
                fprintf(stderr, "[portal] cannot finish an online figure\n");
                continue;
            }
            if (!g_emulated->Retarget(figure.path, figure.source))
                collection::Unlock(figure.source);
            figure.source.clear();
        }
}

void PortalFinishOnline()
{
    std::lock_guard lock(g_onlineMutex);
    for (const auto& figure : g_onlineFigures)
        if (!figure.source.empty())
        {
            FigureData data;
            if (!LoadFigure(figure.path, data) || !SaveFigure(figure.source, data))
                fprintf(stderr, "[portal] cannot flush an online figure\n");
            collection::Unlock(figure.source);
        }
}

bool PortalIsMixed() { return g_mixed != nullptr; }

bool PortalSlotIsReal(int slot) { return g_mixed != nullptr && g_mixed->IsRealSlot(slot); }

std::string PortalSlotFigure(int slot)
{
    if (netplay::Online() && slot >= 0 && slot < PortalSlotCount)
    {
        std::lock_guard lock(g_onlineMutex);
        if (netplay::SeatAllowed(netplay::Hosting(), g_onlinePlayers[slot]))
            return g_onlinePlanned[slot];
    }
    return g_emulated ? g_emulated->SlotPath(slot) : std::string();
}

std::string PortalFigureFolder() { return FigureFolder(); }

bool PortalLogEnabled() { return g_log; }

namespace
{
    std::string JsonString(const std::string& text)
    {
        std::string json = "\"";
        for (unsigned char c : text)
        {
            if (c == '"' || c == '\\')
                json += { '\\', char(c) };
            else if (c < 0x20)
            {
                char escaped[8];
                snprintf(escaped, sizeof(escaped), "\\u%04X", c);
                json += escaped;
            }
            else
                json += char(c);
        }
        return json + "\"";
    }
}

int PortalProbeMain()
{
    std::vector<PortalProbe> portals;
    std::pair<const char*, std::string> transports[] = { { "hid", ProbeHidPortals(portals) },
        { "usb", ProbeUsbPortals(portals) } };
    for (const auto& [kind, error] : transports)
    {
        if (!error.empty())
            printf("{\"transport\":\"%s\",\"available\":false,\"status\":%s}\n", kind,
                JsonString("error: " + error).c_str());
    }
    const std::vector<std::string> notes = ProbeNotes();
    for (const std::string& note : notes)
        printf("{\"note\":%s}\n", JsonString(note).c_str());
    // For a person: one block on stderr. For the launcher: the JSON lines on stdout.
    fprintf(stderr, "portal probe: %zu portal(s) found\n", portals.size());
    for (const std::string& note : notes)
        fprintf(stderr, "  %s\n", note.c_str());
    for (const auto& [kind, error] : transports)
    {
        if (!error.empty())
            fprintf(stderr, "  %s: unavailable (%s)\n", kind, error.c_str());
    }
    for (const PortalProbe& probe : portals)
    {
        fprintf(stderr, "  %s %04x:%04x \"%s\": %s%s%s\n", probe.kind, probe.vendor, probe.product, probe.name.c_str(),
            probe.opened ? "opened" : "not opened", probe.details.empty() ? "" : ", ", probe.details.c_str());
        if (!probe.id.empty())
            fprintf(stderr, "    model: %s %s; Spyro's Adventure: %s%s%s%s\n", probe.id.c_str(), probe.model.c_str(),
                probe.verdict.c_str(), probe.verdictReason.empty() ? "" : " (", probe.verdictReason.c_str(),
                probe.verdictReason.empty() ? "" : ")");
        for (const auto& [method, result] : probe.methods)
            fprintf(stderr, "    %-14s %s\n", method.c_str(), result.c_str());
        fprintf(stderr, "    => %s%s\n", probe.status.c_str(),
            probe.write.empty() ? "" : (", the game writes with " + probe.write).c_str());
    }
    fflush(stderr);

    for (const PortalProbe& probe : portals)
    {
        std::string extra;
        if (!probe.methods.empty())
        {
            extra = ",\"write\":" + JsonString(probe.write) + ",\"methods\":{";
            for (size_t i = 0; i < probe.methods.size(); i++)
                extra +=
                    (i > 0 ? "," : "") + JsonString(probe.methods[i].first) + ":" + JsonString(probe.methods[i].second);
            extra += "}";
        }
        if (!probe.details.empty())
            extra += ",\"details\":" + JsonString(probe.details);
        if (!probe.id.empty())
            extra += ",\"id\":" + JsonString(probe.id) + ",\"model\":" + JsonString(probe.model) +
                     ",\"verdict\":" + JsonString(probe.verdict) +
                     ",\"verdict_reason\":" + JsonString(probe.verdictReason);
        printf("{\"kind\":\"%s\",\"vid\":\"%04x\",\"pid\":\"%04x\",\"product\":%s,\"opened\":%s,\"status\":%s%s}\n",
            probe.kind, probe.vendor, probe.product, JsonString(probe.name).c_str(), probe.opened ? "true" : "false",
            JsonString(probe.status).c_str(), extra.c_str());
    }
    printf("{\"done\":true,\"count\":%zu}\n", portals.size());
    fflush(stdout);
    return 0;
}

void PortalSetHotkeys(int addScancode, int clearScancode)
{
    g_addKey = addScancode;
    g_clearKey = clearScancode;
}
