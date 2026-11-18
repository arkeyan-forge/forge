// Chests added from the launcher: the request and status files, the room left, the plan against the limit, and the
// claim against a cancel. Synthetic kinds and a scratch folder under the test's working directory.
#include "host/collection/chests.h"
#include "host/collection/save_backup.h"
#include <cstdio>
#include <string_view>

namespace fs = std::filesystem;

namespace
{
    int g_checks, g_failures;

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

    constexpr std::string_view Kinds[] = { "small", "big", "huge" };

    void WriteText(const fs::path& file, std::string_view text)
    {
        CHECK(savebackup::WriteFileAtomic(file, text.data(), text.size()));
    }

    void ParseChecks()
    {
        auto request = chests::ParseRequest("slot=2\nsmall=5\nhuge=20\n", Kinds);
        CHECK(request && request->slot == 2);
        CHECK(request && request->counts == (std::vector<int>{ 5, 0, 20 }) && request->Total() == 25);

        // Windows line ends, spaces, an unknown kind and a bad number: the kinds known are kept, the rest count 0.
        request = chests::ParseRequest("slot=0\r\n big = 3 \r\nother=4\nsmall=x\nhuge=-2\n", Kinds);
        CHECK(request && request->slot == 0 && request->counts == (std::vector<int>{ 0, 3, 0 }));

        // No slot, or a slot that is not a number: no request.
        CHECK(!chests::ParseRequest("small=5\n", Kinds));
        CHECK(!chests::ParseRequest("slot=one\nsmall=5\n", Kinds));
        CHECK(!chests::ParseRequest("slot=-1\nsmall=5\n", Kinds));

        // A count past the limit is the limit: a file cannot ask for more than the pile can hold.
        request = chests::ParseRequest("slot=1\nbig=100000000000\n", Kinds);
        CHECK(request && request->counts[1] == chests::PileLimit);

        const chests::Request asked{ 1, { 2, 0, 7 } };
        CHECK(chests::FormatRequest(asked, Kinds) == "slot=1\nsmall=2\nbig=0\nhuge=7\n");
        CHECK(chests::ParseRequest(chests::FormatRequest(asked, Kinds), Kinds) == asked);

        const chests::Status status{ 0, { 1, 2, 3 }, 4 };
        CHECK(status.Total() == 10);
        const std::string text = chests::FormatStatus(status, Kinds);
        CHECK(text == "slot=0\npile=10\npile.small=1\npile.big=2\npile.huge=3\npile.other=4\nlimit=99\n");
        CHECK(chests::ParseStatus(text, Kinds) == status);
        CHECK(!chests::ParseStatus("pile=3\n", Kinds));
    }

    void RoomChecks()
    {
        CHECK(chests::RoomLeft(4, 0) == 95);
        CHECK(chests::RoomLeft(4, 10) == 85);
        CHECK(chests::RoomLeft(90, 20) == 0);
        CHECK(chests::RoomLeft(120, 0) == 0);

        // In the kinds' order, each kind cut at what is left.
        const chests::Request asked{ 0, { 5, 20, 0 } };
        CHECK(chests::Plan(asked, 0) == (std::vector<int>{ 5, 20, 0 }));
        CHECK(chests::Plan(asked, 80) == (std::vector<int>{ 5, 14, 0 }));
        CHECK(chests::Plan(asked, 99) == (std::vector<int>{ 0, 0, 0 }));
        CHECK(chests::Plan(asked, 150) == (std::vector<int>{ 0, 0, 0 }));
        // The pile grew in the meantime (chests earned in a level): the limit still holds.
        const chests::Request full{ 0, { 50, 49, 0 } };
        CHECK(chests::Plan(full, 3) == (std::vector<int>{ 50, 46, 0 }));
    }

    void FolderChecks(const fs::path& folder)
    {
        std::error_code error;
        fs::remove_all(folder, error);
        chests::Folder files(folder);
        const fs::path request = folder / "chests_add.txt", status = folder / "chests.txt";

        // Nothing waiting.
        CHECK(!files.Claim(0, Kinds));

        // The status: written, and not rewritten while it does not change.
        const chests::Status pile{ 1, { 2, 0, 1 }, 0 };
        CHECK(files.WriteStatus(pile, Kinds));
        CHECK(savebackup::ReadFile(status) == chests::FormatStatus(pile, Kinds));
        CHECK(!fs::exists(savebackup::TempPath(status)));
        CHECK(!files.WriteStatus(pile, Kinds));
        CHECK(files.WriteStatus({ 1, { 3, 0, 1 }, 0 }, Kinds));

        // A request for another slot waits, untouched.
        WriteText(request, "slot=2\nsmall=3\n");
        CHECK(!files.Claim(1, Kinds));
        CHECK(savebackup::ReadFile(request) == "slot=2\nsmall=3\n");

        // The right slot: claimed once, and the file is gone.
        auto claimed = files.Claim(2, Kinds);
        CHECK(claimed && claimed->slot == 2 && claimed->counts == (std::vector<int>{ 3, 0, 0 }));
        CHECK(!fs::exists(request) && !fs::exists(folder / "chests_add.claimed"));
        CHECK(!files.Claim(2, Kinds));

        // Cancelled (the launcher deleted the file): nothing to claim.
        WriteText(request, "slot=1\nbig=2\n");
        fs::remove(request, error);
        CHECK(!files.Claim(1, Kinds));

        // A file that is not a request is left for the launcher to replace.
        WriteText(request, "nonsense");
        CHECK(!files.Claim(1, Kinds));
        CHECK(fs::exists(request));
        fs::remove_all(folder, error);

        // No folder (no player folder known): nothing written, nothing claimed.
        chests::Folder none{ fs::path() };
        CHECK(!none.WriteStatus(pile, Kinds));
        CHECK(!none.Claim(1, Kinds));
    }
}

int main()
{
    ParseChecks();
    RoomChecks();
    FolderChecks(fs::current_path() / "chests-test-scratch");
    printf("ArkeyanChestsTest: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
