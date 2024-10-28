#include "Player.hh"

#include "adt/Arr.hh"
#include "app.hh"
#include "logs.hh"

#include <cstdlib>
#include <cwchar>
#include <cwctype>

constexpr String aAcceptedFileEndings[] {
    ".mp3",
    ".opus",
    ".flac",
    ".wav",
    ".m4a",
    ".ogg",
    ".umx",
    ".s3m",
};

static bool
acceptedFormat(String s)
{
    for (auto& ending : aAcceptedFileEndings)
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

static inline u16
selectedToSongIdx(Player* s)
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
        return selectedToSongIdx(s);
    }
    else return res;
}

void
PlayerFocusSelected(Player* s)
{
    s->focused = selectedToSongIdx(s);
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

void
PlayerSelectFocused(Player* s)
{
    s->selected = s->aSongIdxs[s->focused];
    const String& sPath = app::g_aArgs[s->selected];
    LOG_NOTIFY("selected({}): {}\n", s->selected, sPath);

    audio::MixerPlay(app::g_pMixer, sPath);
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
    long currIdx = selectedToSongIdx(s) + 1;
    if (currIdx >= VecSize(&s->aSongIdxs))
    {
        /* TODO: if repeat method... */
        app::g_bRunning = false;
        return;
    }

    s->selected = s->aSongIdxs[currIdx];

    audio::MixerPlay(app::g_pMixer, app::g_aArgs[s->selected]);
}
