// The models that show a button over a glyph texture (host/glyphs.h, Model): a draw is skipped only when its index count
// and the hash of its indices are a listed model's letters, and only while a style other than the Xbox 360's draws the
// glyph; the texture matrix is set to the identity only for the model's shader with the glyph bound; a keycap's label
// (KeycapLabel). No SDL, no GPU, no game data: ./ArkeyanGlyphModelsTest
#include "host/glyphs.h"
#include "host/art/alchemy_archive.h"
#include "mash_prompts.h"
#define XXH_INLINE_ALL
#include <xxhash.h>
#include <array>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace glyphs;

namespace
{
    int g_checks = 0, g_failures = 0;
#define EXPECT(condition)                                                                                              \
    do                                                                                                                 \
    {                                                                                                                  \
        g_checks++;                                                                                                    \
        if (!(condition))                                                                                              \
        {                                                                                                              \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition);                                               \
            g_failures++;                                                                                              \
        }                                                                                                              \
    } while (0)

    // A miniature IGZ with two index-buffer objects and their memory handles. Every value is synthetic.
    std::vector<uint8_t> PromptModel(std::span<const uint16_t> face, std::span<const uint16_t> letters,
        bool extraMesh = false, uint32_t objectCount = 2)
    {
        std::vector<uint8_t> bytes(4096);
        auto word = [&](size_t at, uint32_t value) {
            for (int i = 0; i < 4; i++)
                bytes[at + size_t(i)] = uint8_t(value >> (24 - 8 * i));
        };
        memcpy(bytes.data(), "IGZ\x01", 4);
        word(4, 9);
        word(0x18, 0x100);
        word(0x1C, 0x300);
        word(0x28, 0x800);
        word(0x2C, 0x800);
        size_t next = 0x100;
        auto record = [&](const char* tag, uint32_t count, size_t size) {
            const size_t start = next, length = (16 + size + 3) & ~size_t(3);
            for (int i = 0; i < 4; i++)
                bytes[start + size_t(i)] = uint8_t(tag[3 - i]);
            word(start + 4, count);
            word(start + 8, uint32_t(length));
            word(start + 12, 16);
            next += length;
            return start + 16;
        };
        const char type[] = "igIndexBuffer";
        const size_t types = record("TMET", 1, sizeof(type));
        memcpy(bytes.data() + types, type, sizeof(type));
        const size_t objects = record("RVTB", objectCount, 1);
        bytes[objects] = 0x40; // pool offsets 0 and 16 in four-byte units
        const size_t handles = record("TMHN", extraMesh ? 3 : 2, extraMesh ? 24 : 16);
        auto mesh = [&](size_t index, std::span<const uint16_t> indices) {
            const uint32_t offset = 0x100 + uint32_t(index) * 0x100;
            word(handles + index * 8, uint32_t(indices.size() * 2));
            word(handles + index * 8 + 4, offset);
            for (size_t i = 0; i < indices.size(); i++)
            {
                bytes[0x800 + offset + i * 2] = uint8_t(indices[i] >> 8);
                bytes[0x800 + offset + i * 2 + 1] = uint8_t(indices[i]);
            }
        };
        mesh(0, face);
        mesh(1, letters);
        if (extraMesh)
            mesh(2, face);
        return bytes;
    }

    void TestMashPrompts()
    {
        const uint16_t face[] = { 0, 1, 2, 2, 1, 3 };
        const uint16_t letters[] = { 0, 1, 2, 2, 3, 4, 4, 5, 6 };
        const auto file = art::alchemy::IgzFile::Parse(PromptModel(face, letters));
        EXPECT(file.has_value());
        if (!file)
            return;
        const Glyph expected[] = { Glyph::A, Glyph::B, Glyph::X, Glyph::Y };
        for (size_t i = 0; i < alchemy::MashPrompts.size(); i++)
        {
            const Texture& texture = alchemy::MashTextures[i];
            Prompt prompt = alchemy::MashPrompts[i];
            EXPECT(texture.glyph == expected[i]);
            EXPECT(texture.plate && prompt.picture == texture.hash);
            EXPECT(prompt.faceCount == 0 && prompt.lettersCount == 0);
            EXPECT(PromptAssetMatches(prompt.asset, prompt.asset));
            EXPECT(ResolvePrompt(prompt, *file));
            EXPECT(prompt.faceCount == 6 && prompt.lettersCount == 9);
            EXPECT(prompt.picture == texture.hash && prompt.uScale == 1.0f);
        }
        EXPECT(alchemy::MashTextures[2].glyph == Glyph::X); // the chest uses key_x, never key_a or the letter X
        EXPECT(int(alchemy::MashTextures[2].glyph) == 2); // FaceButton's key binding index
        EXPECT(KeycapLabel("G", "G", "G") == "G");

        EXPECT(PromptAssetMatches("models@!UI!ButtonPrompt!XENON!Mash_X.igz", alchemy::MashPrompts[2].asset));
        EXPECT(
            PromptAssetMatches("content/models\\UI\\ButtonPrompt\\XENON\\Mash_X`igz", alchemy::MashPrompts[2].asset));
        EXPECT(!PromptAssetMatches("models/ui/buttonprompt/xenon/mash_x_copy.igz", alchemy::MashPrompts[2].asset));
        EXPECT(!PromptAssetMatches("models/ui/buttonprompt/xenon/mash_y.igz", alchemy::MashPrompts[2].asset));
        EXPECT(!PromptAssetMatches("othermodels/ui/buttonprompt/xenon/mash_x.igz", alchemy::MashPrompts[2].asset));
        EXPECT(!PromptAssetMatches("models/ui/buttonprompt/xenon/mash_x.igz", ""));

        Prompt prompt = alchemy::MashPrompts[2];
        EXPECT(ResolvePrompt(prompt, *file));
        // Hashes are of stored big-endian bytes, as DrawIndices sees them, regardless of the host's byte order.
        const uint8_t storedFace[] = { 0, 0, 0, 1, 0, 2, 0, 2, 0, 1, 0, 3 };
        const uint8_t storedLetters[] = { 0, 0, 0, 1, 0, 2, 0, 2, 0, 3, 0, 4, 0, 4, 0, 5, 0, 6 };
        const std::span<const Prompt> prompts(&prompt, 1);
        for (Style style :
            { Style::Keyboard, Style::PlayStation, Style::PlayStation3, Style::Nintendo, Style::XboxOne })
        {
            uint64_t faceHash = 0, lettersHash = 0;
            EXPECT(PromptPartOf(prompts, style, 6, storedFace, faceHash) == PromptPart::Face);
            EXPECT(PromptPartOf(prompts, style, 9, storedLetters, lettersHash) == PromptPart::Letters);
            EXPECT(PromptOf(prompts, 6, faceHash, 9, lettersHash) == &prompt);
            EXPECT(PromptOf(prompts, 6, faceHash, 9, lettersHash + 1) == nullptr);
        }
        uint64_t hash = 0;
        EXPECT(PromptPartOf(prompts, Style::Xbox360, 6, storedFace, hash) == PromptPart::None);
        EXPECT(PromptPartOf(prompts, Style::Xbox360, 9, storedLetters, hash) == PromptPart::None);
        EXPECT(PromptPartOf(prompts, Style::Keyboard, 3, storedFace, hash) == PromptPart::None);

        std::array<Prompt, 4> resolved = alchemy::MashPrompts;
        for (size_t i = 0; i < resolved.size(); i++)
        {
            std::array<uint16_t, 9> variant = { 0, 1, 2, 2, 3, 4, 4, 5, 6 };
            if (i & 1)
                std::swap(variant[0], variant[1]);
            if (i & 2)
                std::swap(variant[3], variant[4]);
            const auto model = art::alchemy::IgzFile::Parse(PromptModel(face, variant));
            EXPECT(model && ResolvePrompt(resolved[i], *model));
        }
        for (size_t i = 0; i < resolved.size(); i++)
        {
            const Prompt& selected = resolved[i];
            EXPECT(PromptOf(resolved, selected.faceCount, selected.faceHash, selected.lettersCount,
                       selected.lettersHash) == &selected);
            EXPECT(selected.picture == alchemy::MashTextures[i].hash);
        }

        auto fails = [&](std::vector<uint8_t> bytes) {
            const auto invalid = art::alchemy::IgzFile::Parse(std::move(bytes));
            EXPECT(invalid.has_value());
            Prompt unchanged = prompt;
            EXPECT(invalid && !ResolvePrompt(unchanged, *invalid));
            EXPECT(unchanged.faceHash == prompt.faceHash && unchanged.lettersHash == prompt.lettersHash);
        };
        fails(PromptModel(face, letters, true)); // extra candidate: do not guess
        fails(PromptModel(face, letters, false, 1)); // not the expected two-mesh model
        fails(PromptModel(face, face)); // equal counts cannot identify the backing disc
        const uint16_t sparse[] = { 0, 1, 200, 2, 1, 3 };
        fails(PromptModel(sparse, letters)); // not a dense triangle list
        const uint16_t zero[] = { 0, 0, 0, 0, 0, 0 };
        fails(PromptModel(zero, letters));
        const uint16_t shortList[] = { 0, 1 };
        fails(PromptModel(shortList, letters));
        auto truncated = PromptModel(face, letters);
        truncated.resize(0x900);
        fails(std::move(truncated));
        auto otherVersion = PromptModel(face, letters);
        otherVersion[7] = 8;
        fails(std::move(otherVersion));
        const auto reversed = art::alchemy::IgzFile::Parse(PromptModel(letters, face));
        Prompt reordered = prompt;
        EXPECT(reversed && ResolvePrompt(reordered, *reversed));
        EXPECT(reordered.faceHash == prompt.faceHash && reordered.lettersHash == prompt.lettersHash);
        const uint16_t offsetFace[] = { 10, 11, 12, 12, 11, 13 };
        const auto offset = art::alchemy::IgzFile::Parse(PromptModel(offsetFace, letters));
        Prompt shifted = prompt;
        EXPECT(offset && ResolvePrompt(shifted, *offset));
        EXPECT(shifted.faceCount == prompt.faceCount && shifted.faceHash != prompt.faceHash);
    }
}

