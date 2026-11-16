// Unit test of the picture lookups' memory of the portraits folders (icons.h): a folder is read once, a picture written
// here is found at once, one written by another process within the folder's next look, a deleted one never. Synthetic
// pictures in a scratch folder only.
//   ArkeyanPictureLookupTest        Exit code 0 = pass.
#include "host/collection/catalogue.h"
#include "host/collection/icons.h"
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace collection;

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

    // The C runtime's own environment, which skyenv reads (on Windows SDL's setter does not reach it). One thread.
    void SetEnv(const char* name, const std::string& value)
    {
#ifdef _WIN32
        _putenv_s(name, value.c_str());
#else
        setenv(name, value.c_str(), 1); // NOLINT(concurrency-mt-unsafe)
#endif
    }

    std::string FileOf(const std::string& path) { return fs::path(path).filename().string(); }

    void TestLookups(const fs::path& folder)
    {
        fs::create_directories(folder / "artworks");
        SetEnv("SKY_PORTRAITS", folder.string());
        ForgetPictureFolders();
        const Toy* eruptor = Catalogue::Get().Find(9);
        const Toy* jetVac = Catalogue::Get().Find(100);
        const Toy* top = Catalogue::Get().Find(2004);
        CHECK(eruptor != nullptr && jetVac != nullptr && top != nullptr);
        if (eruptor == nullptr || jetVac == nullptr || top == nullptr)
            return;
        const Image square{ 4, 4, std::vector<uint32_t>(16, 0xFF336699u) };
        CHECK(SavePng(square, (folder / "eruptor.png").string()));
        const std::string found = PortraitPath("ssa", *eruptor);
        CHECK(FileOf(found) == "eruptor.png");

        // Asked again and again (every visible card, every frame): no folder read again.
        const size_t listed = PictureFolderListings();
        for (int i = 0; i < 100; i++)
        {
            CHECK(PortraitPath("ssa", *eruptor) == found);
            CHECK(BestPicturePath("ssa", *eruptor) == found);
            CHECK(HalfPicturePath(*top).empty()); // no file at all: every candidate name of every game
        }
        CHECK(PictureFolderListings() == listed);

        // Written by this process: found at once.
        CHECK(SavePng(square, (folder / "eruptor_lightcore.png").string()));
        CHECK(FileOf(PortraitPath("ssa", *eruptor, 0x1206)) == "eruptor_lightcore.png");

        // Deleted: never handed out, the next name is found instead.
        fs::remove(folder / "eruptor_lightcore.png");
        CHECK(FileOf(PortraitPath("ssa", *eruptor, 0x1206)) == "eruptor.png");

        // Written by another process (here: not through the library): found once the folder is looked at again.
        fs::copy_file(folder / "eruptor.png", folder / "eagle.png");
        bool seen = false;
        for (uint64_t start = SDL_GetTicks(); !seen && SDL_GetTicks() - start < 5000;)
        {
            seen = FileOf(PortraitPath("ssa", *jetVac)) == "eagle.png";
            if (!seen)
                SDL_Delay(50);
        }
        CHECK(seen);

        // Any case, as a stat answered on macOS and Windows; the path names the file as it is.
        fs::remove(folder / "eruptor.png");
        CHECK(SavePng(square, (folder / "ToY_ErUpToR_HuD.png").string()));
        CHECK(FileOf(PortraitPath("ssa", *eruptor)) == "ToY_ErUpToR_HuD.png");

        // Another data root: its own folders.
        const fs::path other = folder.parent_path() / "other";
        fs::create_directories(other);
        SetEnv("SKY_PORTRAITS", other.string());
        CHECK(PortraitPath("ssa", *eruptor).empty());
        SetEnv("SKY_PORTRAITS", folder.string());
        CHECK(FileOf(PortraitPath("ssa", *eruptor)) == "ToY_ErUpToR_HuD.png");
    }
}

int main()
{
    // A scratch folder: SKYLANDERS_DATA when the harness sets one, else the temporary folder.
    const char* data = SDL_getenv_unsafe("SKYLANDERS_DATA");
    const fs::path scratch =
        fs::path(data != nullptr && *data ? data : fs::temp_directory_path().string()) / "ArkeyanPictureLookupTest";
    std::error_code error;
    fs::remove_all(scratch, error);
    TestLookups(scratch / "portraits");
    fs::remove_all(scratch, error);
    printf("picture lookup test: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
