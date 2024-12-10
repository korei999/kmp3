#include "input.hh"

#include "Win.hh"
#include "adt/logs.hh"
#include "keybinds.hh"

#include <cmath>
#include <ncurses.h>

namespace platform
{
namespace ncurses
{
namespace input
{

void
WinProcKey([[maybe_unused]] Win* s, wint_t ch)
{
    for (const auto& k : keybinds::inl_aKeys)
    {
        if ((k.key > 0 && k.key == ch) || (k.ch > 0 && k.ch == (u32)ch))
            keybinds::resolveKey(k.pfn, k.arg);
    }
}

void
WinProcMouse(Win* s, MEVENT ev)
{
    auto& pl = *app::g_pPlayer;
    auto& mix = *app::g_pMixer;
    int height, width;
    getmaxyx(stdscr, height, width);
    const int split = std::round(f64(height) * (1.0 - pl.statusToInfoWidthRatio));
    const long listOff = split + 3;
    const long sliderOff = split;

    LOG("x: {}, y: {}, key: {}, split: {}\n", ev.x, ev.y, ev.bstate, split);

    /* click on slider */
    if (ev.y == sliderOff)
    {
        const long xOff = g_timeStringSize + 2;
        if (ev.bstate & BUTTON1_CLICKED)
        {
            if (ev.x <= xOff)
            {
                if (ev.bstate & BUTTON1_CLICKED) audio::MixerTogglePause(&mix);
                return;
            }

            f64 target = f64(ev.x - xOff) / f64(width - xOff - 2);
            target *= audio::MixerGetMaxMS(app::g_pMixer);
            audio::MixerSeekMS(app::g_pMixer, target);
            return;
        }

        else if (ev.bstate == keys::MOUSEDOWN) audio::MixerSeekLeftMS(&mix, 5000);
        else if (ev.bstate == keys::MOUSEUP) audio::MixerSeekRightMS(&mix, 5000);

        return;
    }

    /* scroll ontop of volume */
    if (ev.y == split + 1)
    {
        if (ev.bstate == keys::MOUSEUP) audio::MixerVolumeUp(app::g_pMixer, 0.1f);
        else if (ev.bstate == keys::MOUSEDOWN) audio::MixerVolumeDown(app::g_pMixer, 0.1f);

        return;
    }

    /* click on song list */
    if (ev.y < listOff || ev.y >= height - 2) return;

    long target = s->firstIdx + ev.y - listOff;
    target = utils::clamp(
        target,
        long(s->firstIdx),
        long(s->firstIdx + height - listOff - 3)
    );

    if (ev.bstate & BUTTON1_CLICKED)
        PlayerFocus(&pl, target);
    if (ev.bstate & BUTTON3_CLICKED)
    {
        PlayerFocus(&pl, target);
        PlayerSelectFocused(&pl);
    }
    else if (ev.bstate == keys::MOUSEUP)
        PlayerFocus(&pl, pl.focused - 22);
    else if (ev.bstate == keys::MOUSEDOWN)
        PlayerFocus(&pl, pl.focused + 22);
}

} /*namespace input */
} /*namespace ncurses */
} /*namespace platform */
