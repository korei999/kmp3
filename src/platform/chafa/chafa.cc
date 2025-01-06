#include "chafa.hh"

#include "adt/defer.hh"
#include "adt/logs.hh"
#include "defaults.hh"
#include "TextBuff.hh"

#include <chafa/chafa.h>
#include <cmath>

#include <sys/ioctl.h>

/* https://github.com/hpjansson/chafa/blob/master/examples/ncurses.c */
/* https://github.com/hpjansson/chafa/blob/master/examples/adaptive.c */

#define BUFFER_MAX 4096

namespace platform
{
namespace chafa
{

struct TermSize
{
    int widthCells {};
    int heightCells {};
    int widthPixels {};
    int heightPixels {};
};

[[nodiscard]] static int
formatToPixelType(const IMAGE_PIXEL_FORMAT eFormat)
{
    switch (eFormat)
    {
        case IMAGE_PIXEL_FORMAT::NONE: return -1;

        case IMAGE_PIXEL_FORMAT::RGB8: return CHAFA_PIXEL_RGB8;

        case IMAGE_PIXEL_FORMAT::RGBA8_PREMULTIPLIED:
        case IMAGE_PIXEL_FORMAT::RGBA8_UNASSOCIATED:
        return CHAFA_PIXEL_RGBA8_PREMULTIPLIED;
    }

    return -1;
}

[[nodiscard]] static int
getFormatChannelNumber(const IMAGE_PIXEL_FORMAT eFormat)
{
    switch (eFormat)
    {
        case IMAGE_PIXEL_FORMAT::NONE: return 0;

        case IMAGE_PIXEL_FORMAT::RGBA8_UNASSOCIATED:
        case IMAGE_PIXEL_FORMAT::RGBA8_PREMULTIPLIED:
        return 4;

        case IMAGE_PIXEL_FORMAT::RGB8: return 3;
    }

    return 0;
}

static void
getTTYSize(TermSize* s)
{
    TermSize term_size;

    term_size.widthCells = term_size.heightCells = term_size.widthPixels = term_size.heightPixels = -1;

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
        bool have_winsz = false;

        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) >= 0 || ioctl(STDERR_FILENO, TIOCGWINSZ, &w) >= 0 ||
            ioctl(STDIN_FILENO, TIOCGWINSZ, &w) >= 0)
            have_winsz = true;

        if (have_winsz)
        {
            term_size.widthCells = w.ws_col;
            term_size.heightCells = w.ws_row;
            term_size.widthPixels = w.ws_xpixel;
            term_size.heightPixels = w.ws_ypixel;
        }
    }
#endif

    if (term_size.widthCells <= 0)
        term_size.widthCells = -1;
    if (term_size.heightCells <= 2)
        term_size.heightCells = -1;

    /* If .ws_xpixel and .ws_ypixel are filled out, we can calculate
     * aspect information for the font used. Sixel-capable terminals
     * like mlterm set these fields, but most others do not. */

    if (term_size.widthPixels <= 0 || term_size.heightPixels <= 0)
    {
        term_size.widthPixels = -1;
        term_size.heightPixels = -1;
    }

    *s = term_size;
}

