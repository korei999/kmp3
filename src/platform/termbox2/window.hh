#pragma once

#include "IWindow.hh"

using namespace adt;

namespace platform::termbox2
{
namespace window
{

extern Arena* g_pFrameArena;
extern u16 g_firstIdx;
extern int g_prevImgWidth;

/* old api */
bool init(Arena* pAlloc);
void destroy();
void procEvents();
void draw();
void seekFromInput();
void subStringSearch();
long getImgOffset();

} /* namespace window */

struct Win : IWindow
{
    virtual bool start(Arena* pArena) final { return window::init(pArena); }
    virtual void destroy() final { window::destroy(); }
    virtual void draw() final { window::draw(); }
    virtual void procEvents() final { window::procEvents(); }
    virtual void seekFromInput() final { window::seekFromInput(); }
    virtual void subStringSearch() final { window::subStringSearch(); }
};

} /* namespace platform::termbox2 */
