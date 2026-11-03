#include "Window.h"

#include "klib/Ctx.h"
#include "klib/Gpa.h"

#include "app.h"
#include "keybinds.h"

#include <math.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <fcntl.h>
#include <poll.h>

static bool WindowRawModeEnable(Window* s);
static void WindowRawModeDisable(Window* s);
static void WindowAdjustListHeight(Window* s);
static int WindowCalcImageHeightSplit(Window* s);
static void WindowSigwinchHandler(int);
static void WindowResize(Window* s);
static void WindowFixFirstIdx(Window* s);
static void WindowProcInput(Window* s);
static void WindowProcMouse(Window* s, WindowMouseInput in);

static WindowInput WindowReadStdin(Window* s, const int timeoutMs);
static input_READ_STATUS WindowReadWChar(Window* s);
static WindowMouseInput WindowParseMouse(Window* s, const char* pBuff, ssize_t buffSize, ssize_t nRead);
static int WindowParseSeq(Window* s, const char* pBuff, ssize_t buffSize, ssize_t nRead);

static k_String WindowAllocTimeString(Window* s, k_Arena* pArena);
K_ALWAYS_INLINE static TEXT_BUFF_STYLE WindowVolumeStringColorToStyle(const int x);

static void WindowTooSmall(Window* s, int width, int height);
static void WindowUpdateTitle(Window* s);
static void WindowDrawTime(Window* s);
static void WindowDrawTimeSlider(Window* s);
static void WindowDrawVolume(Window* s);
static void WindowDrawInfo(Window* s);
static void WindowDrawSongList(Window* s);
static void WindowDrawScrollBar(Window* s);
static void WindowDrawBottomLine(Window* s);
static void WindowDrawErrorMsg(Window* s);

#ifdef OPT_CHAFA
void WindowDrawCoverImage(Window* s);
#endif

#ifdef OPT_CHAFA
void
WindowDrawCoverImage(Window* s)
{
    Player* pPl = app_g_pPlayer;

    const k_time_Type time = K_MAX(s->lastResizeTime, s->time);
    if (pPl->bSelectionChanged || (pPl->bRedrawImage && (k_time_diff(time, s->lastImageRedrawTime) >= K_TIME_MSEC*app_g_config.imageUpdateRateLimit))
        /* Prevent to redraw too often if window is getting resized too aggressively. */
    )
    {
        pPl->bRedrawImage = false;
        pPl->bSelectionChanged = false;

        const int split = pPl->imgHeight;

        if (!app_g_bChafaSymbols) TextBuffClearKittyImages(&s->textBuff);
        TextBuffForceClean(&s->textBuff, 1, 1, s->prevImgWidth + 1, split + 1);

        Image img = ffmpeg_DecoderGetCoverImage(&app_g_decoder);

        platform_chafa_IMAGE_LAYOUT eLayout = app_g_bSixelOrKitty ? platform_chafa_IMAGE_LAYOUT_RAW : platform_chafa_IMAGE_LAYOUT_LINES;

        /* NOTE: using textBuff's dedicated image arena */
        platform_chafa_Image chafaImg = platform_chafa_allocImage(
            &s->textBuff.imgArena.base, eLayout, img, split, s->termSize.width
        );

        TextBuffDrawImage(&s->textBuff, 1, 1, &chafaImg);

        s->prevImgWidth = chafaImg.width;
        s->lastImageRedrawTime = k_time_now();
    }
}
#endif

static k_String
WindowAllocTimeString(Window* s, k_Arena* pArena)
{
    audio_IMixer* pMix = app_g_pMixer;

    const double sampleRateRatio = (double)pMix->sampleRate / (double)pMix->changedSampleRate;

    const uint64_t t = round((pMix->currMs / 1000.0) * sampleRateRatio);
    const uint64_t totalT = round((audio_IMixerGetTotalMS(pMix) / 1000.0) * sampleRateRatio);

    const uint64_t currMin = t / 60;
    const uint64_t currSec = t - (60 * currMin);

    const uint64_t maxMin = totalT / 60;
    const uint64_t maxSec = totalT - (60 * maxMin);

    k_print_Builder pb = {0};
    k_print_BuilderInit(&pb, (k_print_BuilderInitOpts){.pAllocOrNull = &pArena->base, .preallocOrBufferSize = s->termSize.width + 1});
    k_print_BuilderPrintSv(&pb, K_SV("time: {u64}:{:2 > f0:u64} / {u64}:{:2 > f0:u64}"), currMin, currSec, maxMin, maxSec);
    if (pMix->sampleRate != pMix->changedSampleRate)
        k_print_BuilderPrintSv(&pb, K_SV(" ({i}% speed)"),
            (int)round(((double)pMix->changedSampleRate / (double)pMix->sampleRate) * 100.0)
        );

    return k_StringCreateSv(&pArena->base, k_print_BuilderToSv(&pb));
}

K_ALWAYS_INLINE static TEXT_BUFF_STYLE
WindowVolumeStringColorToStyle(const int x)
{
#define STYLE(style) TEXT_BUFF_STYLE_##style
        if (x <= 33) return STYLE(GREEN);
        else if (x > 33 && x <= 66) return STYLE(GREEN);
        else if (x > 66 && x <= 100) return STYLE(YELLOW);
        else return STYLE(RED);
#undef STYLE
}

static bool
WindowRawModeEnable(Window* s)
{
    if (tcgetattr(STDIN_FILENO, &s->termiosOg) == -1)
    {
        K_CTX_LOG_ERROR("tcgetattr() failed");
        return false;
    }

    struct termios raw = s->termiosOg;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG);
    raw.c_cc[VMIN] = 0; /* disable blocking on read() */
    /*raw.c_cc[VTIME] = 1;*/

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
    {
        K_CTX_LOG_ERROR("tcgetattr() failed");
        return false;
    }

    return true;
}

