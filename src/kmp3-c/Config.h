#pragma once

#include "klib/common.h"

typedef struct
{
    int maxVolume;
    int volume;
    int updateRate;
    int imageUpdateRateLimit;
    uint64_t minSampleRate;
    uint64_t maxSampleRate;
    double fontAspectRatio;
    int mouseScrollStep;
    uint8_t imageHeight;
    uint8_t minImageHeight;
    uint8_t maxImageHeight;
    double doubleClickDelay;
    const char* ntsMprisName;
    int minWidth;
    int minHeight;
    ssize_t arenaReserveVirtualSpace;
} Config;
