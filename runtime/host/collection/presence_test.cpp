// Presence snapshots, using synthetic level ids and a scratch folder under the test's working directory.
#include "host/collection/presence.h"
#include "host/collection/save_backup.h"
#include "host/toybox_remote.h"
#include "game.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <thread>

namespace fs = std::filesystem;

namespace
{
    int g_checks, g_failures, g_levelEvents;

    void Check(bool condition, const char* what, int line)
    {
        g_checks++;
        if (!condition)
        {
            g_failures++;
            fprintf(stderr, "FAIL line %d: %s\n", line, what);
        }
    }
#define CHECK(condition) Check((condition), #condition, __LINE__)

    void WriterChecks(const fs::path& folder)
    {
        presence::Writer writer(folder);
        presence::State state;
        const fs::path file = folder / "presence.txt";
        const fs::path temp = savebackup::TempPath(file);
        CHECK(writer.Write(state));
        const std::string initial = "level=\nmenu=1\nplayers=1\n";
        CHECK(savebackup::ReadFile(file) == initial);
        CHECK(!fs::exists(temp));

        // A hard link keeps the previous inode: an in-place truncate would change both names.
        const fs::path previous = folder / "previous.txt";
        fs::create_hard_link(file, previous);
        state = { "test_zone\\test_stage", false, 2 };
        CHECK(writer.Write(state));
        CHECK(savebackup::ReadFile(file) == "level=test_zone\\test_stage\nmenu=0\nplayers=2\n");
        CHECK(savebackup::ReadFile(previous) == initial);
        CHECK(!fs::exists(temp));

        // A no-op must neither rename the target nor even touch the temporary sibling.
        const auto stamp = fs::file_time_type::clock::now() - std::chrono::hours(24);
        fs::last_write_time(file, stamp);
        const auto savedStamp = fs::last_write_time(file);
        std::ofstream(temp, std::ios::binary) << "untouched";
        CHECK(!writer.Write(state));
        CHECK(fs::last_write_time(file) == savedStamp);
        CHECK(savebackup::ReadFile(temp) == "untouched");
        fs::remove(temp);

        state.players = 1;
        CHECK(writer.Write(state));
        CHECK(savebackup::ReadFile(file) == "level=test_zone\\test_stage\nmenu=0\nplayers=1\n");
        state.menu = true;
        CHECK(writer.Write(state));
        CHECK(savebackup::ReadFile(file) == "level=test_zone\\test_stage\nmenu=1\nplayers=1\n");

        // An unwritable temporary sibling preserves the current file and does not cache the failed state.
        const std::string beforeFailure = savebackup::ReadFile(file);
        fs::create_directory(temp);
        std::ofstream(temp / "blocker") << "synthetic";
        state.level = "test_next";
        CHECK(!writer.Write(state));
        CHECK(savebackup::ReadFile(file) == beforeFailure);
        fs::remove_all(temp);
        CHECK(writer.Write(state));
        CHECK(savebackup::ReadFile(file) == "level=test_next\nmenu=1\nplayers=1\n");
        CHECK(!fs::exists(temp));

        CHECK(!writer.Write({ "test\nmenu=0", false, 1 }));
        CHECK(!writer.Write({ "test\rplayers=2", false, 1 }));
        CHECK(!writer.Write({ "test", false, 3 }));
        CHECK(!presence::Writer({}).Write({}));
    }

    void RuntimeChecks(const fs::path& folder)
    {
        const fs::path file = folder / "presence.txt";
        presence::Begin(folder);
        CHECK(savebackup::ReadFile(file) == "level=\nmenu=1\nplayers=1\n");
        CHECK(game::InMenuLevel());
        game::LevelStarted("test_level");
        CHECK(!game::InMenuLevel());
        CHECK(g_levelEvents == 1);
        CHECK(savebackup::ReadFile(file) == "level=test_level\nmenu=0\nplayers=1\n");
        const auto levelStamp = fs::last_write_time(file);
        game::LevelStarted("test_level");
        CHECK(g_levelEvents == 1 && fs::last_write_time(file) == levelStamp);
        presence::SetSecondPlayer(true);
        CHECK(savebackup::ReadFile(file) == "level=test_level\nmenu=0\nplayers=1\n");
        presence::Flush();
        CHECK(savebackup::ReadFile(file) == "level=test_level\nmenu=0\nplayers=2\n");
        game::LevelStarted("test_menu");
        CHECK(game::InMenuLevel());
        CHECK(savebackup::ReadFile(file) == "level=test_menu\nmenu=1\nplayers=2\n");
        presence::SetSecondPlayer(false);
        presence::Flush();
        CHECK(savebackup::ReadFile(file) == "level=test_menu\nmenu=1\nplayers=1\n");
        const auto stamp = fs::last_write_time(file);
        presence::SetSecondPlayer(false);
        presence::Flush();
        game::LevelStarted("test_menu");
        CHECK(fs::last_write_time(file) == stamp);

        // A new session replaces a stale level and player count before its first level arrives.
        presence::Begin(folder);
        CHECK(savebackup::ReadFile(file) == "level=\nmenu=1\nplayers=1\n");
#if defined(_WIN32)
        _putenv_s("SKY_TOYBOX_LINK", "0");
#else
        setenv("SKY_TOYBOX_LINK", "0", 1);
#endif
        toyboxremote::Start(folder.string(), "test");
        presence::SetSecondPlayer(true);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (
            savebackup::ReadFile(file) != "level=\nmenu=1\nplayers=2\n" && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        CHECK(savebackup::ReadFile(file) == "level=\nmenu=1\nplayers=2\n");
        CHECK(!fs::exists(folder / toyboxlink::FileName));
        presence::SetSecondPlayer(false);
        toyboxremote::Stop();
        CHECK(savebackup::ReadFile(file) == "level=\nmenu=1\nplayers=1\n");
    }
}

// A synthetic adapter exercises the runtime's real level-start hook without any title code.
const game::Adapter& game::Current()
{
    static const GameInfo info{ "test", "Test", {}, "Test" };
    static constexpr std::string_view menus[] = { "test_menu" };
    static const Adapter adapter = [] {
        Adapter value{ info, nullptr, false, nullptr, {}, {} };
        value.menuLevels = menus;
        return value;
    }();
    return adapter;
}

void ToyboxLevelStarted() { g_levelEvents++; }

int main()
{
    const fs::path scratch = fs::current_path() / ("presence-test-" + toyboxlink::NewToken());
    WriterChecks(scratch / "writer");
    RuntimeChecks(scratch / "runtime");
    fs::remove_all(scratch);
    printf("Presence: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
