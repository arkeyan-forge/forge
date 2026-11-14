// Disc-free translation checks. Optional corpus mode keeps all inputs and outputs local.
#include "shader_recompiler.h"
#include "dxc_compiler.h"
#include "shader_cache_key.h"
#include <fstream>
#include <iterator>

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

static std::string Read(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
        throw std::runtime_error("cannot read input");
    return { std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
}

static uint32_t Swizzle(const char* text)
{
    uint32_t value = 0;
    for (uint32_t i = 0; i < 4; ++i)
        value |= ((uint32_t(std::string_view("xyzw").find(text[i])) - i) & 3) << (2 * i);
    return value;
}

static TextureFetchInstruction Fetch(FetchOpcode opcode, TextureDimension dimension = TextureDimension::Texture2D)
{
    TextureFetchInstruction fetch{};
    fetch.opcode = opcode;
    fetch.dimension = dimension;
    fetch.srcRegister = 3;
    fetch.srcSwizzle = 0 | (1 << 2) | (2 << 4);
    fetch.dstRegister = 4;
    fetch.dstSwizzle = 0 | (1 << 3) | (2 << 6) | (3 << 9);
    fetch.useRegLod = 1;
    return fetch;
}

static AluInstruction Cube(const char* a, const char* b)
{
    AluInstruction instr{};
    instr.vectorOpcode = AluVectorOpcode::Cube;
    instr.scalarOpcode = AluScalarOpcode::RetainPrev;
    instr.src1Select = instr.src2Select = 1;
    instr.src1Register = instr.src2Register = 2;
    instr.src1Swizzle = Swizzle(a);
    instr.src2Swizzle = Swizzle(b);
    instr.vectorWriteMask = 15;
    return instr;
}

// An ALU instruction in a fetch slot of Program(): both are three words.
static TextureFetchInstruction Slot(const AluInstruction& alu)
{
    TextureFetchInstruction slot;
    static_assert(sizeof(slot) == sizeof(alu));
    memcpy(&slot, &alu, sizeof(slot));
    return slot;
}

static void Put32(std::vector<uint8_t>& data, size_t at, uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i)
        data[at + i] = uint8_t(value >> (24 - i * 8));
}

// The entire container and every instruction are invented here. No game bytes are fixtures.
static std::vector<uint8_t> Program(
    const std::vector<uint64_t>& cf, const std::vector<TextureFetchInstruction>& fetches, bool loopConstant = false)
{
    const size_t virtualSize = loopConstant ? 256 : 128;
    const size_t clauses = (cf.size() + 1) / 2;
    const size_t size = 12 * (clauses + fetches.size());
    std::vector<uint8_t> data(virtualSize + size);
    Put32(data, 0, 0x102A1100); // pixel container
    Put32(data, 4, uint32_t(virtualSize));
    Put32(data, 8, uint32_t(size));
    Put32(data, 24, 64); // shader header
    Put32(data, 68, uint32_t(size));
    Put32(data, 92, PIXEL_SHADER_OUTPUT_COLOR0);
    if (loopConstant)
    {
        Put32(data, 20, 128); // definition table
        Put32(data, 128 + 24, (8992u << 16) | 1); // one invented i0
        Put32(data, 128 + 28, 2); // two iterations
    }
    for (size_t i = 0; i < clauses; ++i)
    {
        uint64_t a = cf[2 * i], b = 2 * i + 1 < cf.size() ? cf[2 * i + 1] : 0;
        Put32(data, virtualSize + i * 12, uint32_t(a));
        Put32(data, virtualSize + 4 + i * 12, uint32_t((a >> 32) | (b << 16)));
        Put32(data, virtualSize + 8 + i * 12, uint32_t(b >> 16));
    }
    for (size_t i = 0; i < fetches.size(); ++i)
    {
        uint32_t words[3];
        static_assert(sizeof(words) == sizeof(TextureFetchInstruction));
        memcpy(words, &fetches[i], sizeof(words));
        for (unsigned j = 0; j < 3; ++j)
            Put32(data, virtualSize + (clauses + i) * 12 + j * 4, words[j]);
    }
    return data;
}

static uint64_t Exec(unsigned address, unsigned count, bool end = false)
{
    uint64_t sequence = 0;
    for (unsigned i = 0; i < count; ++i)
        sequence |= 1ull << (2 * i);
    return address | (uint64_t(count) << 12) | (sequence << 16) | (uint64_t(end ? 2 : 1) << 44);
}

