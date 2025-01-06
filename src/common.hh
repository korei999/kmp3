#pragma once

#include "adt/Arr.hh"
#include "adt/defer.hh"
#include "app.hh"

#include <cmath>
#include <cwctype>

namespace common
{

struct InputBuff {
    wchar_t m_aBuff[64] {};
    u32 m_idx = 0;
    WINDOW_READ_MODE m_eCurrMode {};
    WINDOW_READ_MODE m_eLastUsedMode {};

    /* */

    void zeroOutBuff() { memset(m_aBuff, 0, sizeof(m_aBuff)); }
    Span<wchar_t> getSpan() { return Span(m_aBuff, utils::size(m_aBuff)); }
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
procSeekString(const Span<wchar_t> spBuff)
{
    bool bPercent = false;
    bool bColon = false;

    Arr<char, 32> aMinutesBuff {};
    Arr<char, 32> aSecondsBuff {};

    for (auto& wch : spBuff)
    {
        if (wch == L'%') bPercent = true;
        else if (wch == L':')
        {
            /* leave if there is one more colon or bPercent */
            if (bColon || bPercent) break;
            bColon = true;
        }
        else if (iswdigit(wch))
        {
            Arr<char, 32>* pTargetArray = bColon ? &aSecondsBuff : &aMinutesBuff;
            if (spBuff.idx(&wch) < pTargetArray->getCap() - 1)
                pTargetArray->push(char(wch)); /* wdigits are equivalent to char */
        }
    }

    if (aMinutesBuff.getSize() == 0)
        return;

    if (bPercent)
    {
        s64 maxMS = app::g_pMixer->getTotalMS();

        app::g_pMixer->seekMS(maxMS * (f64(atoll(aMinutesBuff.data())) / 100.0));
    }
    else
    {
        ssize sec;
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

    g_input.m_eLastUsedMode = g_input.m_eCurrMode = WINDOW_READ_MODE::SEARCH;
    defer( g_input.m_eCurrMode = WINDOW_READ_MODE::NONE );

    g_input.zeroOutBuff();
    g_input.m_idx = 0;

    do
    {
        pl.subStringSearch(pArena, {g_input.m_aBuff, utils::size(g_input.m_aBuff)});
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
    g_input.m_eLastUsedMode = g_input.m_eCurrMode = WINDOW_READ_MODE::SEEK;
    defer( g_input.m_eCurrMode = WINDOW_READ_MODE::NONE );

    g_input.zeroOutBuff();
    g_input.m_idx = 0;

    do
    {
        pfnDraw(pDrawArg);
    } while (pfnRead(pReadArg) != READ_STATUS::DONE);

    procSeekString(g_input.getSpan());
}

} /* namespace common */
