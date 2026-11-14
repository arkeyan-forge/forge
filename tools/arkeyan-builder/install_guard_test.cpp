// Real install steps with invented inputs. Every child call is intercepted; no program or game is launched.
#define Run GuardRun
#define RunStartError GuardStartError
#define main SkyBuildMain
#include "main.cpp"
#undef main
#undef RunStartError
#undef Run
int Run(const std::vector<std::string>& args, const RunOptions& options); // process.cpp's, for the real compiler
#ifdef _WIN32
#include <io.h>
#define dup _dup
#define dup2 _dup2
#define close _close
#define fileno _fileno
#else
#include <unistd.h>
#endif

static int checks = 0, failures = 0;
#define CHECK(condition)                                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        ++checks;                                                                                                      \
        if (!(condition))                                                                                              \
        {                                                                                                              \
            ++failures;                                                                                                \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition);                                               \
        }                                                                                                              \
    } while (0)

static std::function<int(const std::vector<std::string>&, const RunOptions&)> child;
static unsigned long startError = 0;
int GuardRun(const std::vector<std::string>& args, const RunOptions& options)
{
    startError = 0;
    if (!child)
        throw std::runtime_error("unexpected child request");
    return child(args, options);
}
unsigned long GuardStartError() { return startError; }

static void Put(const fs::path& path, const std::string& text)
{
    fs::create_directories(path.parent_path());
    WriteText(path, text);
}

static void Container(const fs::path& path)
{
    std::string bytes(0x48, '\0');
    bytes[0] = 0x10;
    bytes[1] = 0x2A;
    bytes[2] = 0x11;
    bytes[3] = path.extension() == ".vs" ? 1 : 0;
    bytes[7] = 0x40;
    bytes[11] = 8;
    Put(path, bytes);
}

static std::string Failed(const std::function<void()>& action)
{
    try
    {
        action();
    }
    catch (const Failure& failure)
    {
        return failure.message;
    }
    CHECK(false);
    return {};
}

static std::pair<int, std::string> CaptureMain(std::vector<std::string> args)
{
    std::vector<char*> argv;
    for (auto& arg : args)
        argv.push_back(arg.data());
    FILE* capture = tmpfile();
    if (!capture)
        throw std::runtime_error("cannot capture test output");
    fflush(stdout);
    int saved = dup(fileno(stdout));
    if (saved < 0 || dup2(fileno(capture), fileno(stdout)) < 0)
        throw std::runtime_error("cannot redirect test output");
    int status = SkyBuildMain(int(argv.size()), argv.data());
    fflush(stdout);
    dup2(saved, fileno(stdout));
    close(saved);
    rewind(capture);
    std::string text;
    char buffer[4096];
    for (size_t n; (n = fread(buffer, 1, sizeof(buffer), capture)) != 0;)
        text.append(buffer, n);
    fclose(capture);
    return { status, text };
}

static Build Fixture(const fs::path& root)
{
    Build build;
    build.options.out = root / "output";
    build.options.kit = root / "kit";
    build.options.iso = root / "disc";
    build.work = build.options.out / "ssabuild-work";
    build.game = ReadGame("synthetic", { { "runtime", "SyntheticRuntime" }, { "movie_shaders", "2" } });
    build.platform = "windows-x64";
    fs::create_directories(build.work / "logs");
    fs::create_directories(build.work / "ppc");
    Put(build.options.iso / "gamedata.manifest", "synthetic\n");
    Put(build.options.iso / "default.xex", "invented bytes, no executable code\n");
    Put(build.options.kit / "bin" / (std::string("XenosRecomp") + Exe), "synthetic translator\n");
    Put(build.options.kit / "share" / "shader_common.h", "synthetic common\n");
    Put(build.work / "shaders/input/disc.ps", "synthetic disc input\n");
    build.exe = build.options.out / "SyntheticRuntime";
    return build;
}