// The locals of shaderMain declared without a value (a read of one is undefined: Apple and AMD give 0, NVIDIA anything),
// plus one if its output is not zero-initialised. XenosRecomp gives every temp, export and state a value first.
static unsigned UninitialisedLocals(const std::string& source)
{
    size_t body = source.find("shaderMain(");
    if (body == std::string::npos)
        return 1;
    unsigned found = source.find("output = (PixelShaderOutput)0;", body) == std::string::npos &&
                     source.find("output = (Interpolators)0;", body) == std::string::npos;
    static const char* const types[] = { "float", "half", "int", "uint", "bool", "CubeMapData" };
    for (size_t at = body; at < source.size();)
    {
        size_t end = source.find('\n', at);
        std::string_view line(source.data() + at, (end == std::string::npos ? source.size() : end) - at);
        at = end == std::string::npos ? source.size() : end + 1;
        line.remove_prefix(std::min(line.size(), line.find_first_not_of(" \t")));
        for (const char* type : types)
        {
            size_t length = strlen(type);
            if (line.compare(0, length, type) != 0)
                continue;
            size_t name = length;
            while (name < line.size() && (isdigit(line[name]) || line[name] == 'x'))
                ++name; // float4, float4x4
            if (name < line.size() && line[name] == ' ' && line.find('=') == std::string_view::npos &&
                line.find('(') == std::string_view::npos && !line.empty() && line.back() == ';')
                ++found;
        }
    }
    return found;
}

static std::string Translate(const std::vector<uint8_t>& program, const std::string& common)
{
    ShaderRecompiler recompiler;
    recompiler.recompile(program.data(), common);
    CHECK(UninitialisedLocals(recompiler.out) == 0);
    return recompiler.out;
}

static void Compile(const std::string& source)
{
    DxcCompiler compiler;
    for (bool spirv : { false, true })
    {
        auto blob = compiler.compile(source, true, false, spirv);
        CHECK(blob != nullptr);
        if (blob)
            blob->Release();
    }
}

static void CompileVertex(const std::string& source)
{
    DxcCompiler compiler;
    for (bool spirv : { false, true })
    {
        auto blob = compiler.compile(source, false, false, spirv);
        CHECK(blob != nullptr);
        if (blob)
            blob->Release();
    }
}

// A vertex shader has no derivatives: every fetch samples level 0, never an implicit-LOD Sample, and names a helper
// that exists (an unnormalized 2D fetch once called tfetch2DVertexDenorm; a 1D one, tfetch2D).
static void VertexFetches(const std::string& common)
{
    struct Case
    {
        TextureDimension dimension;
        bool denorm;
        const char* helper;
    };
    for (const Case& c : { Case{ TextureDimension::Texture1D, false, "tfetch2DLod(" },
             Case{ TextureDimension::Texture2D, false, "tfetch2DVertex(" },
             Case{ TextureDimension::Texture2D, true, "tfetch2DLodDenorm(" },
             Case{ TextureDimension::Texture3D, false, "tfetch3DLod(" } })
    {
        auto fetch = Fetch(FetchOpcode::TextureFetch, c.dimension);
        fetch.texCoordDenorm = c.denorm;
        auto program = Program({ Exec(1, 1, true) }, { fetch });
        Put32(program, 0, 0x102A1101); // vertex container
        auto source = Translate(program, common);
        CHECK(source.find(c.helper) != std::string::npos);
        CHECK(source.find("VertexDenorm") == std::string::npos);
        CompileVertex(source);
    }
}

// The shader cache's key (XenosRecomp's main.cpp): an entry serves a run only with the same compilers' arguments and the
// streams it needs. A cache filled by an older build (key 1: "every stream this build has", even without DXIL) is compiled
// again, not read as shaders with no DXIL and no error.
static void CacheKey()
{
    const uint64_t configuration = 0xABCDEF1234567000ull;
    const uint32_t dxil = 1, spirv = 2, air = 4;
    CHECK(!CacheKeyCovers(1, configuration, spirv | air));
    CHECK(!CacheKeyCovers(0x100 | dxil | spirv | air, configuration, dxil));
    CHECK(CacheKeyCovers(configuration | 0x100 | dxil | spirv, configuration, spirv));
    CHECK(!CacheKeyCovers(configuration | 0x100 | spirv | air, configuration, dxil | spirv));
    CHECK(!CacheKeyCovers((configuration ^ 0x1000) | 0x100 | spirv, configuration, spirv));
    const std::string arguments = DxcCompiler::signature();
    for (const char* argument :
        { "-T vs_6_0", "-T ps_6_0", "-spirv", "-fvk-invert-y", "-Qstrip_reflect", "-DSPEC_CONSTANTS=0" })
        CHECK(arguments.find(argument) != std::string::npos);
}

