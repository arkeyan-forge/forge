// Unit test of the picture lookups (icons.h): the memory of the portraits folders (a folder is read once, a picture
// written here is found at once, one written by another process within the folder's next look, a deleted one never), and
// which game's picture a figure shows (its original game by default, the player's choices, own pictures). Synthetic
// pictures in a scratch folder only.
//   ArkeyanPictureLookupTest        Exit code 0 = pass.
#include "host/collection/catalogue.h"
#include "host/collection/collection.h"
#include "host/collection/datadir.h"
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

    void UnsetEnv(const char* name)
    {
#ifdef _WIN32
        _putenv_s(name, "");
#else
        unsetenv(name); // NOLINT(concurrency-mt-unsafe)
#endif
    }

    std::string FileOf(const std::string& path) { return fs::path(path).filename().string(); }

    // "<game id>/<file>" for a picture in a game's portraits folder: which game the rule picked.
    std::string GameFileOf(const std::string& path)
    {
        if (path.empty())
            return {};
        for (const Game& game : Catalogue::Get().Games())
            if (fs::path(path).parent_path() == datadir::Game(game.id) / "portraits")
                return game.id + "/" + FileOf(path);
        return "?/" + FileOf(path);
    }

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

        // Asked again and again (every visible card, every frame): no folder read again once each was read.
        CHECK(BestPicturePath("ssa", *eruptor) == found && HalfPicturePath(*top).empty());
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

    void TestOrigins()
    {
        const Catalogue& catalogue = Catalogue::Get();
        const Toy* spyro = catalogue.Find(16);
        CHECK(spyro != nullptr && spyro->game == "ssa");
        if (spyro == nullptr)
            return;
        auto originOf = [&](uint16_t variant) {
            const Game* game = catalogue.Origin(*spyro, variant);
            return game != nullptr ? game->id : std::string();
        };
        CHECK(originOf(0) == "ssa");
        CHECK(originOf(0x1801) == "giants"); // Series 2
        CHECK(originOf(0x2C02) == "swapforce"); // Dark Mega Ram
        CHECK(originOf(0x3810) == "trapteam"); // Eon's Elite
        // No variant of the catalogue is older than its toy's own game.
        for (const Toy& toy : catalogue.Toys())
        {
            const Game* own = catalogue.FindGame(toy.game);
            CHECK(own != nullptr);
            for (const Variant& variant : toy.variants)
                if (own != nullptr && catalogue.Origin(toy, variant.id) != nullptr)
                    CHECK(catalogue.Origin(toy, variant.id)->year >= own->year);
        }
    }

    // Spyro (Spyro's Adventure) and Series 2 Spyro (Giants) with pictures cached by several games.
    void TestChoices(const fs::path& data)
    {
        UnsetEnv("SKY_PORTRAITS");
        SetEnv("SKYLANDERS_DATA", data.string());
        ForgetPictureFolders();
        const Toy* spyro = Catalogue::Get().Find(16);
        CHECK(spyro != nullptr);
        if (spyro == nullptr)
            return;
        const Image square{ 4, 4, std::vector<uint32_t>(16, 0xFF336699u) };
        auto put = [&](const char* game, const char* name) {
            fs::path folder = datadir::Game(game) / "portraits";
            fs::create_directories(folder);
            CHECK(SavePng(square, (folder / (std::string(name) + ".png")).string()));
        };
        put("ssa", "toy_spyrojr_hud");
        put("superchargers", "collection_spyro_normal");
        put("superchargers", "collection_spyro_reposed"); // Series 2

        // The original game's picture, not the newest one's.
        CHECK(GameFileOf(PortraitPath("ssa", *spyro)) == "ssa/toy_spyrojr_hud.png");
        CHECK(GameFileOf(BestPicturePath("ssa", *spyro)) == "ssa/toy_spyrojr_hud.png");
        // Series 2 came with Giants: without Giants, the first game after it (its variant picture).
        CHECK(GameFileOf(PortraitPath("ssa", *spyro, 0x1801)) == "superchargers/collection_spyro_reposed.png");
        // A variant's own picture from a later game over the original game's plain one.
        put("giants", "spyrojr");
        CHECK(GameFileOf(PortraitPath("ssa", *spyro, 0x1801)) == "superchargers/collection_spyro_reposed.png");
        put("giants", "spyro_series2");
        CHECK(GameFileOf(PortraitPath("ssa", *spyro, 0x1801)) == "giants/spyro_series2.png");
        CHECK((PictureGames(*spyro, 0) == std::vector<std::string>{ "ssa", "giants", "superchargers" }));

        // The choice for every figure.
        CHECK(PicturesDefault() == PicturesOrigin);
        uint64_t generation = PictureGeneration();
        ForgetPictureFolders(); // dropping the caches is no change (the launcher forgets the folders as it drops them)
        CHECK(PictureGeneration() == generation);
        CHECK(SetPicturesDefault(PicturesNewest));
        CHECK(PictureGeneration() != generation);
        CHECK(PicturesDefault() == PicturesNewest);
        CHECK(GameFileOf(PortraitPath("ssa", *spyro)) == "superchargers/collection_spyro_normal.png");
        CHECK(GameFileOf(PortraitPath("ssa", *spyro, 0x1801)) == "superchargers/collection_spyro_reposed.png");
        CHECK(SetPicturesDefault("giants"));
        CHECK(GameFileOf(PortraitPath("ssa", *spyro)) == "giants/spyrojr.png");
        CHECK(SetPicturesDefault("trapteam")); // no picture there: the original game's
        CHECK(GameFileOf(PortraitPath("ssa", *spyro)) == "ssa/toy_spyrojr_hud.png");
        CHECK(SetPicturesDefault(PicturesOrigin));
        CHECK(GameFileOf(PortraitPath("ssa", *spyro)) == "ssa/toy_spyrojr_hud.png");

        // One card's choice: Spyro and Series 2 Spyro apart.
        CHECK(SetPictureChoice(16, 0, "superchargers"));
        CHECK(PictureChoice(16, 0) == "superchargers");
        CHECK(GameFileOf(PortraitPath("ssa", *spyro)) == "superchargers/collection_spyro_normal.png");
        CHECK(GameFileOf(PortraitPath("ssa", *spyro, 0x1801)) == "giants/spyro_series2.png");
        CHECK(SetPictureChoice(16, 0, "trapteam")); // no picture there: the default rule
        CHECK(GameFileOf(PortraitPath("ssa", *spyro)) == "ssa/toy_spyrojr_hud.png");
        CHECK(SetPictureChoice(16, 0x1801, "ssa")); // the chosen game's plain picture over another's variant one
        CHECK(GameFileOf(PortraitPath("ssa", *spyro, 0x1801)) == "ssa/toy_spyrojr_hud.png");
        CHECK(SetPictureChoice(16, 0x1801, ""));
        CHECK(PictureChoice(16, 0x1801).empty());
        CHECK(!SetPictureChoice(16, 0, PictureOwn)); // only with a picture

        // The player's own picture: over everything, square, gone once removed.
        const Image wide{ 600, 300, std::vector<uint32_t>(size_t(600) * 300, 0xFF00FF00u) };
        generation = PictureGeneration();
        std::string why;
        CHECK(SetOwnPicture(16, 0, wide, &why));
        CHECK(why.empty());
        CHECK(PictureGeneration() != generation);
        CHECK(PictureChoice(16, 0) == PictureOwn);
        const std::string own = OwnPicturePath(16, 0);
        CHECK(!own.empty() && PortraitPath("ssa", *spyro) == own && BestPicturePath("ssa", *spyro) == own);
        Image saved = LoadPng(own);
        CHECK(saved.width == OwnPictureSize && saved.height == OwnPictureSize);
        CHECK(!saved.Empty() && saved.rgba[0] >> 24 == 0 &&
              saved.rgba[size_t(256) * OwnPictureSize + 256] == 0xFF00FF00u);
        CHECK(GameFileOf(PortraitPath("ssa", *spyro, 0x1801)) == "giants/spyro_series2.png"); // that card only
        CHECK(!SetOwnPicture(16, 0x1801, Image{}, &why) && !why.empty());
        CHECK(RemoveOwnPicture(16, 0));
        CHECK(OwnPicturePath(16, 0).empty() && !fs::exists(own));
        CHECK(PictureChoice(16, 0).empty()); // back to the default
        CHECK(SetPictureChoice(16, 0, ""));
        CHECK(GameFileOf(PortraitPath("ssa", *spyro)) == "ssa/toy_spyrojr_hud.png");

        // Written by another process (the launcher while a game runs): seen within the next look.
        const fs::path settings = fs::path(datadir::FromText(collection::Folder())) / "pictures" / "pictures.json";
        generation = PictureGeneration();
        {
            FILE* file = fopen(settings.string().c_str(), "wb");
            CHECK(file != nullptr);
            if (file != nullptr)
            {
                fputs(R"({"format": 1, "revision": 99, "default": "newest", "cards": {}})", file);
                fclose(file);
            }
        }
        bool seen = false;
        for (uint64_t start = SDL_GetTicks(); !seen && SDL_GetTicks() - start < 5000;)
        {
            seen = PictureGeneration() != generation;
            if (!seen)
                SDL_Delay(50);
        }
        CHECK(seen);
        CHECK(GameFileOf(PortraitPath("ssa", *spyro)) == "superchargers/collection_spyro_normal.png");
    }

    // A Magic Trap holding Chompy Mage (storage id 1, villain 1001): the trap's picture with the villain's at its bottom,
    // centred; nothing for an empty trap, a villain with no picture, another toy or a card with its own picture.
    void TestTrapPicture(const fs::path& data)
    {
        SetEnv("SKYLANDERS_DATA", data.string());
        ForgetPictureFolders();
        const Toy* trap = Catalogue::Get().Find(210);
        const Toy* eruptor = Catalogue::Get().Find(9);
        CHECK(trap != nullptr && trap->category == Category::Trap && eruptor != nullptr);
        if (trap == nullptr || eruptor == nullptr)
            return;
        const fs::path folder = datadir::Game("trapteam") / "portraits";
        fs::create_directories(folder);
        const std::string path = (folder / "210_12290.png").string();
        CHECK(SavePng(Image{ 100, 100, std::vector<uint32_t>(size_t(100) * 100, 0xFFFF0000u) }, path));
        // A red villain on a transparent margin: only its visible part is fitted.
        Image villain{ 60, 60, std::vector<uint32_t>(size_t(60) * 60, 0) };
        for (int y = 10; y < 50; y++)
            for (int x = 10; x < 50; x++)
                villain.rgba[size_t(y) * 60 + x] = 0xFF0000FFu;
        CHECK(SavePng(villain, (folder / "1001_villaincaptured.png").string()));
        ForgetPictureFolders();

        CHECK(TrapPicture(*trap, 0x3002, 0, path).Empty());
        CHECK(TrapPicture(*trap, 0x3002, 2, path).Empty()); // Dr Krankcase: no picture cached
        CHECK(TrapPicture(*eruptor, 0, 1, path).Empty());
        CHECK(TrapPicture(*trap, 0x3002, 1, {}).Empty());
        const Image held = TrapPicture(*trap, 0x3002, 1, path);
        CHECK(held.width == 100 && held.height == 100);
        if (held.width == 100 && held.height == 100)
        {
            auto at = [&](int x, int y) { return held.rgba[size_t(y) * 100 + x]; };
            CHECK(at(50, 76) == 0xFF0000FFu && at(36, 76) == 0xFF0000FFu && at(63, 76) == 0xFF0000FFu); // the villain
            CHECK(at(20, 20) == 0xFFFF0000u && at(50, 97) == 0xFFFF0000u && at(50, 50) == 0xFFFF0000u);
            // Inside the circle the Toy Box crops its portraits to.
            for (int y = 0; y < 100; y++)
                for (int x = 0; x < 100; x++)
                    if (at(x, y) != 0xFFFF0000u)
                        CHECK((x - 49.5f) * (x - 49.5f) + (y - 49.5f) * (y - 49.5f) < 50.0f * 50.0f);
        }

        // The player's own picture stays as it is.
        CHECK(SetOwnPicture(210, 0x3002, Image{ 8, 8, std::vector<uint32_t>(64, 0xFF00FF00u) }));
        CHECK(TrapPicture(*trap, 0x3002, 1, path).Empty());
        CHECK(RemoveOwnPicture(210, 0x3002));
        CHECK(!TrapPicture(*trap, 0x3002, 1, path).Empty());
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
    SetEnv("SKY_COLLECTION", (scratch / "collection").string()); // before the catalogue reads the folder's own
    TestLookups(scratch / "portraits");
    TestOrigins();
    TestChoices(scratch / "data");
    TestTrapPicture(scratch / "data");
    fs::remove_all(scratch, error);
    printf("picture lookup test: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
