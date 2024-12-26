#pragma once

#include "adt/Arena.hh"

using namespace adt;

enum class WINDOW_READ_MODE : u8 { NONE, SEARCH, SEEK };

struct IWindow
{
    bool m_bRedraw {};

    virtual bool start(Arena* pArena) = 0;
    virtual void destroy() = 0;
    virtual void draw() = 0;
    virtual void procEvents() = 0;
    virtual void seekFromInput() = 0;
    virtual void subStringSearch() = 0;
};

struct DummyWindow : IWindow
{
    virtual bool start(Arena*) override final { return true; };
    virtual void destroy() override final { };
    virtual void draw() override final {};
    virtual void procEvents() override final {};
    virtual void seekFromInput() override final {};
    virtual void subStringSearch() override final {};
};
