#include "Player.hh"

#include "adt/Arr.hh"
#include "adt/logs.hh"
#include "adt/file.hh"
#include "app.hh"
#include "defaults.hh"
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
    if (ns >= long(m_vSongIdxs.getSize())) ns = 0;
    m_focused = ns;
}

void
Player::focusPrev()
{
    long ns = m_focused - 1;
    if (ns < 0) ns = m_vSongIdxs.getSize() - 1;
    m_focused = ns;
}

void
Player::focus(long i)
{
    m_focused = utils::clamp(i, 0L, long(m_vSongIdxs.getSize() - 1));
}

void
Player::focusLast()
{
    focus(m_vSongIdxs.getSize() - 1);
}

u16
Player::findSongIdxFromSelected()
{
    if (m_vSongs.empty())
        return 0;

    u16 res = NPOS16;

    for (const auto& idx : m_vSearchIdxs)
    {
        if (idx == m_selected)
        {
            res = m_vSearchIdxs.idx(&idx);
            break;
        }
    }

    if (res == NPOS16)
    {
        setDefaultSearchIdxs();
        setDefaultSongIdxs();
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
Player::focusSelectedCenter()
{
    focusSelected();
    app::g_pWin->centerAroundSelection();
}

void
Player::setDefaultIdxs(VecBase<u16>* pIdxs)
{
    ssize size = m_vSongs.getSize();
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

    m_vSearchIdxs.setSize(m_pAlloc, 0);
    for (u16 songIdx : m_vSongIdxs)
    {
        const String song = m_vShortSongs[songIdx];

        aSongToUpper.zeroOut();
        mbstowcs(aSongToUpper.data(), song.data(), song.getSize());
        for (auto& wc : aSongToUpper)
            wc = towupper(wc);

        if (wcsstr(aSongToUpper.data(), aUpperRight.data()) != nullptr)
            m_vSearchIdxs.push(m_pAlloc, songIdx);
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
    if (m_vSongIdxs.getSize() <= m_focused)
    {
        LOG_WARN("PlayerSelectFocused(): out of range selection: (vec.size: {})\n", m_vSongIdxs.getSize());
        return;
    }

    m_selected = m_vSongIdxs[m_focused];
    m_bSelectionChanged = true;

    const String& sPath = m_vSongs[m_selected];
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
    else if (currIdx >= m_vSongIdxs.getSize())
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

    m_selected = m_vSongIdxs[currIdx];
    m_bSelectionChanged = true;

    app::g_pMixer->play(m_vSongs[m_selected]);
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
Player::select(long i)
{
    if (m_vSongs.empty() || m_vSearchIdxs.empty())
        return;

    long idx = utils::clamp(i, 0L, long(m_vSearchIdxs.lastI()));
    m_selected = m_vSearchIdxs[idx];

    app::g_pMixer->play(m_vSongs[m_selected]);
    updateInfo();
}

void
Player::selectNext()
{
    long idx = (findSongIdxFromSelected() + 1) % m_vSearchIdxs.getSize();
    select(idx);
}

void
Player::selectPrev()
{
    long idx = (findSongIdxFromSelected() + (m_vSearchIdxs.getSize()) - 1) % m_vSearchIdxs.getSize();
    select(idx);
}

void
Player::copySearchToSongIdxs()
{
    m_vSongIdxs.setSize(m_pAlloc, m_vSearchIdxs.getSize());
    utils::copy(m_vSongIdxs.data(), m_vSearchIdxs.data(), m_vSearchIdxs.getSize());
}

void
Player::setImgSize(long height)
{
    if (height < 0) height = 10;
    m_imgHeight = height;
    m_imgWidth = std::round((m_imgHeight * (1920.0/1080.0)) / defaults::FONT_ASPECT_RATIO);
    m_bSelectionChanged = true;

    if (app::g_pWin)
    {
        app::g_pWin->m_bClear = true;
        app::g_pWin->adjustListHeight();
    }
}

void
Player::destroy()
{
    //
}

Player::Player(IAllocator* p, int nArgs, char** ppArgs)
    : m_pAlloc(p), m_vSongs(p, nArgs), m_vShortSongs(p, nArgs), m_vSongIdxs(p, nArgs), m_vSearchIdxs(p, nArgs)
{
    for (int i = 0; i < nArgs; ++i)
    {
        if (acceptedFormat(ppArgs[i]))
        {
            m_vSongs.push(m_pAlloc, ppArgs[i]);
            m_vShortSongs.push(m_pAlloc, file::getPathEnding(m_vSongs.last()));

            if (m_vSongs.last().getSize() > m_longestString)
                m_longestString = m_vSongs.last().getSize();
        }
    }

    setDefaultSongIdxs();
    setDefaultSearchIdxs();
}
