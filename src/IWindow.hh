#pragma once

#include "adt/Arena.hh"

using namespace adt;

enum class WINDOW_READ_MODE : u8 { NONE, SEARCH, SEEK };

struct IWindow
{
    i16 m_listHeight {};
    bool m_bRedraw {};
    bool m_bClear {};

    virtual bool start(Arena* pArena) = 0;
    virtual void destroy() = 0;
    virtual void draw() = 0;
    virtual void procEvents() = 0;
    virtual void seekFromInput() = 0;
    virtual void subStringSearch() = 0;
    virtual void centerAroundSelection() = 0;
    virtual void adjustListHeight() = 0;
};

struct DummyWindow : IWindow
{
    virtual bool start(Arena*) final { return true; };
    virtual void destroy() final { };
    virtual void draw() final {};
    virtual void procEvents() final {};
    virtual void seekFromInput() final {};
    virtual void subStringSearch() final {};
    virtual void centerAroundSelection() final {};
    virtual void adjustListHeight() final {};
};
