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

#ifdef MPRIS_LIB
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
    Arena arena(SIZE_1M);
    defer( ArenaFreeAll(&arena) );

    app::g_pPlayer->focused = 0;
    PlayerSelectFocused(app::g_pPlayer);

#ifdef MPRIS_LIB
    mpris::init();
    thrd_t mprisThrd {};
    thrd_create(&mprisThrd, mprisPollLoop, {});
    defer( thrd_join(mprisThrd, {}) );
#endif

    do
    {
        platform::TermboxRender(&arena.base);
        platform::TermboxProcEvents(&arena.base);

        ArenaReset(&arena);
    } while (app::g_bRunning);
}

} /* namespace frame */
