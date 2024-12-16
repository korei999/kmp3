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
        if (app::g_pMixer->bUpdateMpris)
        {
            app::g_pMixer->bUpdateMpris = false;
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
    app::g_pWin = app::allocWindow(app::g_pPlayer->pAlloc);
    if (app::g_pWin == nullptr)
    {
        CERR("app::allocWindow(): failed\n");
        return;
    }

    Arena arena(SIZE_8M);
    defer( ArenaFreeAll(&arena) );

    if (WindowStart(app::g_pWin, &arena) == false)
    {
        CERR("failed to start window\n");
        return;
    }
    defer( WindowDestroy(app::g_pWin) );

    app::g_pPlayer->focused = 0;
    PlayerSelectFocused(app::g_pPlayer);

#ifdef USE_MPRIS
    mpris::init();
    thrd_t mprisThrd {};
    thrd_create(&mprisThrd, mprisPollLoop, {});
    defer( thrd_join(mprisThrd, {}) );
#endif

    do
    {
        WindowDraw(app::g_pWin);
        WindowProcEvents(app::g_pWin);

        ArenaReset(&arena);
    }
    while (app::g_bRunning);
}

} /* namespace frame */