static void
WindowRawModeDisable(Window* s)
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &s->termiosOg);
}

static void
WindowAdjustListHeight(Window* s)
{
    const int split = WindowCalcImageHeightSplit(s);
    const int height = s->termSize.height;
    s->listHeight = height - split - 2;
}

static int
WindowCalcImageHeightSplit(Window* s)
{
    const Image img = ffmpeg_DecoderGetCoverImage(&app_g_decoder);
    if (img.height > 0 && img.width > 0)
        return app_g_pPlayer->imgHeight + 1;
    else return 12; /* Default offset from the top to the start of the list. */
}

static void
WindowSigwinchHandler(int sig)
{
    (void)sig;
    app_g_pWin->bNeedsResize = true;
}

static void
WindowResize(Window* s)
{
    Player* pPl = app_g_pPlayer;
    pPl->bRedrawImage = true;

    s->termSize = WindowTermSizeGet();
    K_CTX_LOG_DEBUG("term: {PTermSize}", &s->termSize);

    TextBuffResize(&s->textBuff, s->termSize.width, s->termSize.height);
    s->lastResizeTime = k_time_now();

    s->bClear = true;
    WindowAdjustListHeight(s);
    WindowFixFirstIdx(s);
}

static void
WindowFixFirstIdx(Window* s)
{
    Player* pPl = app_g_pPlayer;
    const int16_t listHeight = s->listHeight - 2;

    const int64_t focused = pPl->focusedI;
    int16_t first = s->firstIdx;

    if (pPl->vSearchIdxs.size < listHeight) /* List is too small anyway case. */
    {
        first = 0;
        goto done;
    }
    else if (focused > first + listHeight) /* Scroll down case. */
    {
        first = focused - listHeight;
    }
    else if (focused < first) /* Scroll up case. */
    {
        first = focused;
    }

    /* Case when we are at the last page but the list is not long enough. */
    if (focused >= pPl->vSearchIdxs.size - listHeight - 1)
    {
        const int16_t maxListSizeDiff = (first + listHeight + 1) - pPl->vSearchIdxs.size;
        if (maxListSizeDiff > 0 && first >= maxListSizeDiff)
            first -= maxListSizeDiff;
    }

done:
    s->firstIdx = first;
}

static void
WindowProcInput(Window* s)
{
    const WindowInput in = WindowReadStdin(s, app_g_config.updateRate);

    s->bUpdateFirstIdx = false;

    /* BUG: Touchpad double clicks are two release events in the row for some reason. */
    if (in.mouse.eKey == WINDOW_MOUSE_KEY_RELEASE && s->lastInput.mouse.eKey == WINDOW_MOUSE_KEY_RELEASE)
    {
        WindowMouseInput mi = s->lastInput.mouse;
        mi.eKey = WINDOW_MOUSE_KEY_LEFT;
        WindowProcMouse(s, mi);
    }

    switch (in.eType)
    {
        case WINDOW_INPUT_TYPE_KB:
        {
            for (ssize_t i = 0; i < K_ASIZE(keybinds_aKeys); ++i)
            {
                const keybinds_Key* pK = keybinds_aKeys + i;
                if ((pK->key > 0 && pK->key == in.key) || (pK->ch > 0 && pK->ch == in.key))
                    keybinds_exec(pK->pfn, pK->arg);
            }
        }
        break;

        case WINDOW_INPUT_TYPE_MOUSE:
        WindowProcMouse(s, in.mouse);
        break;

        case WINDOW_INPUT_TYPE_TIMEOUT:
        break;
    }

    s->lastInput = in;
}

