// The Lux engine's movie player: skipping movies, for test runs and [video] skip_intro_movies.
#include "game.h"
#include "game_hooks.h"
#include "host/settings.h"
#include "memory.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <ppc_config.h>
#include <ppc_context.h>
#include "host/env.h"

namespace
{
    // movie+0 is the path the game built ("game:\movies\fre\legal.bik").
    std::string MoviePath(uint32_t movie)
    {
        std::string path;
        for (uint32_t i = 0; i < 260; i++)
        {
            uint8_t c = g_memory.Read<uint8_t>(movie + i);
            if (c == 0)
                break;
            path.push_back(char(c));
        }
        return path;
    }
}

#ifdef GAME_HOOK_Movie_Update
PPC_FUNC_IMPL(GAME_IMP(Movie_Update));
// Movie_Update(movie): returns movie+0x80, "still playing", which the callers check. SKY_SKIP_MOVIES=1 lets it play
// one frame, then clears the flag, as when the movie reaches its last frame, so the game moves on (test runs). With
// [video] skip_intro_movies=1 the same happens only to the start-up logos (movie+0 holds the movie's path); the story
// movies still play. Each skip is logged as "[movie] skipped <name>".
PPC_FUNC(GAME_HOOK(Movie_Update))
{
    static const bool skipAll = skyenv::Get("SKY_SKIP_MOVIES") != nullptr && atoi(skyenv::Get("SKY_SKIP_MOVIES")) != 0;
    uint32_t movie = ctx.r3.u32;
    GAME_IMP(Movie_Update)(ctx, base);
    // "[movie] <name> ended after <n> updates": a movie that played to its own end (tools/pipelines/ssa_ending.sh).
    static uint32_t counted = 0, updates = 0;
    if (movie != counted)
        counted = movie, updates = 0;
    if (ctx.r3.u8 == 0)
    {
        if (updates != 0)
            fprintf(stderr, "[movie] %s ended after %u updates\n", game::MovieName(MoviePath(movie)).c_str(), updates);
        updates = 0;
        return;
    }
    updates++;
    std::string path = MoviePath(movie);
    bool intro = SkipIntroMovies() && game::IsIntroMovie(path);
    if (!skipAll && !intro)
        return;
    if (intro)
        fprintf(stderr, "[movie] skipped %s\n", game::MovieName(path).c_str());
    g_memory.Write<uint8_t>(movie + 0x80, 0);
    ctx.r3.u64 = 0;
}
#endif
