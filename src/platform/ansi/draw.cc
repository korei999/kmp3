#include "draw.hh"

#include "app.hh"
#include "common.hh"
#include "defaults.hh"
#include "platform/chafa/chafa.hh"

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

        Opt<ffmpeg::Image> oCoverImg = audio::MixerGetCoverImage(app::g_pMixer);
        if (oCoverImg)
        {
            const auto& img = oCoverImg.data;
            int split = common::getHorizontalSplitPos(g_termSize.height);

            f64 scaleFactor = f64(split - 1) / f64(img.height);
            int scaledWidth = std::round(img.width * scaleFactor / defaults::FONT_ASPECT_RATIO);
            int scaledHeight = std::round(img.height * scaleFactor);
            int hdiff = g_termSize.width - 2 - scaledWidth;
            int vdiff = split - 1 - scaledHeight;
            const int hOff = std::round(hdiff / 2.0);
            const int vOff = std::round(vdiff / 2.0);

            /*platform::chafa::showImage(s->pArena, img, g_termSize.height, g_termSize.width, 0, 0);*/

            String sImg = platform::chafa::getImageString(s->pArena, img, g_termSize.height - 2, g_termSize.width - 2);
            TextBuffCursorAt(&s->textBuff, 0, 0);
            TextBuffClear(&s->textBuff);
            TextBuffPush(&s->textBuff, sImg.pData, sImg.size);
            /*TextBuffPush(&s->textBuff, "\r\n");*/
        }
    }
}

void
update(Win* s)
{
    auto* pTB = &s->textBuff;
    int width = g_termSize.width;
    int height = g_termSize.height;

    s_time = utils::timeNowMS();

    /*TextBuffCursorAt(pTB, 0, 0);*/
    /*TextBuffClear(pTB);*/

    drawCoverImage(s);

    TextBuffCursorAt(pTB, width / 2, height / 2);
    TextBuffPush(pTB, "HELLO BIDEN");

    /* NOTE: careful with multithreading/signals + shared frame arena */
    TextBuffFlush(pTB);
    TextBuffReset(pTB);
}

} /* namespace draw */
} /* namespace ansi */
} /* namespace platform */
