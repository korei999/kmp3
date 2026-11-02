#pragma once

#include "Image.h"

#include "klib/String.h"

#include "generated.h"

#include <chafa.h>

typedef uint8_t platform_chafa_IMAGE_LAYOUT;
#define platform_chafa_IMAGE_LAYOUT_RAW 0
#define platform_chafa_IMAGE_LAYOUT_LINES 1

typedef struct
{
    platform_chafa_IMAGE_LAYOUT eLayout;
    union
    {
        k_String sRaw;
        VecString vLines;
    } uData;

    int width;
    int height;
} platform_chafa_Image;

K_NO_DISCARD platform_chafa_Image platform_chafa_allocImage(k_IAllocator* pAlloc, platform_chafa_IMAGE_LAYOUT eLayout, const Image img, int termHeight, int termWidth);
void platform_chafa_detectTerminal(ChafaTermInfo** ppTermInfo, ChafaCanvasMode* pMode, ChafaPixelMode* pPixelMode);
