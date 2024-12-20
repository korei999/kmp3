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

    /* */

    ADT_NO_UB bool start(Arena* pArena) { return pVTable->start(this, pArena); }
    ADT_NO_UB void destroy() { pVTable->destroy(this); }
    ADT_NO_UB void draw() { pVTable->draw(this); }
    ADT_NO_UB void procEvents() { pVTable->procEvents(this); }
    ADT_NO_UB void seekFromInput() { pVTable->seekFromInput(this); }
    ADT_NO_UB void subStringSearch() { pVTable->subStringSearch(this); }
};

template<typename WINDOW_T>
constexpr WindowVTable
WindowVTableGenerate()
{
    return WindowVTable {
        .start = decltype(WindowVTable::start)(methodPointer(&WINDOW_T::start)),
        .destroy = decltype(WindowVTable::destroy)(methodPointer(&WINDOW_T::destroy)),
        .draw = decltype(WindowVTable::draw)(methodPointer(&WINDOW_T::draw)),
        .procEvents = decltype(WindowVTable::procEvents)(methodPointer(&WINDOW_T::procEvents)),
        .seekFromInput = decltype(WindowVTable::seekFromInput)(methodPointer(&WINDOW_T::seekFromInput)),
        .subStringSearch = decltype(WindowVTable::subStringSearch)(methodPointer(&WINDOW_T::subStringSearch)),
    };
}

struct DummyWindow
{
    IWindow super {};

    /* */

    DummyWindow();

    /* */

    bool start(Arena*) { return true; };
    void destroy() {};
    void draw() {};
    void procEvents() {};
    void seekFromInput() {};
    void subStringSearch() {};
};

inline const WindowVTable inl_DummyWindowVTable = WindowVTableGenerate<DummyWindow>();

inline
DummyWindow::DummyWindow() : super(&inl_DummyWindowVTable) {}
