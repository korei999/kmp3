#include "frame.hh"

#include "adt/Arena.hh"
#include "adt/defer.hh"
#include "adt/logs.hh"
#include "app.hh"
#include "defaults.hh"

#ifdef USE_MPRIS
    #include "mpris.hh"
#endif

namespace frame
{

#ifdef USE_MPRIS
static int
mprisPollLoop([[maybe_unused]] void* pNull)
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

    return thrd_success;
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

    Arena arena(SIZE_1M);
    defer( arena.freeAll() );

    if (app::g_pWin->start(&arena) == false)
    {
        CERR("failed to start window\n");
        return;
    }
    defer( app::g_pWin->destroy() );

    app::g_pPlayer->m_focused = 0;
    app::g_pPlayer->selectFocused();

#ifdef USE_MPRIS
    mpris::init();
    thrd_t mprisThrd {};
    thrd_create(&mprisThrd, mprisPollLoop, {});
    defer( thrd_join(mprisThrd, {}) );
#endif

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
