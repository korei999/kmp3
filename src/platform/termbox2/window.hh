#pragma once

#include "adt/Allocator.hh"

using namespace adt;

namespace platform
{
namespace termbox2
{
namespace window
{

extern Allocator* g_pFrameAlloc;
extern bool g_bDrawHelpMenu;
extern u16 g_firstIdx;

void init(Allocator* pAlloc);
void destroy();
void procEvents();
void render();
void seekFromInput();
void subStringSearch();

} /* namespace window */
} /* namespace termbox2 */
} /* namespace platform */