static void
WindowProcMouse(Window* s, WindowMouseInput in)
{
    if (in.eKey == WINDOW_MOUSE_KEY_RELEASE)
    {
        s->bPressed = false;
        s->bHoldTimeSlider = false;
        s->bHoldScrollBar = false;
        return;
    }

    Player* pPl = app_g_pPlayer;
    const int width = s->termSize.width;
    const int height = s->termSize.height;

    const int xOff = s->prevImgWidth + 2;
    const int yOff = WindowCalcImageHeightSplit(s);

    const int listOff = yOff + 1;
    const int timeSliderOff = 10;
    const int volumeOff = 6;

#define HOLD_SCROLL_BAR()                                                                                              \
    {                                                                                                                  \
        s->bHoldScrollBar = true;                                                                                      \
                                                                                                                       \
        const float listSizeFactor = s->listHeight / (float)(pPl->vSearchIdxs.size - 0.9999f);                         \
        const int barHeight = K_MAX(1, s->listHeight * listSizeFactor);                                                \
                                                                                                                       \
        const float sizeToListSize =                                                                                   \
            (float)((pPl->vSearchIdxs.size) + listOff) / (s->listHeight - 0.9999f + barHeight);                        \
        const int tar = round(((in.y - listOff)) * sizeToListSize);                                                    \
        s->firstIdx = K_CLAMP(tar, 0, (pPl->vSearchIdxs.size - s->listHeight) + 1);                                    \
    }

#define HOLD_TIME_SLIDER()                                                                                             \
    {                                                                                                                  \
        s->bHoldTimeSlider = true;                                                                                     \
                                                                                                                       \
        double target = (double)(in.x - xOff - 2) / (double)(width - xOff - 4);                                        \
        target = K_CLAMP(target, 0.0, (double)(width - xOff - 4));                                                     \
        target *= audio_IMixerGetTotalMS(app_g_pMixer);                                                                \
        audio_IMixerSeekMS(app_g_pMixer, target);                                                                      \
    }

    if (s->bHoldScrollBar)
    {
        HOLD_SCROLL_BAR();
        return;
    }

    if (s->bHoldTimeSlider)
    {
        HOLD_TIME_SLIDER();
        return;
    }

    /* click on time slider line */
    if (in.y == timeSliderOff && in.x >= xOff)
    {
        if (in.eKey == WINDOW_MOUSE_KEY_LEFT || in.eKey == WINDOW_MOUSE_KEY_RIGHT)
        {
            /* click on indicator */
            if (in.x >= xOff && in.x <= xOff + 2)
            {
                if (s->bPressed || s->bHoldTimeSlider) goto timeSliderClickDone;

                app_togglePause();
                goto timeSliderClickDone;
            }

            /* click on slider */
            if (width - xOff - 4 > 0)
            {
                if (s->bPressed && !s->bHoldTimeSlider) return;

                HOLD_TIME_SLIDER();
            }
            goto timeSliderClickDone;
        }
        else if (in.eKey == WINDOW_MOUSE_KEY_WHEEL_UP)
        {
            app_seekOff(10000);
        }
        else if (in.eKey == WINDOW_MOUSE_KEY_WHEEL_DOWN)
        {
            app_seekOff(-10000);
        }

timeSliderClickDone:
        s->bPressed = true;
        return;
    }

    /* scroll ontop of volume */
    if (in.y == volumeOff && in.x >= xOff)
    {
        if (in.eKey == WINDOW_MOUSE_KEY_WHEEL_DOWN)
        {
            app_volumeDown(10);
        }
        else if (in.eKey == WINDOW_MOUSE_KEY_WHEEL_UP)
        {
            app_volumeUp(10);
        }
        else if (in.eKey == WINDOW_MOUSE_KEY_LEFT || in.eKey == WINDOW_MOUSE_KEY_RIGHT)
        {
            if (s->bPressed) return;

            s->bPressed = true;
            app_toggleMute();
        }

        return;
    }

    /* click on song list */
    if (in.y < listOff ||
        in.y >= height - 2 ||
        in.y > K_MIN(s->listHeight + yOff, pPl->vSearchIdxs.size + yOff)
    )
    {
        return;
    }

    int target = s->firstIdx + in.y - listOff;
    target = K_CLAMP(
        target,
        s->firstIdx,
        s->firstIdx + height - listOff - 3
    );

    if (in.eKey == WINDOW_MOUSE_KEY_LEFT || in.eKey == WINDOW_MOUSE_KEY_RIGHT)
    {
        /* click on list scrollbar */
        if (in.x == s->termSize.width - 1)
        {
            HOLD_SCROLL_BAR();
            return;
        }
        else
        {
            if (s->bPressed) return;
            else s->bPressed = true;

            const int64_t time = k_time_now();

            {
                PlayerFocus(app_g_pPlayer, target);

                if (target == s->lastMouseSelection &&
                    k_time_diff(time, s->lastMouseSelectionTime) < app_g_config.doubleClickDelay * K_TIME_MSEC
                )
                {
                    PlayerSelectFocused(app_g_pPlayer);
                }
            }

            s->lastMouseSelection = target;
            s->lastMouseSelectionTime = time;
        }
    }
    else if (in.eKey == WINDOW_MOUSE_KEY_WHEEL_UP)
    {
        s->firstIdx -= app_g_config.mouseScrollStep;
        if (s->firstIdx < 0) s->firstIdx = 0;
    }
    else if (in.eKey == WINDOW_MOUSE_KEY_WHEEL_DOWN)
    {
        s->firstIdx = K_CLAMP(
            s->firstIdx + app_g_config.mouseScrollStep,
            0,
            (pPl->vSearchIdxs.size - s->listHeight) + 1
        );
    }

#undef HOLD_SCROLL_BAR
#undef HOLD_TIME_SLIDER
}

static void
WindowTooSmall(Window* s, int width, int height)
{
    k_ArenaState arenaState = k_ArenaStatePush(s->pArena);
    k_print_Builder pb;
    if (!k_print_BuilderInit(&pb, (k_print_BuilderInitOpts){.pAllocOrNull = &s->pArena->base, .preallocOrBufferSize = 128}))
        goto done;

    const int y = (s->termSize.height - 2) / 2;

    {
        const k_StringView svTooSmall = K_SV("window is too small");
        TextBuffString(&s->textBuff, (s->termSize.width - svTooSmall.size) / 2, y, TEXT_BUFF_STYLE_NORM, svTooSmall, 9999999);
    }

    {
        const k_StringView svMinWidth = k_print_BuilderSvToSv(&pb, k_print_BuilderPrint(&pb, "min: ({i}, {i})", width, height));
        TextBuffString(&s->textBuff, (s->termSize.width - svMinWidth.size) / 2, y + 1, TEXT_BUFF_STYLE_NORM, svMinWidth, 9999999);
    }

    k_print_BuilderReset(&pb);
    const k_StringView svWidth = K_SV("width: ");
    const k_StringView svHeight = K_SV("height: ");
    const k_StringView svWidthVal = k_print_BuilderSvToSv(&pb, k_print_BuilderPrint(&pb, "{i}", s->termSize.width));
    const k_StringView svHeightVal = k_print_BuilderSvToSv(&pb, k_print_BuilderPrint(&pb, "{i}", s->termSize.height));

    TEXT_BUFF_STYLE eWidthStyle = TEXT_BUFF_STYLE_GREEN;
    if (s->termSize.width < width)
        eWidthStyle = TEXT_BUFF_STYLE_RED | TEXT_BUFF_STYLE_BOLD;

    TEXT_BUFF_STYLE eHeightStyle = TEXT_BUFF_STYLE_GREEN;
    if (s->termSize.height < height)
        eHeightStyle = TEXT_BUFF_STYLE_RED | TEXT_BUFF_STYLE_BOLD;

    const ssize_t totalSize = svWidth.size + svHeight.size + svWidthVal.size + svHeightVal.size + 2; /* +2 ", " */

    TextBuffStyleSVPair aStrings[] = {
        {TEXT_BUFF_STYLE_NORM, svWidth},
        {eWidthStyle, svWidthVal},
        {TEXT_BUFF_STYLE_NORM, K_SV(", ")},
        {TEXT_BUFF_STYLE_NORM, svHeight},
        {eHeightStyle, svHeightVal},
    };
    TextBuffStrings(&s->textBuff, (s->termSize.width - totalSize) / 2, y + 2, aStrings, K_ASIZE(aStrings), 999999);

done:
    k_ArenaStateRestore(&arenaState);
}

