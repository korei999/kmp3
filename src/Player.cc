#include "Player.hh"

#include "adt/Arr.hh"
#include "app.hh"
#include "logs.hh"

#include <cstdlib>
#include <cwchar>
#include <cwctype>

constexpr String aAcceptedFileEndings[] {
    ".mp2", ".mp3", ".mp4", ".m4a", ".m4b",
    ".fla", ".flac",
    ".ogg", ".opus",
    ".umx", ".s3m",
    ".wav", ".caf", ".aif",
    ".webm",
    ".mkv",
};

static inline bool
acceptedFormat(const String s)
{
    for (const auto ending : aAcceptedFileEndings)
        if (StringEndsWith(s, ending))
            return true;

    return false;
}

void
PlayerNext(Player* s)
{
    long ns = s->focused + 1;
    if (ns >= long(s->aSongIdxs.size)) ns = 0;
    s->focused = ns;
}

void
PlayerPrev(Player* s)
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
PlayerFocusFirst(Player* s)
{
    PlayerFocus(s, 0);
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
            res = VecIdx(&s->aSongIdxs, &idx);
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
    VecSetSize(&s->aSongIdxs, s->pAlloc, 0);

    for (int i = 1; i < app::g_argc; ++i)
        if (acceptedFormat(app::g_aArgs[i]))
            VecPush(&s->aSongIdxs, s->pAlloc, u16(i));
}

void
PlayerSubStringSearch(Player* s, Allocator* pAlloc, wchar_t* pBuff, u32 size)
{
    if (pBuff && wcsnlen(pBuff, size) == 0)
    {
        PlayerSetDefaultIdxs(s);
        return;
    }

    Arr<wchar_t, 64> aUpperRight {};
    for (u32 i = 0; i < size && i < ArrCap(&aUpperRight) && pBuff[i]; ++i)
        ArrPush(&aUpperRight, wchar_t(towupper(pBuff[i])));

    Vec<wchar_t> aSongToUpper(pAlloc, s->longestStringSize + 1);
    VecSetSize(&aSongToUpper, s->longestStringSize + 1);

    VecSetSize(&s->aSongIdxs, s->pAlloc, 0);
    /* 0'th is the name of the program 'argv[0]' */
    for (u32 i = 1; i < VecSize(&s->aShortArgvs); ++i)
    {
        const auto& song = s->aShortArgvs[i];
        if (!acceptedFormat(song)) continue;

        utils::fill(VecData(&aSongToUpper), L'\0', VecSize(&aSongToUpper));
        mbstowcs(VecData(&aSongToUpper), song.pData, song.size);
        for (auto& wc : aSongToUpper)
            wc = towupper(wc);

        if (wcsstr(VecData(&aSongToUpper), aUpperRight.pData) != nullptr)
            VecPush(&s->aSongIdxs, s->pAlloc, u16(i));
    }
}

static void
updateInfo(Player* s)
{
    s->info.title = audio::MixerGetMetadata(app::g_pMixer, "title").data;
    s->info.album = audio::MixerGetMetadata(app::g_pMixer, "album").data;
    s->info.artist = audio::MixerGetMetadata(app::g_pMixer, "artist").data;
}

void
PlayerSelectFocused(Player* s)
{
    s->selected = s->aSongIdxs[s->focused];
    const String& sPath = app::g_aArgs[s->selected];
    LOG_NOTIFY("selected({}): {}\n", s->selected, sPath);

    audio::MixerPlay(app::g_pMixer, sPath);
    updateInfo(s);
}

void
PlayerPause(Player* s, bool bPause)
{
    audio::MixerPause(app::g_pMixer, bPause);
}

void
PlayerTogglePause(Player* s)
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
    else if (currIdx >= VecSize(&s->aSongIdxs))
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
    PLAYER_REPEAT_METHOD rm {};

    if (bForward) rm = PLAYER_REPEAT_METHOD((s->eReapetMethod + 1) % ESIZE);
    else rm = PLAYER_REPEAT_METHOD((s->eReapetMethod + (ESIZE - 1)) % ESIZE);

    s->eReapetMethod = rm;
    return rm;
}

void
PlayerSelectNext(Player* s)
{
    long currIdx = (PlayerFindSongIdxFromSelected(s) + 1) % VecSize(&s->aSongIdxs);
    s->selected = s->aSongIdxs[currIdx];
    audio::MixerPlay(app::g_pMixer, app::g_aArgs[s->selected]);
    updateInfo(s);
}

void
PlayerSelectPrev(Player* s)
{
    long currIdx = (PlayerFindSongIdxFromSelected(s) + (VecSize(&s->aSongIdxs) - 1)) % VecSize(&s->aSongIdxs);
    s->selected = s->aSongIdxs[currIdx];
    audio::MixerPlay(app::g_pMixer, app::g_aArgs[s->selected]);
    updateInfo(s);
}