static void TestInventory(Build& build)
{
    fs::path input = build.work / "shaders/input";
    std::string inventory = DiscShaderInventory(input);
    CHECK(!inventory.empty());
    Container(input / "runtime/0000000000000001.ps");
    CHECK(DiscShaderInventory(input) == inventory);
    CHECK(RuntimeShaders(input / "runtime").size() == 1);
    Put(input / "runtime/0000000000000002.ps", "partial");
    Container(input / "runtime/wrong.ps");
    Container(input / "runtime/0000000000000003.vs");
    fs::resize_file(input / "runtime/0000000000000003.vs", 12);
    CHECK(RuntimeShaders(input / "runtime").size() == 1);
    Put(input / "disc.ps", "changed\n");
    CHECK(DiscShaderInventory(input) != inventory);
    fs::remove(input / "disc.ps");
    CHECK(DiscShaderInventory(input).empty());
    fs::remove_all(input / "runtime");
    Put(input / "disc.ps", "restored\n");
}

static void TestTranslation(Build& build)
{
    fs::path runtime = build.work / "shaders/input/runtime";
    Container(runtime / "0000000000000001.ps");
    child = [&](const auto&, const RunOptions&) {
        startError = 225;
        return -1;
    };
    std::string error = Failed([&] { TranslateShaders(build); });
    CHECK(error.find("Windows Security blocked XenosRecomp") != std::string::npos);
    CHECK(!fs::exists(build.work / "shaders/stamp.txt"));
    // A successful translator may still have dropped the required shader.
    child = [&](const auto&, const RunOptions& run) {
        Put(run.cwd / "shaders/shader_cache.cpp", "\t{ 0x2, 0 },\n");
        Put(run.log, "Skipped 0000000000000001\nNo DXIL for 0000000000000002\n");
        return 0;
    };
    CHECK(Failed([&] { TranslateShaders(build); }).find("was not translated") != std::string::npos);
    CHECK(!fs::exists(build.work / "shaders/stamp.txt"));
    child = [&](const auto&, const RunOptions& run) {
        Put(run.cwd / "shaders/shader_cache.cpp", "\t{ 0x1, 0 },\n");
        Put(run.log, "Skipped 0000000000000002\nNo DXIL for 0000000000000001\n");
        return 0;
    };
    TranslateShaders(build);
    CHECK(fs::exists(build.work / "shaders/stamp.txt"));
    child = {};
    TranslateShaders(build); // cached install still verifies the retained shader
    Put(build.work / "ppc/shader_cache.cpp", "\t{ 0x2, 0 },\n");
    CHECK(Failed([&] { TranslateShaders(build); }).find("was not translated") != std::string::npos);
    fs::remove_all(runtime);
    fs::remove(build.work / "shaders/stamp.txt");
    child = [&](const auto&, const RunOptions& run) {
        Put(run.cwd / "shaders/shader_cache.cpp", "// no entries\n");
        return 0;
    };
    CHECK(Failed([&] { TranslateShaders(build); }).find("no translated shaders") != std::string::npos);
}

static void TestMovies(Build& build)
{
    fs::path runtime = build.work / "shaders/input/runtime";
    fs::path stamp = build.work / "movies/done.txt";
    Put(build.work / "ppc/shader_cache.cpp", "\t{ 0x99, 0 },\n");
    Put(build.work / "shaders/skipped.txt", "");
    Put(stamp, "1\n");
    child = [&](const auto&, const RunOptions& run) {
        Container(run.cwd / "dump/0000000000000001.ps");
        CHECK(!run.poll());
        return -2;
    };
    CHECK(Failed([&] { AddMovieShaders(build); }).find("1 of 2") != std::string::npos);
    CHECK(RuntimeShaders(runtime).size() == 1);
    CHECK(!fs::exists(stamp));
    // Resume keeps the old container; the poll target includes it, and a duplicate contributes nothing.
    SetEnv("SKY_PRESS", "test-previous");
    child = [&](const auto&, const RunOptions& run) {
        CHECK(GetEnv("SKY_PRESS").empty());
        Container(run.cwd / "dump/0000000000000001.ps");
        CHECK(!run.poll());
        Container(run.cwd / "dump/0000000000000002.vs");
        CHECK(run.poll());
        return -3;
    };
    CHECK(AddMovieShaders(build));
    CHECK(GetEnv("SKY_PRESS") == "test-previous");
    SetEnv("SKY_PRESS", "");
    CHECK(RuntimeShaders(runtime).size() == 2);
    CHECK(!fs::exists(stamp)); // capture alone cannot commit completion
    child = {};
    CHECK(!AddMovieShaders(build)); // complete capture retries the rebuild without another child
    Put(stamp, "2\n");
    CHECK(!AddMovieShaders(build));
    fs::remove(runtime / "0000000000000002.vs");
    child = [&](const auto&, const RunOptions&) {
        startError = 226;
        return -1;
    };
    std::string error = Failed([&] { AddMovieShaders(build); });
    CHECK(error.find("install incomplete") != std::string::npos);
    CHECK(error.find("Windows Security blocked SyntheticRuntime") != std::string::npos);
    CHECK(!fs::exists(stamp));
    build.game.movieShaders = 0;
    CHECK(Failed([&] { AddMovieShaders(build); }).find("install incomplete") != std::string::npos);
    build.game.movieShaders = 2;
    build.cross = true;
    CHECK(Failed([&] { AddMovieShaders(build); }).find("native kit") != std::string::npos);
    build.cross = false;
}