static void
detectTerminal(ChafaTermInfo** ppTermInfo, ChafaCanvasMode* pMode, ChafaPixelMode* pPixelMode)
{
    ChafaCanvasMode mode;
    ChafaPixelMode pixelMode;
    ChafaTermInfo* pTermInfo;
    // ChafaTermInfo* pFallbackInfo;
    gchar** ppEnv;

    /* Examine the environment variables and guess what the terminal can do */
    ppEnv = g_get_environ();
    pTermInfo = chafa_term_db_detect(chafa_term_db_get_default(), ppEnv);

    /* See which control sequences were defined, and use that to pick the most
     * high-quality rendering possible */

    if (chafa_term_info_have_seq(pTermInfo, CHAFA_TERM_SEQ_BEGIN_KITTY_IMMEDIATE_IMAGE_V1))
    {
        pixelMode = CHAFA_PIXEL_MODE_KITTY;
        mode = CHAFA_CANVAS_MODE_TRUECOLOR;
    }
    else if (chafa_term_info_have_seq(pTermInfo, CHAFA_TERM_SEQ_BEGIN_SIXELS))
    {
        pixelMode = CHAFA_PIXEL_MODE_SIXELS;
        mode = CHAFA_CANVAS_MODE_TRUECOLOR;
    }
    else
    {
        pixelMode = CHAFA_PIXEL_MODE_SYMBOLS;

        if (chafa_term_info_have_seq(pTermInfo, CHAFA_TERM_SEQ_SET_COLOR_FGBG_DIRECT) &&
            chafa_term_info_have_seq(pTermInfo, CHAFA_TERM_SEQ_SET_COLOR_FG_DIRECT) &&
            chafa_term_info_have_seq(pTermInfo, CHAFA_TERM_SEQ_SET_COLOR_BG_DIRECT))
            mode = CHAFA_CANVAS_MODE_TRUECOLOR;
        else if (chafa_term_info_have_seq(pTermInfo, CHAFA_TERM_SEQ_SET_COLOR_FGBG_256) &&
                 chafa_term_info_have_seq(pTermInfo, CHAFA_TERM_SEQ_SET_COLOR_FG_256) &&
                 chafa_term_info_have_seq(pTermInfo, CHAFA_TERM_SEQ_SET_COLOR_BG_256))
            mode = CHAFA_CANVAS_MODE_INDEXED_240;
        else if (chafa_term_info_have_seq(pTermInfo, CHAFA_TERM_SEQ_SET_COLOR_FGBG_16) &&
                 chafa_term_info_have_seq(pTermInfo, CHAFA_TERM_SEQ_SET_COLOR_FG_16) &&
                 chafa_term_info_have_seq(pTermInfo, CHAFA_TERM_SEQ_SET_COLOR_BG_16))
            mode = CHAFA_CANVAS_MODE_INDEXED_16;
        else if (chafa_term_info_have_seq(pTermInfo, CHAFA_TERM_SEQ_INVERT_COLORS) &&
                 chafa_term_info_have_seq(pTermInfo, CHAFA_TERM_SEQ_RESET_ATTRIBUTES))
            mode = CHAFA_CANVAS_MODE_FGBG_BGFG;
        else
            mode = CHAFA_CANVAS_MODE_FGBG;
    }

    /* Hand over the information to caller */

    *ppTermInfo = pTermInfo;
    *pMode = mode;
    *pPixelMode = pixelMode;

    LOG("pixelMode: {}\n", (int)pixelMode);

    /* Cleanup */
    g_strfreev(ppEnv);
}

#ifdef USE_NCURSES
[[nodiscard]] static ChafaCanvasMode
detectCanvasMode()
{
    /* COLORS is a global variable defined by ncurses. It depends on termcap
     * for the terminal specified in TERM. In order to test the various modes, you
     * could try running this program with either of these:
     *
     * TERM=xterm
     * TERM=xterm-16color
     * TERM=xterm-256color
     * TERM=xterm-direct
     */
    return COLORS  >= (1 << 24) ? CHAFA_CANVAS_MODE_TRUECOLOR
           : COLORS >= (1 << 8) ? CHAFA_CANVAS_MODE_INDEXED_240
           : COLORS >= (1 << 4) ? CHAFA_CANVAS_MODE_INDEXED_16
           : COLORS >= (1 << 3) ? CHAFA_CANVAS_MODE_INDEXED_8
                                : CHAFA_CANVAS_MODE_FGBG;
}
#endif

static GString*
getString(
    const void* pPixels,
    const int pixWidth,
    const int pixHeight,
    const int pixRowStride,
    const ChafaPixelType ePixelType,
    const int widthCells,
    const int heightCells,
    const int cellWidth,
    const int cellHeight
)
{
    ChafaTermInfo* pTermInfo;
    ChafaCanvasMode mode;
    ChafaPixelMode ePixelMode;
    ChafaSymbolMap* pSymbolMap;
    ChafaCanvasConfig* pConfig;
    ChafaCanvas* pCanvas;

    detectTerminal(&pTermInfo, &mode, &ePixelMode);

    /* Specify the symbols we want */
    pSymbolMap = chafa_symbol_map_new();
    chafa_symbol_map_add_by_tags(pSymbolMap, CHAFA_SYMBOL_TAG_BLOCK);

    /* Set up a configuration with the symbols and the canvas size in characters */
    pConfig = chafa_canvas_config_new();
    chafa_canvas_config_set_canvas_mode(pConfig, mode);
    chafa_canvas_config_set_pixel_mode(pConfig, ePixelMode);
    chafa_canvas_config_set_geometry(pConfig, widthCells, heightCells);
    chafa_canvas_config_set_symbol_map(pConfig, pSymbolMap);

    if (cellWidth > 0 && cellHeight > 0)
    {
        /* We know the pixel dimensions of each cell. Store it in the config. */
        chafa_canvas_config_set_cell_geometry(pConfig, cellWidth, cellHeight);
    }

    /* Create canvas */
    pCanvas = chafa_canvas_new(pConfig);

    /* Draw pixels to the canvas */
    chafa_canvas_draw_all_pixels(pCanvas, ePixelType, (u8*)pPixels, pixWidth, pixHeight, pixRowStride);

    /* Build printable strings */
    auto* pGStr = chafa_canvas_print(pCanvas, pTermInfo);

    chafa_canvas_unref(pCanvas);
    chafa_canvas_config_unref(pConfig);
    chafa_symbol_map_unref(pSymbolMap);
    chafa_term_info_unref(pTermInfo);

    return pGStr;
}