static void
WindowUpdateTitle(Window* s)
{
    if (!k_StringEq(&s->sTitle, &app_g_pPlayer->info.sTitle))
    {
        k_StringReallocWith(&s->sTitle, &k_GpaInst()->base, k_StringToSv(&app_g_pPlayer->info.sTitle));
        TextBuffSetTitle(&s->textBuff, k_StringToSv(&s->sTitle));
        K_CTX_LOG_DEBUG("new title: '{PS}'", &s->sTitle);
    }
}

static void
WindowDrawTime(Window* s)
{
    k_ArenaState arenaState = k_ArenaStatePush(s->pArena);

    const int off = s->prevImgWidth + 2;

    k_String sTime = WindowAllocTimeString(s, s->pArena);
    TextBuffString(&s->textBuff, off, 9, TEXT_BUFF_STYLE_NORM, k_StringToSv(&sTime), 99999999);

    k_ArenaStateRestore(&arenaState);
}

static void
WindowDrawTimeSlider(Window* s)
{
    const audio_IMixer* pMix = app_g_pMixer;
    const int width = s->termSize.width;
    const int xOff = s->prevImgWidth + 2;
    const int yOff = 10;

    ssize_t n = 0;

    /* play/pause indicator */
    {
        const bool bPaused = k_atomic_U8LoadRelaxed(&pMix->atom_bPaused);
        const k_StringView svIndicator = bPaused ? K_SV("I>") : K_SV("II");

#define STYLE(style) TEXT_BUFF_STYLE_##style
        TextBuffString(&s->textBuff, xOff, yOff, STYLE(BOLD), svIndicator, 999999);

        n += svIndicator.size;
    }

    /* time slider */
    {
        ;
        const int wMax = width - xOff - n;
        const int64_t time = pMix->currentTimeStamp;
        const int64_t maxTime = pMix->nTotalSamples;
        const double timePlace = ((double)time / (double)maxTime) * (wMax - n - 1);

        for (int i = n + 1, t = 0; i < wMax; ++i, ++t)
        {
            const char* nts = t == floor(timePlace) ? "╂" : "─";
            TextBuffString(&s->textBuff, xOff + i, yOff, STYLE(NORM), K_NTS(nts), 9999999);
        }
    }
#undef STYLE
}

static void
WindowDrawVolume(Window* s)
{
    audio_IMixer* pMix = app_g_pMixer;

    const int width = s->termSize.width;
    const int off = s->prevImgWidth + 2;
    const float vol = pMix->volume;
    const bool bMuted = pMix->bMuted;

    k_ArenaState arenaState = k_ArenaStatePush(s->pArena);

    k_print_Builder pb = {0};
    k_print_BuilderInit(&pb, (k_print_BuilderInitOpts){.pAllocOrNull = &s->pArena->base, .preallocOrBufferSize = width + 1});
    k_StringView svPrinted = k_print_BuilderSvToSv(&pb, k_print_BuilderPrintSv(&pb, K_SV("volume: {:>3:i}"), pMix->volume));

    const ssize_t n = pb.size;
    const int nVolumeBars = (width - off - n - 2) * vol / app_g_config.maxVolume;

#define STYLE(style) TEXT_BUFF_STYLE_##style

    const TEXT_BUFF_STYLE eCol = bMuted ? STYLE(BLUE) : WindowVolumeStringColorToStyle(vol);
    TextBuffString(&s->textBuff, off, 6, STYLE(BOLD) | eCol, svPrinted, 9999999);

    for (int i = off + n + 1, nTimes = 0; i < width && nTimes < nVolumeBars; ++i, ++nTimes)
    {
        TEXT_BUFF_STYLE eCol2;
        wchar_t wc[3] = {0};
        if (bMuted)
        {
            eCol2 = STYLE(BLUE) | STYLE(NORM);
            wc[0] = L'▯';
        }
        else
        {
            eCol2 = WindowVolumeStringColorToStyle((float)nTimes / (((float)(width - off - n - 2 - 1) / app_g_config.maxVolume))) | STYLE(BOLD);
            wc[0] = L'▮';
        }

        TextBuffWideString(&s->textBuff, i, 6, eCol2, wc, wcslen(wc), 999999);
    }

#undef STYLE
    k_ArenaStateRestore(&arenaState);
}

