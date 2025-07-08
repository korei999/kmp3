#pragma once

#include "IWindow.hh"

namespace common
{

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

struct InputBuff
{
    wchar_t m_aBuff[64] {};
    adt::u32 m_idx = 0;
    WINDOW_READ_MODE m_eCurrMode {};
    WINDOW_READ_MODE m_eLastUsedMode {};

    /* */

    void zeroOut() noexcept { memset(m_aBuff, 0, sizeof(m_aBuff)); }
    adt::Span<wchar_t> span() noexcept { return adt::Span {m_aBuff}; }
};

extern InputBuff g_input;

enum class READ_STATUS : adt::u8 { OK_, DONE, BACKSPACE };

[[nodiscard]] adt::StringView
readModeToString(WINDOW_READ_MODE e) noexcept;

[[nodiscard]] adt::StringView allocTimeString(adt::Arena* pArena, int width);

/* fix song list range on new focus */
void fixFirstIdx(adt::u16 listHeight, adt::i16* pFirstIdx);

void procSeekString(const adt::Span<wchar_t> spBuff);

template<typename READ_LAMBDA, typename DRAW_LAMBDA>
requires std::same_as<std::invoke_result_t<READ_LAMBDA>, READ_STATUS> && std::same_as<std::invoke_result_t<DRAW_LAMBDA>, void>
inline void subStringSearch(
    adt::Arena* pArena,
    adt::i16* pFirstIdx,
    READ_LAMBDA clRead,
    DRAW_LAMBDA clDraw
);

template<typename READ_LAMBDA, typename DRAW_LAMBDA>
requires std::same_as<std::invoke_result_t<READ_LAMBDA>, READ_STATUS> && std::same_as<std::invoke_result_t<DRAW_LAMBDA>, void>
inline void seekFromInput(READ_LAMBDA clRead, DRAW_LAMBDA clDraw);

} /* namespace common */
