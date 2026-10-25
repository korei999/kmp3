#include "klib/Ctx.h"
#include "klib/CmdLine.h"
#include "klib/Gpa.h"
#include "klib/WordIt.h"
#include "klib/assert.h"

#include "generated.h"

#include "app.h"
#include "defaults.h"
#include "Player.h"
#include "frame.h"

#include <locale.h>
#include <unistd.h>

K_CMD_LINE_RESULT
main_helpArg(k_CmdLine* pCmdLine, k_CmdLineArg* pCmdArg)
{
    k_Arena* pArena = k_CtxArena();
    k_CmdLinePrintDescriptions(pCmdLine, &pArena->base, stdout);
    return K_CMD_LINE_RESULT_BREAK;
}

K_CMD_LINE_RESULT
main_volumeArg(k_CmdLine* pCmdLine, k_CmdLineArg* pCmdArg, const k_StringView svValue)
{
    const int volume = k_StringViewToInt(svValue, 10);
    app_g_config.volume = K_CLAMP(volume, 0, 100);
    return K_CMD_LINE_RESULT_NEXT;
}

K_CMD_LINE_RESULT
main_noImageArg(k_CmdLine* pCmdLine, k_CmdLineArg* pCmdArg)
{
    app_g_bNoImage = true;
    return K_CMD_LINE_RESULT_NEXT;
}

K_CMD_LINE_RESULT
main_sndioArg(k_CmdLine* pCmdLine, k_CmdLineArg* pCmdArg)
{
#ifdef OPT_SNDIO
    app_g_eMixer = app_MIXER_SNDIO;
    return K_CMD_LINE_RESULT_NEXT;
#else
    k_print(&k_CtxArena()->base, stderr, "compiled without sndio support\n");
    return K_CMD_LINE_RESULT_FAIL;
#endif
}

K_CMD_LINE_RESULT
main_alsaArg(k_CmdLine* pCmdLine, k_CmdLineArg* pCmdArg)
{
#ifdef OPT_ALSA
    app_g_eMixer = app_MIXER_ALSA;
    return K_CMD_LINE_RESULT_NEXT;
#else
    k_print(&k_CtxArena()->base, stderr, "compiled without alsa support\n");
    return K_CMD_LINE_RESULT_FAIL;
#endif
}

K_CMD_LINE_RESULT
main_pipewireArg(k_CmdLine* pCmdLine, k_CmdLineArg* pCmdArg)
{
#ifdef OPT_PIPEWIRE
    app_g_eMixer = app_MIXER_PIPEWIRE;
    return K_CMD_LINE_RESULT_NEXT;
#else
    k_print(&k_CtxArena()->base, stderr, "compiled without pipewire support\n");
    return K_CMD_LINE_RESULT_FAIL;
#endif
}

K_CMD_LINE_RESULT
main_coreaudioArg(k_CmdLine* pCmdLine, k_CmdLineArg* pCmdArg)
{
#ifdef OPT_COREAUDIO
    app_g_eMixer = app_MIXER_COREAUDIO;
    return K_CMD_LINE_RESULT_NEXT;
#else
    k_print(&k_CtxArena()->base, stderr, "compiled without coreaudio support\n");
    return K_CMD_LINE_RESULT_FAIL;
#endif
}

K_CMD_LINE_RESULT
main_logsArg(k_CmdLine* pCmdLine, k_CmdLineArg* pCmdArg, const k_StringView svValue)
{
    k_Arena* pArena = k_CtxArena();

    const int level = k_StringViewToInt(svValue, 10);
    if (level < K_LOGGER_LEVEL_NONE || level > K_LOGGER_LEVEL_DEBUG)
    {
        k_print(&pArena->base, stderr, "invalid log level: {i}\n", level);
        k_CmdLineArgPrintDescription(pCmdArg, &pArena->base, stderr);
        return K_CMD_LINE_RESULT_FAIL;
    }

    k_CtxLogger()->eLogLevel = (K_LOGGER_LEVEL)level;

    return K_CMD_LINE_RESULT_NEXT;
}

K_CMD_LINE_RESULT
main_forceLoggerColorsArg(k_CmdLine* pCmdLine, k_CmdLineArg* pCmdArg)
{
    app_g_bForceLoggerColors = true;
    return K_CMD_LINE_RESULT_NEXT;
}

K_CMD_LINE_RESULT
main_mprisNameArg(k_CmdLine* pCmdLine, k_CmdLineArg* pCmdArg, const k_StringView svValue)
{
    if (svValue.size <= 0)
    {
        k_Arena* pArena = k_CtxArena();
        k_print(&pArena->base, stderr, "invalid mpris name: {PSv}\n", &svValue);
        k_CmdLineArgPrintDescription(pCmdArg, &pArena->base, stderr);
        return K_CMD_LINE_RESULT_FAIL;
    }

    return K_CMD_LINE_RESULT_NEXT;
}

