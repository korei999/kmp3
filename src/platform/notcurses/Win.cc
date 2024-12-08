#include "Win.hh"

#include "adt/defer.hh"
#include "adt/logs.hh"
#include "keybinds.hh"

namespace platform
{
namespace notcurses
{

static int s_aInputMap[preterunicode(500) + 1] {};

static void
fillInputMap()
{
    for (int i = ' '; i < '~'; ++i) s_aInputMap[i] = i;

    s_aInputMap[NCKEY_UP] = keys::ARROWUP;
    s_aInputMap[NCKEY_DOWN] = keys::ARROWDOWN;
    s_aInputMap[NCKEY_LEFT] = keys::ARROWLEFT;
    s_aInputMap[NCKEY_RIGHT] = keys::ARROWRIGHT;

    s_aInputMap[NCKEY_ENTER] = keys::ENTER;

    s_aInputMap[NCKEY_PGUP] = keys::PGUP;
    s_aInputMap[NCKEY_PGDOWN] = keys::PGDN;

    s_aInputMap[NCKEY_HOME] = keys::HOME;
    s_aInputMap[NCKEY_END] = keys::END;
}

bool
WinStart(Win* s, Arena* pArena)
{
    s->pArena = pArena;
    s->pNC = notcurses_init({}, {});
    fillInputMap();

    if (!s->pNC) CERR("notcurses_init() failed\n");

    return s->pNC != nullptr;
}

void
WinDestroy(Win* s)
{
    notcurses_stop(s->pNC);
}

void
WinDraw(Win* s)
{
    auto* pStd = notcurses_stdplane(s->pNC);
    auto* pReel = ncreel_create(pStd, {});
    defer( ncreel_destroy(pReel) );

    notcurses_render(s->pNC);
}

void
WinProcEvents(Win* s)
{
    ncinput in {};
    notcurses_get(s->pNC, {}, &in);

    s64 key = in.id < utils::size(s_aInputMap) ? s_aInputMap[in.id] : 0;
    LOG("'{}', id: {}, key: '{}', shift: {}, ctrl: {}, alt: {}\n",
        in.utf8, in.id, key, ncinput_shift_p(&in), ncinput_ctrl_p(&in), ncinput_alt_p(&in)
    );

    if (in.id == NCKEY_RESIZE)
    {
        notcurses_refresh(s->pNC, {}, {});
        return;
    }

    if (key == L'D' && ncinput_ctrl_p(&in)) key = keys::CTRL_D;
    else if (key == L'U' && ncinput_ctrl_p(&in)) key = keys::CTRL_U;

    if (key > 0 && (in.evtype == NCTYPE_PRESS || in.evtype == NCTYPE_REPEAT))
    {
        for (const auto& k : keybinds::inl_aKeys)
        {
            if (key == k.key || key == k.ch)
                resolvePFN(k.pfn, k.arg);
        }
    }
}

void
WinSeekFromInput(Win* s)
{
}

void
WinSubStringSearch(Win* s)
{
}

} /* namespace notcurses */
} /* namespace platform */
