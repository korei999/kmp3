#include "common.hh"

#include "adt/Array.hh"

#include <cwctype>

using namespace adt;

namespace common
{

InputBuff g_input {};

StringView
allocTimeString(Arena* pArena, int width)
{
    auto& mix = *app::g_pMixer;
    char* pBuff = (char*)pArena->zalloc(1, width + 1);

    f64 sampleRateRatio = f64(mix.getSampleRate()) / f64(mix.getChangedSampleRate());

    u64 t = std::round(mix.getCurrentMS() / 1000.0 * sampleRateRatio);
    u64 totalT = std::round(mix.getTotalMS() / 1000.0 * sampleRateRatio);

    u64 currMin = t / 60;
    u64 currSec = t - (60 * currMin);

    u64 maxMin = totalT / 60;
    u64 maxSec = totalT - (60 * maxMin);

    int n = snprintf(pBuff, width, "time: %llu:%02llu / %llu:%02llu", currMin, currSec, maxMin, maxSec);
    if (mix.getSampleRate() != mix.getChangedSampleRate())
    {
        print::toBuffer(pBuff + n, width - n, " ({}% speed)",
            int(std::round(f64(mix.getChangedSampleRate()) / f64(mix.getSampleRate()) * 100.0))
        );
    }

    return pBuff;
}

void
fixFirstIdx(u16 listHeight, i16* pFirstIdx)
{
    const Player& pl = app::player();

    const long focused = pl.m_focused;
    adt::i16 first = *pFirstIdx;

    if (pl.m_vSearchIdxs.size() < listHeight)
        first = 0;
    else if (focused > first + listHeight)
        first = focused - listHeight;
    else if (focused < first)
        first = focused;

    *pFirstIdx = first;
}

void
procSeekString(const Span<wchar_t> spBuff)
{
    bool bPercent = false;
    bool bColon = false;

    Array<char, 32> aMinutesBuff {};
    Array<char, 32> aSecondsBuff {};

    for (auto& wch : spBuff)
    {
        if (wch == L'%')
        {
            bPercent = true;
        }
        else if (wch == L':')
        {
            /* leave if there is one more colon or bPercent */
            if (bColon || bPercent) break;
            bColon = true;
        }
        else if (iswdigit(wch))
        {
            Array<char, 32>* pTargetArray = bColon ? &aSecondsBuff : &aMinutesBuff;
            if (spBuff.idx(&wch) < pTargetArray->cap() - 1)
                pTargetArray->push(char(wch)); /* wdigits are equivalent to char */
        }
    }

    if (aMinutesBuff.size() == 0)
        return;

    if (bPercent)
    {
        i64 maxMS = app::g_pMixer->getTotalMS();

        app::g_pMixer->seekMS(maxMS * (f64(atoll(aMinutesBuff.data())) / 100.0));
    }
    else
    {
        ssize sec;
        if (aSecondsBuff.size() == 0) sec = atoll(aMinutesBuff.data());
        else sec = atoll(aSecondsBuff.data()) + atoll(aMinutesBuff.data())*60;

        app::g_pMixer->seekMS(sec * 1000);
    }
}

} /* namespace common */
