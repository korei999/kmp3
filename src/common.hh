#pragma once

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
    WINDOW_READ_MODE eCurrMode {};
    WINDOW_READ_MODE eLastUsedMode {};

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
constexpr wchar_t CURSOR_BLOCK[] {L'█', L'\0'};

[[nodiscard]] inline constexpr String
readModeToString(WINDOW_READ_MODE e)
{
    constexpr String map[] {"", "searching: ", "time: "};
    return map[int(e)];
}

[[nodiscard]] inline String
allocTimeString(Arena* pArena, int width)
{
    auto& mix = *app::g_pMixer;
    char* pBuff = (char*)pArena->zalloc(1, width + 1);

    f64 sampleRateRatio = f64(mix.getSampleRate()) / f64(mix.getChangedSampleRate());

    u64 t = std::round(mix.getCurrentMS() / 1000.0 * sampleRateRatio);
    u64 totalT = std::round(mix.getTotalMS() / 1000.0 * sampleRateRatio);

    /*u64 t = std::round(f64(mix.getCurrentTimeStamp()) / f64(mix.getNChannels()) / f64(mix.getChangedSampleRate()));*/
    /*u64 totalT = std::round(f64(mix.getTotalSamplesCount()) / f64(mix.getNChannels()) / f64(mix.getChangedSampleRate()));*/

    u64 currMin = t / 60;
    u64 currSec = t - (60 * currMin);

    u64 maxMin = totalT / 60;
    u64 maxSec = totalT - (60 * maxMin);

    int n = snprintf(pBuff, width, "time: %llu:%02llu / %llu:%02llu", currMin, currSec, maxMin, maxSec);
    if (mix.getSampleRate() != mix.getChangedSampleRate())
        snprintf(pBuff + n, width - n, " (%d%% speed)", int(std::round(f64(mix.getChangedSampleRate()) / f64(mix.getSampleRate()) * 100.0)));

    return pBuff;
}

/* fix song list range on new focus */
inline void
fixFirstIdx(const u16 listHeight, u16* pFirstIdx)
{
    const auto& pl = *app::g_pPlayer;

    const u16 focused = pl.m_focused;
    u16 first = *pFirstIdx;

    if (focused > first + listHeight)
        first = focused - listHeight;
    else if (focused < first)
        first = focused;
    else if (pl.m_aSongIdxs.getSize() < listHeight)
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
            if (i < pTargetArray->getCap() - 1)
                pTargetArray->push(char(buff[i]));
        }
    }

    if (aMinutesBuff.getSize() == 0) return;

    if (bPercent)
    {
        long maxMS = app::g_pMixer->getTotalMS();

        app::g_pMixer->seekMS(maxMS * (f64(atoll(aMinutesBuff.data())) / 100.0));
    }
    else
    {
        long sec;
        if (aSecondsBuff.getSize() == 0) sec = atoll(aMinutesBuff.data());
        else sec = atoll(aSecondsBuff.data()) + atoll(aMinutesBuff.data())*60;

        app::g_pMixer->seekMS(sec * 1000);
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

    g_input.eLastUsedMode = g_input.eCurrMode = WINDOW_READ_MODE::SEARCH;
    defer( g_input.eCurrMode = WINDOW_READ_MODE::NONE );

    g_input.zeroOutBuff();
    g_input.idx = 0;

    do
    {
        pl.subStringSearch(pArena, g_input.aBuff, utils::size(g_input.aBuff));
        *pFirstIdx = 0;
        pfnDraw(pDrawArg);

    } while (pfnRead(pReadArg) != READ_STATUS::DONE);

    /* fix focused if it ends up out of the list range */
    if (pl.m_focused >= pl.m_aSongIdxs.getSize())
        pl.m_focused = *pFirstIdx;
}

inline void
seekFromInput(
    READ_STATUS (*pfnRead)(void*),
    void* pReadArg,
    void (*pfnDraw)(void*),
    void* pDrawArg
)
{
    g_input.eLastUsedMode = g_input.eCurrMode = WINDOW_READ_MODE::SEEK;
    defer( g_input.eCurrMode = WINDOW_READ_MODE::NONE );

    g_input.zeroOutBuff();
    g_input.idx = 0;

    do
    {
        pfnDraw(pDrawArg);
    } while (pfnRead(pReadArg) != READ_STATUS::DONE);

    procSeekString(g_input.aBuff, utils::size(g_input.aBuff));
}

inline int
getHorizontalSplitPos(int height)
{
    return std::round(f64(height) * (1.0 - app::g_pPlayer->m_statusToInfoWidthRatio));
}


} /* namespace common */
