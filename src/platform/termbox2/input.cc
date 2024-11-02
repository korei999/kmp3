#include "input.hh"

#include "app.hh"
#include "window.hh"
#include "keybinds.hh"

namespace platform
{
namespace termbox2
{
namespace input
{

void
procKey(tb_event* pEv, Allocator* pAlloc)
{
    const auto& key = pEv->key;
    const auto& ch = pEv->ch;
    const auto& mod = pEv->mod;

    for (auto& k : keybinds::gc_aKeys)
    {
        auto& pfn = k.pfn;
        auto& arg = k.arg;

        if ((k.key > 0 && key == k.key) || (k.ch > 0 && ch == k.ch))
        {
            switch (k.arg.eType)
            {
                case keybinds::NONE:
                pfn.none();
                break;

                case keybinds::LONG:
                pfn.long_(arg.uVal.l);
                break;

                case keybinds::F32:
                pfn.f32_(arg.uVal.f);
                break;

                case keybinds::U64:
                pfn.u64_(arg.uVal.u);
                break;

                case keybinds::U64_BOOL:
                pfn.u64Bool(arg.uVal.u64Bool.u, arg.uVal.u64Bool.b);
                break;

                case keybinds::BOOL:
                pfn.bool_(arg.uVal.b);
                break;

                case keybinds::PASS_PALLOC:
                pfn.pass(pAlloc);
                break;
            }
        }
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


} /* namespace input */
} /* namespace termbox2 */
} /* namespace platform */
