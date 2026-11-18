#include "host/texture_pack.h"
#include "host/collection/datadir.h"
#include "host/collection/portable_pack.h"
#define XXH_INLINE_ALL
#include <xxhash.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <fstream>
#include <mutex>
#include <set>
#include <thread>
#include <unordered_map>
#include <unordered_set>

namespace texpack
{
    namespace
    {
        struct Dumped
        {
            uint64_t hash;
            dds::Format format;
            uint32_t width, height;
            std::vector<uint8_t> data;
        };

        struct State
        {
            std::filesystem::path dumpFolder;
            bool dumping = false;
            std::unordered_map<uint64_t, Replacement> index;

            std::mutex mutex; // the dump queue and the sets below
            std::condition_variable wake;
            std::condition_variable idle;
            std::deque<Dumped> queue;
            bool writing = false;
            std::thread writer;
            bool quit = false;
            std::unordered_set<uint64_t> seen;     // hashes ShouldDump() has answered for
            std::set<std::pair<std::filesystem::path, uint64_t>> failed; // replacements already reported
        } g_state;

        void Writer()
        {
            std::unique_lock lock(g_state.mutex);
            for (;;)
            {
                g_state.wake.wait(lock, [] { return !g_state.queue.empty() || g_state.quit; });
                if (g_state.queue.empty())
                    return;
                Dumped item = std::move(g_state.queue.front());
                g_state.queue.pop_front();
                g_state.writing = true;
                lock.unlock();

                std::error_code error;
                std::filesystem::path path = g_state.dumpFolder / (HashName(item.hash) + ".dds");
                if (!std::filesystem::exists(path, error))
                {
                    std::filesystem::create_directories(g_state.dumpFolder, error);
                    std::span<const uint8_t> level(item.data);
                    std::vector<uint8_t> file = dds::Write(item.format, item.width, item.height, std::span(&level, 1));
                    std::filesystem::path temporary = path;
                    temporary += ".tmp";
                    {
                        std::ofstream out(temporary, std::ios::binary);
                        out.write(reinterpret_cast<const char*>(file.data()), std::streamsize(file.size()));
                        if (!out)
                            fprintf(stderr, "[texpack] cannot write %s\n", datadir::Text(temporary).c_str());
                    }
                    std::filesystem::rename(temporary, path, error);
                }

                lock.lock();
                g_state.writing = false;
                g_state.idle.notify_all();
            }
        }

        bool ParseHashName(const std::string& stem, uint64_t& hash)
        {
            if (stem.size() != 16)
                return false;
            hash = 0;
            for (char c : stem)
            {
                uint32_t digit;
                if (c >= '0' && c <= '9')
                    digit = uint32_t(c - '0');
                else if (c >= 'a' && c <= 'f')
                    digit = uint32_t(c - 'a' + 10);
                else if (c >= 'A' && c <= 'F')
                    digit = uint32_t(c - 'A' + 10);
                else
                    return false;
                hash = hash << 4 | digit;
            }
            return true;
        }

        struct Candidate
        {
            std::filesystem::path path;
            uint64_t offset = 0; // where the DDS starts in `path`
            uint64_t size = 0;   // its bytes; 0: the whole file
            std::string name;    // for the log
            uint64_t hash = 0;
            Replacement replacement;
            std::string why; // empty: usable
        };

        // A candidate named <16 hex digits>.dds; false (logged) for any other .dds name, and for a file that is not one.
        bool Named(std::string_view fileName, uint64_t& hash, size_t& ignored)
        {
            std::string extension(fileName.substr(std::min(fileName.rfind('.'), fileName.size())));
            std::transform(extension.begin(), extension.end(), extension.begin(),
                [](unsigned char c) { return char(tolower(c)); });
            if (extension != ".dds")
                return false;
            if (!ParseHashName(std::string(fileName.substr(0, fileName.size() - extension.size())), hash))
            {
                fprintf(stderr, "[texpack] ignored %s: the name is not 16 hex digits\n", std::string(fileName).c_str());
                ignored++;
                return false;
            }
            return true;
        }

