#pragma once

#include "IWindow.hh"

using namespace adt;

namespace platform
{
namespace termbox2
{
namespace window
{

extern Arena* g_pFrameArena;
extern u16 g_firstIdx;

bool init(Arena* pAlloc);
void destroy();
void procEvents();
void draw();
void seekFromInput();
void subStringSearch();

} /* namespace window */

struct Win
{
    IWindow super {};

    Win();
};

inline bool WinStart([[maybe_unused]] Win* s, Arena* pArena) { return window::init(pArena); }
inline void WinDestroy([[maybe_unused]] Win* s) { window::destroy(); }
inline void WinDraw([[maybe_unused]] Win* s) { window::draw(); }
inline void WinProcEvents([[maybe_unused]] Win* s) { window::procEvents(); }
inline void WinSeekFromInput([[maybe_unused]] Win* s) { window::seekFromInput(); }
inline void WinSubStringSearch([[maybe_unused]] Win* s) { window::subStringSearch(); }

inline const WindowVTable inl_WinVTable {
    .start = decltype(WindowVTable::start)(WinStart),
    .destroy = decltype(WindowVTable::destroy)(WinDestroy),
    .draw = decltype(WindowVTable::draw)(WinDraw),
    .procEvents = decltype(WindowVTable::procEvents)(WinProcEvents),
    .seekFromInput = decltype(WindowVTable::seekFromInput)(WinSeekFromInput),
    .subStringSearch = decltype(WindowVTable::subStringSearch)(WinSubStringSearch),
};

inline
Win::Win() : super(&inl_WinVTable) {}

} /* namespace termbox2 */
} /* namespace platform */