static void TestMainFailure(Build& build)
{
    // Let main use cached synthetic disc inputs, then fake code translation and a blocked shader tool.
    fs::remove_all(build.work / "shaders/input/runtime");
    fs::remove(build.work / "shaders/stamp.txt");
    const std::string xex = ReadText(build.options.iso / "default.xex");
#ifdef _WIN32
    const std::string platform = "windows-x64";
#elif defined(__APPLE__)
    const std::string platform = "macos-arm64";
#else
    const std::string platform = "linux-x64";
#endif
    Put(build.options.kit / "kit.txt", "games=synthetic\nplatform=" + platform + "\nzig=0.17.0\n");
    Put(build.options.kit / "config/synthetic/game.txt",
        "runtime=SyntheticRuntime\nxex_sha256=" + Sha256Text(xex) + "\nmovie_shaders=2\ntoml=synthetic.toml\n");
    Put(build.options.kit / "config/synthetic/synthetic.toml", "synthetic config\n");
    Put(build.options.kit / "bin" / (std::string("XenonRecomp") + Exe), "synthetic translator\n");
    Put(build.options.kit / "share/ppc_context.h", "synthetic context\n");
    build.kit = ReadKeyValues(build.options.kit / "kit.txt");
    const std::string header = "synthetic header\n";
    Put(build.options.kit / "kit.txt", ReadText(build.options.kit / "kit.txt") + "ppc_config_sha256.synthetic=" +
                                           Sha256Text(header) + "\nppc_context_sha256=" + Sha256Text(header) + "\n");
    Put(build.work / "disc/default.xex", xex);
    Put(build.work / "disc/stamp.txt", DiscStamp(build));
    Put(build.work / "disc/shaders.txt", DiscShaderInventory(build.work / "shaders/input"));
    fs::path toolchain = build.options.out / "toolchain";
#if defined(_WIN32) || defined(__linux__)
    Put(toolchain / Compiler.compiler, "synthetic compiler\n");
#endif
    child = [&](const std::vector<std::string>& args, const RunOptions& run) {
        if (args[0] == "xcrun")
        {
            Put(run.log, "synthetic SDK\n");
            return 0;
        }
        if (fs::path(args[0]).stem() == "XenonRecomp")
        {
            Put(run.cwd / "ppc/ppc_config.h", header);
            Put(run.cwd / "ppc/ppc_context.h", header);
            return 0;
        }
        CHECK(fs::path(args[0]).stem() == "XenosRecomp");
        startError = 225;
        return -1;
    };
    auto [status, output] =
        CaptureMain({ "Arkeyan-Builder", "--iso", Slashes(build.options.iso), "--out", Slashes(build.options.out),
            "--kit", Slashes(build.options.kit), "--toolchain", Slashes(toolchain), "--jobs", "2" });
    CHECK(status == 1);
    CHECK(output.find("@error XenosRecomp failed") != std::string::npos);
    CHECK(output.find("Windows Security blocked XenosRecomp") != std::string::npos);
    CHECK(output.find("@done") == std::string::npos);
    CHECK(!fs::exists(build.work / "movies/done.txt"));
}

