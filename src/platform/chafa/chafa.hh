#pragma once

#include "adt/Arena.hh"
#include "adt/String.hh"
#include "Image.hh"

#ifdef USE_NCURSES
#include <ncurses.h>
#endif

namespace platform::chafa
{

struct Image
{
    String s {};
    int width {};
    int height {};
};

#ifdef USE_NCURSES
void showImageNCurses(WINDOW* pWin, const ::Image img, const int termHeight, const int termWidth);
#endif

void
showImage(
    Arena* pArena,
    const Image img,
    const int termHeight,
    const int termWidth,
    const int hOff,
    const int vOff
);

[[nodiscard]] Image getImageString(Arena* pArena, const ::Image img, int termHeight, int termWidth);

} /* namespace platform::chafa */
