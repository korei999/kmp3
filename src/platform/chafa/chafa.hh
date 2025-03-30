#pragma once

#include "adt/String.hh"
#include "Image.hh"
#include "adt/Vec.hh"

#include <chafa.h>

namespace platform::chafa
{

enum class IMAGE_LAYOUT : adt::u8 { RAW, LINES };

struct Image
{
    IMAGE_LAYOUT eLayout {};
    union {
        adt::String sRaw;
        adt::Vec<adt::String> vLines;
    } uData {};

    int width {};
    int height {};
};

[[nodiscard]] Image allocImage(adt::IAllocator* pAlloc, IMAGE_LAYOUT eLayout, const ::Image img, int termHeight, int termWidth);

void detectTerminal(ChafaTermInfo** ppTermInfo, ChafaCanvasMode* pMode, ChafaPixelMode* pPixelMode);

} /* namespace platform::chafa */
