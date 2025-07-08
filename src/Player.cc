#include "Player.hh"

#include "app.hh"
#include "defaults.hh"
#include "mpris.hh"

#include "adt/Array.hh"
#include "adt/logs.hh"
#include "adt/file.hh"

#include <cstdlib>
#include <cwchar>
#include <cwctype>

using namespace adt;

constexpr StringView aAcceptedFileEndings[] {
    ".mp2", ".mp3", ".mp4", ".m4a", ".m4b",
    ".fla", ".flac",
    ".ogg", ".opus",
    ".umx", ".s3m",
    ".wav", ".caf", ".aif",
    ".webm",
    ".mkv",
};

bool
Player::acceptedFormat(const StringView s)
{
    return utils::searchI(aAcceptedFileEndings, [&](const StringView ending)
        { return s.endsWith(ending); }
    ) != NPOS;
}

void
Player::focusNext()
{
    long ns = m_focused + 1;
    if (ns >= m_vSongIdxs.size()) ns = 0;
    m_focused = ns;
}

void
Player::focusPrev()
{
    long prev = m_focused - 1;
    if (prev < 0)
    {
        if (m_vSongIdxs.empty()) prev = 0;
        else prev = m_vSongIdxs.size() - 1;
    }
    m_focused = prev;
}

void
Player::focus(long i)
{
    m_focused = utils::clamp(i, 0L, long(m_vSongIdxs.size() - 1));
}

void
Player::focusLast()
{
    focus(m_vSongIdxs.size() - 1);
}

long
Player::findSongI(long toFindI)
{
    if (m_vSongs.empty()) return 0;

again:
    const isize res = utils::searchI(m_vSearchIdxs, [&](u16 e) { return e == toFindI; });

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
    focus(findSongI(m_selected));
}

void
Player::setDefaultIdxs(Vec<u16>* pvIdxs)
{
    pvIdxs->setSize(m_pAlloc, m_vSongs.size());
    for (auto& e : *pvIdxs) e = pvIdxs->idx(&e);
}

void
Player::subStringSearch(Arena* pArena, Span<wchar_t> spBuff)
{
    if (spBuff && wcsnlen(spBuff.data(), spBuff.size()) == 0)
        return;

    Array<wchar_t, 64> aUpperRight {};
    const isize maxLen = utils::min(spBuff.size(), aUpperRight.cap());

    for (isize i = 0; i < maxLen && spBuff[i]; ++i)
        aUpperRight.push(wchar_t(towupper(spBuff[i])));

    Vec<wchar_t> aSongToUpper(pArena, m_longestString + 1);
    aSongToUpper.setSize(pArena, m_longestString + 1);

    m_vSearchIdxs.setSize(m_pAlloc, 0);
    for (const u16 songIdx : m_vSongIdxs)
    {
        const StringView song = m_vShortSongs[songIdx];

        aSongToUpper.zeroOut();
        mbstowcs(aSongToUpper.data(), song.data(), song.size());

        for (auto& wc : aSongToUpper)
            wc = towupper(wc);

        if (wcsstr(aSongToUpper.data(), aUpperRight.data()) != nullptr)
            m_vSearchIdxs.push(m_pAlloc, songIdx);
    }
}

long
Player::nextSelectionI(long selI)
{
    const long currI = findSongI(selI);
    long nextI = currI + 1;

    defer( LOG_WARN("currI: {}, nextI: {}\n", currI, nextI) );

    if (m_eReapetMethod == PLAYER_REPEAT_METHOD::TRACK)
    {
        nextI = currI;
    }
    else if (nextI >= m_vSongIdxs.size())
    {
        if (m_eReapetMethod == PLAYER_REPEAT_METHOD::PLAYLIST)
        {
            nextI = 0;
        }
        else
        {
            app::g_bRunning = false;
            return m_vSongIdxs[currI];
        }
    }

    return m_vSongIdxs[nextI];
}

