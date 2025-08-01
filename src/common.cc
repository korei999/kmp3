#include "common.hh" /* IWYU pragma: keep */

#include "adt/Array.hh"

#include <cwctype>

using namespace adt;

namespace common
{

InputBuff g_input {};

StringView
readModeToString(WINDOW_READ_MODE e) noexcept
{
    constexpr adt::StringView map[] {"", "searching: ", "time: "};
    return map[int(e)];
}

StringView
allocTimeString(Arena* pArena, int width)
{
    auto& mix = app::mixer();
    char* pBuff = (char*)pArena->zalloc(1, width + 1);

    const f64 sampleRateRatio = f64(mix.getSampleRate()) / f64(mix.getChangedSampleRate());

    const u64 t = std::round(mix.getCurrentMS() / 1000.0 * sampleRateRatio);
    const u64 totalT = std::round(mix.getTotalMS() / 1000.0 * sampleRateRatio);

    const u64 currMin = t / 60;
    const u64 currSec = t - (60 * currMin);

    const u64 maxMin = totalT / 60;
    const u64 maxSec = totalT - (60 * maxMin);

    const isize n = print::toBuffer(pBuff, width, "time: {}:{:>02} / {}:{:>02}", currMin, currSec, maxMin, maxSec);
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

    defer( *pFirstIdx = first );

    if (pl.m_vSearchIdxs.size() < listHeight) /* List is too small anyway case. */
    {
        first = 0;
        return;
    }
    else if (focused > first + listHeight) /* Scroll down case. */
    {
        first = focused - listHeight;
    }
    else if (focused < first) /* Scroll up case. */
    {
        first = focused;
    }

    /* Case when we are at the last page but the list is not long enough. */
    if (focused >= pl.m_vSearchIdxs.size() - listHeight - 1)
    {
        const i16 maxListSizeDiff = (first + listHeight + 1) - pl.m_vSearchIdxs.size();
        if (maxListSizeDiff > 0 && first >= maxListSizeDiff)
            first -= maxListSizeDiff;
    }
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
        i64 maxMS = app::mixer().getTotalMS();
        app::mixer().seekMS(maxMS * (f64(StringView(aMinutesBuff.data()).toI64()) / 100.0));
    }
    else
    {
        isize sec;
        if (aSecondsBuff.size() == 0) sec = StringView(aMinutesBuff.data()).toI64();
        else sec = atoll(aSecondsBuff.data()) + StringView(aMinutesBuff.data()).toI64()*60;

        app::mixer().seekMS(sec * 1000);
    }
}

} /* namespace common */
