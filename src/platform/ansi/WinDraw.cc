#include "Win.hh"

#include "adt/guard.hh"
#include "app.hh"
#include "defaults.hh"
#include "adt/ScratchBuffer.hh"

#ifdef OPT_CHAFA
#include "platform/chafa/chafa.hh"
#endif

using namespace adt;

namespace platform::ansi
{

static u8 s_aMemBuff[SIZE_8K] {};
static ScratchBuffer s_scratch(s_aMemBuff);

#ifdef OPT_CHAFA
void
Win::coverImage()
{
    auto& pl = app::player();
    auto& mix = app::mixer();

    if (pl.m_bSelectionChanged && m_time > m_lastResizeTime + defaults::IMAGE_UPDATE_RATE_LIMIT)
    {
        pl.m_bSelectionChanged = false;

        const int split = pl.m_imgHeight;

        m_textBuff.clearKittyImages(); /* shouldn't hurt if TERM is not kitty */
        m_textBuff.forceClean(1, 1, m_prevImgWidth + 1, split + 1);

        Image img = mix.getCoverImage();

        namespace ch = platform::chafa;

        ch::IMAGE_LAYOUT eLayout = app::g_bSixelOrKitty ? ch::IMAGE_LAYOUT::RAW : ch::IMAGE_LAYOUT::LINES;

        /* NOTE: using textBuff's dedicated image arena */
        auto chafaImg = ch::allocImage(
            &m_textBuff.m_imgArena, eLayout, img, split, g_termSize.width
        );

        m_textBuff.image(1, 1, chafaImg);

        m_prevImgWidth = chafaImg.width;
    }
}
#endif

void
Win::info()
{
    const auto& pl = *app::g_pPlayer;
    const int hOff = m_prevImgWidth + 2;
    const int width = g_termSize.width;

    Span sp = s_scratch.nextMem<char>(width*4);

    auto drawLine = [&](
        const int y,
        const StringView sPrefix,
        const StringView sLine,
        const TEXT_BUFF_STYLE eStyle)
    {
        utils::memSet(sp.data(), 0, sp.size());

        ssize n = print::toSpan(sp, sPrefix);
        m_textBuff.string(hOff, y, {}, sp.data());

        if (sLine.size() > 0)
        {
            utils::memSet(sp.data(), 0, sp.size());
            print::toSpan(sp, "{}", sLine);
            m_textBuff.string(hOff + n, y, eStyle, sp.data());
        }
    };

    using STYLE = TEXT_BUFF_STYLE;

    drawLine(1, "title: ", pl.m_info.sTitle, STYLE::BOLD | STYLE::ITALIC | STYLE::YELLOW);
    drawLine(2, "album: ", pl.m_info.sAlbum, STYLE::BOLD);
    drawLine(3, "artist: ", pl.m_info.sArtist, STYLE::BOLD);
}

void
Win::volume()
{
    const auto width = g_termSize.width;
    const int off = m_prevImgWidth + 2;
    const f32 vol = app::g_pMixer->getVolume();
    const bool bMuted = app::g_pMixer->isMuted();

    Span sp = s_scratch.nextMemZero<char>(width + 1);
    ssize n = print::toSpan(sp, "volume: {:>3}", int(std::round(app::g_pMixer->getVolume() * 100.0)));

    const int maxVolumeBars = (width - off - n - 2) * vol * (1.0f/defaults::MAX_VOLUME);

    using STYLE = TEXT_BUFF_STYLE;

    auto volumeColor = [&](int i) -> STYLE
    {
        f32 col = f32(i) / ((f32(width - off - n - 2 - 1) * (1.0f/defaults::MAX_VOLUME)));

        if (col <= 0.33f)
            return STYLE::GREEN;
        else if (col > 0.33f && col <= 0.66f)
            return STYLE::GREEN;
        else if (col > 0.66f && col <= 1.00f)
            return STYLE::YELLOW;
        else return STYLE::RED;
    };

    const STYLE col = bMuted ? STYLE::BLUE : volumeColor(maxVolumeBars - 1);
    m_textBuff.string(off, 6, STYLE::BOLD | col, {sp.data(), n});

    for (int i = off + n + 1, nTimes = 0; i < width && nTimes < maxVolumeBars; ++i, ++nTimes)
    {
        STYLE col;
        wchar_t wc[2] {};
        if (bMuted)
        {
            col = STYLE::BLUE | STYLE::NORM;
            wc[0] = common::CHAR_VOL;
        }
        else
        {
            col = volumeColor(nTimes) | STYLE::BOLD;
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

    StringView sTime = common::allocTimeString(m_pArena, width);
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
        const StringView sIndicator = bPaused ? "I>" : "II";

        using STYLE = TEXT_BUFF_STYLE;
        m_textBuff.string(xOff, yOff, STYLE::BOLD, sIndicator);

        n += sIndicator.size();
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
        if (h < aIdxs.size())
        {
            const u16 songIdx = aIdxs[h];
            const StringView sSong = pl.m_vShortSongs[songIdx];

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
        Span sp = s_scratch.nextMemZero<char>(width + 1);

        ssize n = print::toSpan(sp, "{} / {}", pl.m_selected, pl.m_vShortSongs.size() - 1);
        if (pl.m_eReapetMethod != PLAYER_REPEAT_METHOD::NONE)
        {
            const char* sArg {};
            if (pl.m_eReapetMethod == PLAYER_REPEAT_METHOD::TRACK) sArg = "track";
            else if (pl.m_eReapetMethod == PLAYER_REPEAT_METHOD::PLAYLIST) sArg = "playlist";

            n += print::toSpan({sp.data() + n, sp.size() - 1 - n}, " (repeat {})", sArg);
        }

        m_textBuff.string(width - n - 1, height - 1, {}, {sp.data(), sp.size() - 1});
    }

    if (c::g_input.m_eCurrMode != WINDOW_READ_MODE::NONE ||
        (c::g_input.m_eCurrMode == WINDOW_READ_MODE::NONE &&
         wcsnlen(c::g_input.m_aBuff, utils::size(c::g_input.m_aBuff)) > 0)
    )
    {
        const StringView sReadMode = c::readModeToString(c::g_input.m_eLastUsedMode);

        m_textBuff.string(1, height - 1, {}, sReadMode);
        m_textBuff.wideString(sReadMode.size() + 1, height - 1, {}, c::g_input.getSpan());

        if (c::g_input.m_eCurrMode != WINDOW_READ_MODE::NONE)
        {
            /* just append the cursor character */
            Span spCursor {const_cast<wchar_t*>(common::CURSOR_BLOCK), 3};
            m_textBuff.wideString(sReadMode.size() + c::g_input.m_idx + 1, height - 1, {}, spCursor);
        }
    }
}

void
Win::update()
{
    guard::Mtx lock(&m_mtxUpdate);

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

    m_textBuff.clean();

#ifdef OPT_CHAFA
    if (!app::g_bNoImage)
        coverImage();
#endif

    time();
    timeSlider();
    volume();
    info();
    list();
    bottomLine();

    m_textBuff.present();
}

} /* namespace platform::ansi */
