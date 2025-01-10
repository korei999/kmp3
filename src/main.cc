#include "app.hh"

#include "adt/Vec.hh"
#include "adt/defer.hh"
#include "adt/logs.hh"
#include "defaults.hh"
#include "frame.hh"
#include "adt/FreeList.hh"
#include <cmath>

#ifdef USE_MPRIS
    #include "mpris.hh"
#endif

#include <clocale>
#include <fcntl.h>

#ifdef USE_CHAFA
    #include "platform/chafa/chafa.hh"
#endif

static void
setTermEnv()
{
    const String sTerm = getenv("TERM");
    app::g_sTerm = sTerm;

    if (sTerm == "foot")
        app::g_eTerm = app::TERM::FOOT;
    else if (sTerm == "xterm-kitty")
        app::g_eTerm = app::TERM::KITTY;
    else if (sTerm == "xterm-ghostty")
        app::g_eTerm = app::TERM::GHOSTTY;
    else if (sTerm == "alacritty")
        app::g_eTerm = app::TERM::ALACRITTY;
    else if (sTerm == "xterm-256color")
        app::g_eTerm = app::TERM::XTERM_256COLOR;
    else
        app::g_eTerm = app::TERM::ELSE;

#ifdef USE_CHAFA
    ChafaTermInfo* pTermInfo {};
    ChafaPixelMode pixelMode;
    ChafaCanvasMode mode;

    platform::chafa::detectTerminal(&pTermInfo, &mode, &pixelMode);
    defer( chafa_term_info_unref(pTermInfo) );

    if (pixelMode != CHAFA_PIXEL_MODE_SYMBOLS)
        app::g_bSixelOrKitty = true;
#endif
}

static void
parseArgs(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i)
    {
        auto sArg = String(argv[i]);
        if (sArg.beginsWith("--"))
        {
            if (sArg == "--ansi")
            {
                app::g_eUIFrontend = app::UI_FRONTEND::ANSI;
                LOG_NOTIFY("setting HANDMADE ui\n");
            }
            else if (sArg == "--termbox")
            {
                app::g_eUIFrontend = app::UI_FRONTEND::TERMBOX;
                LOG_NOTIFY("setting TERMBOX ui\n");
            }
            else if (sArg == "--no-image")
            {
                app::g_bNoImage = true;
                LOG_BAD("--no-image: {}\n", app::g_bNoImage);
            }
        }
        else return;
    }
}

static void
startup(int argc, char** argv)
{
    FreeList freeList(SIZE_8M);
    defer( freeList.freeAll() );

    app::g_eUIFrontend = app::UI_FRONTEND::TERMBOX;
    app::g_eMixer = app::MIXER::PIPEWIRE;

    parseArgs(argc, argv);

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

    Player player(&freeList, argc, argv);
    app::g_pPlayer = &player;
    defer( player.destroy() );

    player.m_imgHeight = defaults::IMAGE_HEIGHT;
    player.adjustImgWidth();

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

    if (!player.m_vSongs.empty())
    {
        app::g_bRunning = true;

        /* reopen stdin if pipe was used */
        if (!freopen("/dev/tty", "r", stdin))
            LOG_FATAL("freopen(\"/dev/tty\", \"r\", stdin)\n");

        frame::run();
    }
    else CERR("No accepted input provided\n");
}

int
main(int argc, char** argv)
{
    setlocale(LC_ALL, "");
#ifdef NDEBUG
    close(STDERR_FILENO); /* hide mpg123 and other errors */
#endif

    try
    {
        startup(argc, argv);
    }
    catch (AllocException& ex)
    {
        ex.logErrorMsg();
    }
}
