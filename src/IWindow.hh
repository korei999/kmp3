#pragma once

#include "adt/Arena.hh"

using namespace adt;

enum class READ_MODE : u8 { NONE, SEARCH, SEEK };

struct IWindow;

struct WindowVTable
{
    bool (*start)(IWindow* s, Arena* pArena);
    void (*destroy)(IWindow* s);
    void (*draw)(IWindow* s);
    void (*procEvents)(IWindow* s);
    void (*seekFromInput)(IWindow* s);
    void (*subStringSearch)(IWindow* s);
};

struct IWindow
{
    const WindowVTable* vTable {};
};

ADT_NO_UB inline bool WindowStart(IWindow* s, Arena* pArena) { return s->vTable->start(s, pArena); }
ADT_NO_UB inline void WindowDestroy(IWindow* s) { s->vTable->destroy(s); }
ADT_NO_UB inline void WindowDraw(IWindow* s) { s->vTable->draw(s); }
ADT_NO_UB inline void WindowProcEvents(IWindow* s) { s->vTable->procEvents(s); }
ADT_NO_UB inline void WindowSeekFromInput(IWindow* s) { s->vTable->seekFromInput(s); }
ADT_NO_UB inline void WindowSubStringSearch(IWindow* s) { s->vTable->subStringSearch(s); }

struct DummyWindow
{
    IWindow super {};

    DummyWindow();
};

inline bool DummyStart(DummyWindow* ,Arena*) { return true; }
inline void DummyDestroy(DummyWindow*) {}
inline void DummyDraw(DummyWindow*) {}
inline void DummyProcEvents(DummyWindow*) {}
inline void DummySeekFromInput(DummyWindow*) {}
inline void DummySubStringSearch(DummyWindow*) {}

inline const WindowVTable inl_DummyWindowVTable {
    .start = decltype(WindowVTable::start)(DummyStart),
    .destroy = decltype(WindowVTable::destroy)(DummyDestroy),
    .draw = decltype(WindowVTable::draw)(DummyDraw),
    .procEvents = decltype(WindowVTable::procEvents)(DummyProcEvents),
    .seekFromInput = decltype(WindowVTable::seekFromInput)(DummySeekFromInput),
    .subStringSearch = decltype(WindowVTable::subStringSearch)(DummySubStringSearch),
};

inline
DummyWindow::DummyWindow() : super(&inl_DummyWindowVTable) {}
