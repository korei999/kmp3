#include "Win.hh"

#include "adt/guard.hh"
#include "app.hh"
#include "defaults.hh"
#include "platform/chafa/chafa.hh"
#include "adt/ScratchBuffer.hh"

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

namespace platform::ansi
{

thread_local static u8 tls_aMemBuff[SIZE_8K] {};
thread_local static ScratchBuffer tls_scratch(tls_aMemBuff);

void
Win::clearArea(int x, int y, int width, int height)
{
    const int w = utils::min(g_termSize.width, width);
    const int h = utils::min(g_termSize.height, height);

    auto sp = tls_scratch.getMem<char>(w + 1);
    memset(sp.data(), ' ', w);

    for (int i = y; i < h; ++i)
        m_textBuff.movePush(x, i, {sp.data(), w});
}

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
        clearArea(1, 1, m_prevImgWidth, split + 1);

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
                m_textBuff.movePush(1, 1, chafaImg.uData.sRaw);
            }
            else
            {
                for (ssize lineIdx = 1, i = 0; lineIdx < chafaImg.uData.vLines.getSize(); ++lineIdx, ++i)
                {
                    const auto& sLine = chafaImg.uData.vLines[i];
                    m_textBuff.movePush(1, lineIdx, sLine);
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
    auto& tb = m_textBuff;
    const int hOff = m_prevImgWidth + 2;
    const int width = g_termSize.width;

    auto sp = tls_scratch.getMem<char>(width + 1);

    auto drawLine = [&](
        const int y,
        const String sPrefix,
        const String sLine,
        const String sColor)
    {
        tb.moveClearLine(hOff - 1, y, TEXT_BUFF_ARG::TO_END);

        utils::set(sp.data(), 0, sp.getSize());

        ssize n = print::toSpan(sp, sPrefix);
        tb.push(NORM);
        tb.movePushGlyphs(hOff, y, sp.data(), width - hOff - 1);

        if (sLine.getSize() > 0)
        {
            utils::set(sp.data(), 0, sp.getSize());
            print::toSpan(sp, "{}", sLine);
            tb.push(sColor);
            tb.movePushGlyphs(hOff + n, y, sp.data(), width - hOff - 1 - n);
        }

        tb.push(NORM);
    };

    drawLine(1, "title: ", pl.m_info.title, BOLD ITALIC YELLOW);
    drawLine(2, "album: ", pl.m_info.album, BOLD);
    drawLine(3, "artist: ", pl.m_info.artist, BOLD);
}

void
Win::volume()
{
    auto& tb = m_textBuff;
    const auto width = g_termSize.width;
    const int off = m_prevImgWidth + 2;
    const f32 vol = app::g_pMixer->getVolume();
    const bool bMuted = app::g_pMixer->isMuted();

    Span sp = tls_scratch.getZMem<char>(width + 1);
    ssize n = print::toSpan(sp, "volume: {:>3}", int(std::round(app::g_pMixer->getVolume() * 100.0)));

    const int maxVolumeBars = (width - off - n - 2) * vol * (1.0f/defaults::MAX_VOLUME);

    auto volumeColor = [&](int i) -> String
    {
        f32 col = f32(i) / (f32(width - off - n - 2 - 1) * (1.0f/defaults::MAX_VOLUME));

        if (col <= 0.33f) return GREEN;
        else if (col > 0.33f && col <= 0.66f) return GREEN;
        else if (col > 0.66f && col <= 1.00f) return YELLOW;
        else return RED;
    };

    const String col = bMuted ? BLUE : volumeColor(maxVolumeBars - 1);

    tb.push(NORM);
    tb.moveClearLine(off - 1, 6, TEXT_BUFF_ARG::TO_END);

    tb.push(col);
    tb.push(BOLD);
    tb.movePushGlyphs(off, 6, {sp.data(), n}, width - off);
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
        tb.movePushWString(i, 6, &wc, 3);
    }
}

void
Win::time()
{
    auto& tb = m_textBuff;
    const auto width = g_termSize.width;
    const int off = m_prevImgWidth + 2;

    tb.push(NORM);
    tb.moveClearLine(off - 1, 9, TEXT_BUFF_ARG::TO_END);

    String sTime = common::allocTimeString(m_pArena, width);
    tb.movePushGlyphs(off, 9, sTime, width - off);
}