int main()
{
    TestMashPrompts();
    // Three triangles of a model's letters, and a draw with as many indices but other ones.
    const uint16_t letters[9] = { 0x0000, 0x0100, 0x0200, 0x0200, 0x0100, 0x0300, 0x0400, 0x0500, 0x0600 };
    const uint16_t other[9] = { 0x0000, 0x0200, 0x0100, 0x0200, 0x0100, 0x0300, 0x0400, 0x0500, 0x0600 };
    const uint64_t shaders[] = { 0xABCD, 0xABCF };
    const Model card = { 9, XXH3_64bits(letters, sizeof(letters)), shaders, 5 };
    const Texture textures[] = {
        { 0x1111, "plain glyph", Glyph::A },
        { 0x2222, "card", Glyph::RightTrigger, {}, true, true, &card },
    };

    EXPECT(HidesDraw(textures, Style::Keyboard, 9, letters));
    EXPECT(HidesDraw(textures, Style::PlayStation, 9, letters));
    EXPECT(HidesDraw(textures, Style::XboxOne, 9, letters));
    EXPECT(!HidesDraw(textures, Style::Xbox360, 9, letters)); // the game's own card and letters
    EXPECT(!HidesDraw(textures, Style::Keyboard, 9, other)); // same count, another mesh
    EXPECT(!HidesDraw(textures, Style::Keyboard, 6, letters)); // another count: not hashed
    EXPECT(!HidesDraw(textures, Style::Keyboard, 0, letters));
    EXPECT(!HidesDraw(textures, Style::Keyboard, 9, nullptr));
    EXPECT(!HidesDraw({}, Style::Keyboard, 9, letters));

    // Two models on two textures (a card and a copy with other letters, as a level may carry): each hides its own letters
    // and never the other's or the card's face (a 768-index mesh of the same card is not hashed as letters).
    const uint16_t lettersCopy[12] = { 0x0000, 0x0100, 0x0200, 0x0200, 0x0100, 0x0300, 0x0400, 0x0500, 0x0600, 0x0700,
        0x0800, 0x0900 };
    const Model copy = { 12, XXH3_64bits(lettersCopy, sizeof(lettersCopy)), shaders, 5 };
    const Texture variants[] = {
        { 0x2222, "card", Glyph::RightTrigger, {}, true, true, &card },
        { 0x4444, "card copy", Glyph::RightTrigger, {}, true, true, &copy },
    };
    EXPECT(HidesDraw(variants, Style::Keyboard, 9, letters));
    EXPECT(HidesDraw(variants, Style::Keyboard, 12, lettersCopy));
    const uint16_t otherTwelve[12] = { 0x0000, 0x0100, 0x0200, 0x0300, 0x0100, 0x0200, 0x0400, 0x0500, 0x0600, 0x0700,
        0x0800, 0x0A00 }; // 12 indices, not the copy's letters (letters has 9: reading 12 of them ran past it)
    EXPECT(!HidesDraw(variants, Style::Keyboard, 12, otherTwelve));
    EXPECT(!HidesDraw(variants, Style::Keyboard, 6, letters)); // a face of 6 indices stays
    EXPECT(!HidesDraw(variants, Style::Xbox360, 12, lettersCopy));

    // A keyboard-only glyph keeps the game's picture on a pad, so its letters stay too.
    const Texture keyboardOnly[] = { { 0x3333, "diagram", Glyph::Controller, {}, false, false, &card } };
    EXPECT(HidesDraw(keyboardOnly, Style::Keyboard, 9, letters));
    EXPECT(!HidesDraw(keyboardOnly, Style::Nintendo, 9, letters));

    // The texture matrix: the model's shader with the card drawn as a glyph in one of its slots.
    const uint64_t bound[] = { 0, 0x2222, 0 };
    const uint64_t plain[] = { 0x1111, 0, 0 };
    EXPECT(UvMatrix(textures, 0xABCD, bound) == 5);
    EXPECT(UvMatrix(textures, 0xABCF, bound) == 5); // the model's other pass
    EXPECT(UvMatrix(textures, 0xABCE, bound) == -1); // another shader: its constant 5 means something else
    EXPECT(UvMatrix(textures, 0xABCD, plain) == -1); // the card not drawn as a glyph (Xbox 360 style)
    EXPECT(UvMatrix(textures, 0xABCD, {}) == -1);
    EXPECT(UvMatrix({}, 0xABCD, bound) == -1);

    // World prompts: a face and letters. A face is told by its indices; two prompts that share a face (A and Y) are told by
    // the letters drawn right after it (PairLetters); nothing pairs in the 360's style.
    const uint16_t face[6] = { 0x0000, 0x0100, 0x0200, 0x0200, 0x0100, 0x0300 };
    const uint16_t lettersB[9] = { 0x0000, 0x0100, 0x0200, 0x0300, 0x0400, 0x0500, 0x0600, 0x0700, 0x0800 };
    const uint64_t faceHash = XXH3_64bits(face, sizeof(face));
    const Prompt prompts[] = {
        { 6, faceHash, 9, XXH3_64bits(letters, sizeof(letters)), 0x7001, 256, 128, 0, 1, false, false, 1 },
        { 6, faceHash, 9, XXH3_64bits(lettersB, sizeof(lettersB)), 0x7002, 128, 128, 0, 1, false, false, 1 },
        { 7, 0x1234, 3, 0x5678, 0x7003, 128, 128, 0, 1, false, false, 1 },
    };
    const uint64_t lettersHash = XXH3_64bits(letters, sizeof(letters)),
                   lettersBHash = XXH3_64bits(lettersB, sizeof(lettersB));
    uint64_t hash = 0;
    EXPECT(PromptPartOf(prompts, Style::PlayStation, 6, face, hash) == PromptPart::Face && hash == faceHash);
    EXPECT(PromptPartOf(prompts, Style::Keyboard, 6, face, hash) == PromptPart::Face);
    EXPECT(PromptPartOf(prompts, Style::PlayStation, 9, letters, hash) == PromptPart::Letters && hash == lettersHash);
    EXPECT(PromptPartOf(prompts, Style::PlayStation, 9, lettersB, hash) == PromptPart::Letters && hash == lettersBHash);
    EXPECT(PromptPartOf(prompts, Style::Xbox360, 6, face, hash) == PromptPart::None); // the game's own draws
    EXPECT(PromptPartOf(prompts, Style::Xbox360, 9, letters, hash) == PromptPart::None);
    EXPECT(PromptPartOf(prompts, Style::PlayStation, 6, other, hash) ==
           PromptPart::None); // a face's count, not its indices
    EXPECT(PromptPartOf(prompts, Style::PlayStation, 9, other, hash) == PromptPart::None);
    const uint16_t face7[7] = { 0, 1, 2, 3, 4, 5, 6 };
    EXPECT(PromptPartOf(prompts, Style::PlayStation, 7, face7, hash) == PromptPart::None);
    EXPECT(PromptPartOf(prompts, Style::PlayStation, 4, face, hash) == PromptPart::None); // no mesh of 4 indices
    EXPECT(PromptPartOf({}, Style::PlayStation, 6, face, hash) == PromptPart::None);
    EXPECT(PromptPartOf(prompts, Style::PlayStation, 6, nullptr, hash) == PromptPart::None);
    EXPECT(PromptPartOf(prompts, Style::PlayStation, 0, face, hash) == PromptPart::None);
    // The pair, whichever part came first: the face's hash and the letters' hash name the prompt, so A and Y are told apart.
    EXPECT(PromptOf(prompts, 6, faceHash, 9, lettersHash) == &prompts[0]);
    EXPECT(PromptOf(prompts, 6, faceHash, 9, lettersBHash) == &prompts[1]); // the same face, the other button's letters
    EXPECT(PromptOf(prompts, 6, faceHash, 9, 1) == nullptr); // letters of no prompt
    EXPECT(PromptOf(prompts, 6, faceHash + 1, 9, lettersHash) == nullptr); // another face
    EXPECT(PromptOf(prompts, 7, faceHash, 9, lettersHash) == nullptr); // another count
    EXPECT(PromptOf(prompts, 6, faceHash, 3, lettersHash) == nullptr);
    EXPECT(PromptOf({}, 6, faceHash, 9, lettersHash) == nullptr);

    // The planar coordinates: a 2000 x 1000 face in x and y, four vertices of 16 bytes (position 3 x SNORM16 at 0, FLOAT16_2 at
    // 8, halves swapped: v then u); vertex 4 is not part of the draw. Half 1.0 is 0x3C00, 0.5 is 0x3800.
    auto vertex = [](int16_t x, int16_t y, int16_t z) {
        std::vector<uint8_t> bytes(16, 0xEE);
        memcpy(&bytes[0], &x, 2);
        memcpy(&bytes[2], &y, 2);
        memcpy(&bytes[4], &z, 2);
        return bytes;
    };
    auto build = [&] {
        std::vector<uint8_t> vertices;
        for (auto v : { vertex(-1000, -500, 7), vertex(1000, -500, 7), vertex(-1000, 500, 7), vertex(1000, 500, 7),
                 vertex(0, 0, 0) })
            vertices.insert(vertices.end(), v.begin(), v.end());
        return vertices;
    };
    auto uvOf = [](const std::vector<uint8_t>& vertices, int index) {
        uint16_t halves[2];
        memcpy(halves, &vertices[index * 16 + 8], 4);
        return std::pair<int, int>{ halves[1], halves[0] }; // u, v
    };
    const uint16_t quad[6] = { 0, 1, 2, 2, 1, 3 };
    std::vector<uint8_t> vertices = build();
    Prompt flat = prompts[0];
    EXPECT(PlanarUv(flat, vertices, 16, 0, 8, quad, 0));
    EXPECT(uvOf(vertices, 0) == std::pair(0x0000, 0x0000));
    EXPECT(uvOf(vertices, 1) == std::pair(0x3C00, 0x0000));
    EXPECT(uvOf(vertices, 2) == std::pair(0x0000, 0x3C00));
    EXPECT(uvOf(vertices, 3) == std::pair(0x3C00, 0x3C00));
    EXPECT(uvOf(vertices, 4) == std::pair(0xEEEE, 0xEEEE)); // untouched
    vertices = build();
    flat.flipU = true; // left to right reversed
    EXPECT(PlanarUv(flat, vertices, 16, 0, 8, quad, 0));
    EXPECT(uvOf(vertices, 0) == std::pair(0x3C00, 0x0000));
    EXPECT(uvOf(vertices, 3) == std::pair(0x0000, 0x3C00));
    vertices = build();
    EXPECT(PlanarUv(alchemy::MashPrompts[2], vertices, 16, 0, 8, quad, 0));
    EXPECT(uvOf(vertices, 0) == std::pair(0x0000, 0x3C00));
    EXPECT(uvOf(vertices, 3) == std::pair(0x3C00, 0x0000));
    vertices = build();
    flat.flipU = false;
    flat.uAxis = 1, flat.vAxis = 0; // y runs left to right, x top to bottom
    EXPECT(PlanarUv(flat, vertices, 16, 0, 8, quad, 0));
    EXPECT(uvOf(vertices, 1) == std::pair(0x0000, 0x3C00));
    EXPECT(uvOf(vertices, 2) == std::pair(0x3C00, 0x0000));
    // A base vertex moves the indices; the thin axis has no area; a stream too short for an index is skipped.
    vertices = build();
    const uint16_t shifted[3] = { 0, 1, 2 };
    EXPECT(PlanarUv(prompts[0], vertices, 16, 0, 8, shifted, 1)); // vertices 1, 2, 3
    EXPECT(uvOf(vertices, 1) == std::pair(0x3C00, 0x0000));
    EXPECT(uvOf(vertices, 2) == std::pair(0x0000, 0x3C00));
    EXPECT(uvOf(vertices, 3) == std::pair(0x3C00, 0x3C00));
    EXPECT(uvOf(vertices, 0) == std::pair(0xEEEE, 0xEEEE)); // not named: untouched
    // The game's shader stretches u by 4 about the middle: written divided, the ends at 3/8 and 5/8 (half 0.375 is 0x3600, 0.625 0x3900).
    vertices = build();
    Prompt stretched = prompts[0];
    stretched.uScale = 4;
    EXPECT(PlanarUv(stretched, vertices, 16, 0, 8, quad, 0));
    EXPECT(uvOf(vertices, 0) == std::pair(0x3600, 0x0000));
    EXPECT(uvOf(vertices, 3) == std::pair(0x3900, 0x3C00));
    Prompt thin = prompts[0];
    thin.vAxis = 2;
    vertices = build();
    EXPECT(!PlanarUv(thin, vertices, 16, 0, 8, quad, 0)); // z is 7 everywhere: no area
    const uint16_t beyond[3] = { 0, 3, 200 };
    vertices = build();
    EXPECT(PlanarUv(prompts[0], vertices, 16, 0, 8, beyond, 0)); // vertex 200 does not exist: ignored
    EXPECT(!PlanarUv(prompts[0], vertices, 16, 0, 14, quad, 0)); // coordinates do not fit in the stride

    // Keycaps: a French number row shows its digit, not the symbol it types without Shift (the 3 key: '"').
    EXPECT(KeycapLabel("\"", "3", "3") == "3");
    EXPECT(KeycapLabel("&", "1", "1") == "1");
    EXPECT(KeycapLabel("\xC3\xA9", "2", "2") == "2"); // é
    EXPECT(KeycapLabel("3", "#", "3") == "3"); // US: the digit already
    EXPECT(KeycapLabel("O", "O", "O") == "O");
    EXPECT(KeycapLabel("Backspace", "Backspace", "Backspace") == "Bksp");
    EXPECT(KeycapLabel("Keypad 5", "Keypad 5", "Keypad 5") == "Num5");
    EXPECT(KeycapLabel("", "", "Tab") == "Tab"); // no name on the layout: the setting's
    EXPECT(KeycapLabel(nullptr, nullptr, "Tab") == "Tab");

    printf("glyph models test: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
