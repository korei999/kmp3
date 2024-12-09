#pragma once

#include "adt/types.hh"
#include "ffmpeg.hh"

#include <sixel.h>

using namespace adt;

namespace platform
{
namespace sixel
{

struct Img;

void ImgInit(Img* s);
void ImgDestroy(Img* s);
void ImgPrintBytes(Img* s, ffmpeg::Image img);

struct Img
{
    sixel_encoder_t *pEncoder {};

    Img() = default;
    Img(INIT_FLAG eInit) { if (eInit == INIT_FLAG::INIT) ImgInit(this); }
};


} /* namespace sixel */
} /* namespace platform */
