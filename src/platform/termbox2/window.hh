#pragma once

#include "adt/Arena.hh"

using namespace adt;

namespace platform
{
namespace termbox2
{
namespace window
{

extern Arena* g_pFrameArena;
extern bool g_bDrawHelpMenu;
extern u16 g_firstIdx;

void init(Arena* pAlloc);
void destroy();
void procEvents();
void render();
void seekFromInput();
void subStringSearch();

} /* namespace window */
} /* namespace termbox2 */
} /* namespace platform */