// Whether a SPIR-V module decorates a variable with this BuiltIn (OpDecorate %id BuiltIn <builtIn>).
static bool HasBuiltIn(IDxcBlob* blob, uint32_t builtIn)
{
    auto words = static_cast<const uint32_t*>(blob->GetBufferPointer());
    size_t count = blob->GetBufferSize() / 4;
    for (size_t i = 5; i + 3 < count; i += words[i] >> 16 ? words[i] >> 16 : 1)
        if (words[i] == ((4u << 16) | 71) && words[i + 2] == 11 && words[i + 3] == builtIn)
            return true;
    return false;
}

// The screen-tile light passes and the lights drawn as points depend on two inputs that SPIR-V lacked: a vertex fetch at
// a computed index (the host's gathered vertices) and the pixel shader's param_gen register (position + point coordinate).
static void TestVulkanInputs(const std::string& common)
{
    VertexFetchInstruction fetch{};
    fetch.opcode = FetchOpcode::VertexFetch;
    fetch.srcRegister = 1; // not the vertex index (r0.x)
    fetch.dstRegister = 2;
    fetch.dstSwizzle = 0 | (1 << 3) | (2 << 6) | (3 << 9);
    ShaderRecompiler vertex;
    vertex.vertexElements[0] = { 0, DeclUsage::Position, 0 };
    vertex.recompile(fetch, 0);
    CHECK(vertex.out.find("PULLED_VERTEX(floor(r1.x))") != std::string::npos);
    CHECK(vertex.out.find("__spirv__") == std::string::npos); // every backend reads the gathered vertices

    DxcCompiler compiler;
    const std::string pulled = common +
                               "\nfloat4 shaderMain(in uint iVertexId : SV_VertexID) : SV_Position\n{\n\treturn "
                               "PULLED_VERTEX(floor(float(iVertexId) * 0.25));\n}\n";
    auto blob =
        compiler.compile(pulled, false, false, true); // (DXIL reads g_PulledCount from the shader's own cbuffer)
    CHECK(blob != nullptr);
    if (blob)
        blob->Release();

    // oC0 = max(r3, r3): r3 is live, so a param_gen into it must reach SPIR-V.
    AluInstruction exported{};
    exported.vectorOpcode = AluVectorOpcode::Max;
    exported.scalarOpcode = AluScalarOpcode::RetainPrev;
    exported.src1Select = exported.src2Select = 1;
    exported.src1Register = exported.src2Register = 3;
    exported.src1Swizzle = exported.src2Swizzle = Swizzle("xyzw");
    exported.exportData = 1;
    exported.vectorWriteMask = 15;
    blob = compiler.compile(
        Translate(Program({ Exec(1, 1, true) & ~(1ull << 16) }, { Slot(exported) }), common), true, false, true);
    CHECK(blob != nullptr);
    if (blob)
    {
        CHECK(HasBuiltIn(blob, 16)); // PointCoord: param_gen's ZW
        CHECK(HasBuiltIn(blob, 17)); // FrontFacing: param_gen's X sign
        blob->Release();
    }
}

// shader_common.h's DENORM_TEXEL, and a GPU that normalizes the coordinate (float), scales it back and truncates it to 8
// fractional bits before taking the texel (what NVIDIA does: a coordinate a hair below texel h's edge reads h - 1). Every
// host pixel of a pixel-position fetch (the guest's iPos / scale - 0.5, with an offset) must read its own texel; with the
// edge itself (nudge 0) the model already misses some (279 of 39888); NVIDIA's own arithmetic missed every pixel of
// Trap Team's title passes.
static unsigned DenormTexelMisses(float nudge)
{
    unsigned misses = 0;
    for (uint32_t guestWidth : { 64u, 256u, 640u, 1280u })
        for (float scale : { 1.0f, 2.0f, 3.0f })
        {
            const uint32_t width = uint32_t(float(guestWidth) * scale);
            for (float offset : { -1.0f, 0.0f, 2.0f })
                for (uint32_t h = 4; h + 8 < width; h++)
                {
                    const float texCoord = (float(h) + 0.5f) / scale - 0.5f;
                    const float coordinate = (texCoord + offset) * scale + 0.5f * (scale - 1.0f) + nudge;
                    const float u = coordinate / float(width);
                    const int64_t fixed = int64_t(std::floor(double(u * float(width)) * 256.0));
                    const int64_t want = int64_t(std::floor(double(h) + double(offset) * scale));
                    misses += (fixed >> 8) != want;
                }
        }
    return misses;
}