// kit_format=: a legacy kit, format 2 with every ABI id and a record, refused otherwise.
static void TestKitFormat()
{
    Build build;
    const std::string id(64, 'a');
    build.kit = { { "games", "one,two" } };
    CheckKitFormat(build);
    build.kit["layout_record"] = "1";
    CHECK(Failed([&] { CheckKitFormat(build); }).find("no kit_format") != std::string::npos);
    build.kit = { { "games", "one,two" }, { "kit_format", "3" }, { "layout_record", "1" } };
    CHECK(Failed([&] { CheckKitFormat(build); }).find("format (3)") != std::string::npos);
    build.kit["kit_format"] = "2";
    build.kit.erase("layout_record");
    CHECK(Failed([&] { CheckKitFormat(build); }).find("no layout_record") != std::string::npos);
    build.kit["layout_record"] = "1";
    CHECK(Failed([&] { CheckKitFormat(build); }).find("abi.context") != std::string::npos);
    build.kit["abi.context"] = id;
    build.kit["abi.policy.one"] = id;
    CHECK(Failed([&] { CheckKitFormat(build); }).find("abi.policy.two") != std::string::npos);
    build.kit["abi.policy.two"] = "A" + id.substr(1);
    CHECK(Failed([&] { CheckKitFormat(build); }).find("abi.policy.two") != std::string::npos);
    build.kit["abi.policy.two"] = id;
    CheckKitFormat(build);
    // The policy lines tools/abi/headers.py compares: PPC_CONFIG_ definitions only, whole, in order.
    CHECK(PolicyLines("#pragma once\n#define PPC_CONFIG_H_INCLUDED\n # define\tPPC_CONFIG_SKIP_LR 1\r\n"
                      "#define PPC_CONFIG_x\n#define PPC_CONFIGX\n#definePPC_CONFIG_A\n#define PPC_IMAGE_BASE 0x1ull\n"
                      "//#define PPC_CONFIG_B\n#define PPC_CONFIG_CR_AS_LOCAL") ==
          "#define PPC_CONFIG_H_INCLUDED\n # define\tPPC_CONFIG_SKIP_LR 1\n#define PPC_CONFIG_CR_AS_LOCAL\n");
}

// A format-2 kit's translation: checked against the ABI ids, stamped with the headers' own hashes (a legacy kit's
// stamp of the same translation stays up to date), refused when a header does not carry the kit's ABI.
static void TestAbiTranslation(Build& build)
{
    const std::string config = "#pragma once\n#define PPC_CONFIG_CTR_AS_LOCAL\n#define PPC_IMAGE_BASE 0x82000000ull\n";
    const std::string context = "synthetic public context\n";
    int translations = 0;
    std::string written = config;
    child = [&](const std::vector<std::string>& args, const RunOptions& run) {
        CHECK(fs::path(args[0]).stem() == "XenonRecomp");
        translations++;
        Put(run.cwd / "ppc/ppc_config.h", written);
        Put(run.cwd / "ppc/ppc_context.h", context);
        return 0;
    };
    fs::remove_all(build.work / "ppc");
    build.kit = { { "games", "synthetic" }, { "ppc_config_sha256.synthetic", Sha256Text(config) },
        { "ppc_context_sha256", Sha256Text(context) } };
    TranslateCode(build);
    CHECK(translations == 1);
    const std::string legacy = ReadText(build.work / "ppc/stamp.txt");

    build.kit = { { "games", "synthetic" }, { "kit_format", "2" }, { "layout_record", "1" },
        { "abi.context", Sha256Text(context) },
        { "abi.policy.synthetic", Sha256Text("#define PPC_CONFIG_CTR_AS_LOCAL\n") } };
    TranslateCode(build);
    CHECK(translations == 1);
    CHECK(ReadText(build.work / "ppc/stamp.txt") == legacy);

    // Another policy: retranslated, refused, nothing stamped.
    build.kit["abi.policy.synthetic"] = Sha256Text("#define PPC_CONFIG_SKIP_LR\n");
    CHECK(Failed([&] { TranslateCode(build); }).find("ppc_config.h differs") != std::string::npos);
    CHECK(translations == 2);
    build.kit["abi.policy.synthetic"] = Sha256Text("#define PPC_CONFIG_CTR_AS_LOCAL\n");
    build.kit["abi.context"] = Sha256Text("another context\n");
    CHECK(Failed([&] { TranslateCode(build); }).find("ppc_context.h differs") != std::string::npos);
    build.kit["abi.context"] = Sha256Text(context);
    // Other bounds, the same policy: accepted here (the record step checks the bounds), a new stamp.
    written = "#pragma once\n#define PPC_CONFIG_CTR_AS_LOCAL\n#define PPC_IMAGE_BASE 0x83000000ull\n";
    fs::remove(build.work / "ppc/stamp.txt");
    TranslateCode(build);
    CHECK(translations == 4);
    CHECK(ReadText(build.work / "ppc/ppc_config.h") == written);
    CHECK(ReadText(build.work / "ppc/stamp.txt") != legacy);
    TranslateCode(build);
    CHECK(translations == 4);
    child = {};
}

