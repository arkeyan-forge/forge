// Proves our PowerPC decoder (ppc_decoder.cpp) identical to the one XenonRecomp used before (binutils', GPL): every
// word of each executable's code sections at its address, or all 2^32 words. Run by decoder_compare.sh, locally.
//   ArkeyanDecoderCompare <default.xex>...   ArkeyanDecoderCompare --all
#include "decoder_compare.h"
#include "ppc_decoder.h"
#include <atomic>
#include <cstdio>
#include <cstring>
#include <file.h>
#include <image.h>
#include <mutex>
#include <thread>
#include <vector>

namespace
{
    void OurDecode(uint32_t word, uint64_t address, DecodedWord& out)
    {
        ppc_insn insn;
        memset(&insn, 0xA5, sizeof(insn));
        out.size = ppc::DecodeWord(word, address, insn);
        out.valid = insn.opcode != nullptr;
        out.name = out.valid ? insn.opcode->name : "";
        out.id = out.valid ? insn.opcode->id : -1;
        out.instruction = insn.instruction;
        for (int i = 0; i < 8; i++)
        {
            out.operands[i] = insn.operands[i];
            out.hasOperand[i] = out.valid && insn.opcode->operands[i] != 0;
        }
        memcpy(out.text, insn.op_str, sizeof(out.text));
        out.text[sizeof(out.text) - 1] = 0;
    }

    bool Same(const DecodedWord& a, const DecodedWord& b)
    {
        return a.size == b.size && a.valid == b.valid && strcmp(a.name, b.name) == 0 && a.id == b.id &&
               a.instruction == b.instruction && memcmp(a.operands, b.operands, sizeof(a.operands)) == 0 &&
               memcmp(a.hasOperand, b.hasOperand, sizeof(a.hasOperand)) == 0 && strcmp(a.text, b.text) == 0;
    }

    struct Tally
    {
        std::atomic<uint64_t> words{ 0 }, differences{ 0 };
        std::mutex mutex;
        int shown = 0;

        void Check(uint32_t word, uint64_t address)
        {
            DecodedWord reference, ours;
            ReferenceDecode(word, address, reference);
            OurDecode(word, address, ours);
            ++words;
            if (Same(reference, ours))
                return;
            ++differences;
            std::lock_guard<std::mutex> lock(mutex);
            if (shown++ < 20)
                printf("%08llX %08X: was %s '%s', now %s '%s'\n", (unsigned long long)address, word,
                    reference.valid ? reference.name : "(none)", reference.text, ours.valid ? ours.name : "(none)",
                    ours.text);
        }
    };

    // Runs body(first, last) over [0, count) in slices on every core.
    template <typename Body>
    void Parallel(uint64_t count, Body body)
    {
        const uint64_t slice = 1 << 16;
        std::atomic<uint64_t> next{ 0 };
        std::vector<std::thread> threads;
        for (unsigned t = 0; t < std::max(1u, std::thread::hardware_concurrency()); t++)
            threads.emplace_back([&] {
                for (uint64_t first; (first = next.fetch_add(slice)) < count;)
                    body(first, std::min(count, first + slice));
            });
        for (std::thread& thread : threads)
            thread.join();
    }
} // namespace

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "usage: ArkeyanDecoderCompare <default.xex>... | --all\n");
        return 2;
    }
    Tally tally;
    DecodedWord warm;
    ReferenceDecode(0, 0, warm); // the reference builds its tables on first use
    if (strcmp(argv[1], "--all") == 0)
    {
        const uint64_t address = 0x82000000;
        Parallel(uint64_t(1) << 32, [&](uint64_t first, uint64_t last) {
            for (uint64_t word = first; word < last; word++)
                tally.Check(uint32_t(word), address);
        });
        printf("all words at 0x%llX: %llu compared, %llu differences\n", (unsigned long long)address,
            (unsigned long long)tally.words.load(), (unsigned long long)tally.differences.load());
        return tally.differences == 0 ? 0 : 1;
    }
    for (int a = 1; a < argc; a++)
    {
        const std::vector<uint8_t> file = LoadFile(argv[a]);
        if (file.empty())
        {
            fprintf(stderr, "error: cannot read %s\n", argv[a]);
            return 2;
        }
        const Image image = Image::ParseImage(file.data(), file.size());
        const uint64_t before = tally.words, differencesBefore = tally.differences;
        for (const Section& section : image.sections)
        {
            if ((section.flags & SectionFlags_Code) == 0)
                continue;
            const uint8_t* data = section.data;
            Parallel(section.size / 4, [&](uint64_t first, uint64_t last) {
                for (uint64_t i = first; i < last; i++)
                {
                    const uint8_t* p = data + i * 4;
                    const uint32_t word = uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3];
                    tally.Check(word, section.base + i * 4);
                }
            });
        }
        printf("%s: %llu code words compared, %llu differences\n", argv[a], (unsigned long long)(tally.words - before),
            (unsigned long long)(tally.differences - differencesBefore));
    }
    return tally.differences == 0 ? 0 : 1;
}