static void
WindowDrawInfo(Window* s)
{
    const Player* pPl = app_g_pPlayer;
    const  int hOff = s->prevImgWidth + 2;

#define STYLE(style) TEXT_BUFF_STYLE_##style
#define DRAW_LINE(y, svPrefix, svLine, eStyle)                                                                         \
    {                                                                                                                  \
        const ssize_t n = TextBuffString(&s->textBuff, hOff, y, STYLE(NORM), svPrefix, 9999999);                       \
        if (svLine.size > 0)                                                                                           \
            TextBuffString(&s->textBuff, hOff + n, y, eStyle, svLine, 9999999);                                        \
    }

    DRAW_LINE(1, K_SV("title: "), k_StringToSv(&pPl->info.sTitle), STYLE(BOLD) | STYLE(ITALIC) | STYLE(YELLOW));
    DRAW_LINE(2, K_SV("album: "), k_StringToSv(&pPl->info.sAlbum), STYLE(BOLD));
    DRAW_LINE(3, K_SV("artist: "), k_StringToSv(&pPl->info.sArtist), STYLE(BOLD));

#undef DRAW_LINE
#undef STYLE
}

static void
WindowDrawSongList(Window* s)
{
    const Player* pPl = app_g_pPlayer;
    const int split = WindowCalcImageHeightSplit(s);

    if (s->firstIdx < 0 || s->firstIdx >= pPl->vSearchIdxs.size) return;

    for (ssize_t h = s->firstIdx, i = 0; i < s->listHeight - 1; ++h, ++i)
    {
        if (h >= pPl->vSearchIdxs.size) break;

        const int64_t songIdx = *VecU16GetPConst(&pPl->vSearchIdxs, h);
        const k_StringView svSong = *VecSVGetPConst(&pPl->vShortSongs, songIdx);
        const bool bSelected = songIdx == pPl->selectedI;

#define STYLE(style) TEXT_BUFF_STYLE_##style

        TEXT_BUFF_STYLE eStyle = STYLE(NORM);

        if (h == pPl->focusedI && bSelected)
            eStyle = STYLE(BOLD) | STYLE(YELLOW) | STYLE(REVERSE);
        else if (h == pPl->focusedI)
            eStyle = STYLE(REVERSE);
        else if (bSelected)
            eStyle = STYLE(BOLD) | STYLE(YELLOW);

        /* leave some width space for the scroll bar */
        TextBuffString(&s->textBuff, 1, i + split + 1, eStyle, svSong, s->termSize.width - 3);

#undef STYLE
    }
}

static void
WindowDrawScrollBar(Window* s)
{
#define STYLE(style) TEXT_BUFF_STYLE_##style

    const Player* pPl = app_g_pPlayer;

    if (s->listHeight - 1 >= pPl->vSearchIdxs.size) return;

    const int split = WindowCalcImageHeightSplit(s);
    const float listSizeFactor = s->listHeight / (float)(pPl->vSearchIdxs.size - 0.9999f);
    const int barHeight = K_MAX(1, s->listHeight * listSizeFactor);

    /* bunch of mess to make it look better and not go beyond the list */
    int blockI = s->firstIdx * listSizeFactor;
    if (blockI + barHeight >= s->listHeight - 1)
        blockI -= (blockI + barHeight) - (s->listHeight - 1);

    for (ssize_t i = 0; i < s->listHeight - 1; ++i)
        TextBuffString(&s->textBuff, s->termSize.width - 1, split + i + 1, STYLE(DIM), K_SV("│"), 999999);

    for (ssize_t i = 0; i < barHeight && i + blockI < s->listHeight - 1; ++i)
        TextBuffString(&s->textBuff, s->termSize.width - 1, i + blockI + split + 1, STYLE(DIM), K_SV("█"), 9999999);

#undef STYLE
}

static void
WindowDrawBottomLine(Window* s)
{
    k_Arena* pArena = s->pArena;
    k_ArenaState arenaState = k_ArenaStatePush(pArena);

    const Player* pPl = app_g_pPlayer;
    const int height = s->termSize.height;
    const int width = s->termSize.width;

    /* selected / focused */
    {
        k_print_Builder pb = {0};
        k_print_BuilderInit(&pb, (k_print_BuilderInitOpts){.pAllocOrNull = &pArena->base, .preallocOrBufferSize = width + 1});
        k_print_BuilderPrintSv(&pb, K_SV("{sz} / {sz}"), pPl->selectedI, pPl->vShortSongs.size - 1);
        if (pPl->bQuitOnSongEnd)
        {
            k_print_BuilderPrintSv(&pb, K_SV(" (quit after current)"));
        }
        else if (pPl->eRepeatMethod != PLAYER_REPEAT_METHOD_NONE)
        {
            const char* ntsArg = NULL;
            if (pPl->eRepeatMethod == PLAYER_REPEAT_METHOD_TRACK) ntsArg = "track";
            else if (pPl->eRepeatMethod == PLAYER_REPEAT_METHOD_PLAYLIST) ntsArg = "playlist";

            k_print_BuilderPrintSv(&pb, K_SV(" (repeat {nts})"), ntsArg);
        }
        TextBuffString(&s->textBuff, width - pb.size - 1, height - 1, TEXT_BUFF_STYLE_NORM, k_print_BuilderToSv(&pb), 9999999);
    }

    if (s->inputBuff.eCurrMode != input_READ_MODE_NONE ||
        (s->inputBuff.eCurrMode == input_READ_MODE_NONE && wcsnlen(s->inputBuff.aBuff, K_ASIZE(s->inputBuff.aBuff)) > 0)
    )
    {
        k_print_Builder pb = {0};
        k_print_BuilderInit(&pb, (k_print_BuilderInitOpts){.pAllocOrNull = &pArena->base, .preallocOrBufferSize = width + 1});
        k_print_BuilderPrint(&pb, "{input_READ_MODE}{wnts}{wc}",
            s->inputBuff.eLastUsedMode,
            s->inputBuff.aBuff,
            s->inputBuff.eCurrMode != input_READ_MODE_NONE ? L'█' : L'\0'
        );

        TextBuffString(&s->textBuff, 1, height - 1, TEXT_BUFF_STYLE_NORM, k_print_BuilderToSv(&pb), 99999);
    }

    k_ArenaStateRestore(&arenaState);
}

