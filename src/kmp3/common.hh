#pragma once

#include "common-inl.hh"

#include "app.hh"

namespace common
{

template<typename READ_LAMBDA, typename DRAW_LAMBDA>
requires std::same_as<std::invoke_result_t<READ_LAMBDA>, READ_STATUS> && std::same_as<std::invoke_result_t<DRAW_LAMBDA>, void>
inline void
subStringSearch(
    adt::Arena* pArena,
    adt::i16* pFirstIdx,
    READ_LAMBDA clRead,
    DRAW_LAMBDA clDraw
)
{
    using namespace adt;
    ArenaPushScope arenaScope {pArena};

    auto& pl = *app::g_pPlayer;

    g_input.m_eLastUsedMode = g_input.m_eCurrMode = WINDOW_READ_MODE::SEARCH;
    g_input.zeroOut();

    READ_STATUS eRead {};

    if (pl.m_vSearchIdxs.empty())
    {
        pl.setDefaultSearchIdxs();
        pl.copySearchToSongIdxs();
    }

    clDraw();
    do
    {
        isize prevSearchSize = pl.m_vSearchIdxs.size();

        eRead = clRead();
        if (eRead == READ_STATUS::BACKSPACE)
        {
            if (pl.m_vSearchIdxs.size() != pl.m_vSongs.size())
            {
                pl.setDefaultSearchIdxs();
                pl.copySearchToSongIdxs();
            }
        }

        if (eRead != READ_STATUS::TIMEOUT && eRead != READ_STATUS::DONE)
        {
            pl.subStringSearch(pArena, g_input.span());
            if (prevSearchSize != pl.m_vSearchIdxs.size())
            {
                *pFirstIdx = 0;
                pl.m_focusedI = 0;
            }
        }

        clDraw();
    }
    while (eRead != READ_STATUS::DONE);

    g_input.m_eCurrMode = WINDOW_READ_MODE::NONE;

    pl.copySearchToSongIdxs();

    /* fix focused if it ends up out of the list range */
    if (pl.m_focusedI >= pl.m_vSongIdxs.size())
        pl.m_focusedI = 0;
}

template<typename READ_LAMBDA, typename DRAW_LAMBDA>
requires std::same_as<std::invoke_result_t<READ_LAMBDA>, READ_STATUS> && std::same_as<std::invoke_result_t<DRAW_LAMBDA>, void>
inline void
seekFromInput(READ_LAMBDA clRead, DRAW_LAMBDA clDraw)
{
    g_input.m_eLastUsedMode = g_input.m_eCurrMode = WINDOW_READ_MODE::SEEK;
    defer( g_input.m_eCurrMode = WINDOW_READ_MODE::NONE );

    g_input.zeroOut();
    g_input.m_idx = 0;

    do clDraw();
    while (clRead() != READ_STATUS::DONE);

    procSeekString(g_input.span());
}

} /* namespace common */
