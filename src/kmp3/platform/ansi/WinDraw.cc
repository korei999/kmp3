#include "Win.hh"

#include "app.hh"
#include "defaults.hh"

using namespace adt;

namespace platform::ansi
{

#ifdef OPT_CHAFA
void
Win::coverImage()
{
    auto& pl = app::player();

    if (pl.m_bSelectionChanged && m_time > m_lastResizeTime + defaults::IMAGE_UPDATE_RATE_LIMIT)
    {
        pl.m_bSelectionChanged = false;

        const int split = pl.m_imgHeight;

        if (!app::g_bChafaSymbols) m_textBuff.clearKittyImages();
        m_textBuff.forceClean(1, 1, m_prevImgWidth + 1, split + 1);

        Image img = app::decoder().getCoverImage();

        namespace c = platform::chafa;

        c::IMAGE_LAYOUT eLayout = app::g_bSixelOrKitty ? c::IMAGE_LAYOUT::RAW : c::IMAGE_LAYOUT::LINES;

        /* NOTE: using textBuff's dedicated image arena */
        auto chafaImg = c::allocImage(
            &m_textBuff.m_imgArena, eLayout, img, split, m_termSize.width
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

    ArenaPushGuard pushed {m_pArena};
    Span sp {m_pArena->zallocV<char>(1000), 1000};

    auto clDrawLine = [&](
        const int y,
        const StringView svPrefix,
        const StringView svLine,
        const TEXT_BUFF_STYLE eStyle
    ) -> void
    {
        utils::memSet(sp.data(), 0, sp.size());

        isize n = print::toSpan(sp, svPrefix);
        m_textBuff.string(hOff, y, {}, sp.data());

        if (svLine.size() > 0)
        {
            utils::memSet(sp.data(), 0, sp.size());
            print::toSpan(sp, "{}", svLine);
            m_textBuff.string(hOff + n, y, eStyle, sp.data());
        }
    };

    using STYLE = TEXT_BUFF_STYLE;

    clDrawLine(1, "title: ", pl.m_info.sfTitle, STYLE::BOLD | STYLE::ITALIC | STYLE::YELLOW);
    clDrawLine(2, "album: ", pl.m_info.sfAlbum, STYLE::BOLD);
    clDrawLine(3, "artist: ", pl.m_info.sfArtist, STYLE::BOLD);
}

void
Win::volume()
{
    const auto width = m_termSize.width;
    const int off = m_prevImgWidth + 2;
    const f32 vol = app::mixer().getVolume();
    const bool bMuted = app::mixer().isMuted();

    ArenaPushGuard pushed {m_pArena};
    Span sp {m_pArena->zallocV<char>(width + 1), width + 1};

    const isize n = print::toSpan(sp, "volume: {:>3}", int(std::round(app::mixer().getVolume() * 100.0)));
    const int nVolumeBars = (width - off - n - 2) * vol * (1.0f/defaults::MAX_VOLUME);

    using STYLE = TEXT_BUFF_STYLE;

    auto clVolumeStringColor = [&](f32 x) -> STYLE
    {
        if (x <= 0.33f) return STYLE::GREEN;
        else if (x > 0.33f && x <= 0.66f) return STYLE::GREEN;
        else if (x > 0.66f && x <= 1.01f) return STYLE::YELLOW;
        else return STYLE::RED;
    };

    auto clBarColor = [&](int i) -> STYLE
    {
        const f32 col = f32(i) / ((f32(width - off - n - 2 - 1) * (1.0f/defaults::MAX_VOLUME)));

        return clVolumeStringColor(col);
    };

    const STYLE col = bMuted ? STYLE::BLUE : clVolumeStringColor(vol);
    m_textBuff.string(off, 6, STYLE::BOLD | col, {sp.data(), n});

    for (int i = off + n + 1, nTimes = 0; i < width && nTimes < nVolumeBars; ++i, ++nTimes)
    {
        STYLE col;
        wchar_t wc[3] {};
        if (bMuted)
        {
            col = STYLE::BLUE | STYLE::NORM;
            wc[0] = common::CHAR_VOL;
        }
        else
        {
            col = clBarColor(nTimes) | STYLE::BOLD;
            wc[0] = common::CHAR_VOL_MUTED;
        }

        m_textBuff.wideString(i, 6, col, wc);
    }
}

void
Win::time()
{
    const auto width = m_termSize.width;
    const int off = m_prevImgWidth + 2;

    StringView svTime = common::allocTimeString(m_pArena, width);
    m_textBuff.string(off, 9, {}, svTime);
}

void
Win::timeSlider()
{
    const auto& mix = *app::g_pMixer;
    const auto width = m_termSize.width;
    const int xOff = m_prevImgWidth + 2;
    const int yOff = 10;

    isize n = 0;

    /* play/pause indicator */
    {
        const bool bPaused = mix.isPaused().load(atomic::ORDER::ACQUIRE);
        const StringView svIndicator = bPaused ? "I>" : "II";

        using STYLE = TEXT_BUFF_STYLE;
        m_textBuff.string(xOff, yOff, STYLE::BOLD, svIndicator);

        n += svIndicator.size();
    }

    /* time slider */
    {
        const int wMax = width - xOff - n;
        const auto& time = mix.getCurrentTimeStamp();
        const auto& maxTime = mix.getTotalSamplesCount();
        const f64 timePlace = (f64(time) / f64(maxTime)) * (wMax - n - 1);

        for (long i = n + 1, t = 0; i < wMax; ++i, ++t)
        {
            const char* nts = [&]
            {
                if (t == std::floor(timePlace)) return "╂";
                else return "─";
            }();

            m_textBuff.string(xOff + i, yOff, TEXT_BUFF_STYLE::NORM, nts);
        }
    }
}

void
Win::songList()
{
    const auto& pl = app::player();
    const int split = calcImageHeightSplit();

    if (m_firstIdx < 0 || m_firstIdx >= pl.m_vSearchIdxs.size()) return;

    for (isize h = m_firstIdx, i = 0; i < m_listHeight - 1; ++h, ++i)
    {
        if (h >= pl.m_vSearchIdxs.size()) break;

        const u16 songIdx = pl.m_vSearchIdxs[h];
        const StringView svSong = pl.m_vShortSongs[songIdx];
        const bool bSelected = songIdx == pl.m_selected ? true : false;

        using STYLE = TEXT_BUFF_STYLE;

        STYLE eStyle = STYLE::NORM;

        if (h == pl.m_focused && bSelected)
            eStyle = STYLE::BOLD | STYLE::YELLOW | STYLE::REVERSE;
        else if (h == pl.m_focused)
            eStyle = STYLE::REVERSE;
        else if (bSelected)
            eStyle = STYLE::BOLD | STYLE::YELLOW;

        /* leave some width space for the scroll bar */
        m_textBuff.string(1, i + split + 1, eStyle, svSong, m_termSize.width - 3);
    }
}

void
Win::scrollBar()
{
    using STYLE = TEXT_BUFF_STYLE;

    const auto& pl = app::player();

    if (m_listHeight - 1 >= pl.m_vSearchIdxs.size()) return;

    const int split = calcImageHeightSplit();

    const f32 listSizeFactor = m_listHeight / f32(pl.m_vSearchIdxs.size() - 0.9999f);
    const int barHeight = utils::max(1, static_cast<int>(m_listHeight * listSizeFactor));

    /* bunch of mess to make it look better and not go beyond the list */
    int blockI = static_cast<int>(m_firstIdx*listSizeFactor);
    if (blockI + barHeight >= m_listHeight - 1)
        blockI -= (blockI + barHeight) - (m_listHeight - 1);

    for (isize i = 0; i < m_listHeight - 1; ++i)
        m_textBuff.string(m_termSize.width - 1, split + i + 1, STYLE::DIM, "│");

    for (isize i = 0; i < barHeight && i + blockI < m_listHeight - 1; ++i)
        m_textBuff.string(m_termSize.width - 1, i + blockI + split + 1, STYLE::DIM, "█");
}

void
Win::bottomLine()
{
    namespace c = common;

    const auto& pl = app::player();
    const int height = m_termSize.height;
    const int width = m_termSize.width;

    /* selected / focused */
    {
        ArenaPushGuard pushed {m_pArena};
        Span sp {m_pArena->zallocV<char>(width + 1), width + 1};

        isize n = print::toSpan(sp, "{} / {}", pl.m_selected, pl.m_vShortSongs.size() - 1);
        if (pl.m_eRepeatMethod != PLAYER_REPEAT_METHOD::NONE)
        {
            const char* sArg {};
            if (pl.m_eRepeatMethod == PLAYER_REPEAT_METHOD::TRACK) sArg = "track";
            else if (pl.m_eRepeatMethod == PLAYER_REPEAT_METHOD::PLAYLIST) sArg = "playlist";

            n += print::toSpan({sp.data() + n, sp.size() - 1 - n}, " (repeat {})", sArg);
        }

        m_textBuff.string(width - n - 1, height - 1, {}, {sp.data(), sp.size() - 1});
    }

    if (c::g_input.m_eCurrMode != WINDOW_READ_MODE::NONE ||
        (c::g_input.m_eCurrMode == WINDOW_READ_MODE::NONE &&
         wcsnlen(c::g_input.m_aBuff, utils::size(c::g_input.m_aBuff)) > 0)
    )
    {
        const StringView svReadMode = c::readModeToString(c::g_input.m_eLastUsedMode);

        m_textBuff.string(1, height - 1, {}, svReadMode);
        m_textBuff.wideString(svReadMode.size() + 1, height - 1, {}, c::g_input.span());

        if (c::g_input.m_eCurrMode != WINDOW_READ_MODE::NONE)
        {
            /* just append the cursor character */
            Span spCursor {const_cast<wchar_t*>(common::CURSOR_BLOCK), 3};
            m_textBuff.wideString(svReadMode.size() + c::g_input.m_idx + 1, height - 1, {}, spCursor);
        }
    }
}

void
Win::errorMsg()
{
    auto& pl = app::player();
    int height = m_termSize.height;

    static Player::Msg s_msg;
    static f64 s_time;

    if (!s_msg || s_msg.time == Player::Msg::UNTIL_NEXT)
    {
        Player::Msg newMsg = pl.popErrorMsg();
        if (newMsg)
        {
            s_time = m_time;
            s_msg = newMsg;
            LogInfo("got msg: '{}' size: {}\n", s_msg.sfMsg, s_msg.sfMsg.size());
        }
    }

    if (common::g_input.m_eCurrMode == WINDOW_READ_MODE::NONE && s_msg && s_msg.time != 0.0)
    {
        if (s_time + s_msg.time < m_time)
        {
            LogDebug("killing: '{}'\n", s_msg.sfMsg);
            s_msg.sfMsg.destroy();
            return;
        }

        using STYLE = TEXT_BUFF_STYLE;

        STYLE eStyle = [&]
        {
            switch (s_msg.eType)
            {
                case Player::Msg::TYPE::NOTIFY:
                return STYLE::CYAN | STYLE::ITALIC | STYLE::UNDERLINE;
                break;

                case Player::Msg::TYPE::WARNING:
                return STYLE::YELLOW | STYLE::BOLD;
                break;

                case Player::Msg::TYPE::ERROR:
                return STYLE::RED | STYLE::BOLD;
                break;
            }
            return STYLE::NORM;
        }();

        m_textBuff.string(1, height - 1, eStyle, s_msg.sfMsg);
    }
}

void
Win::update()
{
    LockGuard lock {&m_mtxUpdate};

    const int width = m_termSize.width;
    const int height = m_termSize.height;

    m_time = time::nowMS();

    if (!app::g_bRunning) return;

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
    if (!app::g_bNoImage) coverImage();
#endif

    time();
    timeSlider();
    volume();
    info();
    songList();
    scrollBar();
    bottomLine();
    errorMsg();

    m_textBuff.present();
}

} /* namespace platform::ansi */
