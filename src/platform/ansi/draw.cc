#include "draw.hh"

#include "adt/guard.hh"
#include "app.hh"
#include "common.hh"
#include "defaults.hh"
#include "platform/chafa/chafa.hh"
#include <stdatomic.h>

#define NORM "\x1b[0m"
#define BOLD "\x1b[1m"
#define ITALIC "\x1b[3m"
#define REVERSE "\x1b[7m"

#define RED "\x1b[31m"
#define GREEN "\x1b[32m"
#define YELLOW "\x1b[33m"
#define BLUE "\x1b[34m"
#define MAGENTA "\x1b[35m"
#define CYAN "\x1b[36m"
#define WHITE "\x1b[37m"

#define BG_RED "\x1b[41m"
#define BG_GREEN "\x1b[42m"
#define BG_YELLOW "\x1b[43m"
#define BG_BLUE "\x1b[44m"
#define BG_MAGENTA "\x1b[45m"
#define BG_CYAN "\x1b[46m"
#define BG_WHITE "\x1b[47m"

using namespace adt;

namespace platform
{
namespace ansi
{
namespace draw
{

static void
clearArea(Win* s, int x, int y, int width, int height)
{
    const int w = utils::minVal(g_termSize.width, width);
    const int h = utils::minVal(g_termSize.height, height);

    char* pBuff = (char*)s->pArena->alloc(w + 1, 1);
    memset(pBuff, ' ', w);

    for (int i = y; i < h; ++i)
        s->textBuff.movePush(x, i, pBuff, w);
}

static void
coverImage(Win* s)
{
    if (app::g_pPlayer->bSelectionChanged && s->time > s->lastResizeTime + defaults::IMAGE_UPDATE_RATE_LIMIT)
    {
        app::g_pPlayer->bSelectionChanged = false;

        const int split = app::g_pPlayer->imgHeight;

        s->textBuff.clearKittyImages();
        clearArea(s, 1, 1, s->prevImgWidth, split + 1);

        Opt<ffmpeg::Image> oCoverImg = app::g_pMixer->getCoverImage();
        if (oCoverImg)
        {
            const auto& img = oCoverImg.getData();

            const platform::chafa::Image chafaImg = platform::chafa::getImageString(
                s->pArena, img, split, g_termSize.width
            );

            s->prevImgWidth = chafaImg.width;

            s->textBuff.move(1, 1);
            s->textBuff.push(chafaImg.s);
        }
    }
}

static void
info(Win* s)
{
    const auto& pl = *app::g_pPlayer;
    auto& tb = s->textBuff;
    const int hOff = s->prevImgWidth + 2;
    const int width = g_termSize.width;

    char* pBuff = (char*)s->pArena->alloc(width + 1, 1);

    auto drawLine = [&](
        const int y,
        const String sPrefix,
        const String sLine,
        const String sColor)
    {
        tb.moveClearLine(hOff - 1, y, TEXT_BUFF_ARG::TO_END);

        memset(pBuff, 0, width + 1);
        u32 n = print::toBuffer(pBuff, width, sPrefix);
        tb.push(NORM);
        tb.movePushGlyphs(hOff, y, pBuff, width - hOff - 1);

        if (sLine.getSize() > 0)
        {
            memset(pBuff, 0, width + 1);
            print::toBuffer(pBuff, width, "{}", sLine);
            tb.push(sColor);
            tb.movePushGlyphs(hOff + n, y, pBuff, width - hOff - 1 - n);
        }

        tb.push(NORM);
    };

    drawLine(1, "title: ", pl.info.title, BOLD ITALIC YELLOW);
    drawLine(2, "album: ", pl.info.album, BOLD);
    drawLine(3, "artist: ", pl.info.artist, BOLD);
}

static void
volume(Win* s)
{
    auto& tb = s->textBuff;
    const auto width = g_termSize.width;
    const int off = s->prevImgWidth + 2;
    const f32 vol = app::g_pMixer->m_volume;
    const bool bMuted = app::g_pMixer->m_bMuted;
    constexpr String fmt = "volume: %3d";
    const int maxVolumeBars = (width - off - fmt.getSize() - 2) * vol * 1.0f/defaults::MAX_VOLUME;

    auto volumeColor = [&](int i) -> String {
        f32 col = f32(i) / (f32(width - off - fmt.getSize() - 2 - 1) * (1.0f/defaults::MAX_VOLUME));

        if (col <= 0.33f) return GREEN;
        else if (col > 0.33f && col <= 0.66f) return GREEN;
        else if (col > 0.66f && col <= 1.00f) return YELLOW;
        else return RED;
    };

    const String col = bMuted ? BLUE : volumeColor(maxVolumeBars - 1);

    tb.push(NORM);
    tb.moveClearLine(off - 1, 6, TEXT_BUFF_ARG::TO_END);

    char* pBuff = (char*)s->pArena->zalloc(1, width + 1);
    u32 n = snprintf(pBuff, width, fmt.data(), int(std::round(app::g_pMixer->m_volume * 100.0)));
    tb.push(col);
    tb.push(BOLD);
    tb.movePushGlyphs(off, 6, {pBuff, n}, width - off);
    tb.push(NORM);

    for (int i = off + n + 1, nTimes = 0; i < width && nTimes < maxVolumeBars; ++i, ++nTimes)
    {
        String col;
        wchar_t wc;
        if (bMuted)
        {
            tb.push(NORM);
            col = BLUE;
            wc = common::CHAR_VOL;
        }
        else
        {
            tb.push(BOLD);
            col = volumeColor(nTimes);
            wc = common::CHAR_VOL_MUTED;
        }

        tb.push(col);
        tb.movePushWideString(i, 6, &wc, 3);
    }
}

static void
time(Win* s)
{
    auto& tb = s->textBuff;
    const auto width = g_termSize.width;
    const int off = s->prevImgWidth + 2;

    tb.push(NORM);
    tb.moveClearLine(off - 1, 9, TEXT_BUFF_ARG::TO_END);

    String sTime = common::allocTimeString(s->pArena, width);
    tb.movePushGlyphs(off, 9, sTime, width - off);
}

static void
timeSlider(Win* s)
{
    auto& tb = s->textBuff;
    const auto& mix = *app::g_pMixer;
    const auto width = g_termSize.width;
    const int xOff = s->prevImgWidth + 2;
    const int yOff = 10;

    tb.push(NORM);
    tb.moveClearLine(xOff - 1, yOff, TEXT_BUFF_ARG::TO_END);

    int n = 0;

    /* play/pause indicator */
    {
        bool bPaused = mix.m_bPaused.load(memory_order_relaxed);
        const char* ntsIndicator = bPaused ? "II" : "I>";

        tb.movePush(xOff, yOff, ntsIndicator);

        n += strlen(ntsIndicator);
    }

    /* time slider */
    {
        const int wMax = width - xOff - n;
        const auto& time = mix.m_currentTimeStamp;
        const auto& maxTime = mix.m_totalSamplesCount;
        const f64 timePlace = (f64(time) / f64(maxTime)) * (wMax - n - 1);

        for (long i = n + 1, t = 0; i < wMax; ++i, ++t)
        {
            wchar_t wch = L'━';
            if (t == std::floor(timePlace)) wch = L'╋';

            tb.movePushWideString(xOff + i, yOff, &wch, 3);
        }
    }
}

static void
list(Win* s)
{
    const auto& pl = *app::g_pPlayer;
    auto& tb = s->textBuff;
    const int width = g_termSize.width;
    const int height = g_termSize.height;
    const int split = pl.imgHeight + 1;
    const u16 listHeight = height - split - 2;

    tb.push(NORM);
    for (u16 h = s->firstIdx, i = 0; i < listHeight - 1; ++h, ++i)
    {
        if (h < pl.aSongIdxs.getSize())
        {
            const u16 songIdx = pl.aSongIdxs[h];
            const String sSong = pl.aShortArgvs[songIdx];

            bool bSelected = songIdx == pl.selected ? true : false;

            if (h == pl.focused && bSelected)
                tb.push(BOLD YELLOW REVERSE);
            else if (h == pl.focused)
                tb.push(REVERSE);
            else if (bSelected)
                tb.push(BOLD YELLOW);

            tb.move(0, i + split + 1);
            tb.clearLine(TEXT_BUFF_ARG::EVERYTHING);
            tb.movePushGlyphs(1, i + split + 1, sSong, width - 2);
            tb.push(NORM);
        }
        else
        {
            tb.moveClearLine(0, i + split + 1, TEXT_BUFF_ARG::EVERYTHING);
        }
    }
}

static void
bottomLine(Win* s)
{
    namespace c = common;

    const auto& pl = *app::g_pPlayer;
    auto& tb = s->textBuff;
    int height = g_termSize.height;
    int width = g_termSize.width;

    tb.moveClearLine(0, height - 1, TEXT_BUFF_ARG::EVERYTHING);
    tb.push(NORM);

    /* selected / focused */
    {
        char* pBuff = (char*)s->pArena->zalloc(1, width + 1);

        int n = print::toBuffer(pBuff, width, "{} / {}", pl.selected, pl.aShortArgvs.getSize() - 1);
        if (pl.eReapetMethod != PLAYER_REPEAT_METHOD::NONE)
        {
            const char* sArg {};
            if (pl.eReapetMethod == PLAYER_REPEAT_METHOD::TRACK) sArg = "track";
            else if (pl.eReapetMethod == PLAYER_REPEAT_METHOD::PLAYLIST) sArg = "playlist";

            n += print::toBuffer(pBuff + n, width - n, " (repeat {})", sArg);
        }

        tb.movePush(width - n - 1, height - 1, pBuff, width - 1); }

    if (
        c::g_input.eCurrMode != WINDOW_READ_MODE::NONE ||
        (c::g_input.eCurrMode == WINDOW_READ_MODE::NONE &&
         wcsnlen(c::g_input.aBuff, utils::size(c::g_input.aBuff)) > 0)
    )
    {
        const String sReadMode = c::readModeToString(c::g_input.eLastUsedMode);
        tb.movePushGlyphs(1, height - 1, sReadMode, width - 1);
        tb.movePushWideString(
            sReadMode.getSize() + 1,
            height - 1,
            c::g_input.aBuff,
            utils::size(c::g_input.aBuff) - 2
        );

        if (c::g_input.eCurrMode != WINDOW_READ_MODE::NONE) tb.hideCursor(false);
        else tb.hideCursor(true);
    }
    else tb.hideCursor(true);
}

void
update(Win* s)
{
    guard::Mtx lock(&s->mtxUpdate);

    auto& tb = s->textBuff;
    const int width = g_termSize.width;
    const int height = g_termSize.height;

    s->time = utils::timeNowMS();

    defer(
        tb.flush();
        tb.reset();
    );

    if (width <= 40 || height <= 15)
    {
        tb.clear();
        return;
    }

    if (s->bClear)
    {
        s->bClear = false;
        tb.clear();
    }

    time(s);
    timeSlider(s);

    if (s->bRedraw || app::g_pPlayer->bSelectionChanged)
    {
        s->bRedraw = false;

        coverImage(s);
        info(s);
        volume(s);
        list(s);
        bottomLine(s);
    }
}

} /* namespace draw */
} /* namespace ansi */
} /* namespace platform */
