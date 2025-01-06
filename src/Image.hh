#pragma once

#include "adt/types.hh"

using namespace adt;

enum class IMAGE_PIXEL_FORMAT : int
{
    NONE = -1,
    RGBA8_PREMULTIPLIED,
    RGBA8_UNASSOCIATED,
    RGB8,
};

struct Image
{
    u8* pBuff {};
    int width {};
    int height {};
    IMAGE_PIXEL_FORMAT eFormat {};
};
