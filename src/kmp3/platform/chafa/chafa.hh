#pragma once

#include "Image.hh"

#include <chafa.h>

namespace platform::chafa
{

enum class IMAGE_LAYOUT : u8 { RAW, LINES };

struct Image
{
    IMAGE_LAYOUT eLayout {};
    union {
        String sRaw;
        Vec<String> vLines;
    } uData {};

    int width {};
    int height {};
};

[[nodiscard]] Image allocImage(IAllocator* pAlloc, IMAGE_LAYOUT eLayout, const ::Image img, int termHeight, int termWidth);

void detectTerminal(ChafaTermInfo** ppTermInfo, ChafaCanvasMode* pMode, ChafaPixelMode* pPixelMode);

} /* namespace platform::chafa */
