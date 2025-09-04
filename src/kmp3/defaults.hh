#pragma once

#include "config.hh"

namespace defaults
{

constexpr Config CONFIG {
    .maxVolume = 1.5f, /* (0.0f, 1.0f], > 1.0f might cause distortion. */
    .volume = 0.4f, /* Startup volume. */
    .updateRate = 500, /* Ui update rate (ms). */
    .imageUpdateRateLimit = 100, /* (ms). Is also limited by UPDATE_RATE. */
    .readTimeout = 5000, /* String input timeout (ms). */
    .minSampleRate = 1000,
    .maxSampleRate = 9999999,
    .fontAspectRatio = 1.0 / 2.0, /* Typical monospaced font is 1/2 or 3/5 (width/height). */
    .mouseStep = 4,
    .imageHeight = 11, /* Terminal rows height. */
    .minImageHeight = 10,
    .maxImageHeight = 30,
    .doubleClickDelay = 350.0,
    .ntsMprisName = "a_kmp3", /* Using 'a' to top kmp3 instance in playerctl. */
    .minWidth = 35,
    .minHeight = 17,
};

} /* namespace defaults */
