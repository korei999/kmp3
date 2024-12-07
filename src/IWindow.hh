#pragma once

#include "adt/Arena.hh"

using namespace adt;

struct IWindow;

struct WindowVTable
{
    bool (*start)(IWindow* s, Arena* pArena);
    void (*destroy)(IWindow* s);
    void (*draw)(IWindow* s);
    void (*procEvents)(IWindow* s);
};

struct IWindow
{
    const WindowVTable* vTable {};
};

ADT_NO_UB inline bool WindowStart(IWindow* s, Arena* pArena) { return s->vTable->start(s, pArena); }
ADT_NO_UB inline void WindowDestroy(IWindow* s) { s->vTable->destroy(s); }
ADT_NO_UB inline void WindowDraw(IWindow* s) { s->vTable->draw(s); }
ADT_NO_UB inline void WindowProcEvents(IWindow* s) { s->vTable->procEvents(s); }

struct DummyWindow
{
    IWindow super {};

    DummyWindow();
};

inline bool DummyStart([[maybe_unused]] DummyWindow* s, [[maybe_unused]] Arena* pArena) { return true; }
inline void DummyDestroy([[maybe_unused]] DummyWindow* s) {}
inline void DummyDraw([[maybe_unused]] DummyWindow* s) {}
inline void DummyProcEvents([[maybe_unused]] DummyWindow* s) {}

inline const WindowVTable inl_DummyWindowVTable {
    .start = decltype(WindowVTable::start)(DummyStart),
    .destroy = decltype(WindowVTable::destroy)(DummyDestroy),
    .draw = decltype(WindowVTable::draw)(DummyDraw),
    .procEvents = decltype(WindowVTable::procEvents)(DummyProcEvents),
};

inline
DummyWindow::DummyWindow() : super(&inl_DummyWindowVTable) {}