        // Reads the candidates' headers, several at once (a generated pack holds tens of thousands of files: one at a
        // time, opening them was most of the start-up's cost), then adds them in the order they were listed.
        void Index(std::vector<Candidate>& candidates, const std::string& where, size_t ignored,
            std::chrono::steady_clock::time_point started)
        {
            size_t invalid = 0, indexed = 0;
            std::atomic<size_t> next{ 0 };
            auto read = [&] {
                for (size_t i; (i = next++) < candidates.size();)
                {
                    Candidate& candidate = candidates[i];
                    if (!candidate.why.empty())
                        continue;
                    std::ifstream in(candidate.path, std::ios::binary);
                    in.seekg(std::streamoff(candidate.offset));
                    uint8_t header[dds::MaxHeaderBytes];
                    in.read(reinterpret_cast<char*>(header),
                        std::streamsize(
                            candidate.size != 0 ? std::min<uint64_t>(sizeof(header), candidate.size) : sizeof(header)));
                    std::error_code sizeError;
                    uint64_t size =
                        candidate.size != 0 ? candidate.size : std::filesystem::file_size(candidate.path, sizeError);
                    if (!dds::Parse(std::span<const uint8_t>(header, size_t(std::max<std::streamsize>(in.gcount(), 0))),
                            size, candidate.replacement.info, candidate.why) &&
                        candidate.why.empty())
                        candidate.why = "unreadable";
                }
            };
            unsigned workers = unsigned(
                std::min<size_t>(candidates.size() / 256 + 1, std::max(1u, std::thread::hardware_concurrency())));
            workers = std::min(workers, 8u);
            std::vector<std::thread> threads;
            for (unsigned w = 1; w < workers; w++)
                threads.emplace_back(read);
            read();
            for (std::thread& thread : threads)
                thread.join();
            for (Candidate& candidate : candidates)
            {
                if (!candidate.why.empty())
                {
                    fprintf(stderr, "[texpack] ignored %s: %s (the game's texture stays)\n", candidate.name.c_str(),
                        candidate.why.c_str());
                    invalid++;
                    continue;
                }
                candidate.replacement.path = std::move(candidate.path);
                candidate.replacement.offset = candidate.offset;
                g_state.index[candidate.hash] = std::move(candidate.replacement);
                indexed++;
            }
            long long ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started)
                    .count();
            fprintf(stderr, "[texpack] %zu replacement textures in %s (%zu invalid, %zu ignored; %lld ms)\n", indexed,
                where.c_str(), invalid, ignored, ms);
        }

        void IndexFolder(const std::filesystem::path& folder)
        {
            std::error_code error;
            if (!std::filesystem::is_directory(folder, error))
                return;
            auto started = std::chrono::steady_clock::now();
            size_t ignored = 0;
            std::vector<Candidate> candidates;
            for (std::filesystem::recursive_directory_iterator
                     it(folder, std::filesystem::directory_options::skip_permission_denied, error),
                end;
                !error && it != end; it.increment(error))
            {
                if (!it->is_regular_file(error))
                    continue;
                const std::string name = datadir::Text(it->path().filename());
                uint64_t hash;
                if (Named(name, hash, ignored))
                    candidates.push_back({ it->path(), 0, 0, name, hash, {}, {} });
            }
            Index(candidates, datadir::Text(folder), ignored, started);
        }

