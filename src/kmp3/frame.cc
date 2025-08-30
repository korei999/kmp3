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
    if (!(app::g_pWin = app::allocWindow(app::player().m_pAlloc)))
    {
        print::out("app::allocWindow(): failed\n");
        return;
    }

    Arena arena {SIZE_8G};
    defer( arena.freeAll() );

    if (app::window().start(&arena) == false)
    {
        print::out("failed to start window\n");
        return;
    }

    app::player().m_focused = 0;
    app::player().selectFocused();

#ifdef OPT_MPRIS
    mpris::init();

    Thread thMPris {mpris::pollLoop, nullptr};
    defer(
        /* NOTE: prevent deadlock if something throws */
        app::g_bRunning = false;
        mpris::wakeUp();
        thMPris.join()
    );
#endif

    defer( app::window().destroy() );

    do
    {
        app::window().draw();
        app::window().procEvents();

        arena.reset();
    }
    while (app::g_bRunning);
}

} /* namespace frame */
