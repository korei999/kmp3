#include "Player.hh"

#include "app.hh"
#include "platform/mpris/mpris.hh"

#include <cstdlib>
#include <cwchar>
#include <cwctype>

using namespace adt;

static constexpr StringView aSvAcceptedFileEndings[] {
    ".mp2", ".mp3", ".mp4", ".m4a", ".m4b",
    ".fla", ".flac",
    ".ogg", ".opus",
    ".umx", ".s3m",
    ".wav", ".caf", ".aif",
    ".webm",
    ".mkv",
};

bool
Player::acceptedFormat(const StringView s) noexcept
{
    return utils::searchI(aSvAcceptedFileEndings, [&](const StringView ending)
        { return s.endsWith(ending); }
    ) != NPOS;
}

void
Player::focusNext() noexcept
{
    m_focusedI = m_vSongIdxs.size() > 0 ? utils::cycleForward(m_focusedI, m_vSongIdxs.size()) : 0;
    app::window().m_bUpdateFirstIdx = true;
}

void
Player::focusPrev() noexcept
{
    m_focusedI = m_vSongIdxs.size() > 0 ? utils::cycleBackward(m_focusedI, m_vSongIdxs.size()) : 0;
    app::window().m_bUpdateFirstIdx = true;
}

void
Player::focus(long i) noexcept
{
    m_focusedI = utils::clamp(i, 0l, (long)(m_vSongIdxs.size() - 1));
    app::window().m_bUpdateFirstIdx = true;
}

void
Player::focusLast() noexcept
{
    focus(m_vSongIdxs.size() - 1);
}

long
Player::findSongI(long toFindI)
{
    if (m_vSongs.empty()) return 0;

again:
    const isize res = utils::searchI(m_vSearchIdxs, [toFindI](u16 e) { return e == toFindI; });

    if (res <= NPOS)
    {
        setDefaultSearchIdxs();
        setDefaultSongIdxs();
        goto again;
    }

    return res;
}

void
Player::focusSelected()
{
    focus(findSongI(m_selectedI));
}

void
Player::focusSelectedAtCenter()
{
    focusSelected();

    const isize height = app::window().m_listHeight;

    if (m_vSearchIdxs.size() <= height) return;

    const isize half = (height-2) / 2;
    app::window().m_firstIdx = utils::max(0ll, m_focusedI - half);
}

void
Player::setDefaultIdxs(Vec<u16>* pvIdxs)
{
    pvIdxs->setSize(m_pAlloc, m_vSongs.size());
    for (auto& e : *pvIdxs) e = pvIdxs->idx(&e);
}

void
Player::subStringSearch(Arena* pArena, Span<const wchar_t> spBuff)
{
    ArenaPushScope arenaScope {pArena};

    if (spBuff && wcsnlen(spBuff.data(), spBuff.size()) == 0)
        return;

    Vec<wchar_t> vUpperRight {pArena, spBuff.size() + 1};
    vUpperRight.setSize(pArena, vUpperRight.cap());

    for (isize i = 0; i < spBuff.size() && spBuff[i]; ++i)
        vUpperRight[i] = (wchar_t)towupper(spBuff[i]);

    Vec<wchar_t> vSongToUpper(pArena, m_longestString + 1);
    vSongToUpper.setSize(pArena, vSongToUpper.cap());

    m_vSearchIdxs.setSize(m_pAlloc, 0);
    for (const u16 songIdx : m_vSongIdxs)
    {
        const StringView song = m_vShortSongs[songIdx];

        vSongToUpper.zeroOut();
        mbstowcs(vSongToUpper.data(), song.data(), song.size());

        for (auto& wc : vSongToUpper)
            wc = towupper(wc);

        if (wcsstr(vSongToUpper.data(), vUpperRight.data()) != nullptr)
            m_vSearchIdxs.push(m_pAlloc, songIdx);
    }
}

long
Player::nextSelectionI(long selI)
{
    const long currI = findSongI(selI);
    long nextI = currI + 1;

    defer( LogDebug{"currI: {}, nextI: {}\n", currI, nextI} );

    if (m_eRepeatMethod == PLAYER_REPEAT_METHOD::TRACK)
    {
        nextI = currI;
    }
    else if (nextI >= m_vSongIdxs.size())
    {
        if (m_eRepeatMethod == PLAYER_REPEAT_METHOD::PLAYLIST)
        {
            nextI = 0;
        }
        else
        {
            app::g_vol_bRunning = false;
            return m_vSongIdxs[currI];
        }
    }

    return m_vSongIdxs[nextI];
}

void
Player::updateInfo() noexcept
{
    m_info.sfTitle = app::decoder().getMetadata("title");
    m_info.sfAlbum = app::decoder().getMetadata("album");
    m_info.sfArtist = app::decoder().getMetadata("artist");

    if (m_info.sfTitle.size() == 0)
        m_info.sfTitle = m_vShortSongs[m_selectedI];

    m_bSelectionChanged = true;
}

