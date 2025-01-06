#include "Player.hh"

#include "adt/Arr.hh"
#include "adt/logs.hh"
#include "adt/file.hh"
#include "app.hh"
#include "mpris.hh"

#include <cstdlib>
#include <cwchar>
#include <cwctype>

#ifndef NDEBUG
    #include "adt/FreeList.hh"
#endif

constexpr String aAcceptedFileEndings[] {
    ".mp2", ".mp3", ".mp4", ".m4a", ".m4b",
    ".fla", ".flac",
    ".ogg", ".opus",
    ".umx", ".s3m",
    ".wav", ".caf", ".aif",
    ".webm",
    ".mkv",
};

bool
Player::acceptedFormat(const String s)
{
    for (const auto ending : aAcceptedFileEndings)
        if (s.endsWith(ending))
            return true;

    return false;
}

void
Player::focusNext()
{
    long ns = m_focused + 1;
    if (ns >= long(m_aSongIdxs.getSize())) ns = 0;
    m_focused = ns;
}

void
Player::focusPrev()
{
    long ns = m_focused - 1;
    if (ns < 0) ns = m_aSongIdxs.getSize() - 1;
    m_focused = ns;
}

void
Player::focus(long i)
{
    m_focused = utils::clamp(i, 0L, long(m_aSongIdxs.getSize() - 1));
}

void
Player::focusLast()
{
    focus(m_aSongIdxs.getSize() - 1);
}

u16
Player::findSongIdxFromSelected()
{
    u16 res = NPOS16;

    for (const auto& idx : m_aSongIdxs)
    {
        if (idx == m_selected)
        {
            res = m_aSongIdxs.idx(&idx);
            break;
        }
    }

    if (res == NPOS16)
    {
        setDefaultIdxs(&m_aSongIdxs);
        return findSongIdxFromSelected();
    }
    else return res;
}

void
Player::focusSelected()
{
    focus(findSongIdxFromSelected());
}

void
Player::setDefaultIdxs(VecBase<u16>* pIdxs)
{
    ssize size = m_aSongs.getSize();
    pIdxs->setSize(m_pAlloc, size);

    for (ssize i = 0; i < size; ++i)
        (*pIdxs)[i] = i;
}

void
Player::subStringSearch(Arena* pAlloc, Span<wchar_t> spBuff)
{
    if (spBuff && wcsnlen(spBuff.data(), spBuff.getSize()) == 0)
        return;

    Arr<wchar_t, 64> aUpperRight {};
    ssize maxLen = utils::min(spBuff.getSize(), aUpperRight.getCap());
    for (ssize i = 0; i < maxLen && spBuff[i]; ++i)
        aUpperRight.push(wchar_t(towupper(spBuff[i])));

    Vec<wchar_t> aSongToUpper(pAlloc, m_longestString + 1);
    aSongToUpper.setSize(m_longestString + 1);

    m_aSearchIdxs.setSize(m_pAlloc, 0);
    for (u16 idx : m_aSongIdxs)
    {
        const auto& song = m_aShortSongs[idx];

        aSongToUpper.zeroOut();
        mbstowcs(aSongToUpper.data(), song.data(), song.getSize());
        for (auto& wc : aSongToUpper)
            wc = towupper(wc);

        if (wcsstr(aSongToUpper.data(), aUpperRight.data()) != nullptr)
            m_aSearchIdxs.push(m_pAlloc, u16(m_aShortSongs.idx(&song)));
    }
}

void
Player::updateInfo()
{
    String s0 = app::g_pMixer->getMetadata("title").m_data.clone(m_pAlloc);
    String s1 = app::g_pMixer->getMetadata("album").m_data.clone(m_pAlloc);
    String s2 = app::g_pMixer->getMetadata("artist").m_data.clone(m_pAlloc);

    m_info.title.destroy(m_pAlloc);
    m_info.album.destroy(m_pAlloc);
    m_info.artist.destroy(m_pAlloc);

    m_info.title = s0;
    m_info.album = s1;
    m_info.artist = s2;

    m_bSelectionChanged = true;

#ifndef NDEBUG
    LOG_GOOD("freeList.size: {}\n", ((FreeList*)m_pAlloc)->nBytesAllocated());
#endif
}

void
Player::selectFocused()
{
    if (m_aSongIdxs.getSize() <= m_focused)
    {
        LOG_WARN("PlayerSelectFocused(): out of range selection: (vec.size: {})\n", m_aSongIdxs.getSize());
        return;
    }

    m_selected = m_aSongIdxs[m_focused];
    m_bSelectionChanged = true;

    const String& sPath = m_aSongs[m_selected];
    LOG_NOTIFY("selected({}): {}\n", m_selected, sPath);

    app::g_pMixer->play(sPath);
    updateInfo();
}

void
PlayerPause([[maybe_unused]] Player* s, bool bPause)
{
    app::g_pMixer->pause(bPause);
}

void
Player::togglePause()
{
    app::g_pMixer->togglePause();
}

void
Player::onSongEnd()
{
    long currIdx = findSongIdxFromSelected() + 1;
    if (m_eReapetMethod == PLAYER_REPEAT_METHOD::TRACK)
    {
        currIdx -= 1;
    }
    else if (currIdx >= m_aSongIdxs.getSize())
    {
        if (m_eReapetMethod == PLAYER_REPEAT_METHOD::PLAYLIST)
        {
            currIdx = 0;
        }
        else
        {
            app::g_bRunning = false;
            return;
        }
    }

    m_selected = m_aSongIdxs[currIdx];
    m_bSelectionChanged = true;

    app::g_pMixer->play(m_aSongs[m_selected]);
    updateInfo();
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
Player::selectNext()
{
    long currIdx = (findSongIdxFromSelected() + 1) % m_aSongIdxs.getSize();
    m_selected = m_aSongIdxs[currIdx];
    app::g_pMixer->play(m_aSongs[m_selected]);
    updateInfo();
}

void
Player::selectPrev()
{
    long currIdx = (findSongIdxFromSelected() + (m_aSongIdxs.getSize()) - 1) % m_aSongIdxs.getSize();
    m_selected = m_aSongIdxs[currIdx];
    app::g_pMixer->play(m_aSongs[m_selected]);
    updateInfo();
}

void
Player::copySearchIdxs()
{
    m_aSongIdxs.setSize(m_pAlloc, 0);
    for (auto idx : m_aSearchIdxs)
        m_aSongIdxs.push(m_pAlloc, idx);
}

void
Player::destroy()
{
    //
}

Player::Player(IAllocator* p, int nArgs, char** ppArgs)
    : m_pAlloc(p), m_aSongs(p, nArgs), m_aShortSongs(p, nArgs), m_aSongIdxs(p, nArgs), m_aSearchIdxs(p, nArgs)
{
    for (int i = 0; i < nArgs; ++i)
    {
        if (acceptedFormat(ppArgs[i]))
        {
            m_aSongs.push(m_pAlloc, ppArgs[i]);
            m_aShortSongs.push(m_pAlloc, file::getPathEnding(m_aSongs.last()));

            if (m_aSongs.last().getSize() > m_longestString)
                m_longestString = m_aSongs.last().getSize();
        }
    }

    setDefaultIdxs(&m_aSongIdxs);
    setDefaultIdxs(&m_aSearchIdxs);
}
