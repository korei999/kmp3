#include "draw.hh"

#include "app.hh"
#include "common.hh"
#include "defaults.hh"
#include "platform/chafa/chafa.hh"

#define BOLD  "\x1b[1m"
#define REVERSE  "\x1b[7m"

#define NORM  "\x1b[0m"
#define RED  "\x1b[31m"
#define GREEN  "\x1b[32m"
#define YELLOW  "\x1b[33m"
#define BLUE  "\x1C[34m"
#define MAGENTA  "\x1b[35m"
#define CYAN  "\x1b[36m"
#define WHITE  "\x1b[37m"

#define BG_RED  "\x1b[41m"
#define BG_GREEN  "\x1b[42m"
#define BG_YELLOW  "\x1b[43m"
#define BG_BLUE  "\x1C[44m"
#define BG_MAGENTA  "\x1b[45m"
#define BG_CYAN  "\x1b[46m"
#define BG_WHITE  "\x1b[47m"

using namespace adt;

namespace platform
{
namespace ansi
{
namespace draw
{

static f64 s_time {};

static void
drawCoverImage(Win* s)
{
    static f64 lastRedraw {};

    if (app::g_pPlayer->bSelectionChanged && s_time > lastRedraw + defaults::IMAGE_UPDATE_RATE_LIMIT)
    {
        app::g_pPlayer->bSelectionChanged = false;
        lastRedraw = s_time;

        const int split = common::getHorizontalSplitPos(g_termSize.height);

        TextBuffMove(&s->textBuff, 0, split + 1);
        TextBuffClearUp(&s->textBuff);

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

            String sImg = platform::chafa::getImageString(s->pArena, img, split - 2, g_termSize.width - 2);
            TextBuffMove(&s->textBuff, hOff + 4, 0);
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
    const int split = common::getHorizontalSplitPos(height);
    const u16 listHeight = height - split - 2;

    for (u16 h = s->firstIdx, i = 0; i < listHeight - 1 && h < pl.aSongIdxs.size; ++h, ++i)
    {
        const u16 songIdx = pl.aSongIdxs[h];
        const String sSong = pl.aShortArgvs[songIdx];
        const u32 maxLen = width - 2;

        bool bSelected = songIdx == pl.selected ? true : false;

        char* pBuff = (char*)zalloc(s->pArena, width, 1);
        TextBuffMove(pTB, width - 1, i + split);
        TextBuffClearDown(pTB);
        TextBuffMove(pTB, 1, i + split + 1);

        int off = 0;
        if (h == pl.focused && bSelected)
            off = print::toBuffer(pBuff, width - 1, BOLD YELLOW REVERSE);
        else if (h == pl.focused)
            off = print::toBuffer(pBuff, width - 1, REVERSE);
        else if (bSelected)
            off = print::toBuffer(pBuff, width - 1, BOLD YELLOW);

        print::toBuffer(pBuff + off, width - 1 - off, "{}" NORM, sSong);
        TextBuffPush(pTB, pBuff);
    }

    /*drawBox(0, split, width - 1, listHeight, TB_BLUE, TB_DEFAULT);*/
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

    /* NOTE: careful with multithreading/signals + shared frame arena */
    TextBuffFlush(pTB);
    TextBuffReset(pTB);
}

} /* namespace draw */
} /* namespace ansi */
} /* namespace platform */