void
Player::selectFinal(long selI)
{
    long nFailed = 0;

    while (true)
    {
        if (!app::g_vol_bRunning) return;
        if (app::mixer().play(m_vSongs[selI])) break;

        LogWarn("failed to open: '{}', selI: {}\n", m_vSongs[selI], selI);

        if (++nFailed >= m_vSearchIdxs.size())
        {
            LogInfo("QUIT (nFailed: {}, size: {})\n", nFailed, m_vSearchIdxs.size());
            app::quit();
            return;
        }

        Msg msg {
            .time = 5000.0,
            .eType = Msg::TYPE::ERROR,
        };
        print::toSpan(msg.sfMsg.data(), "failed to open \"{}\"", file::getPathEnding(m_vSongs[selI]));

        if ((msg.sfMsg != m_sfLastMessage) || (time::nowMS() >= (m_lastPushedMessageTime + msg.time)))
            pushErrorMsg(msg);

        selI = nextSelectionI(selI); /* Might set g_bRunning. */
    }

    m_bSelectionChanged = true;
    m_selectedI = selI;
    updateInfo();
}

void
Player::selectFocused()
{
    if (m_vSongIdxs.size() <= m_focusedI)
    {
        LogWarn("out of range selection: (vec.size: {})\n", m_vSongIdxs.size());
        return;
    }

    LogDebug("selected: {}\n", m_vSongs[m_vSongIdxs[m_focusedI]]);
    selectFinal(m_vSongIdxs[m_focusedI]);
}

void
Player::togglePause()
{
    app::mixer().togglePause();
}

void
Player::nextSongIfPrevEnded()
{
    bool bExpected = true;
    if (app::mixer().m_atom_bSongEnd.compareExchange(
            &bExpected, false,
            atomic::ORDER::RELAXED, atomic::ORDER::RELAXED
        )
    )
    {
        selectFinal(nextSelectionI(m_selectedI));
    }
}

PLAYER_REPEAT_METHOD
Player::cycleRepeatMethods(bool bForward)
{
    int rm;
    if (bForward) rm = utils::cycleForward(isize(m_eRepeatMethod), isize(PLAYER_REPEAT_METHOD::ESIZE));
    else rm = utils::cycleBackward(isize(m_eRepeatMethod), isize(PLAYER_REPEAT_METHOD::ESIZE));

    m_eRepeatMethod = PLAYER_REPEAT_METHOD(rm);

    mpris::loopStatusChanged();

    return PLAYER_REPEAT_METHOD(rm);
}

void
Player::select(long i)
{
    if (m_vSongs.empty() || m_vSearchIdxs.empty())
        return;

    const long idx = utils::clamp(i, 0L, long(m_vSearchIdxs.size() - 1));
    selectFinal(m_vSearchIdxs[idx]);
}

void
Player::selectNext()
{
    if (m_vSongs.empty() || m_vSearchIdxs.empty())
    {
        setAllDefaultIdxs();
        focusSelectedAtCenter();
    }

    ADT_ASSERT(m_vSearchIdxs.size() > 0, "size: {}", m_vSearchIdxs.size());
    select(utils::cycleForward(findSongI(m_selectedI), m_vSearchIdxs.size()));
}

void
Player::selectPrev()
{
    if (m_vSongs.empty() || m_vSearchIdxs.empty())
    {
        setAllDefaultIdxs();
        focusSelectedAtCenter();
    }

    ADT_ASSERT(m_vSearchIdxs.size() > 0, "size: {}", m_vSearchIdxs.size());
    select(utils::cycleBackward(findSongI(m_selectedI), m_vSearchIdxs.size()));
}

void
Player::copySearchToSongIdxs()
{
    m_vSongIdxs.setSize(m_pAlloc, m_vSearchIdxs.size());
    utils::memCopy(m_vSongIdxs.data(), m_vSearchIdxs.data(), m_vSearchIdxs.size());
}

void
Player::setImgSize(long height)
{
    height = utils::clamp(height, long(app::g_config.minImageHeight), long(app::g_config.maxImageHeight));

    if (m_imgHeight == height) return;

    m_imgHeight = height;
    adjustImgWidth();

    m_bSelectionChanged = true;
    if (app::g_pWin) app::g_pWin->m_bClear = true;
}

void
Player::adjustImgWidth() noexcept
{
    m_imgWidth = std::round((m_imgHeight * (1920.0/1080.0)) / app::g_config.fontAspectRatio);
}

void
Player::destroy()
{
    m_vSongs.destroy(m_pAlloc);
    m_vShortSongs.destroy(m_pAlloc);
    m_vSongIdxs.destroy(m_pAlloc);
    m_vSearchIdxs.destroy(m_pAlloc);
}

void
Player::pushErrorMsg(const Player::Msg& msg)
{
    LockScope lock {&m_mtxQ};
    m_qErrorMsgs.pushBack(msg);

    m_sfLastMessage = msg.sfMsg;
    m_lastPushedMessageTime = time::nowMS();
}

Player::Msg
Player::popErrorMsg()
{
    {
        LockScope lock {&m_mtxQ};
        if (!m_qErrorMsgs.empty()) return m_qErrorMsgs.popFront();
    }

    return {};
}

Player::Player(IAllocator* p, int nArgs, char** ppArgs)
    : m_pAlloc {p},
      m_vSongs {p, nArgs},
      m_vShortSongs {p, nArgs},
      m_vSongIdxs {p, nArgs},
      m_vSearchIdxs {p, nArgs},
      m_mtxQ {Mutex::TYPE::PLAIN}
{
    for (int i = 0; i < nArgs; ++i)
    {
        if (acceptedFormat(ppArgs[i]))
        {
            m_vSongs.push(m_pAlloc, ppArgs[i]);
            m_vShortSongs.push(m_pAlloc, file::getPathEnding(m_vSongs.last()));

            if (m_vSongs.last().size() > m_longestString)
                m_longestString = m_vSongs.last().size();
        }
    }

    setDefaultSongIdxs();
    setDefaultSearchIdxs();
}
