#pragma once

#include "ffmpeg.hh"

#include <ncurses.h>

namespace platform
{
namespace chafa
{

void showImage(WINDOW* pWin, const ffmpeg::Image img, const int termHeight, const int termWidth);

} /* namespace chafa */
} /* namespace platform */
