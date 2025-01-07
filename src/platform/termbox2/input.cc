#include "input.hh"

#include "defaults.hh"
#include "termbox2/termbox2.h"

#include "app.hh"
#include "window.hh"
#include "keybinds.hh"

#include <cmath>

namespace platform::termbox2::input
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
            keybinds::exec(k.pfn, k.arg);
    }
}

void
procMouse(tb_event* pEv)
{
    const auto& ev = *pEv;

    auto& pl = *app::g_pPlayer;
    const long width = tb_width();
    const long height = tb_height();

    const long xOff = window::getImgOffset();
    const int yOff = pl.m_imgHeight + 1;

    const long listOff = yOff + 1;
    const long sliderOff = 10;
    const long volumeOff = 6;

    /* click on slider */
    if (ev.y == sliderOff && ev.x >= xOff)
    {
        if (ev.key == TB_KEY_MOUSE_LEFT)
        {
            /* click on indicator */
            if (ev.x >= xOff && ev.x <= xOff + 2)
            {
                app::togglePause();
                return;
            }

            f64 target = f64(ev.x - xOff - 4) / f64((width - 1) - xOff - 4);
            target *= app::g_pMixer->getTotalMS();
            app::g_pMixer->seekMS(target);

            return;
        }
        else if (ev.key == TB_KEY_MOUSE_WHEEL_UP) app::seekOff(10000);
        else if (ev.key == TB_KEY_MOUSE_WHEEL_DOWN) app::seekOff(-10000);

        return;
    }

    /* scroll ontop of volume */
    if (ev.y == volumeOff && ev.x >= xOff)
    {
        if (ev.key == TB_KEY_MOUSE_WHEEL_DOWN) app::volumeDown(0.1f);
        else if (ev.key == TB_KEY_MOUSE_WHEEL_UP) app::volumeUp(0.1f);
        else if (ev.key == TB_KEY_MOUSE_LEFT) app::toggleMute();

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
        pl.focus(target);
    if (ev.key == TB_KEY_MOUSE_RIGHT)
    {
        pl.focus(target);
        pl.selectFocused();
    }
    else if (ev.key == TB_KEY_MOUSE_WHEEL_UP)
        pl.focus(pl.m_focused - defaults::MOUSE_STEP);
    else if (ev.key == TB_KEY_MOUSE_WHEEL_DOWN)
        pl.focus(pl.m_focused + defaults::MOUSE_STEP);
}

void
fillInputMap()
{
    /* ascii's are all the same */
    for (ssize i = 0; i < utils::size(s_aInputMap); ++i) s_aInputMap[i] = i;

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

} /* namespace platform::termbox2::input */
