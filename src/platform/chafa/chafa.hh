#pragma once

#include "adt/String.hh"
#include "Image.hh"
#include "adt/Vec.hh"

#include <chafa.h>

#ifdef USE_NCURSES
#include <ncurses.h>
#endif

namespace platform::chafa
{

enum class IMAGE_LAYOUT : u8 { RAW, LINES };

struct Image
{
    IMAGE_LAYOUT eLayout {};
    union {
        String sRaw;
        VecBase<String> vLines;
    } uData {};

    int width {};
    int height {};
};

#ifdef USE_NCURSES
void showImageNCurses(WINDOW* pWin, const ::Image img, const int termHeight, const int termWidth);
#endif

[[nodiscard]] Image allocImage(IAllocator* pAlloc, IMAGE_LAYOUT eLayout, const ::Image img, int termHeight, int termWidth);

void detectTerminal(ChafaTermInfo** ppTermInfo, ChafaCanvasMode* pMode, ChafaPixelMode* pPixelMode);

} /* namespace platform::chafa */