void
Player::updateInfo()
{
    m_info.sfTitle = app::decoder().getMetadata("title");
    m_info.sfAlbum = app::decoder().getMetadata("album");
    m_info.sfArtist = app::decoder().getMetadata("artist");

    if (m_info.sfTitle.size() == 0)
        m_info.sfTitle = m_vShortSongs[m_selected];

    m_bSelectionChanged = true;
}

void
Player::selectFinal(long selI)
{
    long nFailed = 0;

    while (!app::mixer().play(m_vSongs[selI]))
    {
        LOG_WARN("failed to open: '{}', selI: {}\n", m_vSongs[selI], selI);

        if (!app::g_bRunning) return;

        if (++nFailed >= m_vSearchIdxs.size())
        {
            LOG_BAD("QUIT (nFailed: {}, size: {})\n", nFailed, m_vSearchIdxs.size());
            app::quit();
            return;
        }

        Msg msg {
            .time = 5000.0,
            .eType = Msg::TYPE::ERROR,
        };
        print::toSpan(msg.sfMsg.data(), "failed to open \"{}\"", file::getPathEnding(m_vSongs[selI]));

        if ((msg.sfMsg != m_sfLastMessage) || (utils::timeNowMS() >= (m_lastPushedMessageTime + msg.time)))
            pushErrorMsg(msg);

        selI = nextSelectionI(selI); /* Might set g_bRunning. */
    }

    m_bSelectionChanged = true;
    m_selected = selI;
    updateInfo();
}

void
Player::selectFocused()
{
    if (m_vSongIdxs.size() <= m_focused)
    {
        LOG_WARN("PlayerSelectFocused(): out of range selection: (vec.size: {})\n", m_vSongIdxs.size());
        return;
    }

    LOG_GOOD("selected: {}\n", m_vSongs[m_vSongIdxs[m_focused]]);
    selectFinal(m_vSongIdxs[m_focused]);
}

void
Player::togglePause()
{
    app::mixer().togglePause();
}

void
Player::nextSongIfPrevEnded()
{
    int bEnd = true;
    if (app::mixer().m_atom_bSongEnd.compareExchange(
            &bEnd, false,
            atomic::ORDER::RELAXED, atomic::ORDER::RELAXED
        )
    )
    {
        selectFinal(nextSelectionI(m_selected));
    }
}

PLAYER_REPEAT_METHOD
Player::cycleRepeatMethods(bool bForward)
{
    using enum PLAYER_REPEAT_METHOD;

    int rm;
    if (bForward) rm = ((int(m_eReapetMethod) + 1) % int(ESIZE));
    else rm = ((int(m_eReapetMethod) + (int(ESIZE) - 1)) % int(ESIZE));

    m_eReapetMethod = PLAYER_REPEAT_METHOD(rm);

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
    select((findSongI(m_selected) + 1) % m_vSearchIdxs.size());
}

void
Player::selectPrev()
{
    select((findSongI(m_selected) + (m_vSearchIdxs.size()) - 1) % m_vSearchIdxs.size());
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
    height = utils::clamp(height, long(defaults::MIN_IMAGE_HEIGHT), long(defaults::MAX_IMAGE_HEIGHT));

    if (m_imgHeight == height) return;

    m_imgHeight = height;
    adjustImgWidth();

    m_bSelectionChanged = true;
    if (app::g_pWin) app::g_pWin->m_bClear = true;
}

void
Player::adjustImgWidth()
{
    m_imgWidth = std::round((m_imgHeight * (1920.0/1080.0)) / defaults::FONT_ASPECT_RATIO);
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
    LockGuard lock {&m_mtxQ};
    m_qErrorMsgs.pushBack(msg);

    m_sfLastMessage = msg.sfMsg;
    m_lastPushedMessageTime = utils::timeNowMS();
}

Player::Msg
Player::popErrorMsg()
{
    {
        LockGuard lock {&m_mtxQ};
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
        LOG_BAD("'{}'\n", ppArgs[i]);
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
