#include "frame.h"

#include "app.h"

#include "klib/Ctx.h"

#ifdef OPT_MPRIS
    #include "platform/mpris/mpris.h"
#endif

void
frame_run(void)
{
    Window win = {0};
    app_g_pWin = &win;

    if (!WindowInit(&win, k_CtxArena()))
    {
        K_CTX_LOG_ERROR("WindowInit()");
        return;
    }

    k_print_MapAddFormatter(k_CtxPrintMap(), "PTermSize", WindowTermSizePFormatter);
    k_print_MapAddFormatter(k_CtxPrintMap(), "input_READ_MODE", input_ReadModePFormatter);

    app_g_pPlayer->focusedI = 0;
    PlayerSelectFocused(app_g_pPlayer);

#ifdef OPT_MPRIS
    platform_mpris_initLocks();
    platform_mpris_init();

    k_Thread thMPris = {0};
    k_ThreadInit(&thMPris, platform_mpris_pollLoop, NULL);
#endif

    do
    {
        K_ARENA_SCOPE_DELETERS(k_CtxArena())
        {
            win.vol_bRunning = app_g_vol_bRunning;

            PlayerNextSongIfPrevEnded(app_g_pPlayer);
            WindowDraw(&win);
            WindowProcEvents(&win);
        }
    }
    while (app_g_vol_bRunning);

#ifdef OPT_MPRIS
    app_g_vol_bRunning = false;
    platform_mpris_wakeUp();
    k_ThreadJoin(&thMPris);
    platform_mpris_deinit();
    platform_mpris_destroyLocks();
#endif

    WindowDestroy(&win);
}