static void DenormTexelModel(const std::string& common)
{
    CHECK(common.find(
              "#define DENORM_TEXEL(texCoord, offset, scale) (((texCoord) + (offset)) * (scale) + 0.5 * ((scale) - "
              "1.0) + 1.0 / 256.0)") != std::string::npos);
    CHECK(DenormTexelMisses(0.0f) > 0);
    CHECK(DenormTexelMisses(1.0f / 256.0f) == 0);
}

static void Test(const std::string& common)
{
    DenormTexelModel(common);
    // The scan itself: a temp without a value, an output left as it came, then both given one.
    CHECK(UninitialisedLocals("shaderMain(\n\tPixelShaderOutput output = (PixelShaderOutput)0;\n\tfloat4 r6;\n") == 1);
    CHECK(UninitialisedLocals("shaderMain(\n\tPixelShaderOutput output;\n\tbool p0 = false;\n") == 1);
    CHECK(UninitialisedLocals("shaderMain(\n\tInterpolators output = (Interpolators)0;\n\tfloat4 r6 = 0.0;\n") == 0);
    CacheKey();
    VertexFetches(common);
    TestVulkanInputs(common);

    auto setter = Fetch(FetchOpcode::SetTextureLod);
    setter.srcSwizzle = 3;
    setter.isPredicated = 1;
    setter.predCondition = 0;
    ShaderRecompiler direct;
    direct.isPixelShader = true;
    direct.recompile(setter, false);
    CHECK(direct.out == "if (!p0) textureLod = r3.w;\n");

    for (auto dimension : { TextureDimension::Texture1D, TextureDimension::Texture2D, TextureDimension::Texture3D,
             TextureDimension::TextureCube })
    {
        auto fetch = Fetch(FetchOpcode::TextureFetch, dimension);
        fetch.lodBias = -12;
        fetch.isPredicated = 1;
        fetch.predCondition = 1;
        ShaderRecompiler result;
        result.isPixelShader = true;
        result.recompile(fetch, false);
        CHECK(result.out.find("if (p0)") != std::string::npos);
        CHECK(result.out.find("Lod(") != std::string::npos);
        CHECK(result.out.find(", textureLod + (-0.75)") != std::string::npos);
        CHECK(result.out.find(".xyzw;") != std::string::npos);
        if (dimension == TextureDimension::Texture2D)
        {
            fetch.texCoordDenorm = 1;
            fetch.offsetX = 3;
            fetch.offsetY = -1;
            fetch.srcSwizzle = 1 | (0 << 2);
            result = {};
            result.isPixelShader = true;
            result.recompile(fetch, false);
            CHECK(result.out.find("tfetch2DLodDenorm(") != std::string::npos);
            CHECK(result.out.find("r3.yx, float2(1.5, -0.5)") != std::string::npos);
            CHECK(result.out.find("g_ScaledTextures") != std::string::npos);
        }
        auto source = Translate(Program({ Exec(1, 2, true) }, { setter, fetch }), common);
        CHECK(source.find("float textureLod = 0.0;") < source.find("if (!p0) textureLod"));
        // Every denormalized fetch, with or without an explicit LOD, on Metal and HLSL, takes its host texel coordinate from
        // DENORM_TEXEL (DenormTexelModel below): the host pixel's own texel at any resolution scale, on every GPU.
        size_t denormOffsets = 0;
        for (size_t at = source.find("DENORM_TEXEL(texCoord, offset, scale) /"); at != std::string::npos;
            at = source.find("DENORM_TEXEL(texCoord, offset, scale) /", at + 1))
            ++denormOffsets;
        CHECK(denormOffsets == 4);
        CHECK(source.find("* scale + 0.5 * (scale - 1.0))") == std::string::npos);
        Compile(source);
    }

    // Setter in a bool branch, fetch after it. State belongs to the invocation, not to the branch.
    const uint64_t jump = (11ull << 44) | 2 | (128ull << 34) | (1ull << 42);
    auto source = Translate(
        Program({ jump, Exec(2, 1), Exec(3, 1, true) }, { setter, Fetch(FetchOpcode::TextureFetch) }), common);
    CHECK(source.find("float textureLod") < source.find("if ((g_Booleans"));
    CHECK(source.find("textureLod = r3.w;") < source.find("\n\t}\n\tr4.xyzw = tfetch2DLod("));
    Compile(source);

    // An invented backwards jump exercises the dispatch loop; declaration must precede it.
    const uint64_t back = (11ull << 44) | (1ull << 14) | (1ull << 33) | (1ull << 42);
    source = Translate(
        Program({ Exec(2, 1), back, Exec(3, 1, true) }, { setter, Fetch(FetchOpcode::TextureFetch) }), common);
    CHECK(source.find("float textureLod") < source.find("while (true)"));
    CHECK(source.find("PC_DISPATCH") != std::string::npos);
    Compile(source);

    // Structured guest loop: setter inside the loop, fetch after it, one persistent state declaration.
    source = Translate(Program({ 7ull << 44, Exec(2, 1), 8ull << 44, Exec(3, 1, true) },
                           { setter, Fetch(FetchOpcode::TextureFetch) }, true),
        common);
    auto state = source.find("float textureLod = 0.0;");
    CHECK(state < source.find("for (aL = 0; aL < i0.x; aL++)"));
    CHECK(source.find("float textureLod", state + 1) == std::string::npos);
    CHECK(source.find("textureLod = r3.w;") > source.find("for (aL"));
    CHECK(source.find("\n\t}\n\tr4.xyzw = tfetch2DLod(") != std::string::npos);
    Compile(source);

    auto implicit = Fetch(FetchOpcode::TextureFetch);
    implicit.useRegLod = 0;
    implicit.useCompLod = 1;
    source = Translate(Program({ Exec(1, 1, true) }, { implicit }), common);
    CHECK(source.find("textureLod") == std::string::npos);
    CHECK(source.find("tfetch2DLod") == std::string::npos);

    auto canonical = Cube("zzxy", "yxzz");
    direct = {};
    direct.isPixelShader = true;
    direct.recompile(canonical);
    CHECK(direct.out.find("cube(r2, &cubeMapData)") != std::string::npos);
    CHECK(direct.out.find("cube(r2, cubeMapData)") != std::string::npos);
    auto permuted = Cube("yywx", "xwyy");
    permuted.src2Register = 5;
    permuted.src1Negate = 1;
    permuted.src2Register |= 128; // abs modifier
    permuted.vectorWriteMask = 3; // inputs still need all lanes
    direct = {};
    direct.isPixelShader = true;
    direct.recompile(permuted);
    CHECK(direct.out.find("float4((-r2.yywx).z, (abs(r5.xwyy)).x, (abs(r5.xwyy)).z, 0.0)") != std::string::npos);

    // Compile a noncanonical cube expression inside an invented complete shader.
    auto insertion = source.find("\n\tr4.xyzw = tfetch2D(");
    CHECK(insertion != std::string::npos);
    source.insert(insertion, "\n" + direct.out);
    Compile(source);

    // One instruction's vector and scalar operations read their sources before either writes: a scalar reading the
    // register the vector one writes sees its old value (Giants' lit waterfall: r5.w = r4.x; ps = r5.w).
    AluInstruction coissued{};
    coissued.vectorOpcode = AluVectorOpcode::Max;
    coissued.src1Select = coissued.src2Select = 1;
    coissued.src1Register = coissued.src2Register = 4;
    coissued.vectorDest = 5;
    coissued.vectorWriteMask = 8; // r5.w
    coissued.scalarOpcode = AluScalarOpcode::Maxs;
    coissued.src3Select = 1;
    coissued.src3Register = 5;
    coissued.src3Swizzle = Swizzle("wwww");
    coissued.scalarDest = 5;
    coissued.scalarWriteMask = 1; // r5.x
    direct = {};
    direct.isPixelShader = true;
    direct.recompile(coissued);
    CHECK(direct.out.find("ps = max(r5.w, r5.w);") != std::string::npos);
    CHECK(direct.out.find("ps = max(r5.w, r5.w);") < direct.out.find("r5.w = "));
    CHECK(direct.out.find("r5.w = ") < direct.out.find("r5.x = ps;"));
    // No hazard: the usual order, vector first.
    coissued.src3Register = 6;
    direct = {};
    direct.isPixelShader = true;
    direct.recompile(coissued);
    CHECK(direct.out.find("r5.w = ") < direct.out.find("ps = max(r6.w, r6.w);"));
    // r50 is not r5; r5.x is not the r5.w the vector operation writes.
    coissued.src3Register = 50;
    direct = {};
    direct.isPixelShader = true;
    direct.recompile(coissued);
    CHECK(direct.out.find("r5.w = ") < direct.out.find("ps = max(r50.w, r50.w);"));
    coissued.src3Register = 5;
    coissued.src3Swizzle = Swizzle("xxxx");
    direct = {};
    direct.isPixelShader = true;
    direct.recompile(coissued);
    CHECK(direct.out.find("r5.w = ") < direct.out.find("ps = max(r5.x, r5.x);"));

    // No constant table: the synthetic g_C covers every register, but the shader's own literal (def c5) wins, as in
    // D3D9 (it read g_C(5), the buffer, before). c6 has no literal and still reads the buffer.
    AluInstruction max{};
    max.vectorOpcode = AluVectorOpcode::Max;
    max.scalarOpcode = AluScalarOpcode::RetainPrev;
    max.src1Register = 5;
    max.src2Register = 6;
    max.vectorWriteMask = 15;
    auto literal = Program({ Exec(1, 1, true) & ~(1ull << 16) }, { Slot(max) }, true);
    Put32(literal, 128 + 20, ((256u + 5) << 16) | 4); // def c5: four floats, in place of i0
    Put32(literal, 128 + 24, uint32_t(literal.size() - 256));
    Put32(literal, 128 + 28, 0); // end of the float definitions; no int or bool ones
    literal.resize(literal.size() + 16);
    for (unsigned i = 0; i < 4; ++i)
        Put32(literal, literal.size() - 16 + i * 4, 0x3F800000 + i); // invented values
    source = Translate(literal, common);
    CHECK(source.find("float4 c5 = asfloat(uint4(0x3F800000, 0x3F800001, 0x3F800002, 0x3F800003));") !=
          std::string::npos);
    CHECK(source.find("max(c5.") != std::string::npos);
    CHECK(source.find("g_C(5)") == std::string::npos);
    CHECK(source.find("g_C(6)") != std::string::npos);
    Compile(source);
}

