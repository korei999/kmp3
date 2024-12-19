#pragma once

#include "adt/Arena.hh"
#include "ffmpeg.hh"

#ifdef USE_NCURSES
#include <ncurses.h>
#endif

namespace platform
{
namespace chafa
{

struct Image
{
    String s {};
    int width {};
    int height {};
};

#ifdef USE_NCURSES
void showImageNCurses(WINDOW* pWin, const ffmpeg::Image img, const int termHeight, const int termWidth);
#endif

void
showImage(
    Arena* pArena,
    const ffmpeg::Image img,
    const int termHeight,
    const int termWidth,
    const int hOff,
    const int vOff
);

[[nodiscard]] Image getImageString(Arena* pArena, const ffmpeg::Image img, int termHeight, int termWidth);

} /* namespace chafa */
} /* namespace platform */
