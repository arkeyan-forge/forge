// Checks the launcher's disc verification and settings file without showing any UI.
// usage: ArkeyanRuntimeLauncherTest <game disc .iso> <scratch dir> [<damaged copy of the image>...]
#include "launcher.h"
#include "vfs.h"
#include "collection/game_data.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>

namespace
{
    int g_failures = 0;

#define EXPECT(condition)                                                                                              \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(condition))                                                                                              \
        {                                                                                                              \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition);                                               \
            g_failures++;                                                                                              \
        }                                                                                                              \
    } while (0)

    const char* StatusName(GameSourceStatus status)
    {
        switch (status)
        {
        case GameSourceStatus::Ok: return "Ok";
        case GameSourceStatus::NotFound: return "NotFound";
        case GameSourceStatus::Unreadable: return "Unreadable";
        case GameSourceStatus::Folder: return "Folder";
        case GameSourceStatus::NotXboxDisc: return "NotXboxDisc";
        case GameSourceStatus::NotXbox360Game: return "NotXbox360Game";
        case GameSourceStatus::Damaged: return "Damaged";
        case GameSourceStatus::WrongGame: return "WrongGame";
        case GameSourceStatus::WrongVersion: return "WrongVersion";
        }
        return "?";
    }

    GameSourceStatus Check(const std::filesystem::path& path)
    {
        GameSourceCheck check = CheckGameSource(path.string(), *FindGame("ssa"));
        printf("  %-14s %s\n", StatusName(check.status), path.filename().string().c_str());
        if (!check.message.empty())
            printf("    -> %s\n", check.message.c_str());
        return check.status;
    }

    void Put32LE(uint8_t* p, uint32_t v)
    {
        p[0] = uint8_t(v);
        p[1] = uint8_t(v >> 8);
        p[2] = uint8_t(v >> 16);
        p[3] = uint8_t(v >> 24);
    }
    void Put32BE(uint8_t* p, uint32_t v)
    {
        p[0] = uint8_t(v >> 24);
        p[1] = uint8_t(v >> 16);
        p[2] = uint8_t(v >> 8);
        p[3] = uint8_t(v);
    }

    void WriteFile(const std::filesystem::path& path, const std::vector<uint8_t>& data)
    {
        std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char*>(data.data()), data.size());
    }

    // Minimal XDVDFS image (plain layout: volume descriptor at sector 32) whose root holds one file.
    void WriteDiscImage(const std::filesystem::path& path, const std::string& name, const std::vector<uint8_t>& file,
        uint32_t extraDeclaredSize = 0)
    {
        constexpr uint32_t Sector = 2048;
        std::vector<uint8_t> image(34 * Sector + file.size());
        memcpy(&image[32 * Sector], "MICROSOFT*XBOX*MEDIA", 20);
        Put32LE(&image[32 * Sector + 20], 33);
        Put32LE(&image[32 * Sector + 24], Sector);

        uint8_t* entry = &image[33 * Sector]; // no left/right subtrees
        Put32LE(entry + 4, 34);
        Put32LE(entry + 8, uint32_t(file.size()) + extraDeclaredSize);
        entry[12] = 0x80;
        entry[13] = uint8_t(name.size());
        std::copy(name.begin(), name.end(), entry + 14);

        memcpy(&image[34 * Sector], file.data(), file.size());
        WriteFile(path, image);
    }

    // XEX2 header carrying only the execution info.
    std::vector<uint8_t> FakeXex(uint32_t titleId, uint32_t mediaId, uint32_t version)
    {
        std::vector<uint8_t> xex(0x100);
        Put32BE(&xex[0x00], 0x58455832);
        Put32BE(&xex[0x14], 1);
        Put32BE(&xex[0x18], 0x00040006);
        Put32BE(&xex[0x1C], 0x40);
        Put32BE(&xex[0x40], mediaId);
        Put32BE(&xex[0x44], version);
        Put32BE(&xex[0x4C], titleId);
        return xex;
    }

    std::vector<uint8_t> RandomBytes(size_t size)
    {
        std::mt19937 random(1234);
        std::vector<uint8_t> data(size);
        for (uint8_t& byte : data)
            byte = uint8_t(random());
        return data;
    }

    void SetVariable(const char* name, const std::string& value)
    {
#if defined(_WIN32)
        _putenv_s(name, value.c_str());
#else
        setenv(name, value.c_str(), 1); // NOLINT(concurrency-mt-unsafe): the test has one thread
#endif
    }

    void UnsetVariable(const char* name)
    {
#if defined(_WIN32)
        _putenv_s(name, "");
#else
        unsetenv(name); // NOLINT(concurrency-mt-unsafe)
#endif
    }

    std::string ResolveHeadless(std::vector<std::string> arguments)
    {
        arguments.insert(arguments.begin(), "SSARecomp");
        std::vector<char*> argv;
        argv.reserve(arguments.size());
        for (std::string& argument : arguments)
            argv.push_back(argument.data());
        return ResolveGameSource(int(argv.size()), argv.data(), *FindGame("ssa"), false);
    }
}

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        fprintf(stderr,
            "usage: ArkeyanRuntimeLauncherTest <game disc .iso> <scratch dir> [<damaged copy of the image>...]\n");
        return 2;
    }
    std::filesystem::path iso = argv[1];
    std::filesystem::path scratch = std::filesystem::path(argv[2]) / "launcher_test";
    std::filesystem::remove_all(scratch);
    std::filesystem::create_directories(scratch);
    const GameInfo& game = *FindGame("ssa");
    // Started as a game opened on its own would be: a scratch data folder and launcher.ini, no Games folder given.
    const std::filesystem::path settings = scratch / "launcher.ini";
    SetVariable("SKYLANDERS_DATA", (scratch / "data").string());
    SetVariable("SKY_LAUNCHER_INI", settings.string());
    for (const char* name : { "SKY_SETTINGS", "SKY_GAMES_DIR", "SKY_LAUNCHER_RESTARTS" })
        UnsetVariable(name);
    using Clock = std::chrono::steady_clock;
    auto milliseconds = [](Clock::time_point start) {
        return (long long)std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start).count();
    };

    // FIPS 180-4 examples, covering one- and two-block padding.
    auto sha256 = [](std::string_view text) {
        return Sha256Hex({ reinterpret_cast<const uint8_t*>(text.data()), text.size() });
    };
    EXPECT(sha256("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    EXPECT(sha256("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    EXPECT(sha256("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
           "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    EXPECT(sha256(std::string(1000000, 'a')) == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");

    printf("Game table: %zu game(s); \"ssa\" = %s, %zu release(s)\n", g_games.size(), game.name, game.releases.size());
    EXPECT(FindGame("unknown") == nullptr);

    printf("Player's disc image:\n");
    auto start = Clock::now();
    EXPECT(Check(iso) == GameSourceStatus::Ok);
    printf("  verified in %lld ms\n", milliseconds(start));

    DiscFileSystem disc;
    EXPECT(disc.Open(iso.string().c_str()));
    start = Clock::now();
    TitleMetadata metadata = ReadTitleMetadata(disc.ReadAll("default.xex"));
    const std::vector<uint8_t>& png = metadata.iconPng;
    bool isPng = png.size() > 24 && memcmp(png.data(), "\x89PNG\r\n\x1a\n", 8) == 0;
    EXPECT(!metadata.name.empty());
    EXPECT(metadata.titleId == 0x415608AB);
    EXPECT(isPng);
    if (isPng)
    {
        auto be32 = [&](size_t offset) {
            return uint32_t(png[offset]) << 24 | png[offset + 1] << 16 | png[offset + 2] << 8 | png[offset + 3];
        };
        printf("  XDBF title name \"%s\", icon PNG %ux%u (%zu bytes), read in %lld ms (kept in memory only)\n",
            metadata.name.c_str(), be32(16), be32(20), png.size(), milliseconds(start));
    }

    printf("Wrong inputs:\n");
    EXPECT(Check(scratch / "missing.iso") == GameSourceStatus::NotFound);

    std::filesystem::create_directories(scratch / "extracted");
    WriteFile(scratch / "extracted" / "default.xex", FakeXex(0x415608AB, 0x53CE7B97, 2));
    EXPECT(Check(scratch / "extracted") == GameSourceStatus::Folder);

    WriteFile(scratch / "locked.iso", RandomBytes(4096));
    std::filesystem::permissions(scratch / "locked.iso", std::filesystem::perms::none);
    EXPECT(Check(scratch / "locked.iso") == GameSourceStatus::Unreadable);
    std::filesystem::permissions(scratch / "locked.iso", std::filesystem::perms::owner_all);

    WriteFile(scratch / "empty.iso", {});
    EXPECT(Check(scratch / "empty.iso") == GameSourceStatus::NotXboxDisc);
    WriteFile(scratch / "random.iso", RandomBytes(1 << 20));
    EXPECT(Check(scratch / "random.iso") == GameSourceStatus::NotXboxDisc);

    WriteDiscImage(scratch / "original_xbox.iso", "default.xbe", RandomBytes(4096));
    EXPECT(Check(scratch / "original_xbox.iso") == GameSourceStatus::NotXbox360Game);
    WriteDiscImage(scratch / "not_a_xex.iso", "default.xex", RandomBytes(4096));
    EXPECT(Check(scratch / "not_a_xex.iso") == GameSourceStatus::Damaged);
    WriteDiscImage(scratch / "cut_short.iso", "default.xex", FakeXex(0x415608AB, 0x53CE7B97, 2), 4096);
    EXPECT(Check(scratch / "cut_short.iso") == GameSourceStatus::Damaged);
    WriteDiscImage(scratch / "other_game.iso", "default.xex", FakeXex(0x4D5307E6, 0x11111111, 0));
    EXPECT(Check(scratch / "other_game.iso") == GameSourceStatus::WrongGame);
    WriteDiscImage(scratch / "other_region.iso", "default.xex", FakeXex(0x415608AB, 0x12345678, 0x00000100));
    EXPECT(Check(scratch / "other_region.iso") == GameSourceStatus::WrongVersion);
    WriteDiscImage(scratch / "modified.iso", "default.xex", FakeXex(0x415608AB, 0x53CE7B97, 2));
    EXPECT(Check(scratch / "modified.iso") == GameSourceStatus::Damaged);

    printf("Another edition (the player's default.xex with another media ID and version in its header):\n");
    {
        std::vector<uint8_t> xex = disc.ReadAll("default.xex");
        auto be32 = [&](size_t at) {
            return uint32_t(xex[at]) << 24 | uint32_t(xex[at + 1]) << 16 | uint32_t(xex[at + 2]) << 8 | xex[at + 3];
        };
        size_t info = 0;
        for (uint32_t i = 0; i < be32(0x14); i++)
            if (be32(0x18 + i * 8) == 0x00040006)
                info = be32(0x18 + i * 8 + 4);
        EXPECT(info != 0);
        Put32BE(&xex[info], 0x1234ABCD);
        Put32BE(&xex[info + 4], 0x0000000E);
        WriteDiscImage(scratch / "header_only_edition.iso", "default.xex", xex);
        EXPECT(Check(scratch / "header_only_edition.iso") == GameSourceStatus::Ok);
        // The same header over other code: one byte of the executable's data changed.
        xex[be32(0x08) + (xex.size() - be32(0x08)) / 2] ^= 0x5A;
        WriteDiscImage(scratch / "other_code_edition.iso", "default.xex", xex);
        EXPECT(Check(scratch / "other_code_edition.iso") == GameSourceStatus::WrongVersion);
        std::filesystem::remove(scratch / "header_only_edition.iso");
        std::filesystem::remove(scratch / "other_code_edition.iso");
    }

    printf("Damaged copies of the player's image:\n");
    for (int i = 3; i < argc; i++)
    {
        GameSourceStatus status = Check(argv[i]);
        EXPECT(status != GameSourceStatus::Ok);
    }

    printf("Command line without UI:\n");
    EXPECT(ResolveHeadless({ iso.string() }) == iso.string());
    EXPECT(ResolveHeadless({}).empty());
    EXPECT(ResolveHeadless({ (scratch / "random.iso").string() }).empty());

    printf("Opened on its own:\n");
    {
        LauncherConfig remembered;
        remembered["ssa"]["iso"] = iso.string();
        EXPECT(SaveLauncherConfig(settings.string(), remembered));
        EXPECT(ResolveHeadless({}) == iso.string());

        remembered["ssa"]["iso"] = (scratch / "moved.iso").string();
        EXPECT(SaveLauncherConfig(settings.string(), remembered));
        EXPECT(ResolveHeadless({}).empty());
        EXPECT(StartFromLauncherMessage(game, false) == "Start Skylanders Spyro's Adventure from Arkeyan Launcher.");
        EXPECT(StartFromLauncherMessage(game, true) ==
               "Start Skylanders Spyro's Adventure from Arkeyan Launcher.\n\nThe disc image it used last time is "
               "missing or has changed.");

        // The copied game data in a Games folder named only in launcher.ini (moved by Arkeyan Launcher).
        std::vector<uint8_t> xex = disc.ReadAll("default.xex");
        gamedata::Source source;
        source.files = { { "default.xex", xex.size() } };
        source.read = [&](const gamedata::SourceFile&, uint64_t position, void* buffer, size_t size) {
            size_t count = std::min<size_t>(size, xex.size() - size_t(position));
            memcpy(buffer, xex.data() + position, count);
            return count;
        };
        source.xexSha256 = Sha256Hex(xex);
        const std::filesystem::path games = scratch / "Games";
        const std::filesystem::path copy = games / "SSA" / "gamedata";
        gamedata::CopyOptions options;
        options.freeBytes = 1ll << 40;
        EXPECT(gamedata::Copy(source, copy, options).outcome == gamedata::CopyResult::Outcome::Done);
        remembered["launcher"]["games"] = games.string();
        EXPECT(SaveLauncherConfig(settings.string(), remembered));
        std::string found = ResolveHeadless({});
        EXPECT(std::filesystem::path(found) == copy);
        UnsetVariable("SKY_GAMES_DIR");
        std::filesystem::remove(settings);
    }

    printf("Settings file:\n");
    LauncherConfig config;
    config[""]["version"] = "1";
    config["ssa"]["iso"] = iso.string();
    config["giants"]["iso"] = "/Volumes/USB disk/Giants [Disc] #1 = d\xc3\xa9j\xc3\xa0 vu's.iso";
    EXPECT(SaveLauncherConfig(settings.string(), config));
    EXPECT(LoadLauncherConfig(settings.string()) == config);
    std::ifstream saved(settings);
    printf("%s", std::string(std::istreambuf_iterator<char>(saved), {}).c_str());

    std::string handEdited =
        "# comment\r\n  [ ssa ]  \r\n\r\n iso = /a b/c.iso \r\n; other\r\nnot a pair\r\n[empty]\r\n";
    WriteFile(scratch / "edited.ini", { handEdited.begin(), handEdited.end() });
    LauncherConfig edited = LoadLauncherConfig((scratch / "edited.ini").string());
    EXPECT(edited.size() == 1 && edited["ssa"].size() == 1 && edited["ssa"]["iso"] == "/a b/c.iso");
    EXPECT(LoadLauncherConfig((scratch / "missing.ini").string()).empty());

    std::filesystem::remove_all(scratch);
    printf(g_failures == 0 ? "All launcher checks passed\n" : "%d launcher check(s) failed\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
