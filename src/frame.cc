#include "frame.hh"

#include "adt/Arena.hh"
#include "adt/defer.hh"
#include "adt/logs.hh"
#include "app.hh"
#include "defaults.hh"
#include "adt/Thread.hh"

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
        if (app::g_pMixer->mprisHasToUpdate().load(atomic::ORDER::ACQUIRE))
        {
            app::g_pMixer->mprisSetToUpdate(false);
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

#ifdef OPT_MPRIS
    mpris::init();

    Thread thMPris(mprisPollLoop, {});
    defer(
        /* NOTE: prevent deadlock if something throws */
        app::g_bRunning = false;
        thMPris.join()
    );
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