static void
WindowDrawErrorMsg(Window* s)
{
    Player* pPl = app_g_pPlayer;
    int height = s->termSize.height;

again:
    if (k_StringSize(&s->currMsg.sMsg) <= 0 || s->currMsg.time == PLAYER_MSG_UNTIL_NEXT)
    {
        PlayerMsg newMsg = PlayerPopErrorMsg(pPl);
        if (k_StringSize(&newMsg.sMsg) > 0)
        {
            s->errMsgTime = s->time;
            s->currMsg = newMsg;
            K_CTX_LOG_DEBUG("got msg: '{nts}' size: {sz}\n", k_StringData(&s->currMsg.sMsg), k_StringSize(&s->currMsg.sMsg));
        }
    }

    if (s->inputBuff.eCurrMode == input_READ_MODE_NONE && k_StringSize(&s->currMsg.sMsg) > 0 && s->currMsg.time > 0)
    {
        if (k_time_diff(s->time, s->errMsgTime) >= s->currMsg.time)
        {
            K_CTX_LOG_DEBUG("killing: '{PS}'\n", &s->currMsg.sMsg);
            k_StringDestroy(&s->currMsg.sMsg, &k_GpaInst()->base);
            goto again;
        }

#define STYLE(name) TEXT_BUFF_STYLE_##name

        TEXT_BUFF_STYLE eStyle;
        switch (s->currMsg.eType)
        {
            case PLAYER_MSG_TYPE_NOTIFY:
            eStyle = STYLE(CYAN) | STYLE(ITALIC) | STYLE(UNDERLINE);
            break;

            case PLAYER_MSG_TYPE_WARNING:
            eStyle = STYLE(YELLOW) | STYLE(BOLD);
            break;

            case PLAYER_MSG_TYPE_ERROR:
            eStyle = STYLE(RED) | STYLE(BOLD);
            break;

            default:
            eStyle = STYLE(NORM);
        }

#undef STYLE

        TextBuffString(&s->textBuff, 1, height - 1, eStyle, k_StringToSv(&s->currMsg.sMsg), 9999999);
    }
}

static WindowInput
WindowReadStdin(Window* s, const int timeoutMs)
{
    char aBuff[128] = {0};

    struct pollfd aPollFds[2] = {
        {.fd = STDIN_FILENO, .events = POLLIN},
        {.fd = s->aFdsWakeUp[0], .events = POLLIN},
    };

    poll(aPollFds, K_ASIZE(aPollFds), 1000);
    ssize_t nRead = read(STDIN_FILENO, aBuff, sizeof(aBuff));

    if (aPollFds[1].revents & POLLIN)
    {
        uint64_t t = 0;
        K_UNUSED int _ = read(s->aFdsWakeUp[0], &t, 8);
    }

    if (nRead == 0)
    {
        return (WindowInput){.mouse = WINDOW_INVALID_MOUSE, .eType = WINDOW_INPUT_TYPE_TIMEOUT};
    }
    else if (nRead > 1)
    {
        WindowMouseInput mouse = WindowParseMouse(s, aBuff, sizeof(aBuff), nRead);
        if (!WindowMouseInputEq(&mouse, &WINDOW_INVALID_MOUSE))
        {
            return (WindowInput){
                .mouse = mouse,
                .eType = WINDOW_INPUT_TYPE_MOUSE,
            };
        }

        int k = WindowParseSeq(s, aBuff, sizeof(aBuff), nRead);
        if (k != 0)
        {
            return (WindowInput){
                .key = k,
                .eType = WINDOW_INPUT_TYPE_KB,
            };
        }
    }

    wchar_t wc = {0};
    mbtowc(&wc, aBuff, sizeof(aBuff));

    return (WindowInput){
        .key = wc,
        .eType = WINDOW_INPUT_TYPE_KB,
    };
}

static input_READ_STATUS
WindowReadWChar(Window* s)
{
    WindowInput in = WindowReadStdin(s, app_g_config.updateRate);
    int wc = in.key;

    /* TODO: this should probably be in common::subStringSearch(). */
    if (in.eType == WINDOW_INPUT_TYPE_TIMEOUT)
    {
        return input_READ_STATUS_TIMEOUT;
    }
    else if (wc == 0 || wc == KEY_CTRL_C || wc == KEY_ESC || wc == KEY_ENTER)
    {
        return input_READ_STATUS_DONE;
    }
    else if (wc == KEY_CTRL_W)
    {
        if (s->inputBuff.idx > 0)
            input_BufferZeroOut(&s->inputBuff);
        else if (s->inputBuff.idx <= 0)
            return input_READ_STATUS_BACKSPACE;
    }
    else if (wc == KEY_BACKSPACE)
    {
        if (s->inputBuff.idx > 0)
            s->inputBuff.aBuff[--s->inputBuff.idx] = L'\0';
        else return input_READ_STATUS_BACKSPACE;
    }
    else if (wc)
    {
        if (s->inputBuff.idx < K_ASIZE(s->inputBuff.aBuff) - 1)
            s->inputBuff.aBuff[s->inputBuff.idx++] = wc;
    }

    return input_READ_STATUS_GOOD;
}

