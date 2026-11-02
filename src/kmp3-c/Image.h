#pragma once

#include <stdint.h>

typedef enum
{
    IMAGE_PIXEL_FORMAT_NONE = -1,
    IMAGE_PIXEL_FORMAT_RGBA8_PREMULTIPLIED,
    IMAGE_PIXEL_FORMAT_RGBA8_UNASSOCIATED,
    IMAGE_PIXEL_FORMAT_RGB8,
} IMAGE_PIXEL_FORMAT;

typedef struct Image Image;
struct Image
{
    uint8_t* pBuff;
    int width;
    int height;
    IMAGE_PIXEL_FORMAT eFormat;
};
