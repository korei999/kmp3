#pragma once

#include "defaults.hh"

struct Config
{
    adt::f32 maxVolume = defaults::MAX_VOLUME;
    adt::f32 volume = defaults::VOLUME;
    int updateRate = defaults::UPDATE_RATE;
    int imageUpdateRateLimit = defaults::IMAGE_UPDATE_RATE_LIMIT;
    int readTimeout = defaults::READ_TIMEOUT;
    adt::u64 minSampleRate = defaults::MIN_SAMPLE_RATE;
    adt::u64 maxSampleRate = defaults::MAX_SAMPLE_RATE;
    adt::f64 fontAspectRatio = defaults::FONT_ASPECT_RATIO;
    int mouseStep = defaults::MOUSE_STEP;
    adt::u8 imageHeight = defaults::IMAGE_HEIGHT;
    adt::u8 minImageHeight = defaults::MIN_IMAGE_HEIGHT;
    adt::u8 maxImageHeight = defaults::MAX_IMAGE_HEIGHT;
    adt::f64 doubleClickDelay = defaults::DOUBLE_CLICK_DELAY;
    const char* ntsMprisName = defaults::MPRIS_NAME;
};
