#include "app.hh"

#include "adt/Vec.hh"
#include "adt/defer.hh"
#include "adt/file.hh"
#include "adt/logs.hh"
#include "defaults.hh"
#include "frame.hh"
#include "adt/FreeList.hh"

#ifdef USE_MPRIS
    #include "mpris.hh"
#endif

#include <clocale>
#include <fcntl.h>

void
setTermEnv()
{
    const String sTerm = getenv("TERM");
    LOG_WARN("ntsTerm: '{}'\n", sTerm);

    if (sTerm == "foot")
        app::g_eTerm = app::TERM::FOOT;
    else if (sTerm == "xterm-kitty")
        app::g_eTerm = app::TERM::KITTY;
    else if (sTerm == "xterm-ghostty")
        app::g_eTerm = app::TERM::GHOSTTY;
    else
        app::g_eTerm = app::TERM::XTERM;

    app::g_sTerm = sTerm;
}

int
main(int argc, char** argv)
{
    setlocale(LC_ALL, "");
#ifdef NDEBUG
    close(STDERR_FILENO); /* hide mpg123 and other errors */
#endif

    FreeList freeList(SIZE_8M);
    defer( freeList.freeAll() );

    app::g_eUIFrontend = app::UI_FRONTEND::ANSI;
    app::g_eMixer = app::MIXER::PIPEWIRE;

    if (argc > 1)
    {
        if (argv[1] == String("--ansi"))
        {
            app::g_eUIFrontend = app::UI_FRONTEND::ANSI;
            LOG_NOTIFY("setting HANDMADE ui\n");
        }
        else if (argv[1] == String("--termbox"))
        {
            app::g_eUIFrontend = app::UI_FRONTEND::TERMBOX;
            LOG_NOTIFY("setting TERMBOX ui\n");
        }
#ifdef USE_NCURSES
        else if (argv[1] == String("--ncurses"))
        {
            app::g_eUIFrontend = app::UI_FRONTEND::NCURSES;
            LOG_NOTIFY("setting NCURSES ui\n");
            return 1;
        }
#endif
    }

    Vec<String> aInput(&freeList, argc);
    if (argc < 2) /* use stdin instead */
    {
        int flags = fcntl(0, F_GETFL, 0);
        fcntl(0, F_SETFL, flags | O_NONBLOCK);

        char* line = nullptr;
        size_t len = 0;
        ssize_t nread = 0;

        while ((nread = getline(&line, &len, stdin)) != -1)
        {
            String s = StringAlloc(&freeList, line, nread);
            s.removeNLEnd();
            aInput.push(s);
        }

        ::free(line);

        /* make fake `argv` so core code works as usual */
        argc = aInput.getSize() + 1;
        argv = (char**)freeList.zalloc(argc, sizeof(argv));

        for (int i = 1; i < argc; ++i)
            argv[i] = aInput[i - 1].data();
    }

    app::g_argc = argc;
    app::g_argv = argv;

    for (int i = 0; i < argc; ++i)
        app::g_aArgs.push(&freeList, {app::g_argv[i]});

    Player player(&freeList, argc, argv);
    app::g_pPlayer = &player;
    defer( player.destroy() );

    player.setDefaultIdxs();

    u32 nAccepted = 0;
    u32 longsetSize = 0;
    for (int i = 0; i < app::g_argc; ++i)
    {
        player.m_aShortArgvs.push(player.m_pAlloc, file::getPathEnding(app::g_aArgs[i]));
        if (player.acceptedFormat(player.m_aShortArgvs.last()))
            ++nAccepted;

        if (app::g_aArgs[i].getSize() > longsetSize)
            longsetSize = app::g_aArgs[i].getSize();
    }
    player.m_longestStringSize = longsetSize;
    player.m_imgHeight = 10;
    player.m_statusToInfoWidthRatio = 0.4;
    player.m_eReapetMethod = PLAYER_REPEAT_METHOD::PLAYLIST;
    player.m_bSelectionChanged = true;

    app::g_pMixer = app::allocMixer(&freeList);
    app::g_pMixer->init();
    app::g_pMixer->setVolume(defaults::VOLUME);
    defer( app::g_pMixer->destroy() );

#ifdef USE_MPRIS
    mpris::initLocks();
    defer(
        if (mpris::g_bReady) mpris::destroy();
        mpris::destroyLocks();
    );
#endif

    setTermEnv();

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
