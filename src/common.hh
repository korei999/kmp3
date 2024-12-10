#include "adt/Arr.hh"
#include "adt/defer.hh"
#include "app.hh"

#include <cmath>
#include <cwctype>

namespace common
{

struct InputBuff {
    wchar_t aBuff[64] {};
    u32 idx = 0;
    READ_MODE eCurrMode {};
    READ_MODE eLastUsedMode {};

    void zeroOutBuff() { memset(aBuff, 0, sizeof(aBuff)); }
};

extern InputBuff g_input;

enum class READ_STATUS : u8 { OK_, DONE };

constexpr u32 CHAR_TL = L'┏';
constexpr u32 CHAR_TR = L'┓';
constexpr u32 CHAR_BL = L'┗';
constexpr u32 CHAR_BR = L'┛';
constexpr u32 CHAR_T = L'━';
constexpr u32 CHAR_B = L'━';
constexpr u32 CHAR_L = L'┃';
constexpr u32 CHAR_R = L'┃';
constexpr u32 CHAR_VOL = L'▯';
constexpr u32 CHAR_VOL_MUTED = L'▮';
static const wchar_t* CURSOR_BLOCK = L"█";

inline constexpr String
readModeToString(READ_MODE e)
{
    constexpr String map[] {"", "searching: ", "time: "};
    return map[int(e)];
}

inline String
allocTimeString(Arena* pArena, int width)
{
    auto& mixer = *app::g_pMixer;
    char* pBuff = (char*)zalloc(pArena, 1, width + 1);

    u64 t = std::round(f64(mixer.currentTimeStamp) / f64(mixer.nChannels) / f64(mixer.changedSampleRate));
    u64 totalT = std::round(f64(mixer.totalSamplesCount) / f64(mixer.nChannels) / f64(mixer.changedSampleRate));

    u64 currMin = t / 60;
    u64 currSec = t - (60 * currMin);

    u64 maxMin = totalT / 60;
    u64 maxSec = totalT - (60 * maxMin);

    int n = snprintf(pBuff, width, "%llu:%02llu / %llu:%02llu", currMin, currSec, maxMin, maxSec);
    if (mixer.sampleRate != mixer.changedSampleRate)
        snprintf(pBuff + n, width - n, " (%d%% speed)", int(std::round(f64(mixer.changedSampleRate) / f64(mixer.sampleRate) * 100.0)));

    return pBuff;
}

/* fix song list range after focus change */
inline void
fixFirstIdx(const u16 listHeight, u16* pFirstIdx)
{
    const auto& pl = *app::g_pPlayer;

    const u16 focused = pl.focused;
    u16 first = *pFirstIdx;

    if (focused > first + listHeight)
        first = focused - listHeight;
    else if (focused < first)
        first = focused;
    else if (pl.aSongIdxs.size < listHeight)
        first = 0;

    *pFirstIdx = first;
}

inline void
procSeekString(const wchar_t* pBuff, u32 size)
{
    bool bPercent = false;
    bool bColon = false;

    Arr<char, 32> aMinutesBuff {};
    Arr<char, 32> aSecondsBuff {};

    const auto& buff = pBuff;
    for (long i = 0; buff[i] && i < size; ++i)
    {
        if (buff[i] == L'%') bPercent = true;
        else if (buff[i] == L':')
        {
            /* leave if there is one more colon or bPercent */
            if (bColon || bPercent) break;
            bColon = true;
        }
        else if (iswdigit(buff[i]))
        {
            Arr<char, 32>* pTargetArray = bColon ? &aSecondsBuff : &aMinutesBuff;
            if (i < ArrCap(pTargetArray) - 1)
                ArrPush(pTargetArray, char(buff[i]));
        }
    }

    if (aMinutesBuff.size == 0) return;

    if (bPercent)
    {
        long maxMS = audio::MixerGetMaxMS(app::g_pMixer);

        audio::MixerSeekMS(app::g_pMixer, maxMS * (f64(atoll(aMinutesBuff.aData)) / 100.0));
    }
    else
    {
        long sec;
        if (aSecondsBuff.size == 0) sec = atoll(aMinutesBuff.aData);
        else sec = atoll(aSecondsBuff.aData) + atoll(aMinutesBuff.aData)*60;

        audio::MixerSeekMS(app::g_pMixer, sec * 1000);
    }
}

inline void
subStringSearch(
    Arena* pArena,
    READ_STATUS (*pfnRead)(void*),
    void* pReadArg,
    void (*pfnDraw)(void*),
    void* pDrawArg,
    u16* pFirstIdx
)
{
    auto& pl = *app::g_pPlayer;

    g_input.eLastUsedMode = g_input.eCurrMode = READ_MODE::SEARCH;
    defer( g_input.eCurrMode = READ_MODE::NONE );

    g_input.zeroOutBuff();
    g_input.idx = 0;

    do
    {
        PlayerSubStringSearch(&pl, pArena, g_input.aBuff, utils::size(g_input.aBuff));
        *pFirstIdx = 0;
        pfnDraw(pDrawArg);

    } while (pfnRead(pReadArg) != READ_STATUS::DONE);

    /* fix focused if it ends up out of the list range */
    if (pl.focused >= (VecSize(&pl.aSongIdxs)))
        pl.focused = *pFirstIdx;
}

inline void
seekFromInput(
    READ_STATUS (*pfnRead)(void*),
    void* pReadArg,
    void (*pfnDraw)(void*),
    void* pDrawArg
)
{
    g_input.eLastUsedMode = g_input.eCurrMode = READ_MODE::SEEK;
    defer( g_input.eCurrMode = READ_MODE::NONE );

    g_input.zeroOutBuff();
    g_input.idx = 0;

    do
    {
        pfnDraw(pDrawArg);
    } while (pfnRead(pReadArg) != READ_STATUS::DONE);

    procSeekString(g_input.aBuff, utils::size(g_input.aBuff));
}

} /* namespace common */
