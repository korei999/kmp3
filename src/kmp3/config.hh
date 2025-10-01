#pragma once

struct Config
{
    int maxVolume {};
    int volume {};
    int updateRate {};
    int imageUpdateRateLimit {};
    u64 minSampleRate {};
    u64 maxSampleRate {};
    f64 fontAspectRatio {};
    int mouseScrollStep {};
    u8 imageHeight {};
    u8 minImageHeight {};
    u8 maxImageHeight {};
    f64 doubleClickDelay {};
    const char* ntsMprisName {};
    int minWidth {};
    int minHeight {};
    isize frameArenaReserveVirtualSpace {};
};
