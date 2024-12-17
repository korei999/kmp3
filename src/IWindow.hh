#pragma once

#include "adt/Arena.hh"

using namespace adt;

enum class WINDOW_READ_MODE : u8 { NONE, SEARCH, SEEK };

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
    const WindowVTable* pVTable {};

    ADT_NO_UB bool start(Arena* pArena) { return pVTable->start(this, pArena); }
    ADT_NO_UB void destroy() { pVTable->destroy(this); }
    ADT_NO_UB void draw() { pVTable->draw(this); }
    ADT_NO_UB void procEvents() { pVTable->procEvents(this); }
    ADT_NO_UB void seekFromInput() { pVTable->seekFromInput(this); }
    ADT_NO_UB void subStringSearch() { pVTable->subStringSearch(this); }
};

struct DummyWindow
{
    IWindow super {};

    DummyWindow();

    bool start(Arena* pArena) { return true; };
    void destroy() {};
    void draw() {};
    void procEvents() {};
    void seekFromInput() {};
    void subStringSearch() {};
};

inline const WindowVTable inl_DummyWindowVTable {
    .start = decltype(WindowVTable::start)(+[](DummyWindow*, Arena*) {}),
    .destroy = decltype(WindowVTable::destroy)(+[](DummyWindow*) {}),
    .draw = decltype(WindowVTable::draw)(+[](DummyWindow*) {}),
    .procEvents = decltype(WindowVTable::procEvents)(+[](DummyWindow*) {}),
    .seekFromInput = decltype(WindowVTable::seekFromInput)(+[](DummyWindow*) {}),
    .subStringSearch = decltype(WindowVTable::subStringSearch)(+[](DummyWindow*) {}),
};

inline
DummyWindow::DummyWindow() : super(&inl_DummyWindowVTable) {}
