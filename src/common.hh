#pragma once

#include "app.hh"

#include "adt/defer.hh"


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

template<typename READ_LAMBDA, typename DRAW_LAMBDA>
    requires std::same_as<std::invoke_result_t<READ_LAMBDA>, READ_STATUS> &&
             std::same_as<std::invoke_result_t<DRAW_LAMBDA>, void>
inline void
subStringSearch(
    adt::Arena* pArena,
    adt::i16* pFirstIdx,
    READ_LAMBDA clRead,
    DRAW_LAMBDA clDraw
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
        clDraw();

        eRead = clRead();
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

template<typename READ_LAMBDA, typename DRAW_LAMBDA>
    requires std::same_as<std::invoke_result_t<READ_LAMBDA>, READ_STATUS> &&
             std::same_as<std::invoke_result_t<DRAW_LAMBDA>, void>
inline void
seekFromInput(READ_LAMBDA clRead, DRAW_LAMBDA clDraw)
{
    g_input.m_eLastUsedMode = g_input.m_eCurrMode = WINDOW_READ_MODE::SEEK;
    defer( g_input.m_eCurrMode = WINDOW_READ_MODE::NONE );

    g_input.zeroOut();
    g_input.m_idx = 0;

    do clDraw();
    while (clRead() != READ_STATUS::DONE);

    procSeekString(g_input.getSpan());
}

} /* namespace common */
