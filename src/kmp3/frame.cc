#include "frame.hh"

#include "app.hh"

#ifdef OPT_MPRIS
    #include "platform/mpris/mpris.hh"
#endif

using namespace adt;

namespace frame
{

void
run()
{
    /* There is just one ui for now. */
    platform::ansi::Win ansiWindow {};
    app::g_pWin = &ansiWindow;

    Arena arena {SIZE_8G};
    defer( arena.freeAll() );

    if (app::window().start(&arena) == false)
    {
        print::out("failed to start window\n");
        return;
    }

    app::player().m_focusedI = 0;
    app::player().selectFocused();

#ifdef OPT_MPRIS
    mpris::init();

    Thread thMPris {mpris::pollLoop, nullptr};
    defer(
        /* NOTE: prevent deadlock if something throws */
        app::g_vol_bRunning = false;
        mpris::wakeUp();
        thMPris.join()
    );
#endif

    defer( app::window().destroy() );

    do
    {
        app::player().nextSongIfPrevEnded();
        app::window().draw();
        app::window().procEvents();

        arena.reset();
    }
    while (app::g_vol_bRunning);
}

} /* namespace frame */
