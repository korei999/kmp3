#pragma once

#include "IWindow.hh"

namespace platform::termbox2
{
namespace window
{

extern adt::Arena* g_pFrameArena;
extern adt::i16 g_firstIdx;
extern int g_prevImgWidth;

/* old api */
bool start(adt::Arena* pAlloc);
void destroy();
void procEvents();
void draw();
void seekFromInput();
void subStringSearch();
long getImgOffset();
void adjustListHeight();
void centerSelection();

} /* namespace window */

struct Win : IWindow
{
    virtual bool start(adt::Arena* pArena) final { return window::start(pArena); }
    virtual void destroy() final { window::destroy(); }
    virtual void draw() final { window::draw(); }
    virtual void procEvents() final { window::procEvents(); }
    virtual void seekFromInput() final { window::seekFromInput(); }
    virtual void subStringSearch() final { window::subStringSearch(); }
    virtual void centerAroundSelection() final { window::centerSelection(); }
    virtual void adjustListHeight() final { window::adjustListHeight(); }
};

} /* namespace platform::termbox2 */
