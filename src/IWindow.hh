#pragma once

#include "adt/Arena.hh"

enum class WINDOW_READ_MODE : adt::u8 { NONE, SEARCH, SEEK };

struct IWindow
{
    adt::i16 m_listHeight {};
    bool m_bRedraw {};
    bool m_bClear {};

    virtual bool start(adt::Arena* pArena) = 0;
    virtual void destroy() = 0;
    virtual void draw() = 0;
    virtual void procEvents() = 0;
    virtual void seekFromInput() = 0;
    virtual void subStringSearch() = 0;
    virtual void adjustListHeight() = 0;
    virtual void forceResize() = 0;
};

struct DummyWindow : IWindow
{
    virtual bool start(adt::Arena*) final { return true; };
    virtual void destroy() final { };
    virtual void draw() final {};
    virtual void procEvents() final {};
    virtual void seekFromInput() final {};
    virtual void subStringSearch() final {};
    virtual void adjustListHeight() final {};
    virtual void forceResize() final {};
};