#ifdef USE_NCURSES
[[nodiscard]] static ChafaCanvas*
createCanvas(const int width, const int height)
{
    ChafaSymbolMap* symbol_map;
    ChafaCanvasConfig* config;
    ChafaCanvas* canvas;
    ChafaCanvasMode mode = detectCanvasMode();

    /* Specify the symbols we want: Box drawing and block elements are both
     * useful and widely supported. */

    symbol_map = chafa_symbol_map_new();
    chafa_symbol_map_add_by_tags(symbol_map, CHAFA_SYMBOL_TAG_SPACE);
    chafa_symbol_map_add_by_tags(symbol_map, CHAFA_SYMBOL_TAG_BLOCK);
    chafa_symbol_map_add_by_tags(symbol_map, CHAFA_SYMBOL_TAG_BORDER);

    /* Set up a configuration with the symbols and the canvas size in characters */

    config = chafa_canvas_config_new();
    chafa_canvas_config_set_canvas_mode(config, mode);
    chafa_canvas_config_set_symbol_map(config, symbol_map);

    /* Reserve one row below canvas for status text */
    chafa_canvas_config_set_geometry(config, width, height);

    /* Apply tweaks for low-color modes */

    if (mode == CHAFA_CANVAS_MODE_INDEXED_240)
    {
        /* We get better color fidelity using DIN99d in 240-color mode.
         * This is not needed in 16-color mode because it uses an extra
         * preprocessing step instead, which usually performs better. */
        chafa_canvas_config_set_color_space(config, CHAFA_COLOR_SPACE_DIN99D);
    }

    if (mode == CHAFA_CANVAS_MODE_FGBG)
    {
        /* Enable dithering in monochromatic mode so gradients become
         * somewhat legible. */
        chafa_canvas_config_set_dither_mode(config, CHAFA_DITHER_MODE_ORDERED);
    }

    /* Create canvas */

    canvas = chafa_canvas_new(config);

    chafa_symbol_map_unref(symbol_map);
    chafa_canvas_config_unref(config);
    return canvas;
}
#endif

[[maybe_unused]] static void
paintCanvas(
    ChafaCanvas *canvas,
    const u8* pBuff,
    const int width,
    const int height,
    const IMAGE_PIXEL_FORMAT eFormat
)
{
    int convertedFormat = formatToPixelType(eFormat);
    LOG_NOTIFY("convertFormat: {}\n", convertedFormat);

    if (convertedFormat == -1) return;

    int nChannels = getFormatChannelNumber(eFormat);

    chafa_canvas_draw_all_pixels(
        canvas,
        (ChafaPixelType)convertedFormat,
        pBuff,
        width,
        height,
        width * nChannels
    );
}

#ifdef USE_NCURSES
static void
canvasToNCurses(WINDOW* pWin, ChafaCanvas* canvas, const int width, const int height, const int off)
{
    ChafaCanvasMode mode = detectCanvasMode();

    int pair = 256; /* Reserve lower pairs for application in direct-color mode */
    const int halfOff = std::round(off / 2.0);

    for (int y = 0; y < height - 1; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            wchar_t wc[2];
            cchar_t cch;
            int fg, bg;

            /* wchar_t is 32-bit in glibc, but this may not work on e.g. Windows */
            wc[0] = chafa_canvas_get_char_at(canvas, x, y);
            wc[1] = 0;

            if (mode == CHAFA_CANVAS_MODE_TRUECOLOR)
            {
                chafa_canvas_get_colors_at(canvas, x, y, &fg, &bg);
                init_extended_pair(pair, fg, bg);
            }
            else if (mode == CHAFA_CANVAS_MODE_FGBG)
            {
                pair = 0;
            }
            else
            {
                /* In indexed color mode, we've probably got enough pairs
                 * to just let ncurses allocate and reuse as needed. */
                chafa_canvas_get_raw_colors_at(canvas, x, y, &fg, &bg);
                pair = alloc_pair(fg, bg);
            }

            setcchar(&cch, wc, A_NORMAL, -1, &pair);
            mvwadd_wch(pWin, y, x + halfOff, &cch);
            ++pair;
        }
    }
}
#endif

