#include "frame.hh"

#include "app.hh"

#ifdef OPT_MPRIS
    #include "platform/mpris/mpris.hh"
#endif

namespace frame
{

void
run()
{
    /* There is just one ui for now. */
    platform::ansi::Win ansiWindow {};
    app::g_pWin = &ansiWindow;

    Arena* pArena = IThreadPool::inst()->arena();

    if (app::window().start(pArena) == false)
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
        try
        {
            app::player().nextSongIfPrevEnded();
            app::window().draw();
            app::window().procEvents();
        }
        catch (const AllocException& ex)
        {
            LogError{"{}\n", ex.what()};
        }

        pArena->reset();
    }
    while (app::g_vol_bRunning);
}

} /* namespace frame */