K_CMD_LINE_RESULT
main_versionArg(k_CmdLine* pCmdLine, k_CmdLineArg* pCmdArg)
{
    k_print(&k_CtxArena()->base, stdout, PROJECT_NAME " " PROJECT_VERSION "-" PROJECT_COMMIT_HASH "\n");
    return K_CMD_LINE_RESULT_BREAK;
}

static bool
main_parseArgs(int argc, char** argv, k_CmdLine** ppCmdLine, k_CmdLineArg* pCmdLineArgs, ssize_t argsSize)
{
    k_Arena* pArena = k_CtxArena();
    bool bReturnStatus = true;

    *ppCmdLine = k_CmdLineAlloc(
        &pArena->base, stderr,
        K_SV("kmp3"), K_SV("[option]... [file]..."),
        pCmdLineArgs, argsSize
    );

    if (!*ppCmdLine)
    {
        bReturnStatus = false;
        goto done;
    }

    K_CMD_LINE_RESULT eCmdLineParseResult = k_CmdLineParse(*ppCmdLine, argc, argv);
    if (eCmdLineParseResult != K_CMD_LINE_RESULT_SUCCESS)
    {
        bReturnStatus = false;
        goto done;
    }

done:
    return bReturnStatus;
}

static void
main_checkTermEnv(void)
{
    const char* ntsTermEnv = getenv("TERM");
    app_g_svTerm = K_NTS(ntsTermEnv);

    if (k_StringViewEq(app_g_svTerm, K_SV("foot")))
        app_g_eTerm = app_TERM_FOOT;
    else if (k_StringViewEq(app_g_svTerm, K_SV("xterm-kitty")))
        app_g_eTerm = app_TERM_KITTY;
    else if (k_StringViewEq(app_g_svTerm, K_SV("xterm-ghostty")))
        app_g_eTerm = app_TERM_GHOSTTY;
    else if (k_StringViewEq(app_g_svTerm, K_SV("alacritty")))
        app_g_eTerm = app_TERM_ALACRITTY;
    else if (k_StringViewEq(app_g_svTerm, K_SV("xterm-256color")))
        app_g_eTerm = app_TERM_XTERM_256COLOR;
    else app_g_eTerm = app_TERM_OTHER;
}

