#pragma once

#include "adt/Arena.hh"

using namespace adt;

enum class WINDOW_READ_MODE : u8 { NONE, SEARCH, SEEK };

struct IWindow
{
    virtual bool start(Arena* pArena) = 0;
    virtual void destroy() = 0;
    virtual void draw() = 0;
    virtual void procEvents() = 0;
    virtual void seekFromInput() = 0;
    virtual void subStringSearch() = 0;
};

struct DummyWindow : IWindow
{
    virtual bool start(Arena*) override { return true; };
    virtual void destroy() override {};
    virtual void draw() override {};
    virtual void procEvents() override {};
    virtual void seekFromInput() override {};
    virtual void subStringSearch() override {};
};
