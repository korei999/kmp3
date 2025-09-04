#include "app.hh"

#include "defaults.hh"
#include "frame.hh"

#ifdef OPT_MPRIS
    #include "platform/mpris/mpris.hh"
#endif

#include <clocale>
#include <fcntl.h>
#include <sys/ioctl.h>

#ifdef OPT_CHAFA
    #include "platform/chafa/chafa.hh"
#endif

using namespace adt;

static Logger s_logger;
static ArgvParser s_cmdParser;

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
    new(&s_cmdParser) ArgvParser{StdAllocator::inst(), stderr, "[options...] [file...]", argc, argv, {
        {
            .bNeedsValue = false,
            .sOneDash = "h",
            .sTwoDashes = "help",
            .sUsage = "show this message",
            .pfn = [](ArgvParser*, void*, const StringView, const StringView) {
                return ArgvParser::RESULT::SHOW_ALL_USAGE;
            },
        },
        {
            .bNeedsValue = true,
            .sTwoDashes = "volume",
            .sUsage = "set startup volume",
            .pfn = [](ArgvParser*, void*, const StringView, const StringView svVal) {
                app::g_config.volume = svVal.toF64();
                return ArgvParser::RESULT::GOOD;
            },
        },
        {
            .bNeedsValue = false,
            .sTwoDashes = "no-image",
            .sUsage = "disable cover images",
            .pfn = [](ArgvParser*, void*, const StringView, const StringView) {
                app::g_bNoImage = true;
                return ArgvParser::RESULT::GOOD;
            },
        },
        {
            .bNeedsValue = false,
            .sTwoDashes = "sndio",
            .sUsage = "use sndio audio driver",
            .pfn = []([[maybe_unused]] ArgvParser* pSelf, void*, const StringView, const StringView) {
#ifdef OPT_SNDIO
                app::g_eMixer = app::MIXER::SNDIO;
                return ArgvParser::RESULT::GOOD;
#else
                print::toFILE(pSelf->m_pFile, "compiled without sndio support\n");
                return ArgvParser::RESULT::QUIT_NICELY;
#endif
            },
        },
        {
            .bNeedsValue = false,
            .sTwoDashes = "alsa",
            .sUsage = "use alsa audio driver",
            .pfn = []([[maybe_unused]] ArgvParser* pSelf, void*, const StringView, const StringView) {
#ifdef OPT_ALSA
                app::g_eMixer = app::MIXER::ALSA;
                return ArgvParser::RESULT::GOOD;
#else
                print::toFILE(pSelf->m_pFile, "compiled without alsa support\n");
                return ArgvParser::RESULT::QUIT_NICELY;
#endif
            },
        },
        {
            .bNeedsValue = false,
            .sTwoDashes = "pipewire",
            .sUsage = "use pipewire audio driver",
            .pfn = []([[maybe_unused]] ArgvParser* pSelf, void*, const StringView, const StringView) {
#ifdef OPT_PIPEWIRE
                app::g_eMixer = app::MIXER::PIPEWIRE;
                return ArgvParser::RESULT::GOOD;
#else
                print::toFILE(pSelf->m_pFile, "compiled without pipewire support\n");
                return ArgvParser::RESULT::QUIT_NICELY;
#endif
            },
        },
        {
            .bNeedsValue = false,
            .sTwoDashes = "coreaudio",
            .sUsage = "use coreaudio audio driver",
            .pfn = []([[maybe_unused]] ArgvParser* pSelf, void*, const StringView, const StringView) {
#ifdef OPT_COREAUDIO
                app::g_eMixer = app::MIXER::COREAUDIO;
                return ArgvParser::RESULT::GOOD;
#else
                print::toFILE(pSelf->m_pFile, "compiled without pipewire support\n");
                return ArgvParser::RESULT::QUIT_NICELY;
#endif
            },
        },
        {
            .bNeedsValue = true,
            .sOneDash = "l",
            .sTwoDashes = "logs",
            .sUsage = "set log level [-1, 0, 1, 2, 3] (none, errors, warnings, info, debug)",
            .pfn = [](ArgvParser*, void*, const StringView, const StringView svVal) {
                ILogger::LEVEL eLevel = static_cast<ILogger::LEVEL>(svVal.toI64());
                if (eLevel < ILogger::LEVEL::NONE || eLevel > ILogger::LEVEL::DEBUG)
                    return ArgvParser::RESULT::QUIT_BADLY;

                app::g_eLogLevel = eLevel;
                return ArgvParser::RESULT::GOOD;
            },
        },
        {
            .bNeedsValue = false,
            .sTwoDashes = "forceLoggerColors",
            .sUsage = "force colored output for the logger",
            .pfn = [](ArgvParser*, void*, const StringView, const StringView) {
                app::g_bForceLoggerColors = true;
                return ArgvParser::RESULT::GOOD;
            },
        },
        {
            .bNeedsValue = true,
            .sTwoDashes = "mpris-name",
            .sUsage = "set name prefix for mpris-dbus instance",
            .pfn = [](ArgvParser* pSelf, void*, const StringView, const StringView svVal) {
                if (svVal.size() <= 0)
                {
                    print::toFILE(pSelf->m_pFile, "failed to get the name\n");
                    return ArgvParser::RESULT::QUIT_BADLY;
                }
                app::g_config.ntsMprisName = svVal.data();
                return ArgvParser::RESULT::GOOD;
            },
        },
        {
            .bNeedsValue = false,
            .sOneDash = "v",
            .sTwoDashes = "version",
            .sUsage = "print " PROJECT_NAME "'s version",
            .pfn = [](ArgvParser* pSelf, void*, const StringView, const StringView) {
                print::toFILE(pSelf->m_pAlloc, pSelf->m_pFile, PROJECT_NAME " " PROJECT_VERSION "-" PROJECT_COMMIT_HASH "\n");
                return ArgvParser::RESULT::QUIT_NICELY;
            },
        },
    }};

    const ArgvParser::RESULT eResult = s_cmdParser.parse();
    if (eResult == ArgvParser::RESULT::FAILED)
    {
        s_cmdParser.printUsage(StdAllocator::inst());
        exit(1);
    }
    else if (argc <= 1)
    {
        s_cmdParser.printUsage(StdAllocator::inst());
        exit(0);
    }
    else if (eResult == ArgvParser::RESULT::QUIT_NICELY ||
        eResult == ArgvParser::RESULT::SHOW_USAGE ||
        eResult == ArgvParser::RESULT::SHOW_EXTRA ||
        eResult == ArgvParser::RESULT::SHOW_ALL_USAGE
    )
    {
        exit(0);
    }
    else if (eResult == ArgvParser::RESULT::QUIT_BADLY)
    {
        exit(1);
    }
}

