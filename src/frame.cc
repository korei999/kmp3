#include "frame.hh"

#include "adt/Arena.hh"
#include "adt/defer.hh"
#include "adt/logs.hh"
#include "app.hh"
#include "defaults.hh"
#include "adt/Thread.hh"

#ifdef USE_MPRIS
    #include "mpris.hh"
#endif

namespace frame
{

#ifdef USE_MPRIS
static int
mprisPollLoop(void*)
{
    while (app::g_bRunning)
    {
        mpris::proc();
        if (app::g_pMixer->mprisHasToUpdate().load(std::memory_order_relaxed))
        {
            app::g_pMixer->mprisSetToUpdate(false);
            mpris::destroy();
            mpris::init();
        }

        utils::sleepMS(defaults::MPRIS_UPDATE_RATE);
    }

    return THREAD_STATUS::SUCCESS;
}
#endif

void
run()
{
    app::g_pWin = app::allocWindow(app::g_pPlayer->m_pAlloc);
    if (app::g_pWin == nullptr)
    {
        CERR("app::allocWindow(): failed\n");
        return;
    }

    Arena arena(SIZE_1M * 2);
    defer( arena.freeAll() );

    if (app::g_pWin->start(&arena) == false)
    {
        CERR("failed to start window\n");
        return;
    }

    app::g_pPlayer->m_focused = 0;
    app::g_pPlayer->selectFocused();

#ifdef USE_MPRIS
    mpris::init();
    /*thrd_t mprisThrd {};*/
    /*thrd_create(&mprisThrd, mprisPollLoop, {});*/

    Thread thMPris(mprisPollLoop, {});

    defer(
        /* NOTE: prevent deadlock if something throws */
        app::g_bRunning = false;
        thMPris.join()
    );

    /*defer( thrd_join(mprisThrd, {}) );*/
#endif

    defer( app::g_pWin->destroy() );

    do
    {
        app::g_pWin->draw();
        app::g_pWin->procEvents();

        arena.shrinkToFirstBlock();
        arena.reset();
    }
    while (app::g_bRunning);
}

} /* namespace frame */