static int Corpus(const char* manifest, const char* output, const char* common, bool compile)
{
    auto include = Read(common);
    std::ifstream list(manifest);
    if (!list)
        throw std::runtime_error("cannot read manifest");
    std::filesystem::create_directories(output);
    unsigned count = 0, rejected = 0, compiled = 0, uninitialised = 0;
    std::string path;
    while (std::getline(list, path))
    {
        std::string translated;
        try
        {
            auto data = Read(path);
            ShaderRecompiler recompiler;
            recompiler.recompile(reinterpret_cast<const uint8_t*>(data.data()), include);
            translated = recompiler.out;
            if (compile)
            {
                DxcCompiler compiler;
                for (uint32_t variant = 0; variant <= recompiler.specConstantsMask; ++variant)
                {
                    if (variant & ~recompiler.specConstantsMask)
                        continue;
                    auto blob = compiler.compile(translated, recompiler.isPixelShader, false, false, variant);
                    if (!blob)
                        throw std::runtime_error("DXIL compilation failed");
                    blob->Release();
                }
                auto blob = compiler.compile(translated, recompiler.isPixelShader, false, true);
                if (!blob)
                    throw std::runtime_error("SPIR-V compilation failed");
                blob->Release();
                ++compiled;
            }
        }
        catch (const std::exception& error)
        {
            ++rejected;
            // Do not write source or input names to a shared report.
            fprintf(stderr, "input %u rejected: %s\n", count, error.what());
        }
        if (!translated.empty() && UninitialisedLocals(translated) != 0)
        {
            ++uninitialised;
            fprintf(stderr, "input %u: a local or the output without a value\n", count);
        }
        std::ofstream(std::filesystem::path(output) / (std::to_string(count++) + ".hlsl"), std::ios::binary)
            << translated;
    }
    printf("%u inputs, %u rejected, %u compiled to DXIL and SPIR-V, %u with a local declared without a value\n", count,
        rejected, compiled, uninitialised);
    return rejected || uninitialised ? 1 : 0;
}

int main(int argc, char** argv)
{
    try
    {
        if (argc == 5)
        {
            const std::string mode = argv[4];
            if (mode != "--compile" && mode != "--translate")
                throw std::runtime_error("unknown corpus mode");
            return Corpus(argv[1], argv[2], argv[3], mode == "--compile");
        }
        if (argc != 2)
            throw std::runtime_error("expected common header, or manifest output common --translate|--compile");
        Test(Read(argv[1]));
        printf("%d checks, %d failures\n", checks, failures);
        return failures ? 1 : 0;
    }
    catch (const std::exception& error)
    {
        fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
