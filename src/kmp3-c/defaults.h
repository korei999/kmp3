#pragma once

#include "Config.h"
#include "klib/IAllocator.h"

static const Config defaults_CONFIG = {
    .maxVolume = 150, /* (0, 100], > 100 might cause distortions. */
    .volume = 40, /* Startup volume. */
    .updateRate = 500, /* Ui update rate (ms). */
    .imageUpdateRateLimit = 100, /* (ms). */
    .minSampleRate = 1000,
    .maxSampleRate = 9999999,
    .fontAspectRatio = 1.0 / 2.0, /* Typical monospaced font is 1/2 or 3/5 (width/height). */
    .mouseScrollStep = 4,
    .imageHeight = 11, /* Terminal rows height. */
    .minImageHeight = 10,
    .maxImageHeight = 30,
    .doubleClickDelay = 350.0,
    .ntsMprisName = "a_kmp3", /* Using 'a' to top kmp3 instance in playerctl. */
    .minWidth = 35,
    .minHeight = 17,
    .arenaReserveVirtualSpace = K_SIZE_1M * 64,
};