#ifdef USE_NCURSES
void
showImageNCurses(WINDOW* pWin, const ffmpeg::Image img, const int termHeight, const int termWidth)
{
    if (formatToPixelType(img.eFormat) == -1) return;

    f64 scaleFactor = f64(termHeight) / f64(img.height);
    int scaledWidth = std::round(img.width * scaleFactor / defaults::FONT_ASPECT_RATIO);
    int diff = termWidth - scaledWidth;

    LOG_GOOD("termWidth: {}, scaledWidth: {}, diff: {}\n", termWidth, scaledWidth, diff);
    LOG_WARN("result aspect: {}\n", f64(termWidth) / f64(termHeight));

    ChafaCanvas* canvas = createCanvas(scaledWidth, termHeight);

    paintCanvas(canvas, img.pBuff, img.width, img.height, img.eFormat);
    canvasToNCurses(pWin, canvas, scaledWidth, termHeight, diff);

    chafa_canvas_unref(canvas);
}
#endif

void
showImage(Arena* pArena, const ::Image img, const int termHeight, const int termWidth, const int hOff, const int vOff)
{
    TermSize termSize {};
    getTTYSize(&termSize);

    f64 fontRatio = defaults::FONT_ASPECT_RATIO;
    int cellWidth = -1, cellHeight = -1; /* Size of each character cell, in pixels */
    int widthCells {}, heightCells {};         /* Size of output image, in cells */

    if (termWidth > 0 && termHeight > 0 && termSize.widthPixels > 0 && termSize.heightPixels > 0)
    {
        cellWidth = termSize.widthPixels / termSize.widthCells;
        cellHeight = termSize.heightPixels / termSize.heightCells;
        fontRatio = f64(cellWidth) / f64(cellHeight);
    }

    /*widthCells = termSize.widthCells;*/
    /*heightCells = termSize.heightCells;*/
    widthCells = termWidth;
    heightCells = termHeight;

    chafa_calc_canvas_geometry(
        img.width, img.height,
        &widthCells, &heightCells,
        fontRatio, true, false
    );

    LOG_WARN("termWidth: {}, termHeight: {}, pixWidth: {}, pixHeigh: {}\n",
        termWidth, termHeight, termSize.widthPixels, termSize.heightPixels
    );

    auto* pGStr = getString(
        img.pBuff,
        img.width,
        img.height,
        img.width * getFormatChannelNumber(img.eFormat),
        (ChafaPixelType)formatToPixelType(img.eFormat),
        widthCells,
        heightCells,
        cellWidth,
        cellHeight
    );
    defer( g_string_free(pGStr, true) );

    {
        TextBuff textBuff(pArena);
        defer( textBuff.flush() );

        auto at = [&](int x, int y) {
            char aBuff[128] {};
            u32 n = print::toBuffer(aBuff, sizeof(aBuff) - 1, "\x1b[H\x1b[{}C\x1b[{}B", x, y);
            textBuff.push(aBuff, n);
        };

        //auto clearLine = [&] {
        //    TextBuffPush(&textBuff, "\x1b[K\r\n");
        //};

        //at(0, 0);
        //for (int i = 0; i < heightCells; ++i)
        //    clearLine();

        /* set centered position */
        int maxVOff = utils::max(vOff, 0);
        int maxHOff = utils::max(hOff, 0);
        at(maxHOff, maxVOff);
    }

    fputs(pGStr->str, stdout);
    fputc('\n', stdout);
}

Image
getImageString(Arena* pArena, const ::Image img, int termHeight, int termWidth)
{
    TermSize termSize {};
    getTTYSize(&termSize);

    f64 fontRatio = defaults::FONT_ASPECT_RATIO;
    int cellWidth = -1, cellHeight = -1; /* Size of each character cell, in pixels */
    int widthCells {}, heightCells {};         /* Size of output image, in cells */

    if (termWidth > 0 && termHeight > 0 && termSize.widthPixels > 0 && termSize.heightPixels > 0)
    {
        cellWidth = termSize.widthPixels / termSize.widthCells;
        cellHeight = termSize.heightPixels / termSize.heightCells;
        fontRatio = f64(cellWidth) / f64(cellHeight);
    }

    /*widthCells = termSize.widthCells;*/
    /*heightCells = termSize.heightCells;*/
    widthCells = termWidth;
    heightCells = termHeight;

    chafa_calc_canvas_geometry(
        img.width, img.height,
        &widthCells, &heightCells,
        fontRatio, true, false
    );

    LOG_GOOD("formatSize: {}\n", getFormatChannelNumber(img.eFormat));

    auto* pGStr = getString(
        img.pBuff,
        img.width,
        img.height,
        img.width * getFormatChannelNumber(img.eFormat),
        (ChafaPixelType)formatToPixelType(img.eFormat),
        widthCells,
        heightCells,
        cellWidth,
        cellHeight
    );
    defer( g_string_free(pGStr, true) );

    auto sRet = StringAlloc(pArena, pGStr->str, pGStr->len);
    assert(sRet.getSize() == (ssize)pGStr->len);
    return {
        .s = sRet,
        .width = widthCells,
        .height = heightCells,
    };
}

} /* namespace chafa */
} /* namespace platform */