static void TestExecutableResources(Build& build)
{
    build.vars["CXX"] = Slashes(build.options.kit / "compiler/bin/clang++");
    build.kit["game_version." + build.game.id] = "1.2.3";
    // Cache invented icon files so this check reads no disc artwork and only exercises resource compilation.
    for (const char* extension : { "png", "ico", "icns" })
        Put(build.options.out / (std::string("game_icon.") + extension), "synthetic icon\n");
    Put(build.work / "icon/game_icon.key", DiscStamp(build) + "recipe=2\n");
    fs::path object = build.work / "icon/game_icon.o";
    Put(object, "old resource\n");
    int calls = 0;
    child = [&](const std::vector<std::string>& args, const RunOptions& run) {
        CHECK(fs::path(args[0]).stem().string().find("windres") != std::string::npos);
        ++calls;
        if (calls == 1)
        {
            Put(run.cwd / "game_icon.o", "partial resource\n");
            return 1;
        }
        if (calls == 3)
            Put(run.cwd / "game_icon.o", "complete resource\n");
        return 0;
    };
    CHECK(Failed([&] { ExecutableResources(build); }).find("executable resources") != std::string::npos);
    CHECK(!fs::exists(object));
    // Success without producing an object is also an error; neither failure may become a cached success.
    CHECK(Failed([&] { ExecutableResources(build); }).find("executable resources") != std::string::npos);
    CHECK(!fs::exists(object));
    CHECK(ExecutableResources(build) == "icon/game_icon.o");
    CHECK(ExecutableResources(build) == "icon/game_icon.o");
    CHECK(calls == 3);
}

// An invented executable: data, code, a data gap, code. Code bounds: the envelope of the two code sections.
static GuestLayout InventedLayout()
{
    const GuestSection sections[] = { { 0x82000000, 0x10000, false }, { 0x82010000, 0x40000, true },
        { 0x82050000, 0x10000, false }, { 0x82060000, 0xB0000, true } };
    return ParsedGuestLayout(0x82000000, 0x200000, sections);
}

static std::string GeneratedConfig(const GuestLayout& layout)
{
    char text[256];
    snprintf(text, sizeof(text),
        "#pragma once\n#define PPC_CONFIG_SKIP_LR\n\n#define PPC_IMAGE_BASE 0x%Xull\n#define PPC_IMAGE_SIZE 0x%Xull\n"
        "#define PPC_CODE_BASE 0x%Xull\n#define PPC_CODE_SIZE 0x%Xull\n\n",
        layout.imageBase, layout.imageSize, layout.codeBase, layout.codeSize);
    return text;
}

