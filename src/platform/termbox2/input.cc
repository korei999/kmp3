#include "input.hh"

#include "app.hh"
#include "window.hh"
#include "keybinds.hh"
#include "common.hh"

#include "termbox2/termbox2.h"

#include <cmath>

namespace platform
{
namespace termbox2
{
namespace input
{

static u16 s_aInputMap[0xffff + 1] {};

void
procKey(tb_event* pEv)
{
    const auto key = s_aInputMap[pEv->key];
    const u32 ch = pEv->ch <= utils::size(s_aInputMap) ? s_aInputMap[pEv->ch] : 0;

    for (const auto& k : keybinds::inl_aKeys)
    {
        if ((k.key > 0 && k.key == key) || (k.ch > 0 && k.ch == ch))
            keybinds::resolveKey(k.pfn, k.arg);
    }

    if (key == TB_KEY_F1)
        utils::toggle(&window::g_bDrawHelpMenu);
}

void
procMouse(tb_event* pEv)
{
    auto& pl = *app::g_pPlayer;
    auto& mix = *app::g_pMixer;
    const long listOff = pl.statusAndInfoHeight + 4;
    const long sliderOff = pl.statusAndInfoHeight + 2;
    const auto& ev = *pEv;
    const long width = tb_width();
    const long height = tb_height();

    /* click on slider */
    if (ev.y == sliderOff)
    {
        constexpr long xOff = 2; /* offset from the icon */
        if (ev.key == TB_KEY_MOUSE_LEFT)
        {
            if (ev.x <= xOff)
            {
                if (ev.key == TB_KEY_MOUSE_LEFT) audio::MixerTogglePause(&mix);
                return;
            }

            f64 target = f64(ev.x - xOff) / f64(width - xOff - 1);
            target *= audio::MixerGetMaxMS(app::g_pMixer);
            audio::MixerSeekMS(app::g_pMixer, target);
            return;
        }
        else if (ev.key == TB_KEY_MOUSE_WHEEL_DOWN) audio::MixerSeekLeftMS(&mix, 5000);
        else if (ev.key == TB_KEY_MOUSE_WHEEL_UP) audio::MixerSeekRightMS(&mix, 5000);

        return;
    }

    /* scroll ontop of volume */
    if (ev.y <= pl.statusAndInfoHeight && ev.x < std::round(f64(width) * pl.statusToInfoWidthRatio))
    {
        if (ev.key == TB_KEY_MOUSE_WHEEL_UP) audio::MixerVolumeUp(app::g_pMixer, 0.1f);
        else if (ev.key == TB_KEY_MOUSE_WHEEL_DOWN) audio::MixerVolumeDown(app::g_pMixer, 0.1f);

        return;
    }

    /* click on song list */
    if (ev.y < listOff || ev.y >= height - 2) return;

    long target = window::g_firstIdx + ev.y - listOff;
    target = utils::clamp(
        target,
        long(window::g_firstIdx),
        long(window::g_firstIdx + tb_height() - listOff - 3)
    );

    if (ev.key == TB_KEY_MOUSE_LEFT)
        PlayerFocus(&pl, target);
    if (ev.key == TB_KEY_MOUSE_RIGHT)
    {
        PlayerFocus(&pl, target);
        PlayerSelectFocused(&pl);
    }
    else if (ev.key == TB_KEY_MOUSE_WHEEL_UP)
        PlayerFocus(&pl, pl.focused - 22);
    else if (ev.key == TB_KEY_MOUSE_WHEEL_DOWN)
        PlayerFocus(&pl, pl.focused + 22);
}

void
fillInputMap()
{
    /* ascii's are all the same */
    for (int i = ' '; i < '~'; ++i) s_aInputMap[i] = i;

    s_aInputMap[TB_KEY_ENTER] = keys::ENTER;
    s_aInputMap[TB_KEY_CTRL_C] = keys::CTRL_C;

    s_aInputMap[TB_KEY_PGDN] = keys::PGDN;
    s_aInputMap[TB_KEY_PGUP] = keys::PGUP;

    s_aInputMap[TB_KEY_CTRL_D] = keys::CTRL_D;
    s_aInputMap[TB_KEY_CTRL_U] = keys::CTRL_U;

    s_aInputMap[TB_KEY_HOME] = keys::HOME;
    s_aInputMap[TB_KEY_END] = keys::END;

    s_aInputMap[TB_KEY_ARROW_UP] = keys::ARROWUP;
    s_aInputMap[TB_KEY_ARROW_DOWN] = keys::ARROWDOWN;
    s_aInputMap[TB_KEY_ARROW_LEFT] = keys::ARROWLEFT;
    s_aInputMap[TB_KEY_ARROW_RIGHT] = keys::ARROWRIGHT;
}

} /* namespace input */
} /* namespace termbox2 */
} /* namespace platform */
