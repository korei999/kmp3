#pragma once

#include "adt/print.hh"

#include <sys/ioctl.h>

namespace platform
{
namespace ansi
{

struct TermSize
{
    int width {};
    int height {};
    int pixWidth {};
    int pixHeight {};
};

[[nodiscard]] static TermSize
getTermSize()
{
    TermSize term;

    term.width = term.height = term.pixWidth = term.pixHeight = -1;

#ifdef G_OS_WIN32
    {
        HANDLE chd = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO csb_info;

        if (chd != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(chd, &csb_info))
        {
            term_size.width_cells = csb_info.srWindow.Right - csb_info.srWindow.Left + 1;
            term_size.height_cells = csb_info.srWindow.Bottom - csb_info.srWindow.Top + 1;
        }
    }
#else
    {
        struct winsize w;
        bool bHaveWINSZ = false;

        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) >= 0 || ioctl(STDERR_FILENO, TIOCGWINSZ, &w) >= 0 ||
            ioctl(STDIN_FILENO, TIOCGWINSZ, &w) >= 0)
            bHaveWINSZ = true;

        if (bHaveWINSZ)
        {
            term.width = w.ws_col;
            term.height = w.ws_row;
            term.pixWidth = w.ws_xpixel;
            term.pixHeight = w.ws_ypixel;
        }
    }
#endif

    if (term.width <= 0)
        term.width = -1;
    if (term.height <= 2)
        term.height = -1;

    /* If .ws_xpixel and .ws_ypixel are filled out, we can calculate
     * aspect information for the font used. Sixel-capable terminals
     * like mlterm set these fields, but most others do not. */

    if (term.pixWidth <= 0 || term.pixHeight <= 0)
    {
        term.pixWidth = -1;
        term.pixHeight = -1;
    }

    return term;
}

} /* namespace ansi */
} /* namespace platform */

namespace adt
{
namespace print
{

inline u32
formatToContext(Context ctx, [[maybe_unused]] FormatArgs fmtArgs, const platform::ansi::TermSize x)
{
    ctx.fmt = "[width: {}, height: {}, pixWidth: {}, pixHeight: {}]";
    ctx.fmtIdx = 0;
    return printArgs(ctx, x.width, x.height, x.pixWidth, x.pixHeight);
}

} /* namespace print */
} /* namespace adt */