static void
startup(int argc, char** argv)
{
    StdAllocator alloc;

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
    defer( s_cmdParser.destroy() );

#ifndef NDEBUG
    app::g_eLogLevel = ILogger::LEVEL::DEBUG;
#endif

#ifndef ADT_LOGGER_DISABLE
    new(&s_logger) Logger{stderr, app::g_eLogLevel, 512, app::g_bForceLoggerColors};
    ILogger::setGlobal(&s_logger);
    defer( s_logger.destroy() );
#endif

    setlocale(LC_ALL, "");

    VecManaged<char*> aInput;
    defer( aInput.destroy() );

    i64 nBytes = 0;
    char* pInput = nullptr;
    defer( alloc.free(pInput) );

    if (ioctl(STDIN_FILENO, FIONREAD, &nBytes) != -1 && nBytes > 0)
    {
        pInput = alloc.zallocV<char>(nBytes + 1);
        int nRead = 0;
        ADT_RUNTIME_EXCEPTION_FMT((nRead = read(STDIN_FILENO, pInput, nBytes)) > 0, "nRead: {}", nRead);
        StringView svArgs {pInput, nBytes};

        aInput.emplace(argv[0]);

        for (auto sv : StringWordIt {svArgs, "\n"})
        {
            ADT_ASSERT(sv.data()[sv.size()] == '\n', "");
            sv.data()[sv.size()] = '\0'; /* These must be null terminated. */
            aInput.emplace(sv.data());
        }

        argc = aInput.size();
        argv = aInput.data();
    }

    Player player {&alloc, argc, argv};
    app::g_pPlayer = &player;
    defer( player.destroy() );

    player.m_imgHeight = app::g_config.imageHeight;
    player.adjustImgWidth();

    player.m_eRepeatMethod = PLAYER_REPEAT_METHOD::PLAYLIST;
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

        /* Hide mpg123 and other errors. */
        if (app::g_eLogLevel == ILogger::LEVEL::NONE)
            ADT_ASSERT_ALWAYS(freopen("/dev/null", "w", stderr), "");

        ADT_RUNTIME_EXCEPTION_FMT(freopen("/dev/tty", "r", stdin), "{}", strerror(errno));

        frame::run();
    }
    else
    {
        print::err("No accepted input provided\n");
        s_cmdParser.printUsage(StdAllocator::inst());
    }
}

int
main(int argc, char** argv)
{
    static_assert(defaults::MAX_VOLUME != 0.0f);
    static_assert(defaults::UPDATE_RATE > 0);
    static_assert(defaults::IMAGE_UPDATE_RATE_LIMIT > 0);
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
