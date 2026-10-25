#include "frame.h"

#include "app.h"

#include "klib/Ctx.h"

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

    do
    {
        K_ARENA_SCOPE_DELETERS(k_CtxArena())
        {
            win.vol_bRunning = app_g_vol_bRunning;

            WindowDraw(&win);
            WindowProcEvents(&win);
        }
    }
    while (app_g_vol_bRunning);
}
