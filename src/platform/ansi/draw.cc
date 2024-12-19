#include "draw.hh"

#include "adt/guard.hh"
#include "app.hh"
#include "common.hh"
#include "defaults.hh"
#include "platform/chafa/chafa.hh"

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

static f64 s_time {};

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
    static f64 lastRedraw {};
    static u32 prevWidth {};

    if (app::g_pPlayer->bSelectionChanged && s_time > lastRedraw + defaults::IMAGE_UPDATE_RATE_LIMIT)
    {
        app::g_pPlayer->bSelectionChanged = false;
        lastRedraw = s_time;

        const int split = app::g_pPlayer->imgHeight;

        s->textBuff.clearKittyImages();
        clearArea(s, 1, 1, s->prevImgWidth, split + 1);

        Opt<ffmpeg::Image> oCoverImg = app::g_pMixer->getCoverImage();
        if (oCoverImg)
        {
            const auto& img = oCoverImg.getData();

            f64 scaleFactor = f64(split - 1) / f64(img.height);
            int scaledWidth = std::round(img.width * scaleFactor / defaults::FONT_ASPECT_RATIO);
            int scaledHeight = std::round(img.height * scaleFactor);
            int hdiff = g_termSize.width - 2 - scaledWidth;
            int vdiff = split - 1 - scaledHeight;
            const int hOff = std::round(hdiff / 2.0);
            const int vOff = std::round(vdiff / 2.0);

            s->prevImgWidth = scaledWidth + 3;

            const String sImg = platform::chafa::getImageString(
                s->pArena, img, split, g_termSize.width - 2
            );

            s->textBuff.move(1, 1);
            s->textBuff.push(sImg.pData, sImg.size);
        }
    }
}

static void
info(Win* s)
{
    const auto& pl = *app::g_pPlayer;
    auto& tb = s->textBuff;
    const int hOff = s->prevImgWidth + 1;
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

        if (sLine.size > 0)
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
list(Win* s)
{
    const auto& pl = *app::g_pPlayer;
    auto& tb = s->textBuff;
    const int width = g_termSize.width;
    const int height = g_termSize.height;
    const int split = pl.imgHeight + 1;
    const u16 listHeight = height - split - 2;

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

    /* selected / focused */
    {
        char* pBuff = (char*)s->pArena->zalloc(1, width + 1);

        int n = print::toBuffer(pBuff, width, "{} / {}", pl.selected, pl.aShortArgvs.size - 1);
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
            sReadMode.size + 1,
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

    s_time = utils::timeNowMS();

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

    if (s->bRedraw || app::g_pPlayer->bSelectionChanged)
    {
        s->bRedraw = false;

        coverImage(s);
        info(s);
        list(s);
        bottomLine(s);
    }
}

} /* namespace draw */
} /* namespace ansi */
} /* namespace platform */
