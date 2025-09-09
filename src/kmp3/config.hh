#pragma once

struct Config
{
    adt::f32 maxVolume {};
    adt::f32 volume {};
    int updateRate {};
    int imageUpdateRateLimit {};
    adt::u64 minSampleRate {};
    adt::u64 maxSampleRate {};
    adt::f64 fontAspectRatio {};
    int mouseScrollStep {};
    adt::u8 imageHeight {};
    adt::u8 minImageHeight {};
    adt::u8 maxImageHeight {};
    adt::f64 doubleClickDelay {};
    const char* ntsMprisName {};
    int minWidth {};
    int minHeight {};
    adt::isize frameArenaReserveVirtualSpace {};
};
