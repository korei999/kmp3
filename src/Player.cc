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

static void
allocMetaData(Player* s)
{
    s->info.title = StringAlloc(s->pAlloc, audio::MixerGetMetadata(app::g_pMixer, "title").data);
    s->info.album = StringAlloc(s->pAlloc, audio::MixerGetMetadata(app::g_pMixer, "album").data);
    s->info.artist = StringAlloc(s->pAlloc, audio::MixerGetMetadata(app::g_pMixer, "artist").data);
}

static void
freeMetaData(Player* s)
{
    s->info.title.destroy(s->pAlloc);
    s->info.album.destroy(s->pAlloc);
    s->info.artist.destroy(s->pAlloc);
}

bool
PlayerAcceptedFormat(const String s)
{
    for (const auto ending : aAcceptedFileEndings)
        if (StringEndsWith(s, ending))
            return true;

    return false;
}

void
PlayerFocusNext(Player* s)
{
    long ns = s->focused + 1;
    if (ns >= long(s->aSongIdxs.size)) ns = 0;
    s->focused = ns;
}

void
PlayerFocusPrev(Player* s)
{
    long ns = s->focused - 1;
    if (ns < 0) ns = s->aSongIdxs.size - 1;
    s->focused = ns;
}

void
PlayerFocus(Player* s, long i)
{
    s->focused = utils::clamp(i, 0L, long(s->aSongIdxs.size - 1));
}

void
PlayerFocusLast(Player* s)
{
    PlayerFocus(s, s->aSongIdxs.size - 1);
}

u16
PlayerFindSongIdxFromSelected(Player* s)
{
    u16 res = NPOS16;

    for (const auto& idx : s->aSongIdxs)
    {
        if (idx == s->selected)
        {
            res = s->aSongIdxs.idx(&idx);
            break;
        }
    }

    if (res == NPOS16)
    {
        PlayerSetDefaultIdxs(s);
        return PlayerFindSongIdxFromSelected(s);
    }
    else return res;
}

void
PlayerFocusSelected(Player* s)
{
    s->focused = PlayerFindSongIdxFromSelected(s);
}

void
PlayerSetDefaultIdxs(Player* s)
{
    s->aSongIdxs.setSize(s->pAlloc, 0);

    for (int i = 1; i < app::g_argc; ++i)
        if (PlayerAcceptedFormat(app::g_aArgs[i]))
            s->aSongIdxs.push(s->pAlloc, u16(i));
}

void
PlayerSubStringSearch(Player* s, Arena* pAlloc, wchar_t* pBuff, u32 size)
{
    if (pBuff && wcsnlen(pBuff, size) == 0)
    {
        PlayerSetDefaultIdxs(s);
        return;
    }

    Arr<wchar_t, 64> aUpperRight {};
    for (u32 i = 0; i < size && i < aUpperRight.getCap() && pBuff[i]; ++i)
        aUpperRight.push(wchar_t(towupper(pBuff[i])));

    Vec<wchar_t> aSongToUpper(&pAlloc->super, s->longestStringSize + 1);
    aSongToUpper.setSize(s->longestStringSize + 1);

    s->aSongIdxs.setSize(s->pAlloc, 0);
    /* 0'th is the name of the program 'argv[0]' */
    for (u32 i = 1; i < s->aShortArgvs.getSize(); ++i)
    {
        const auto& song = s->aShortArgvs[i];
        if (!PlayerAcceptedFormat(song)) continue;

        aSongToUpper.zeroOut();
        mbstowcs(aSongToUpper.data(), song.pData, song.size);
        for (auto& wc : aSongToUpper)
            wc = towupper(wc);

        if (wcsstr(aSongToUpper.data(), aUpperRight.aData) != nullptr)
            s->aSongIdxs.push(s->pAlloc, u16(i));
    }
}

static void
updateInfo(Player* s)
{
    /* it's save to free nullptr */
    freeMetaData(s);
    /* clone string to avoid data races with ffmpeg */
    allocMetaData(s);

    s->bSelectionChanged = true;

#ifndef NDEBUG
    LOG_GOOD("freeList.size: {}\n", _FreeListGetBytesAllocated((FreeList*)s->pAlloc));
#endif
}

void
PlayerSelectFocused(Player* s)
{
    if (s->aSongIdxs.getSize() <= s->focused)
    {
        LOG_WARN("PlayerSelectFocused(): out of range selection: (vec.size: {})\n", s->aSongIdxs.getSize());
        return;
    }

    s->selected = s->aSongIdxs[s->focused];
    const String& sPath = app::g_aArgs[s->selected];
    LOG_NOTIFY("selected({}): {}\n", s->selected, sPath);

    audio::MixerPlay(app::g_pMixer, sPath);
    updateInfo(s);
}

void
PlayerPause([[maybe_unused]] Player* s, bool bPause)
{
    audio::MixerPause(app::g_pMixer, bPause);
}

void
PlayerTogglePause([[maybe_unused]] Player* s)
{
    audio::MixerTogglePause(app::g_pMixer);
}

void
PlayerOnSongEnd(Player* s)
{
    long currIdx = PlayerFindSongIdxFromSelected(s) + 1;
    if (s->eReapetMethod == PLAYER_REPEAT_METHOD::TRACK)
    {
        currIdx -= 1;
    }
    else if (currIdx >= s->aSongIdxs.getSize())
    {
        if (s->eReapetMethod == PLAYER_REPEAT_METHOD::PLAYLIST)
        {
            currIdx = 0;
        }
        else
        {
            app::g_bRunning = false;
            return;
        }
    }

    s->selected = s->aSongIdxs[currIdx];

    audio::MixerPlay(app::g_pMixer, app::g_aArgs[s->selected]);
    updateInfo(s);
}

PLAYER_REPEAT_METHOD
PlayerCycleRepeatMethods(Player* s, bool bForward)
{
    using enum PLAYER_REPEAT_METHOD;

    int rm;
    if (bForward) rm = ((int(s->eReapetMethod) + 1) % int(ESIZE));
    else rm = ((int(s->eReapetMethod) + (int(ESIZE) - 1)) % int(ESIZE));

    s->eReapetMethod = PLAYER_REPEAT_METHOD(rm);

    mpris::loopStatusChanged();

    return PLAYER_REPEAT_METHOD(rm);
}

void
PlayerSelectNext(Player* s)
{
    long currIdx = (PlayerFindSongIdxFromSelected(s) + 1) % s->aSongIdxs.getSize();
    s->selected = s->aSongIdxs[currIdx];
    audio::MixerPlay(app::g_pMixer, app::g_aArgs[s->selected]);
    updateInfo(s);
}

void
PlayerSelectPrev(Player* s)
{
    long currIdx = (PlayerFindSongIdxFromSelected(s) + (s->aSongIdxs.getSize()) - 1) % s->aSongIdxs.getSize();
    s->selected = s->aSongIdxs[currIdx];
    audio::MixerPlay(app::g_pMixer, app::g_aArgs[s->selected]);
    updateInfo(s);
}

void
PlayerDestroy([[maybe_unused]] Player* s)
{
    //
}
