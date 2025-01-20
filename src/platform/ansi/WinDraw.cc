#include "Win.hh"

#include "adt/guard.hh"
#include "app.hh"
#include "defaults.hh"
#include "platform/chafa/chafa.hh"
#include "adt/ScratchBuffer.hh"

using namespace adt;

namespace platform::ansi
{

thread_local static u8 tls_aMemBuff[SIZE_8K] {};
thread_local static ScratchBuffer tls_scratch(tls_aMemBuff);

void
Win::coverImage()
{
    auto& pl = *app::g_pPlayer;
    auto& mix = *app::g_pMixer;

    if (pl.m_bSelectionChanged && m_time > m_lastResizeTime + defaults::IMAGE_UPDATE_RATE_LIMIT)
    {
        pl.m_bSelectionChanged = false;

        const int split = pl.m_imgHeight;

        m_textBuff.clearKittyImages(); /* shouldn't hurt if TERM is not kitty */
        m_textBuff.clearImage(1, 1, m_prevImgWidth + 1, split + 1);

        Opt<Image> oCoverImg = mix.getCoverImage();
        if (oCoverImg)
        {
            const auto& img = oCoverImg.value();

            namespace ch = platform::chafa;

            ch::IMAGE_LAYOUT eLayout = app::g_bSixelOrKitty ? ch::IMAGE_LAYOUT::RAW : ch::IMAGE_LAYOUT::LINES;

            const auto chafaImg = ch::allocImage(
                m_pArena, eLayout, img, split, g_termSize.width
            );

            if (eLayout == ch::IMAGE_LAYOUT::RAW)
            {
                m_textBuff.image(1, 1, chafaImg.width, chafaImg.height, chafaImg.uData.sRaw);
            }
            else
            {
                for (ssize lineIdx = 1, i = 0; lineIdx < chafaImg.uData.vLines.getSize(); ++lineIdx, ++i)
                {
                    const auto& sLine = chafaImg.uData.vLines[i];
                    m_textBuff.image(1, lineIdx, chafaImg.width, chafaImg.height, sLine);
                }
            }

            m_prevImgWidth = chafaImg.width;
        }
    }
}

void
Win::info()
{
    const auto& pl = *app::g_pPlayer;
    const int hOff = m_prevImgWidth + 2;
    const int width = g_termSize.width;

    Span sp = tls_scratch.nextMem<char>(width*4);

    auto drawLine = [&](
        const int y,
        const String sPrefix,
        const String sLine,
        const TEXT_BUFF_STYLE eStyle)
    {
        utils::set(sp.data(), 0, sp.getSize());

        ssize n = print::toSpan(sp, sPrefix);
        m_textBuff.string(hOff, y, {}, sp.data());

        if (sLine.getSize() > 0)
        {
            utils::set(sp.data(), 0, sp.getSize());
            print::toSpan(sp, "{}", sLine);
            m_textBuff.string(hOff + n, y, eStyle, sp.data());
        }
    };

    using STYLE = TEXT_BUFF_STYLE;

    drawLine(1, "title: ", pl.m_info.title, STYLE::BOLD | STYLE::ITALIC | STYLE::YELLOW);
    drawLine(2, "album: ", pl.m_info.album, STYLE::BOLD);
    drawLine(3, "artist: ", pl.m_info.artist, STYLE::BOLD);
}

void
Win::volume()
{
    const auto width = g_termSize.width;
    const int off = m_prevImgWidth + 2;
    const f32 vol = app::g_pMixer->getVolume();
    const bool bMuted = app::g_pMixer->isMuted();

    Span sp = tls_scratch.nextMemZero<char>(width + 1);
    ssize n = print::toSpan(sp, "volume: {:>3}", int(std::round(app::g_pMixer->getVolume() * 100.0)));

    const int maxVolumeBars = (width - off - n - 2) * vol * (1.0f/defaults::MAX_VOLUME);

    using STYLE = TEXT_BUFF_STYLE;

    auto volumeColor = [&](int i) -> STYLE
    {
        f32 col = f32(i) / (f32(width - off - n - 2 - 1) * (1.0f/defaults::MAX_VOLUME));

        if (col <= 0.33f) return GREEN;
        else if (col > 0.33f && col <= 0.66f) return GREEN;
        else if (col > 0.66f && col <= 1.00f) return YELLOW;
        else return RED;
    };

    const STYLE col = bMuted ? BLUE : volumeColor(maxVolumeBars - 1);
    m_textBuff.string(off, 6, STYLE::BOLD | col, {sp.data(), n});

    for (int i = off + n + 1, nTimes = 0; i < width && nTimes < maxVolumeBars; ++i, ++nTimes)
    {
        STYLE col;
        wchar_t wc[2] {};
        if (bMuted)
        {
            col = BLUE | NORM;
            wc[0] = common::CHAR_VOL;
        }
        else
        {
            col = volumeColor(nTimes) | BOLD;
            wc[0] = common::CHAR_VOL_MUTED;
        }

        m_textBuff.wideString(i, 6, col, {wc, 3});
    }
}

void
Win::time()
{
    const auto width = g_termSize.width;
    const int off = m_prevImgWidth + 2;

    String sTime = common::allocTimeString(m_pArena, width);
    m_textBuff.string(off, 9, {}, sTime);
}

void
Win::timeSlider()
{
    const auto& mix = *app::g_pMixer;
    const auto width = g_termSize.width;
    const int xOff = m_prevImgWidth + 2;
    const int yOff = 10;

    int n = 0;

    /* play/pause indicator */
    {
        bool bPaused = mix.isPaused().load(std::memory_order_relaxed);
        const String sIndicator = bPaused ? "I>" : "II";

        using STYLE = TEXT_BUFF_STYLE;
        m_textBuff.string(xOff, yOff, STYLE::BOLD, sIndicator);

        n += sIndicator.getSize();
    }

    /* time slider */
    {
        const int wMax = width - xOff - n;
        const auto& time = mix.getCurrentTimeStamp();
        const auto& maxTime = mix.getTotalSamplesCount();
        const f64 timePlace = (f64(time) / f64(maxTime)) * (wMax - n - 1);

        for (long i = n + 1, t = 0; i < wMax; ++i, ++t)
        {
            wchar_t wch[2] = {L'━', {}};
            if (t == std::floor(timePlace)) wch[0] = L'╋';

            m_textBuff.wideString(xOff + i, yOff, {}, {wch, 3});
        }
    }
}

void
Win::list()
{
    const auto& pl = *app::g_pPlayer;
    const int split = pl.m_imgHeight + 1;

    const auto& aIdxs = pl.m_vSearchIdxs;

    for (ssize h = m_firstIdx, i = 0; i < m_listHeight - 1; ++h, ++i)
    {
        if (h < aIdxs.getSize())
        {
            const u16 songIdx = aIdxs[h];
            const String sSong = pl.m_vShortSongs[songIdx];

            bool bSelected = songIdx == pl.m_selected ? true : false;

            using STYLE = TEXT_BUFF_STYLE;

            STYLE eStyle = STYLE::NORM;

            if (h == pl.m_focused && bSelected)
                eStyle = STYLE::BOLD | STYLE::YELLOW | STYLE::REVERSE;
            else if (h == pl.m_focused)
                eStyle = STYLE::REVERSE;
            else if (bSelected)
                eStyle = STYLE::BOLD | STYLE::YELLOW;

            m_textBuff.string(1, i + split + 1, eStyle, sSong);
        }
    }
}

void
Win::bottomLine()
{
    namespace c = common;

    const auto& pl = *app::g_pPlayer;
    int height = g_termSize.height;
    int width = g_termSize.width;

    /* selected / focused */
    {
        Span sp = tls_scratch.nextMemZero<char>(width + 1);

        ssize n = print::toSpan(sp, "{} / {}", pl.m_selected, pl.m_vShortSongs.getSize() - 1);
        if (pl.m_eReapetMethod != PLAYER_REPEAT_METHOD::NONE)
        {
            const char* sArg {};
            if (pl.m_eReapetMethod == PLAYER_REPEAT_METHOD::TRACK) sArg = "track";
            else if (pl.m_eReapetMethod == PLAYER_REPEAT_METHOD::PLAYLIST) sArg = "playlist";

            n += print::toSpan({sp.data() + n, sp.getSize() - 1 - n}, " (repeat {})", sArg);
        }

        m_textBuff.string(width - n - 1, height - 1, {}, {sp.data(), sp.getSize() - 1});
    }

    if (c::g_input.m_eCurrMode != WINDOW_READ_MODE::NONE ||
        (c::g_input.m_eCurrMode == WINDOW_READ_MODE::NONE &&
         wcsnlen(c::g_input.m_aBuff, utils::size(c::g_input.m_aBuff)) > 0)
    )
    {
        const String sReadMode = c::readModeToString(c::g_input.m_eLastUsedMode);

        m_textBuff.string(1, height - 1, {}, sReadMode);
        m_textBuff.wideString(sReadMode.getSize() + 1, height - 1, {}, c::g_input.getSpan());

        if (c::g_input.m_eCurrMode != WINDOW_READ_MODE::NONE)
        {
            /* just append the cursor character */
            Span spCursor {const_cast<wchar_t*>(common::CURSOR_BLOCK), 3};
            m_textBuff.wideString(sReadMode.getSize() + c::g_input.m_idx + 1, height - 1, {}, spCursor);
        }
    }
}

void
Win::update()
{
    guard::Mtx lock(&m_mtxUpdate);

    /*auto& pl = *app::g_pPlayer;*/
    const int width = g_termSize.width;
    const int height = g_termSize.height;

    m_time = utils::timeNowMS();

    if (width <= 40 || height <= 15)
    {
        m_textBuff.erase();
        return;
    }

    if (m_bClear)
    {
        m_bClear = false;
        m_textBuff.erase();
    }

    /*if (m_bRedraw || pl.m_bSelectionChanged)*/
    {
        m_textBuff.clean();

        m_bRedraw = false;

        if (!app::g_bNoImage)
            coverImage();

        time(); /* redraw if image size changed */
        timeSlider();

        volume();
        info();
        list();

        bottomLine();

        m_textBuff.present();
    }
}

} /* namespace platform::ansi */
