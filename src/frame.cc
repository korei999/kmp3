#include "frame.hh"

#include "adt/Arena.hh"
#include "adt/defer.hh"
#include "app.hh"
#include "platform/Termbox.hh"

namespace frame
{

void
run()
{
    Arena arena(SIZE_1M);
    defer( ArenaFreeAll(&arena) );

    do
    {
        platform::TermboxRender(&arena.base);
        platform::TermboxProcEvents(&arena.base);

        ArenaReset(&arena);
    }
    while (app::g_bRunning);
}

} /* namespace frame */