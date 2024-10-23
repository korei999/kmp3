#include "frame.hh"

#include "adt/Arena.hh"
#include "adt/defer.hh"
#include "adt/guard.hh"
#include "app.hh"
#include "platform/Termbox.hh"

namespace frame
{

static mtx_t s_mtxUpdate {};
cnd_t g_cndUpdate {};

void
run()
{
    mtx_init(&s_mtxUpdate, mtx_plain);
    cnd_init(&g_cndUpdate);
    defer(
        mtx_destroy(&s_mtxUpdate);
        cnd_destroy(&g_cndUpdate);
    );

    platform::TermboxInit();
    defer( platform::TermboxStop() );

    Arena arena(SIZE_1M);
    defer( ArenaFreeAll(&arena) );

    do
    {
        platform::TermboxRender(&arena.base);
        platform::TermboxProcEvents(&arena.base);

        ArenaReset(&arena);
    }
    while (app::g_bRunning && app::g_pMixer->bRunning);
}

} /* namespace frame */
