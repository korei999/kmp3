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
    long ns = this->focused + 1;
    if (ns >= long(this->aSongIdxs.size)) ns = 0;
    this->focused = ns;
}

void
Player::focusPrev()
{
    long ns = this->focused - 1;
    if (ns < 0) ns = this->aSongIdxs.size - 1;
    this->focused = ns;
}

void
Player::focus(long i)
{
    this->focused = utils::clamp(i, 0L, long(this->aSongIdxs.size - 1));
}

void
Player::focusLast()
{
    this->focus(this->aSongIdxs.size - 1);
}

u16
Player::findSongIdxFromSelected()
{
    u16 res = NPOS16;

    for (const auto& idx : this->aSongIdxs)
    {
        if (idx == this->selected)
        {
            res = this->aSongIdxs.idx(&idx);
            break;
        }
    }

    if (res == NPOS16)
    {
        this->setDefaultIdxs();
        return this->findSongIdxFromSelected();
    }
    else return res;
}

void
Player::focusSelected()
{
    this->focused = this->findSongIdxFromSelected();
}

void
Player::setDefaultIdxs()
{
    this->aSongIdxs.setSize(this->pAlloc, 0);

    for (int i = 1; i < app::g_argc; ++i)
        if (this->acceptedFormat(app::g_aArgs[i]))
            this->aSongIdxs.push(this->pAlloc, u16(i));
}

void
Player::subStringSearch(Arena* pAlloc, wchar_t* pBuff, u32 size)
{
    if (pBuff && wcsnlen(pBuff, size) == 0)
    {
        this->setDefaultIdxs();
        return;
    }

    Arr<wchar_t, 64> aUpperRight {};
    for (u32 i = 0; i < size && i < aUpperRight.getCap() && pBuff[i]; ++i)
        aUpperRight.push(wchar_t(towupper(pBuff[i])));

    Vec<wchar_t> aSongToUpper(&pAlloc->super, this->longestStringSize + 1);
    aSongToUpper.setSize(this->longestStringSize + 1);

    this->aSongIdxs.setSize(this->pAlloc, 0);
    /* 0'th is the name of the program 'argv[0]' */
    for (u32 i = 1; i < this->aShortArgvs.getSize(); ++i)
    {
        const auto& song = this->aShortArgvs[i];
        if (!this->acceptedFormat(song)) continue;

        aSongToUpper.zeroOut();
        mbstowcs(aSongToUpper.data(), song.pData, song.size);
        for (auto& wc : aSongToUpper)
            wc = towupper(wc);

        if (wcsstr(aSongToUpper.data(), aUpperRight.aData) != nullptr)
            this->aSongIdxs.push(this->pAlloc, u16(i));
    }
}

static void
updateInfo(Player* s)
{
    String sTitle = s->info.title;
    String sAlbum = s->info.album;
    String sArtist = s->info.artist;

    s->info.title = StringAlloc(s->pAlloc, app::g_pMixer->getMetadata("title").data);
    s->info.album = StringAlloc(s->pAlloc, app::g_pMixer->getMetadata("album").data);
    s->info.artist = StringAlloc(s->pAlloc, app::g_pMixer->getMetadata("artist").data);

    sTitle.destroy(s->pAlloc);
    sAlbum.destroy(s->pAlloc);
    sArtist.destroy(s->pAlloc);

    s->bSelectionChanged = true;

#ifndef NDEBUG
    LOG_GOOD("freeList.size: {}\n", _FreeListNBytesAllocated((FreeList*)s->pAlloc));
#endif
}

void
Player::selectFocused()
{
    if (this->aSongIdxs.getSize() <= this->focused)
    {
        LOG_WARN("PlayerSelectFocused(): out of range selection: (vec.size: {})\n", this->aSongIdxs.getSize());
        return;
    }

    this->selected = this->aSongIdxs[this->focused];
    const String& sPath = app::g_aArgs[this->selected];
    LOG_NOTIFY("selected({}): {}\n", this->selected, sPath);

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
    long currIdx = this->findSongIdxFromSelected() + 1;
    if (this->eReapetMethod == PLAYER_REPEAT_METHOD::TRACK)
    {
        currIdx -= 1;
    }
    else if (currIdx >= this->aSongIdxs.getSize())
    {
        if (this->eReapetMethod == PLAYER_REPEAT_METHOD::PLAYLIST)
        {
            currIdx = 0;
        }
        else
        {
            app::g_bRunning = false;
            return;
        }
    }

    this->selected = this->aSongIdxs[currIdx];

    app::g_pMixer->play(app::g_aArgs[this->selected]);
    updateInfo(this);
}

PLAYER_REPEAT_METHOD
Player::cycleRepeatMethods(bool bForward)
{
    using enum PLAYER_REPEAT_METHOD;

    int rm;
    if (bForward) rm = ((int(this->eReapetMethod) + 1) % int(ESIZE));
    else rm = ((int(this->eReapetMethod) + (int(ESIZE) - 1)) % int(ESIZE));

    this->eReapetMethod = PLAYER_REPEAT_METHOD(rm);

    mpris::loopStatusChanged();

    return PLAYER_REPEAT_METHOD(rm);
}

void
Player::selectNext()
{
    long currIdx = (this->findSongIdxFromSelected() + 1) % this->aSongIdxs.getSize();
    this->selected = this->aSongIdxs[currIdx];
    app::g_pMixer->play(app::g_aArgs[this->selected]);
    updateInfo(this);
}

void
Player::selectPrev()
{
    long currIdx = (this->findSongIdxFromSelected() + (this->aSongIdxs.getSize()) - 1) % this->aSongIdxs.getSize();
    this->selected = this->aSongIdxs[currIdx];
    app::g_pMixer->play(app::g_aArgs[this->selected]);
    updateInfo(this);
}

void
Player::destroy()
{
    //
}
