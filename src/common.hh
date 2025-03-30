#pragma once

#include "app.hh"

#include "adt/defer.hh"

#include <cmath>
#include <cwctype>

namespace common
{

struct InputBuff {
    wchar_t m_aBuff[64] {};
    adt::u32 m_idx = 0;
    WINDOW_READ_MODE m_eCurrMode {};
    WINDOW_READ_MODE m_eLastUsedMode {};

    /* */

    void zeroOut() { memset(m_aBuff, 0, sizeof(m_aBuff)); }
    adt::Span<wchar_t> getSpan() { return adt::Span{m_aBuff}; }
};

enum class READ_STATUS : adt::u8 { OK_, DONE, BACKSPACE };

constexpr adt::u32 CHAR_TL = L'┏';
constexpr adt::u32 CHAR_TR = L'┓';
constexpr adt::u32 CHAR_BL = L'┗';
constexpr adt::u32 CHAR_BR = L'┛';
constexpr adt::u32 CHAR_T = L'━';
constexpr adt::u32 CHAR_B = L'━';
constexpr adt::u32 CHAR_L = L'┃';
constexpr adt::u32 CHAR_R = L'┃';
constexpr adt::u32 CHAR_VOL = L'▯';
constexpr adt::u32 CHAR_VOL_MUTED = L'▮';
constexpr wchar_t CURSOR_BLOCK[] {L'█', L'\0'};

[[nodiscard]] inline constexpr adt::StringView
readModeToString(WINDOW_READ_MODE e) noexcept
{
    constexpr adt::StringView map[] {"", "searching: ", "time: "};
    return map[int(e)];
}

[[nodiscard]] adt::StringView allocTimeString(adt::Arena* pArena, int width);

/* fix song list range on new focus */
void fixFirstIdx(adt::u16 listHeight, adt::i16* pFirstIdx);

void procSeekString(const adt::Span<wchar_t> spBuff);

extern InputBuff g_input;

template<READ_STATUS (*FN_READ)(void*), void (*FN_DRAW)(void*)>
inline void
subStringSearch(
    adt::Arena* pArena,
    adt::i16* pFirstIdx,
    void* pReadArg,
    void* pDrawArg
)
{
    auto& pl = *app::g_pPlayer;

    g_input.m_eLastUsedMode = g_input.m_eCurrMode = WINDOW_READ_MODE::SEARCH;
    defer( g_input.m_eCurrMode = WINDOW_READ_MODE::NONE );

    g_input.zeroOut();
    g_input.m_idx = 0;

    auto savedFirst = *pFirstIdx;
    int nSearches = 0;
    READ_STATUS eRead {};

    do
    {
        *pFirstIdx = 0;
        pl.subStringSearch(pArena, g_input.getSpan());
        FN_DRAW(pDrawArg);

        eRead = FN_READ(pReadArg);
        if (eRead == READ_STATUS::BACKSPACE)
        {
            if (pl.m_vSearchIdxs.size() != pl.m_vSongs.size())
            {
                pl.setDefaultSearchIdxs();
                pl.copySearchToSongIdxs();
            }
        }
        ++nSearches;
    }
    while (eRead != READ_STATUS::DONE);

    pl.copySearchToSongIdxs();

    if (nSearches == 1) *pFirstIdx = savedFirst;

    /* fix focused if it ends up out of the list range */
    if (pl.m_focused >= pl.m_vSongIdxs.size())
        pl.m_focused = 0;
}

template<READ_STATUS (*FN_READ)(void*), void (*FN_DRAW)(void*)>
inline void
seekFromInput(void* pReadArg, void* pDrawArg)
{
    g_input.m_eLastUsedMode = g_input.m_eCurrMode = WINDOW_READ_MODE::SEEK;
    defer( g_input.m_eCurrMode = WINDOW_READ_MODE::NONE );

    g_input.zeroOut();
    g_input.m_idx = 0;

    do FN_DRAW(pDrawArg);
    while (FN_READ(pReadArg) != READ_STATUS::DONE);

    procSeekString(g_input.getSpan());
}

} /* namespace common */
