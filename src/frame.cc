#include "frame.hh"

#include "adt/Arena.hh"
#include "adt/defer.hh"
#include "app.hh"
#include "platform/Termbox.hh"

#ifdef MPRIS_LIB
    #include "mpris.hh"
#endif

namespace frame
{

void
run()
{
    Arena arena(SIZE_1M);
    defer( ArenaFreeAll(&arena) );

    app::g_pPlayer->focused = 0;
    PlayerSelectFocused(app::g_pPlayer);

#ifdef MPRIS_LIB
    mpris::init();
#endif

    do
    {
        platform::TermboxRender(&arena.base);
        platform::TermboxProcEvents(&arena.base);

#ifdef MPRIS_LIB
        mpris::proc();
        if (app::g_pMixer->bPlaybackStarted)
        {
            app::g_pMixer->bPlaybackStarted = false;
            mpris::destroy();
            mpris::init();
        }
#endif

        ArenaReset(&arena);
    } while (app::g_bRunning);
}

} /* namespace frame */
