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
extern int g_timeStringSize;

/* old api */
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

    bool start(Arena* pArena) { return window::init(pArena); }
    void destroy() { window::destroy(); }
    void draw() { window::draw(); }
    void procEvents() { window::procEvents(); }
    void seekFromInput() { window::seekFromInput(); }
    void subStringSearch() { window::subStringSearch(); }
};

inline const WindowVTable inl_WinVTable = WindowVTableGenerate<Win>();

inline
Win::Win() : super(&inl_WinVTable) {}

} /* namespace termbox2 */
} /* namespace platform */