static bool
main_startup(int argc, char** argv)
{
    setlocale(LC_ALL, "");

#if OPT_PIPEWIRE
    app_g_eMixer = app_MIXER_PIPEWIRE;
#elif defined OPT_ALSA
    app_g_eMixer = app_MIXER_ALSA;
#elif defined OPT_SNDIO
    app_g_eMixer = app_MIXER_SNDIO;
#elif defined __APPLE__
    app_g_eMixer = app_MIXER_COREAUDIO;
#endif

    k_CmdLineArg aCmdLineArgs[] = {
        (k_CmdLineArg){
            .cShortName = 'h',
            .svLongName = K_SV("help"),
            .svDescription = K_SV("show this message"),
            .pfnHandler = main_helpArg,
        },
        (k_CmdLineArg){
            .cShortName = 'v',
            .svLongName = K_SV("volume"),
            .svDescription = K_SV("value: integer in [0, 100) range"),
            .bNeedsValue = true,
            .pfnValueHandler = main_volumeArg,
        },
        (k_CmdLineArg){
            .svLongName = K_SV("no-image"),
            .svDescription = K_SV("disable cover images"),
            .pfnHandler = main_noImageArg,
        },
        (k_CmdLineArg){
            .svLongName = K_SV("sndio"),
            .svDescription = K_SV("use sndio audio backend"),
            .pfnHandler = main_sndioArg,
        },
        (k_CmdLineArg){
            .svLongName = K_SV("alsa"),
            .svDescription = K_SV("use alsa audio backend"),
            .pfnHandler = main_alsaArg,
        },
        (k_CmdLineArg){
            .svLongName = K_SV("pipewire"),
            .svDescription = K_SV("use pipewire audio backend"),
            .pfnHandler = main_pipewireArg,
        },
        (k_CmdLineArg){
            .svLongName = K_SV("coreaudio"),
            .svDescription = K_SV("use pipewire audio backend"),
            .pfnHandler = main_coreaudioArg,
        },
        (k_CmdLineArg){
            .cShortName = 'l',
            .svLongName = K_SV("logs"),
            .svDescription = K_SV("value: log level [-1, 0, 1, 2, 3] (none, errors, warnings, info, debug)"),
            .bNeedsValue = true,
            .pfnValueHandler = main_logsArg,
        },
        (k_CmdLineArg){
            .svLongName = K_SV("forceLoggerColors"),
            .svDescription = K_SV("force colored output for the logger"),
            .pfnHandler = main_forceLoggerColorsArg,
        },
        (k_CmdLineArg){
            .svLongName = K_SV("mpris-name"),
            .svDescription = K_SV("value: string prefix that will be used for mpris-dbus instance"),
            .bNeedsValue = true,
            .pfnValueHandler = main_mprisNameArg,
        },
        (k_CmdLineArg){
            .svLongName = K_SV("version"),
            .svDescription = K_SV("print " PROJECT_NAME "'s version"),
            .pfnHandler = main_versionArg,
        },
    };
    k_CmdLine* pCmdLine = NULL;
    if (!main_parseArgs(argc, argv, &pCmdLine, aCmdLineArgs, K_ASIZE(aCmdLineArgs)))
        return false;

    k_Arena* pArena = k_CtxArena();
    VecPChar vInput = {0};
    VecChar vReadBuff = {0};
    bool bReturnStatus = true;

    /* Read from pipe, reformat into argc/argv. */
    if (!isatty(STDIN_FILENO))
    {
        char aReadBuff[K_SIZE_1K*4];
        int nRead = 0;
        while (true)
        {
            nRead = read(STDIN_FILENO, aReadBuff, sizeof(aReadBuff));
            if (nRead < 0)
            {
                if (errno == EINTR) continue;
                else break;
            }
            else if (nRead == 0)
            {
                break;
            }
            else if (nRead > 0)
            {
                VecCharPushMany(&vReadBuff, &pArena->base, aReadBuff, nRead);
            }
        }

        if (vReadBuff.size > 0)
        {
            k_StringView svArgs = {.pData = vReadBuff.pData, .size = vReadBuff.size};

            VecPCharPush(&vInput, &pArena->base, (const char**)&argv[0]);

            /* \0 for separator because we modify the array with \0s as we read it. */
            for (k_WordIt word = k_WordItCreate(svArgs, K_SV(" \n\0")); !k_WordItDone(&word); k_WordItNext(&word))
            {
                const k_StringView sv = k_WordItToSv(&word);
                K_ASSERT(sv.pData[sv.size] == '\n' || sv.pData[sv.size] == ' ', ""); /* Avoiding range checks. */
                sv.pData[sv.size] = '\0'; /* These must be null terminated. */
                VecPCharPush(&vInput, &pArena->base, (const char**)&sv.pData);
            }

            argc = vInput.size;
            argv = vInput.pData;
        }
    }

    Player player = {0};
    PlayerInit(&player, (void*)&k_s_gpa.base, argc, argv);
    player.imgHeight = app_g_config.imageHeight;
    app_g_pPlayer = &player;

    main_checkTermEnv();

    if (player.vSongs.size > 0)
    {
        FILE* pFile = freopen("/dev/tty", "r", stdin);
        if (!pFile)
        {
            k_print(&pArena->base, stderr, "freopen() failed somehow\n");
            bReturnStatus = false;
            goto done;
        }

        app_g_eMixer = app_MIXER_DUMMY;
        app_g_pMixer = app_allocMixer(&pArena->base);
        audio_IMixerStart(app_g_pMixer);

        app_g_vol_bRunning = true;
        frame_run();
    }
    else
    {
        k_print(&pArena->base, stderr, "No accepted input provided\n");
        if (pCmdLine) k_CmdLinePrintUsage(pCmdLine, &pArena->base, stdout);
        goto done;
    }

done:
    PlayerDestroy(&player);
    return bReturnStatus;
}

int
main(int argc, char** argv)
{
    k_Ctx* pCtx = k_CtxAllocGlobal(
        (k_LoggerInitOpts){
            .eFlags = K_LOGGER_FLAG_COLORS | K_LOGGER_FLAG_SOURCE,
            .ringBufferSize = K_SIZE_1K*4,
            .fd = STDERR_FILENO,
            .eLogLevel = K_LOGGER_LEVEL_DEBUG,
        },
        (k_ThreadPoolInitOpts){
            .arenaReserve = defaults_CONFIG.arenaReserveVirtualSpace,
        }
    );

    if (!pCtx)
    {
        fprintf(stderr, "runtime allocation failed\n");
        return false;
    }

    main_startup(argc, argv);

    k_CtxDestroyGlobal();
}

#include "klib/unityBuild-inc.h"
#include "app.c"
#include "Player.c"
#include "TextBuff.c"
#include "audio.c"
#include "frame.c"
#include "Window.c"

#ifdef OPT_PIPEWIRE
    #include "platform/pipewire/Mixer.c"
#endif
