#include "frame.hh"

#include "app.hh"
#include "defaults.hh"

#include "adt/Arena.hh"
#include "adt/Thread.hh"
#include "adt/defer.hh"
#include "adt/logs.hh"

#ifdef OPT_MPRIS
    #include "mpris.hh"
#endif

using namespace adt;

namespace frame
{

#ifdef OPT_MPRIS
static THREAD_STATUS
mprisPollLoop(void*)
{
    while (app::g_bRunning)
    {
        mpris::proc();
        if (app::mixer().mprisHasToUpdate().load(atomic::ORDER::ACQUIRE))
        {
            app::mixer().mprisSetToUpdate(false);
            mpris::destroy();
            mpris::init();
        }

        utils::sleepMS(defaults::MPRIS_UPDATE_RATE);
    }

    return THREAD_STATUS(0);
}
#endif

void
run()
{
    if (!(app::g_pWin = app::allocWindow(app::player().m_pAlloc)))
    {
        CERR("app::allocWindow(): failed\n");
        return;
    }

    Arena arena(SIZE_1M * 2);
    defer( arena.freeAll() );

    if (app::window().start(&arena) == false)
    {
        CERR("failed to start window\n");
        return;
    }

    app::player().m_focused = 0;
    app::player().selectFocused();

#ifdef OPT_MPRIS
    mpris::init();

    Thread thMPris {mprisPollLoop, {}};
    defer(
        /* NOTE: prevent deadlock if something throws */
        app::g_bRunning = false;
        thMPris.join()
    );
#endif

    defer( app::window().destroy() );

    do
    {
        app::window().draw();
        app::window().procEvents();

        arena.shrinkToFirstBlock();
        arena.reset();
    }
    while (app::g_bRunning);
}

} /* namespace frame */
