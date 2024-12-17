#include "draw.hh"

#include "app.hh"
#include "common.hh"
#include "defaults.hh"
#include "platform/chafa/chafa.hh"

#define NORM "\x1b[0m"
#define BOLD "\x1b[1m"
#define REVERSE "\x1b[7m"

#define RED "\x1b[31m"
#define GREEN "\x1b[32m"
#define YELLOW "\x1b[33m"
#define BLUE "\x1C[34m"
#define MAGENTA "\x1b[35m"
#define CYAN "\x1b[36m"
#define WHITE "\x1b[37m"

#define BG_RED "\x1b[41m"
#define BG_GREEN "\x1b[42m"
#define BG_YELLOW "\x1b[43m"
#define BG_BLUE "\x1C[44m"
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

    const char space = ' ';
    for (int i = y; i < h; ++i)
        for (int j = x; j < w; ++j)
            TextBuffMovePush(&s->textBuff, j, i, &space, 1);
}

static void
clearLine(Win* s, int x, int y, int width)
{
    const int w = utils::minVal(g_termSize.width, width);

    for (int i = y; i < w; ++i)
        TextBuffMovePush(&s->textBuff, i, y, " ");
}

static void
drawCoverImage(Win* s)
{
    static f64 lastRedraw {};
    static u32 prevWidth {};

    if (app::g_pPlayer->bSelectionChanged && s_time > lastRedraw + defaults::IMAGE_UPDATE_RATE_LIMIT)
    {
        app::g_pPlayer->bSelectionChanged = false;
        lastRedraw = s_time;

        const int split = app::g_pPlayer->imgHeight;

        clearArea(s, 0, 0, g_termSize.width, split + 1);
        /*TextBuffClearKittyImages(&s->textBuff);*/

        Opt<ffmpeg::Image> oCoverImg = audio::MixerGetCoverImage(app::g_pMixer);
        if (oCoverImg)
        {
            const auto& img = oCoverImg.data;

            f64 scaleFactor = f64(split - 1) / f64(img.height);
            int scaledWidth = std::round(img.width * scaleFactor / defaults::FONT_ASPECT_RATIO);
            int scaledHeight = std::round(img.height * scaleFactor);
            int hdiff = g_termSize.width - 2 - scaledWidth;
            int vdiff = split - 1 - scaledHeight;
            const int hOff = std::round(hdiff / 2.0);
            const int vOff = std::round(vdiff / 2.0);

            const String sImg = platform::chafa::getImageString(
                s->pArena, img, split, g_termSize.width - 2
            );

            TextBuffMove(&s->textBuff, 1, 1);
            TextBuffPush(&s->textBuff, sImg.pData, sImg.size);
        }
    }
}

static void
drawSongList(Win* s)
{
    const auto& pl = *app::g_pPlayer;
    auto* pTB = &s->textBuff;
    const int width = g_termSize.width;
    const int height = g_termSize.height;
    /*const int split = common::getHorizontalSplitPos(height);*/
    const int split = pl.imgHeight + 1;
    const u16 listHeight = height - split - 2;

    clearArea(s, 0, split, width, height);

    for (u16 h = s->firstIdx, i = 0; i < listHeight - 1 && h < pl.aSongIdxs.size; ++h, ++i)
    {
        const u16 songIdx = pl.aSongIdxs[h];
        const String sSong = pl.aShortArgvs[songIdx];

        bool bSelected = songIdx == pl.selected ? true : false;

        if (h == pl.focused && bSelected)
            TextBuffPush(pTB, BOLD YELLOW REVERSE);
        else if (h == pl.focused)
            TextBuffPush(pTB, REVERSE);
        else if (bSelected)
            TextBuffPush(pTB, BOLD YELLOW);

        TextBuffMovePushGlyphs(pTB, 1, i + split + 1, sSong, width - 2);
        TextBuffPush(pTB, NORM);
    }
}

static void
drawBottomLine(Win* s)
{
    namespace c = common;

    const auto& pl = *app::g_pPlayer;
    auto* pTB = &s->textBuff;
    int height = g_termSize.height;
    int width = g_termSize.width;

    /* selected / focused */
    {
        char* pBuff = (char*)zalloc(s->pArena, 1, width + 1);

        int n = print::toBuffer(pBuff, width, "{} / {}", pl.selected, pl.aShortArgvs.size - 1);
        if (pl.eReapetMethod != PLAYER_REPEAT_METHOD::NONE)
        {
            const char* sArg {};
            if (pl.eReapetMethod == PLAYER_REPEAT_METHOD::TRACK) sArg = "track";
            else if (pl.eReapetMethod == PLAYER_REPEAT_METHOD::PLAYLIST) sArg = "playlist";

            n += print::toBuffer(pBuff + n, width - n, " (repeat {})", sArg);
        }

        TextBuffMovePush(pTB, width - n - 2, height - 1, pBuff, width - 2); }

    if (
        c::g_input.eCurrMode != READ_MODE::NONE ||
        (c::g_input.eCurrMode == READ_MODE::NONE && wcsnlen(c::g_input.aBuff, utils::size(c::g_input.aBuff)) > 0)
    )
    {
        const String sReadMode = c::readModeToString(c::g_input.eLastUsedMode);
        TextBuffMovePushGlyphs(pTB, 1, height - 1, sReadMode, width - 2);
        TextBuffMovePushWideString(pTB, sReadMode.size + 1, height - 1, c::g_input.aBuff, utils::size(c::g_input.aBuff) - 2);

        if (c::g_input.eCurrMode != READ_MODE::NONE) TextBuffHideCursor(pTB, false);
        else TextBuffHideCursor(pTB, true);
    }
    else TextBuffHideCursor(pTB, true);
}

void
update(Win* s)
{
    auto* pTB = &s->textBuff;
    const int width = g_termSize.width;
    const int height = g_termSize.height;
    const int split = common::getHorizontalSplitPos(height);

    s_time = utils::timeNowMS();

    drawCoverImage(s);
    drawSongList(s);
    drawBottomLine(s);

    /* NOTE: careful with multithreading/signals + shared frame arena */
    TextBuffFlush(pTB);
    TextBuffReset(pTB);
}

} /* namespace draw */
} /* namespace ansi */
} /* namespace platform */
