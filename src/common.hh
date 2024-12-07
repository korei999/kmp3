#include "app.hh"

#include <cmath>

namespace common
{

constexpr u32 CHAR_TL = L'┏';
constexpr u32 CHAR_TR = L'┓';
constexpr u32 CHAR_BL = L'┗';
constexpr u32 CHAR_BR = L'┛';
constexpr u32 CHAR_T = L'━';
constexpr u32 CHAR_B = L'━';
constexpr u32 CHAR_L = L'┃';
constexpr u32 CHAR_R = L'┃';
constexpr u32 CHAR_VOL = L'▯';
constexpr u32 CHAR_VOL_MUTED = L'▮';

inline String
allocTimeString(Arena* pArena, int width)
{
    auto& mixer = *app::g_pMixer;
    char* pBuff = (char*)zalloc(pArena, 1, width + 1);

    u64 t = std::round(f64(mixer.currentTimeStamp) / f64(mixer.nChannels) / f64(mixer.changedSampleRate));
    u64 totalT = std::round(f64(mixer.totalSamplesCount) / f64(mixer.nChannels) / f64(mixer.changedSampleRate));

    u64 currMin = t / 60;
    u64 currSec = t - (60 * currMin);

    u64 maxMin = totalT / 60;
    u64 maxSec = totalT - (60 * maxMin);

    int n = snprintf(pBuff, width, "time: %llu:%02llu / %llu:%02llu", currMin, currSec, maxMin, maxSec);
    if (mixer.sampleRate != mixer.changedSampleRate)
        snprintf(pBuff + n, width - n, " (%d%% speed)", int(std::round(f64(mixer.changedSampleRate) / f64(mixer.sampleRate) * 100.0)));

    return pBuff;
}

} /* namespace common */
