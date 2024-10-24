#include "app.hh"

#include "adt/Arena.hh"
#include "adt/Vec.hh"
#include "adt/defer.hh"
#include "adt/file.hh"
#include "frame.hh"
#include "logs.hh"
#include "platform/pipewire/Mixer.hh"

#include <clocale>
#include <fcntl.h>

int
main(int argc, char** argv)
{
    setlocale(LC_ALL, "");
    Arena arena(SIZE_1K * 8);
    defer( ArenaFreeAll(&arena) );

    Vec<String> aInput(&arena.base, argc);
    if (argc < 2) /* use stdin instead */
    {
        int flags = fcntl(0, F_GETFL, 0);
        fcntl(0, F_SETFL, flags | O_NONBLOCK);

        char* line = nullptr;
        size_t len = 0;
        ssize_t nread = 0;

        while ((nread = getline(&line, &len, stdin)) != -1)
        {
            LOG("len: {}, nread: {}\n", len, nread);
            String s = StringAlloc(&arena.base, line, nread);
            StringRemoveNLEnd(&s);
            VecPush(&aInput, s);
        }

        ::free(line);

        /* make fake `argv` so core code works as usual */
        argc = VecSize(&aInput) + 1;
        argv = (char**)ArenaAlloc(&arena, argc, sizeof(char));

        for (int i = 1; i < argc; ++i)
            argv[i] = aInput[i - 1].pData;
    }

    app::g_argc = argc;
    app::g_argv = argv;

    for (int i = 0; i < argc; ++i)
        VecPush(&app::g_aArgs, &arena.base, {app::g_argv[i]});

    Player player(&arena.base, argc, argv);
    app::g_pPlayer = &player;

    PlayerSetDefaultIdxs(&player);

    u32 longsetSize = 0;
    for (int i = 0; i < argc; ++i)
    {
        VecPush(
            &player.aShortArgvs,
            player.pAlloc,
            file::getPathEnding(app::g_aArgs[i])
        );

        if (app::g_aArgs[i].size > longsetSize)
            longsetSize = app::g_aArgs[i].size;
    }
    player.longestStringSize = longsetSize;
    player.statusAndInfoHeight = 4;
    player.statusToInfoWidthRatio = 0.4;

    platform::pipewire::Mixer mixer(&arena.base);
    platform::pipewire::MixerInit(&mixer);
    defer( platform::pipewire::MixerDestroy(&mixer) );
    app::g_pMixer = &mixer.base;

    if (argc > 1)
    {
        app::g_bRunning = true;

        /* reopen stdin to if pipe was used */
        if (!freopen("/dev/tty", "r", stdin))
            LOG_FATAL("freopen(\"/dev/tty\", \"r\", stdin)\n");

        frame::run();
    }
}