        // The textures-hd/ entries of a portable copy's pack (host/collection/portable_pack.h); false when it holds none.
        bool IndexPack(const std::filesystem::path& pack)
        {
            auto started = std::chrono::steady_clock::now();
            std::vector<portable::Entry> entries;
            if (!portable::ReadIndex(pack, entries))
                return false;
            size_t ignored = 0;
            std::vector<Candidate> candidates;
            bool any = false;
            for (const portable::Entry& entry : entries)
            {
                if (!entry.name.starts_with(portable::HdTexturesPrefix))
                    continue;
                any = true;
                const std::string name = entry.name.substr(entry.name.rfind('/') + 1);
                uint64_t hash;
                if (Named(name, hash, ignored))
                    candidates.push_back(
                        { pack, entry.offset, entry.size, entry.name, hash, {}, entry.size == 0 ? "empty" : "" });
            }
            if (any)
                Index(candidates, datadir::Text(pack), ignored, started);
            return any;
        }
    }

    uint64_t ContentHash(uint32_t format, uint32_t width, uint32_t height, const void* data, size_t size)
    {
        return XXH3_64bits_withSeed(data, size, uint64_t(format) | uint64_t(width) << 8 | uint64_t(height) << 24);
    }

    std::string HashName(uint64_t hash)
    {
        char name[17];
        snprintf(name, sizeof(name), "%016llx", static_cast<unsigned long long>(hash));
        return name;
    }

    void Configure(const std::filesystem::path& dump, bool dumping, const std::filesystem::path& packs, bool replacing,
        const std::filesystem::path& generated, const std::filesystem::path& portablePack)
    {
        g_state.index.clear();
        g_state.dumpFolder = dump;
        g_state.dumping = dumping;
        if (dumping)
        {
            fprintf(stderr, "[texpack] dumping textures to %s\n", datadir::Text(dump).c_str());
            g_state.writer = std::thread(Writer);
            atexit(Flush);
        }
        // First: the player's own files below replace their entries. A portable copy that holds HD textures reads its
        // own, not the folder's.
        const bool fromPack = !portablePack.empty() && !generated.empty() && IndexPack(portablePack);
        if (!fromPack && !generated.empty())
            IndexFolder(generated);
        if (replacing)
            IndexFolder(packs);
    }

    void ConfigureForGame(
        std::string_view gameId, bool dumping, bool replacing, bool hd, const std::filesystem::path& portablePack)
    {
        std::filesystem::path game = datadir::Game(gameId);
        if (game.empty())
            return;
        Configure(game / "textures-dump", dumping, game / "textures", replacing,
            hd ? datadir::GameFiles(gameId) / "textures-hd" : std::filesystem::path(), portablePack);
    }

    bool Dumping() { return g_state.dumping; }

    bool Active() { return g_state.dumping || !g_state.index.empty(); }

    size_t Files() { return g_state.index.size(); }

    const Replacement* Find(uint64_t hash)
    {
        auto it = g_state.index.find(hash);
        return it != g_state.index.end() ? &it->second : nullptr;
    }

    bool ShouldDump(uint64_t hash)
    {
        if (!g_state.dumping)
            return false;
        std::lock_guard lock(g_state.mutex);
        return g_state.seen.insert(hash).second;
    }

    void Dump(uint64_t hash, dds::Format format, uint32_t width, uint32_t height, std::vector<uint8_t>&& data)
    {
        std::lock_guard lock(g_state.mutex);
        g_state.queue.push_back({ hash, format, width, height, std::move(data) });
        g_state.wake.notify_one();
    }

    bool ReadLevel(const Replacement& replacement, uint32_t level, std::vector<uint8_t>& out)
    {
        const dds::Info& info = replacement.info;
        out.resize(dds::SizeOfLevel(info.format, info.width, info.height, level).Bytes());
        std::ifstream in(replacement.path, std::ios::binary);
        in.seekg(std::streamoff(replacement.offset + dds::LevelOffset(info, level)));
        in.read(reinterpret_cast<char*>(out.data()), std::streamsize(out.size()));
        if (!in)
        {
            std::lock_guard lock(g_state.mutex);
            if (g_state.failed.insert({ replacement.path, replacement.offset }).second)
                fprintf(stderr, "[texpack] cannot read %s: the texture is left black\n",
                    datadir::Text(replacement.path).c_str());
            std::fill(out.begin(), out.end(), 0);
            return false;
        }
        dds::ConvertTexels(info, out.data(), out.size());
        return true;
    }

    void Flush()
    {
        if (!g_state.writer.joinable())
            return;
        {
            std::unique_lock lock(g_state.mutex);
            g_state.idle.wait(lock, [] { return g_state.queue.empty() && !g_state.writing; });
            g_state.quit = true;
            g_state.wake.notify_all();
        }
        g_state.writer.join();
    }
}