// The record the builder writes, compiled and linked by the real compiler with the runtime's checks: accepted for its
// image, refused for another image of the same layout; no record or two records do not link.
static void CompileLayoutRecord(const Build& build, const std::string& imageSha256)
{
    std::string compiler = GetEnv("SKY_ACCENTS_COMPILER");
    if (compiler.empty())
        compiler = SKY_ACCENTS_COMPILER;
    if (compiler.empty())
    {
        printf("layout record: compiler execution unverified (cross build; set SKY_ACCENTS_COMPILER)\n");
        return;
    }
    fs::path dir = build.work / "layout";
    const GuestLayout layout = InventedLayout();
    Put(dir / "check.cpp",
        "#include \"guest_layout.h\"\n#include <cstdio>\n#include <cstdlib>\n"
        "int main(int, char** argv)\n{\n"
        "    const GuestLayout parsed{ uint32_t(strtoul(argv[1], 0, 16)), uint32_t(strtoul(argv[2], 0, 16)),\n"
        "        uint32_t(strtoul(argv[3], 0, 16)), uint32_t(strtoul(argv[4], 0, 16)) };\n"
        "    std::string problem = CheckGuestLayout(g_guestLayoutRecord, parsed, argv[5]);\n"
        "    fprintf(stderr, \"%s\\n\", problem.c_str());\n    return problem.empty() ? 0 : 1;\n}\n");
    RunOptions run;
    run.cwd = dir;
    run.log = dir / "compiler.log";
    auto compile = [&](std::vector<std::string> args) {
        args.insert(args.begin(), { compiler, "-std=c++20" });
#ifdef _WIN32
        args.push_back("-static");
#endif
        return Run(args, run);
    };
    const std::string runtime = SKY_RUNTIME_DIR "/guest_layout.cpp";
    const std::string program = Slashes(dir / "check.exe");
    CHECK(compile({ "-c", "guest_layout_record.cpp", "-o", "record.o" }) == 0);
    fs::copy_file(dir / "record.o", dir / "record2.o", fs::copy_options::overwrite_existing);
    CHECK(compile({ "check.cpp", runtime, "record.o", "-o", program }) == 0);
    char bounds[4][16];
    snprintf(bounds[0], 16, "%X", layout.imageBase);
    snprintf(bounds[1], 16, "%X", layout.imageSize);
    snprintf(bounds[2], 16, "%X", layout.codeBase);
    snprintf(bounds[3], 16, "%X", layout.codeSize);
    CHECK(Run({ program, bounds[0], bounds[1], bounds[2], bounds[3], imageSha256 }, run) == 0);
    CHECK(Run({ program, bounds[0], bounds[1], bounds[2], bounds[3], std::string(64, 'e') }, run) == 1);
    CHECK(ReadText(run.log).find("another executable") != std::string::npos);
    snprintf(bounds[3], 16, "%X", layout.codeSize - 0x10000);
    CHECK(Run({ program, bounds[0], bounds[1], bounds[2], bounds[3], imageSha256 }, run) == 1);
    fs::remove(program);
    CHECK(compile({ "check.cpp", runtime, "-o", program }) != 0); // no record
    CHECK(compile({ "check.cpp", runtime, "record.o", "record2.o", "-o", program }) != 0); // two records
    for (const char* name : { "check.cpp", "check.exe", "record.o", "record2.o", "compiler.log" })
        fs::remove(dir / name);
}

