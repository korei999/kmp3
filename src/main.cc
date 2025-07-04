#include "app.hh"

#include "defaults.hh"
#include "frame.hh"

#ifdef OPT_MPRIS
    #include "mpris.hh"
#endif

#include <clocale>
#include <fcntl.h>
#include <sys/ioctl.h>

#ifdef OPT_CHAFA
    #include "platform/chafa/chafa.hh"
#endif

#include "adt/Vec.hh"
#include "adt/defer.hh"
#include "adt/logs.hh"

using namespace adt;

static void
setTermEnv()
{
    const StringView svTerm = getenv("TERM");
    app::g_svTerm = svTerm;

    if (svTerm == "foot")
        app::g_eTerm = app::TERM::FOOT;
    else if (svTerm == "xterm-kitty")
        app::g_eTerm = app::TERM::KITTY;
    else if (svTerm == "xterm-ghostty")
        app::g_eTerm = app::TERM::GHOSTTY;
    else if (svTerm == "alacritty")
        app::g_eTerm = app::TERM::ALACRITTY;
    else if (svTerm == "xterm-256color")
        app::g_eTerm = app::TERM::XTERM_256COLOR;
    else app::g_eTerm = app::TERM::ELSE;

#ifdef OPT_CHAFA
    ChafaTermInfo* pTermInfo {};
    ChafaPixelMode pixelMode;
    ChafaCanvasMode mode;

    platform::chafa::detectTerminal(&pTermInfo, &mode, &pixelMode);
    defer( chafa_term_info_unref(pTermInfo) );

    if (pixelMode != CHAFA_PIXEL_MODE_SYMBOLS)
    {
        app::g_bSixelOrKitty = true;
    }
    else
    {
        app::g_bChafaSymbols = true;
    #ifndef OPT_CHAFA_SYMBOLS
        app::g_bNoImage = true;
    #endif
    }
#else
    app::g_bNoImage = true;
#endif
}

static void
parseArgs(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i)
    {
        const StringView svArg = argv[i];
        if (svArg.beginsWith("--"))
        {
            if (svArg == "--ansi")
            {
                app::g_eUIFrontend = app::UI::ANSI;
                LOG_NOTIFY("setting HANDMADE ui\n");
            }
            else if (svArg.beginsWith("--volume"))
            {
                if (argc > i + 1)
                {
                    const StringView svNext = argv[++i];
                    app::g_config.volume = svNext.toF64();
                    continue;
                }
                else
                {
                    print::out("missing arg after '--volume'\n");
                    exit(0);
                }
            }
            else if (svArg == "--no-image")
            {
                app::g_bNoImage = true;
                LOG_BAD("--no-image: {}\n", app::g_bNoImage);
            }
            else if (svArg == "--sndio")
            {
#ifdef OPT_SNDIO
                app::g_eMixer = app::MIXER::SNDIO;
#else
                print::out("compiled without sndio\n");
                exit(0);
#endif
            }
            else if (svArg == "--alsa")
            {
#ifdef OPT_ALSA
                app::g_eMixer = app::MIXER::ALSA;
#else
                print::out("compiled without alsa\n");
                exit(0);
#endif
            }
            else if (svArg == "--pipewire")
            {
#ifdef OPT_PIPEWIRE
                app::g_eMixer = app::MIXER::PIPEWIRE;
#else
                print::out("compiled without pipewire\n");
                exit(0);
#endif
            }
            else if (svArg == "--coreaudio")
            {
#ifdef OPT_COREAUDIO
                app::g_eMixer = app::MIXER::COREAUDIO;
#else
                print::out("compiled without coreaudio\n");
                exit(0);
#endif
            }
        }
        else return;
    }
}

static void
startup(int argc, char** argv)
{
    StdAllocator alloc;
    bool bFreeArgv = false;

    app::g_eUIFrontend = app::UI::ANSI;
#if OPT_PIPEWIRE
    app::g_eMixer = app::MIXER::PIPEWIRE;
#elif defined OPT_ALSA
    app::g_eMixer = app::MIXER::ALSA;
#elif defined OPT_SNDIO
    app::g_eMixer = app::MIXER::SNDIO;
#elif defined __APPLE__
    app::g_eMixer = app::MIXER::COREAUDIO;
#endif

    parseArgs(argc, argv);

    setlocale(LC_ALL, "");
#ifdef NDEBUG
    /* Hide mpg123 and other errors. */
    ADT_ASSERT_ALWAYS(freopen("/dev/null", "w", stderr), "");
#endif

    VecManaged<String> aInput;
    defer(
        for (auto& s : aInput) s.destroy(&alloc);
        aInput.destroy()
    );

    int nBytes = 0;
    if (ioctl(STDIN_FILENO, FIONREAD, &nBytes) != -1 && nBytes > 0)
    {
        char* pLine = nullptr;
        size_t len = 0;
        ssize_t nread = 0;

        while ((nread = getline(&pLine, &len, stdin)) != -1)
        {
            auto s = String {&alloc, pLine, nread};
            s.removeNLEnd(true);
            aInput.push(s);
        }

        ::free(pLine);

        /* Make a fake `argv` such that the core code works as usual. */
        argc = aInput.size() + 1;
        argv = alloc.zallocV<char*>(argc);
        bFreeArgv = true;

        for (int i = 1; i < argc; ++i)
            argv[i] = aInput[i - 1].data();
    }

    Player player {&alloc, argc, argv};
    app::g_pPlayer = &player;
    defer( player.destroy() );

    player.m_imgHeight = app::g_config.imageHeight;
    player.adjustImgWidth();

    player.m_eReapetMethod = PLAYER_REPEAT_METHOD::PLAYLIST;
    player.m_bSelectionChanged = true;

    app::decoder().init();
    defer( app::decoder().destroy() );

    app::g_pMixer = &app::allocMixer(&alloc)->init();
    app::mixer().setVolume(app::g_config.volume);
    defer( app::mixer().destroy() );

#ifdef OPT_MPRIS
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
    else
    {
        print::out("No accepted input provided\n");
    }

    if (bFreeArgv) alloc.free(argv);
}

int
main(int argc, char** argv)
{
    static_assert(defaults::MAX_VOLUME != 0.0f);
    static_assert(defaults::UPDATE_RATE > 0);
    static_assert(defaults::IMAGE_UPDATE_RATE_LIMIT > 0);
    static_assert(defaults::MPRIS_UPDATE_RATE > 0.0);
    static_assert(defaults::FONT_ASPECT_RATIO > 0.0);

    try
    {
        startup(argc, argv);
    }
    catch (IException& ex)
    {
        ex.printErrorMsg(stdout);
    }
}