void
Win::timeSlider()
{
    auto& tb = m_textBuff;
    const auto& mix = *app::g_pMixer;
    const auto width = g_termSize.width;
    const int xOff = m_prevImgWidth + 2;
    const int yOff = 10;

    tb.push(NORM);
    tb.moveClearLine(xOff - 1, yOff, TEXT_BUFF_ARG::TO_END);

    int n = 0;

    /* play/pause indicator */
    {
        bool bPaused = mix.isPaused().load(std::memory_order_relaxed);
        const char* ntsIndicator = bPaused ? "I>" : "II";

        tb.movePush(xOff, yOff, ntsIndicator);

        n += strlen(ntsIndicator);
    }

    /* time slider */
    {
        const int wMax = width - xOff - n;
        const auto& time = mix.getCurrentTimeStamp();
        const auto& maxTime = mix.getTotalSamplesCount();
        const f64 timePlace = (f64(time) / f64(maxTime)) * (wMax - n - 1);

        for (long i = n + 1, t = 0; i < wMax; ++i, ++t)
        {
            wchar_t wch = L'━';
            if (t == std::floor(timePlace)) wch = L'╋';

            tb.movePushWString(xOff + i, yOff, &wch, 3);
        }
    }
}

void
Win::list()
{
    const auto& pl = *app::g_pPlayer;
    auto& tb = m_textBuff;
    const int width = g_termSize.width;
    const int split = pl.m_imgHeight + 1;

    const auto& aIdxs = pl.m_vSearchIdxs;

    tb.push(NORM);

    for (ssize h = m_firstIdx, i = 0; i < m_listHeight - 1; ++h, ++i)
    {
        if (h < aIdxs.getSize())
        {
            const u16 songIdx = aIdxs[h];
            const String sSong = pl.m_vShortSongs[songIdx];

            bool bSelected = songIdx == pl.m_selected ? true : false;

            if (h == pl.m_focused && bSelected)
                tb.push(BOLD YELLOW REVERSE);
            else if (h == pl.m_focused)
                tb.push(REVERSE);
            else if (bSelected)
                tb.push(BOLD YELLOW);

            tb.move(0, i + split + 1);
            tb.clearLine(TEXT_BUFF_ARG::EVERYTHING);
            tb.movePushGlyphs(1, i + split + 1, sSong, width - 2);
            tb.push(NORM);

            // tb.setString(1, i + split + 1, sSong);
        }
        else
        {
            tb.moveClearLine(0, i + split + 1, TEXT_BUFF_ARG::EVERYTHING);
        }
    }
}

void
Win::bottomLine()
{
    namespace c = common;

    const auto& pl = *app::g_pPlayer;
    auto& tb = m_textBuff;
    int height = g_termSize.height;
    int width = g_termSize.width;

    tb.moveClearLine(0, height - 1, TEXT_BUFF_ARG::EVERYTHING);
    tb.push(NORM);

    /* selected / focused */
    {
        Span sp = tls_scratch.getZMem<char>(width + 1);

        ssize n = print::toSpan(sp, "{} / {}", pl.m_selected, pl.m_vShortSongs.getSize() - 1);
        if (pl.m_eReapetMethod != PLAYER_REPEAT_METHOD::NONE)
        {
            const char* sArg {};
            if (pl.m_eReapetMethod == PLAYER_REPEAT_METHOD::TRACK) sArg = "track";
            else if (pl.m_eReapetMethod == PLAYER_REPEAT_METHOD::PLAYLIST) sArg = "playlist";

            n += print::toSpan({sp.data() + n, sp.getSize() - 1 - n}, " (repeat {})", sArg);
        }

        tb.movePush(width - n - 1, height - 1, {sp.data(), sp.getSize() - 1});
    }

    if (
        c::g_input.m_eCurrMode != WINDOW_READ_MODE::NONE ||
        (c::g_input.m_eCurrMode == WINDOW_READ_MODE::NONE &&
         wcsnlen(c::g_input.m_aBuff, utils::size(c::g_input.m_aBuff)) > 0)
    )
    {
        const String sReadMode = c::readModeToString(c::g_input.m_eLastUsedMode);
        tb.movePushGlyphs(1, height - 1, sReadMode, width - 1);
        tb.movePushWString(
            sReadMode.getSize() + 1,
            height - 1,
            c::g_input.m_aBuff,
            utils::size(c::g_input.m_aBuff) - 2
        );

        if (c::g_input.m_eCurrMode != WINDOW_READ_MODE::NONE) tb.hideCursor(false);
        else tb.hideCursor(true);
    }
    else tb.hideCursor(true);
}

void
Win::update()
{
    guard::Mtx lock(&m_mtxUpdate);

    auto& pl = *app::g_pPlayer;
    auto& tb = m_textBuff;
    const int width = g_termSize.width;
    const int height = g_termSize.height;

    m_time = utils::timeNowMS();

    defer(
        tb.flush();
        tb.reset();
    );

    if (width <= 40 || height <= 15)
    {
        tb.clear();
        return;
    }

    if (m_bClear)
    {
        m_bClear = false;
        tb.clear();
    }

    time();
    timeSlider();

    if (m_bRedraw || pl.m_bSelectionChanged)
    {
        m_bRedraw = false;

        if (!app::g_bNoImage)
            coverImage();

        time(); /* redraw if image size changed */
        timeSlider();

        volume();
        info();
        list();

        bottomLine();
    }
}

} /* namespace platform::ansi */
