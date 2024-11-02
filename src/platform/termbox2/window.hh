#pragma once

#include "adt/Allocator.hh"

using namespace adt;

namespace platform
{
namespace termbox2
{
namespace window
{

extern bool g_bDrawHelpMenu;
extern u16 g_firstIdx;

void init();
void stop();
void procEvents(Allocator* pAlloc);
void render(Allocator* pAlloc);
void seekFromInput(Allocator* pAlloc);
void subStringSearch(Allocator* pAlloc);

} /* namespace window */
} /* namespace termbox2 */
} /* namespace platform */
