#include "Player.hh"

#include "adt/Arr.hh"
#include "adt/logs.hh"
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
        if (StringEndsWith(s, ending))
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
        setDefaultIdxs();
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
Player::setDefaultIdxs()
{
    m_aSongIdxs.setSize(m_pAlloc, 0);

    for (int i = 1; i < app::g_argc; ++i)
        if (acceptedFormat(app::g_aArgs[i]))
            m_aSongIdxs.push(m_pAlloc, u16(i));
}

void
Player::subStringSearch(Arena* pAlloc, wchar_t* pBuff, u32 size)
{
    if (pBuff && wcsnlen(pBuff, size) == 0)
    {
        setDefaultIdxs();
        return;
    }

    Arr<wchar_t, 64> aUpperRight {};
    for (u32 i = 0; i < size && i < aUpperRight.getCap() && pBuff[i]; ++i)
        aUpperRight.push(wchar_t(towupper(pBuff[i])));

    Vec<wchar_t> aSongToUpper(pAlloc, m_longestStringSize + 1);
    aSongToUpper.setSize(m_longestStringSize + 1);

    m_aSongIdxs.setSize(m_pAlloc, 0);
    /* 0'th is the name of the program 'argv[0]' */
    for (u32 i = 1; i < m_aShortArgvs.getSize(); ++i)
    {
        const auto& song = m_aShortArgvs[i];
        if (!acceptedFormat(song)) continue;

        aSongToUpper.zeroOut();
        mbstowcs(aSongToUpper.data(), song.data(), song.getSize());
        for (auto& wc : aSongToUpper)
            wc = towupper(wc);

        if (wcsstr(aSongToUpper.data(), aUpperRight.data()) != nullptr)
            m_aSongIdxs.push(m_pAlloc, u16(i));
    }
}

static void
updateInfo(Player* s)
{
    String s0 = app::g_pMixer->getMetadata("title").data.clone(s->m_pAlloc);
    String s1 = app::g_pMixer->getMetadata("album").data.clone(s->m_pAlloc);
    String s2 = app::g_pMixer->getMetadata("artist").data.clone(s->m_pAlloc);

    s->m_info.title.destroy(s->m_pAlloc);
    s->m_info.album.destroy(s->m_pAlloc);
    s->m_info.artist.destroy(s->m_pAlloc);

    s->m_info.title = s0;
    s->m_info.album = s1;
    s->m_info.artist = s2;

    s->m_bSelectionChanged = true;

#ifndef NDEBUG
    LOG_GOOD("freeList.size: {}\n", _FreeListNBytesAllocated((FreeList*)s->m_pAlloc));
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

    const String& sPath = app::g_aArgs[m_selected];
    LOG_NOTIFY("selected({}): {}\n", m_selected, sPath);

    app::g_pMixer->play(sPath);
    updateInfo(this);
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

    app::g_pMixer->play(app::g_aArgs[m_selected]);
    updateInfo(this);
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
    app::g_pMixer->play(app::g_aArgs[m_selected]);
    updateInfo(this);
}

void
Player::selectPrev()
{
    long currIdx = (findSongIdxFromSelected() + (m_aSongIdxs.getSize()) - 1) % m_aSongIdxs.getSize();
    m_selected = m_aSongIdxs[currIdx];
    app::g_pMixer->play(app::g_aArgs[m_selected]);
    updateInfo(this);
}

void
Player::destroy()
{
    //
}