/* https://github.com/termbox/termbox2/blob/master/termbox2.h */
static WindowMouseInput
WindowParseMouse(Window* s, const char* pBuff, ssize_t buffSize, ssize_t nRead)
{
    enum
    {
        TYPE_VT200 = 0,
        TYPE_1006,
        TYPE_1015,
        TYPE_MAX
    };

    const char* aCmpMap[TYPE_MAX] = {
        "\x1b[M",
        "\x1b[<",
        "\x1b["
    };

    int type = 0;
    for (; type < TYPE_MAX; type++)
    {
        ssize_t size = strlen(aCmpMap[type]);

        if (buffSize >= size && (strncmp(aCmpMap[type], pBuff, size)) == 0)
            break;
    }

    if (type == TYPE_MAX) return WINDOW_INVALID_MOUSE;

    WindowMouseInput ret = WINDOW_INVALID_MOUSE;

    switch (type)
    {
        case TYPE_VT200:
        {
            if (nRead >= 6)
            {
                int b = pBuff[3] - 0x20;
                int fail = 0;

                switch (b & 3)
                {
                    case 0:
                    ret.eKey = ((b & 64) != 0) ? WINDOW_MOUSE_KEY_WHEEL_UP : WINDOW_MOUSE_KEY_LEFT;
                    break;

                    case 1:
                    ret.eKey = ((b & 64) != 0) ? WINDOW_MOUSE_KEY_WHEEL_DOWN : WINDOW_MOUSE_KEY_MIDDLE;
                    break;

                    case 2:
                    ret.eKey = WINDOW_MOUSE_KEY_RIGHT;
                    break;

                    case 3:
                    ret.eKey = WINDOW_MOUSE_KEY_RELEASE;
                    break;

                    default:
                    ret = WINDOW_INVALID_MOUSE;
                    fail = 1;
                    break;
                }

                if (!fail)
                {
                    /* the coord is 1,1 for upper left */
                    ret.x = ((uint8_t)pBuff[4]) - 0x21;
                    ret.y = ((uint8_t)pBuff[5]) - 0x21;
                }
            }
        }
        break; /* case TYPE_VT200: */

        case TYPE_1006:
        case TYPE_1015:
        {
            ssize_t indexFail = -1;

            enum
            {
                FIRST_M = 0,
                FIRST_SEMICOLON,
                LAST_SEMICOLON,
                FIRST_LAST_MAX
            };

            ssize_t aIndices[FIRST_LAST_MAX] = {indexFail, indexFail, indexFail};
            bool bCapital = 0;

            for (ssize_t i = 0; i < buffSize; ++i)
            {
                if (pBuff[i] == ';')
                {
                    if (aIndices[FIRST_SEMICOLON] == indexFail)
                    {
                        aIndices[FIRST_SEMICOLON] = i;
                    }
                    else
                    {
                        aIndices[LAST_SEMICOLON] = i;
                    }
                }
                else if (aIndices[FIRST_M] == indexFail)
                {
                    if (pBuff[i] == 'm' || pBuff[i] == 'M')
                    {
                        bCapital = (pBuff[i] == 'M');
                        aIndices[FIRST_M] = i;
                    }
                }
            }

            if (aIndices[FIRST_M] == indexFail || aIndices[FIRST_SEMICOLON] == indexFail ||
                aIndices[LAST_SEMICOLON] == indexFail)
            {
                return WINDOW_INVALID_MOUSE;
            }
            else
            {
                int start = (type == TYPE_1015 ? 2 : 3);

                unsigned n1 = strtoul(&pBuff[start], NULL, 10);
                unsigned n2 = strtoul(&pBuff[aIndices[FIRST_SEMICOLON] + 1], NULL, 10);
                unsigned n3 = strtoul(&pBuff[aIndices[LAST_SEMICOLON] + 1], NULL, 10);

                if (type == TYPE_1015)
                {
                    n1 -= 0x20;
                }

                int fail = 0;

                switch (n1 & 3)
                {
                    case 0:
                    ret.eKey = ((n1 & 64) != 0) ? WINDOW_MOUSE_KEY_WHEEL_UP : WINDOW_MOUSE_KEY_LEFT;
                    break;

                    case 1:
                    ret.eKey = ((n1 & 64) != 0) ? WINDOW_MOUSE_KEY_WHEEL_DOWN : WINDOW_MOUSE_KEY_MIDDLE;
                    break;

                    case 2:
                    ret.eKey = WINDOW_MOUSE_KEY_RIGHT;
                    break;

                    case 3:
                    ret.eKey = WINDOW_MOUSE_KEY_RELEASE;
                    break;

                    default:
                    fail = 1;
                    break;
                }

                if (!fail)
                {
                    if (!bCapital)
                    {
                        ret.eKey = WINDOW_MOUSE_KEY_RELEASE;
                    }

                    ret.x = (n2) - 1;
                    ret.y = (n3) - 1;
                }
            }
        }
        break; /* case TYPE_1015: */
    }

    return ret;
}

static int
WindowParseSeq(Window* s, const char* pBuff, ssize_t buffSize, ssize_t nRead)
{
    if (nRead <= 5)
    {
        switch (*(int*)(&pBuff[1]))
        {
            case 17487:
            return KEY_ARROWLEFT;

            case 17231:
            return KEY_ARROWRIGHT;

            case 16719:
            return KEY_ARROWUP;

            case 16975:
            return KEY_ARROWDOWN;

            case 8271195:
            return KEY_PGUP;

            case 8271451:
            return KEY_PGDN;

            case 18511:
            return KEY_HOME;

            case 17999:
            return KEY_END;
        }
    }

    return 0;
}