// A kit with layout_record=: the record checked and written, compiled with its own key, linked exactly once; a
// cancelled compile or a failed link keeps the previous game; an old kit links none.
static void TestLayoutRecord(Build& build)
{
    const GuestLayout layout = InventedLayout();
    const std::string sha = Sha256Text("invented image");
    build.platform = "linux-x64";
    build.options.jobs = 2;
    build.exe = build.options.out / "SyntheticRuntime";
    build.vars["KIT"] = Slashes(build.options.kit);
    build.vars["CXX"] = "zig"; // "zig c++": arguments on the command line, no response file to parse here
    build.kit = { { "games", "synthetic" }, { "platform", "linux-x64" }, { "kit_format", "2" },
        { "layout_record", "1" } };
    Put(build.options.kit / "share/guest_layout.h", ReadText(SKY_RUNTIME_DIR "/guest_layout.h"));
    Put(build.options.kit / "commands/synthetic/compile.args", "${CXX}\nc++\n-c\n${SRC}\n-o\n${OBJ}\n");
    Put(build.options.kit / "commands/synthetic/link.args",
        "${CXX}\nc++\n-o\n${OUT}\n${OBJECTS}\n${KIT}/lib/synthetic/runtime/loader.cpp.o\n");
    Put(build.options.kit / "lib/synthetic/runtime/loader.cpp.o", "invented runtime object\n");
    Put(build.options.kit / "include/indirect_call_guard.h", "// invented\n");
    fs::remove_all(build.work / "ppc");
    Put(build.work / "ppc/ppc_recomp.0.cpp", "// invented translation\n");
    fs::path config = build.work / "ppc/ppc_config.h";

    // The generated bounds must be the executable's, and fit guest memory; the image hash is required.
    Put(config, "#pragma once\n");
    CHECK(Failed([&] { WriteLayoutRecord(build, layout, sha); }).find("no code layout") != std::string::npos);
    Put(config, GeneratedConfig(layout) + "#define PPC_CODE_SIZE 0x10ull\n");
    CHECK(Failed([&] { WriteLayoutRecord(build, layout, sha); }).find("no code layout") != std::string::npos);
    GuestLayout other = layout;
    other.codeSize -= 0x10000;
    Put(config, GeneratedConfig(other));
    CHECK(Failed([&] { WriteLayoutRecord(build, layout, sha); }).find("does not match") != std::string::npos);
    GuestLayout low = layout;
    low.imageBase = 0x70000000;
    low.codeBase = 0x70010000;
    Put(config, GeneratedConfig(low));
    CHECK(Failed([&] { WriteLayoutRecord(build, low, sha); }).find("cannot be used") != std::string::npos);
    Put(config, GeneratedConfig(layout));
    CHECK(Failed([&] { WriteLayoutRecord(build, layout, ""); }).find("SHA-256") != std::string::npos);
    CHECK(!fs::exists(build.work / "layout/guest_layout_record.cpp"));
    CHECK(Failed([&] { Compile(build); }).find("no code layout record") != std::string::npos);
    WriteLayoutRecord(build, layout, sha);
    fs::path source = build.work / "layout/guest_layout_record.cpp";
    CHECK(ReadText(source).find("\"" + sha + "\"") != std::string::npos);
    CHECK(ReadText(source).find("#include \"guest_layout.h\"") != std::string::npos);
    CompileLayoutRecord(build, sha);

    // The step itself: another record version or no header refused; an unreadable executable refused, nothing
    // stamped; the stamp makes a rerun cheap.
    build.kit["layout_record"] = "2";
    CHECK(Failed([&] { LayoutRecord(build); }).find("version 2") != std::string::npos);
    build.kit["layout_record"] = "1";
    CHECK(Failed([&] { LayoutRecord(build); }).find("cannot read the disc's executable") != std::string::npos);
    CHECK(!fs::exists(build.work / "layout/stamp.txt"));
    Put(build.work / "layout/stamp.txt", DiscStamp(build) + "config=" + Sha256File(config) + "\nheader=" +
                                             Sha256File(build.options.kit / "share/guest_layout.h") + "\nrecord=1\n");
    LayoutRecord(build);
    fs::rename(build.options.kit / "share/guest_layout.h", build.options.kit / "share/guest_layout.h.away");
    CHECK(Failed([&] { LayoutRecord(build); }).find("no share/guest_layout.h") != std::string::npos);
    fs::rename(build.options.kit / "share/guest_layout.h.away", build.options.kit / "share/guest_layout.h");

    // Compile: the record with the game code's recipe; a cancelled compile leaves no object and no key.
    std::vector<std::string> compiled;
    std::mutex compiledMutex; // Compile runs the child calls on several threads
    int result = 0;
    child = [&](const std::vector<std::string>& args, const RunOptions& run) {
        {
            std::lock_guard<std::mutex> lock(compiledMutex);
            CHECK(args.size() == 6 && args[1] == "c++" && args[4] == "-o");
            compiled.push_back(args[3]);
        }
        if (result == 0)
            Put(run.cwd / args[5], "invented object\n");
        return result;
    };
    Compile(build);
    std::sort(compiled.begin(), compiled.end());
    CHECK((compiled == std::vector<std::string>{ "layout/guest_layout_record.cpp", "ppc/ppc_recomp.0.cpp" }));
    child = {};
    Compile(build); // up to date
    Put(build.work / "layout/guest_layout.h", ReadText(build.work / "layout/guest_layout.h") + "// changed\n");
    compiled.clear();
    result = -3;
    child = [&](const std::vector<std::string>& args, const RunOptions& run) {
        {
            std::lock_guard<std::mutex> lock(compiledMutex);
            compiled.push_back(args[3]);
        }
        Put(run.cwd / args[5], "partial object\n");
        fs::remove(run.cwd / args[5]);
        return result;
    };
    CHECK(Failed([&] { Compile(build); }).find("guest_layout_record.cpp failed") != std::string::npos);
    CHECK((compiled == std::vector<std::string>{ "layout/guest_layout_record.cpp" }));
    CHECK(!fs::exists(build.work / "obj/guest_layout_record.o") &&
          !fs::exists(build.work / "obj/guest_layout_record.key"));
    CHECK(fs::exists(build.work / "obj/ppc_recomp.0.o"));
    result = 0;
    child = [&](const std::vector<std::string>& args, const RunOptions& run) {
        {
            std::lock_guard<std::mutex> lock(compiledMutex);
            compiled.push_back(args[3]);
        }
        Put(run.cwd / args[5], "invented object\n");
        return 0;
    };
    Compile(build);
    CHECK(compiled.size() == 2 && compiled[1] == "layout/guest_layout_record.cpp");
    CHECK((ObjectList(build) == std::vector<std::string>{ "obj/guest_layout_record.o", "obj/ppc_recomp.0.o" }));

    // Link: the record exactly once; a failed or cancelled link keeps the previous game and its next run relinks.
    for (const char* extension : { "png", "ico", "icns" })
        Put(build.options.out / (std::string("game_icon.") + extension), "synthetic icon\n");
    Put(build.work / "icon/game_icon.key", DiscStamp(build) + "recipe=2\n");
    Put(build.options.kit / "res/THIRD_PARTY_NOTICES.md", "invented notices\n");
    Put(build.options.kit / "res/fonts/invented.ttf", "invented font\n");
    Put(build.exe, "previous game\n");
    Put(build.work / "link.key", "previous key\n");
    for (int failure : { 1, -3 })
    {
        child = [&](const std::vector<std::string>& args, const RunOptions&) {
            CHECK(std::count(args.begin(), args.end(), "obj/guest_layout_record.o") == 1);
            return failure;
        };
        CHECK(Failed([&] { Link(build); }).find("linking failed") != std::string::npos);
        CHECK(ReadText(build.exe) == "previous game\n");
        CHECK(!fs::exists(build.work / "link.key"));
    }
    child = [&](const std::vector<std::string>&, const RunOptions&) {
        Put(build.exe, "new game\n");
        return 0;
    };
    Link(build);
    CHECK(ReadText(build.exe) == "new game\n" && fs::exists(build.work / "link.key"));
    child = {};
    Link(build); // up to date
    Put(build.options.kit / "commands/synthetic/link.args",
        ReadText(build.options.kit / "commands/synthetic/link.args") +
            "${KIT}/lib/synthetic/runtime/guest_layout_record.cpp.o\n");
    CHECK(Failed([&] { Link(build); }).find("2 code layout records, not 1") != std::string::npos);
    CHECK(ReadText(build.exe) == "new game\n");

    // An old kit: no step, no record compiled or linked; a kit record object in its link is refused too.
    build.kit.erase("layout_record");
    build.kit.erase("kit_format");
    CHECK((ObjectList(build) == std::vector<std::string>{ "obj/ppc_recomp.0.o" }));
    CHECK(Failed([&] { Link(build); }).find("1 code layout records, not 0") != std::string::npos);
    Put(build.options.kit / "commands/synthetic/link.args",
        "${CXX}\nc++\n-o\n${OUT}\n${OBJECTS}\n${KIT}/lib/synthetic/runtime/loader.cpp.o\n");
    LayoutRecord(build); // nothing to do: child = {} would throw on any request
    child = [&](const std::vector<std::string>& args, const RunOptions&) {
        CHECK(std::count(args.begin(), args.end(), "obj/guest_layout_record.o") == 0);
        Put(build.exe, "old kit game\n");
        return 0;
    };
    Compile(build); // the ppc object is still fresh: no compile
    Link(build);
    CHECK(ReadText(build.exe) == "old kit game\n");
    child = {};
}

int main(int argc, char** argv)
{
    if (argc != 2)
        return 2;
    try
    {
        fs::path root = fs::absolute(argv[1]);
        fs::remove_all(root);
        Build build = Fixture(root);
        TestInventory(build);
        TestTranslation(build);
        TestMovies(build);
        TestMainFailure(build);
        TestKitFormat();
        TestAbiTranslation(build);
        TestExecutableResources(build);
        Build layout = Fixture(root / "layout");
        TestLayoutRecord(layout);
        fs::remove_all(root);
    }
    catch (const std::exception& error)
    {
        fprintf(stderr, "test exception: %s\n", error.what());
        return 1;
    }
    catch (const Failure& error)
    {
        fprintf(stderr, "unexpected install failure: %s\n", error.message.c_str());
        return 1;
    }
    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
