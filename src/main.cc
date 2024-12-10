#include "app.hh"

#include "adt/Vec.hh"
#include "adt/defer.hh"
#include "adt/file.hh"
#include "adt/logs.hh"
#include "defaults.hh"
#include "frame.hh"
#include "platform/pipewire/Mixer.hh"
#include "adt/FreeList.hh"

#ifdef USE_MPRIS
    #include "mpris.hh"
#endif

#include <clocale>
#include <fcntl.h>

int
main(int argc, char** argv)
{
    setlocale(LC_ALL, "");
#ifdef NDEBUG
    close(STDERR_FILENO); /* hide mpg123 and other errors */
#endif

    FreeList alctr(SIZE_8M);
    defer( freeAll(&alctr) );

#ifdef USE_NCURSES
    app::g_eUIBackend = app::UI_BACKEND::NCURSES;
#else
    app::g_eUIBackend = app::UI_BACKEND::TERMBOX;
#endif

    if (argc > 1)
    {
        if (argv[1] == String("--termbox"))
        {
            app::g_eUIBackend = app::UI_BACKEND::TERMBOX;
            LOG_NOTIFY("setting TERMBOX ui\n");
        }
        else if (argv[1] == String("--ncurses"))
        {
#ifdef USE_NCURSES
            app::g_eUIBackend = app::UI_BACKEND::NCURSES;
            LOG_NOTIFY("setting NCURSES ui\n");
#else
            CERR("Program was built without ncurses support.\n");
            return 1;
#endif
        }
        else if (argv[1] == String("--notcurses"))
        {
#ifdef USE_NOTCURSES
            app::g_eUIBackend = app::UI_BACKEND::NOTCURSES;
            LOG_NOTIFY("setting NOTCURSES ui\n");
#else
            CERR("Program was built without notcurses support.\n");
            return 1;
#endif
        }
    }

    Vec<String> aInput(&alctr.super, argc);
    if (argc < 2) /* use stdin instead */
    {
        int flags = fcntl(0, F_GETFL, 0);
        fcntl(0, F_SETFL, flags | O_NONBLOCK);

        char* line = nullptr;
        size_t len = 0;
        ssize_t nread = 0;

        while ((nread = getline(&line, &len, stdin)) != -1)
        {
            String s = StringAlloc(&alctr.super, line, nread);
            StringRemoveNLEnd(&s);
            VecPush(&aInput, s);
        }

        ::free(line);

        /* make fake `argv` so core code works as usual */
        argc = VecSize(&aInput) + 1;
        argv = (char**)zalloc(&alctr, argc, sizeof(argv));

        for (int i = 1; i < argc; ++i)
            argv[i] = aInput[i - 1].pData;
    }

    app::g_argc = argc;
    app::g_argv = argv;

    for (int i = 0; i < argc; ++i)
        VecPush(&app::g_aArgs, &alctr.super, {app::g_argv[i]});

    Player player(&alctr.super, argc, argv);
    app::g_pPlayer = &player;
    defer( PlayerDestroy(&player) );

    PlayerSetDefaultIdxs(&player);

    u32 nAccepted = 0;
    u32 longsetSize = 0;
    for (int i = 0; i < argc; ++i)
    {
        VecPush(
            &player.aShortArgvs,
            player.pAlloc,
            file::getPathEnding(app::g_aArgs[i])
        );
        if (PlayerAcceptedFormat(VecLast(&player.aShortArgvs)))
            ++nAccepted;

        if (app::g_aArgs[i].size > longsetSize)
            longsetSize = app::g_aArgs[i].size;
    }
    player.longestStringSize = longsetSize;
    player.statusAndInfoHeight = 4;
    player.statusToInfoWidthRatio = 0.4;
    player.eReapetMethod = PLAYER_REPEAT_METHOD::PLAYLIST;
    player.bSelectionChanged = true;

    platform::pipewire::Mixer mixer(&alctr.super);
    platform::pipewire::MixerInit(&mixer);
    defer( platform::pipewire::MixerDestroy(&mixer) );

    mixer.base.volume = defaults::VOLUME;
    app::g_pMixer = &mixer.base;

#ifdef USE_MPRIS
    mpris::initLocks();
    defer(
        if (mpris::g_bReady) mpris::destroy();
        mpris::destroyLocks();
    );
#endif

    if (nAccepted > 0)
    {
        app::g_bRunning = true;

        /* reopen stdin if pipe was used */
        if (!freopen("/dev/tty", "r", stdin))
            LOG_FATAL("freopen(\"/dev/tty\", \"r\", stdin)\n");

        frame::run();
    }
    else CERR("No accepted input provided\n");
}