WindowTermSize
WindowTermSizeGet(void)
{
    WindowTermSize term;

    term.width = term.height = term.pixWidth = term.pixHeight = -1;

#ifdef G_OS_WIN32
    {
        HANDLE chd = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO csb_info;

        if (chd != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(chd, &csb_info))
        {
            term_size.width_cells = csb_info.srWindow.Right - csb_info.srWindow.Left + 1;
            term_size.height_cells = csb_info.srWindow.Bottom - csb_info.srWindow.Top + 1;
        }
    }
#else
    {
        struct winsize w;
        bool bHaveWINSZ = false;

        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) >= 0 || ioctl(STDERR_FILENO, TIOCGWINSZ, &w) >= 0 ||
            ioctl(STDIN_FILENO, TIOCGWINSZ, &w) >= 0
        )
        {
            bHaveWINSZ = true;
        }

        if (bHaveWINSZ)
        {
            term.width = w.ws_col;
            term.height = w.ws_row;
            term.pixWidth = w.ws_xpixel;
            term.pixHeight = w.ws_ypixel;
        }
    }
#endif

    if (term.width <= 0) term.width = -1;
    if (term.height <= 2) term.height = -1;

    /* If .ws_xpixel and .ws_ypixel are filled out, we can calculate
     * aspect information for the font used. Sixel-capable terminals
     * like mlterm set these fields, but most others do not. */

    if (term.pixWidth <= 0 || term.pixHeight <= 0)
    {
        term.pixWidth = -1;
        term.pixHeight = -1;
    }

    return term;
}

ssize_t
WindowTermSizePFormatter(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg)
{
    const WindowTermSize* s = arg;
    return k_print_BuilderPrintFmtArgs(pCtx->pBuilder, pFmtArgs,
        "(width: {i}, height: {i}, pixWidth: {i}, pixHeight: {i})",
        s->width, s->height, s->pixWidth, s->pixHeight
    ).size;
}

bool
WindowInit(Window* s, k_Arena* pArena)
{
    *s = (Window){0};

    s->pArena = pArena;
    s->termSize = WindowTermSizeGet();

    if (pipe(s->aFdsWakeUp) < 0)
    {
        K_CTX_LOG_ERROR("pipe() failed: {nts}", strerror(errno));
        int flags = fcntl(s->aFdsWakeUp[0], F_GETFL, 0);
        fcntl(s->aFdsWakeUp[0], F_SETFL, flags | O_NONBLOCK);
        return false;
    }

    if (!WindowRawModeEnable(s))
    {
        close(s->aFdsWakeUp[0]);
        close(s->aFdsWakeUp[1]);
        return false;
    }

    TextBuffInit(&s->textBuff, s->pArena, s->termSize.width, s->termSize.height);
    WindowAdjustListHeight(s);
    signal(SIGWINCH, WindowSigwinchHandler);

    K_CTX_LOG_DEBUG("start");

    return s->vol_bRunning = true;
}

void
WindowDestroy(Window* s)
{
    WindowRawModeDisable(s);
    TextBuffDestroy(&s->textBuff);
    close(s->aFdsWakeUp[0]);
    close(s->aFdsWakeUp[1]);
    k_StringDestroy(&s->sTitle, &k_GpaInst()->base);
}

void
WindowDraw(Window* s)
{
    s->time = k_time_now();

    if (!s->vol_bRunning) return;

    K_ARENA_SCOPE(s->pArena)
    {
        if (s->bNeedsResize)
        {
            s->bNeedsResize = false;
            WindowResize(s);
        }

        if (s->bClear)
        {
            s->bClear = false;
            TextBuffErase(&s->textBuff);
        }

        TextBuffClean(&s->textBuff);

        if (s->termSize.width < app_g_config.minWidth ||
            s->termSize.height < app_g_config.minHeight
        )
        {
            WindowTooSmall(s, app_g_config.minWidth, app_g_config.minHeight);
        }
        else
        {
#ifdef OPT_CHAFA
            if (!app_g_bNoImage) WindowDrawCoverImage(s);
#endif
            WindowUpdateTitle(s);
            WindowDrawTime(s);
            WindowDrawTimeSlider(s);
            WindowDrawVolume(s);
            WindowDrawInfo(s);
            WindowDrawSongList(s);
            WindowDrawScrollBar(s);
            WindowDrawBottomLine(s);
            WindowDrawErrorMsg(s);
        }

        TextBuffPresent(&s->textBuff);
    }
}

void
WindowProcEvents(Window* s)
{
    if (!s->vol_bRunning) return;

    WindowProcInput(s);
    WindowAdjustListHeight(s);
    if (s->bUpdateFirstIdx)
    {
        WindowFixFirstIdx(s);
        s->bUpdateFirstIdx = false;
    }
}

static input_READ_STATUS WindowReadWCharStub(void* pArg) { Window* s = pArg; return WindowReadWChar(s); }
static void WindowDrawStub(void* pArg) { Window* s = pArg; WindowDraw(s); }

void
WindowSeekFromInput(Window* s)
{
    input_BufferSeekFromInput(
        &s->inputBuff,
        WindowReadWCharStub, s,
        WindowDrawStub, s
    );
}

void
WindowSubStringSearch(Window* s)
{
    input_BufferSubStringSearch(
        &s->inputBuff,
        &s->firstIdx,
        WindowReadWCharStub, s,
        WindowDrawStub, s
    );
}

void
WindowWakeUp(Window* s)
{
    uint64_t t = 1;
    int _ = write(s->aFdsWakeUp[1], &t, sizeof(t));
    (void)_;
}
