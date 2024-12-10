#include "chafa.hh"

#include "adt/logs.hh"
#include "defaults.hh"

#include <chafa/chafa.h>
#include <ncurses.h>
#include <cmath>

/* https://github.com/hpjansson/chafa/blob/master/examples/ncurses.c */

namespace platform
{
namespace chafa
{

[[nodiscard]] static int
convertFormat(const ffmpeg::PIXEL_FORMAT eFormat)
{
    switch (eFormat)
    {
        case ffmpeg::PIXEL_FORMAT::NONE: return -1;

        case ffmpeg::PIXEL_FORMAT::RGB8: return CHAFA_PIXEL_RGB8;

        case ffmpeg::PIXEL_FORMAT::RGBA8_PREMULTIPLIED:
        case ffmpeg::PIXEL_FORMAT::RGBA8_UNASSOCIATED:
        return CHAFA_PIXEL_RGBA8_PREMULTIPLIED;
    }
}

[[nodiscard]] static int
formatChannels(const ffmpeg::PIXEL_FORMAT eFormat)
{
    switch (eFormat)
    {
        case ffmpeg::PIXEL_FORMAT::NONE: return 0;

        case ffmpeg::PIXEL_FORMAT::RGBA8_UNASSOCIATED:
        case ffmpeg::PIXEL_FORMAT::RGBA8_PREMULTIPLIED:
        return 4;

        case ffmpeg::PIXEL_FORMAT::RGB8: return 3;
    }
}

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

static void
paintCanvas(
    ChafaCanvas *canvas,
    const u8* pBuff,
    const int width,
    const int height,
    const ffmpeg::PIXEL_FORMAT eFormat
)
{
    int convertedFormat = convertFormat(eFormat);
    LOG_NOTIFY("convertFormat: {}\n", convertedFormat);

    if (convertedFormat == -1) return;

    int nChannels = formatChannels(eFormat);

    chafa_canvas_draw_all_pixels(
        canvas,
        (ChafaPixelType)convertedFormat,
        pBuff,
        width,
        height,
        width * nChannels
    );
}

static void
canvasToNcurses(WINDOW* pWin, ChafaCanvas* canvas, const int width, const int height, const int off)
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

void
showImage(WINDOW* pWin, const ffmpeg::Image img, const int termHeight, const int termWidth)
{
    if (convertFormat(img.eFormat) == -1) return;

    f64 scaleFactor = f64(termHeight) / f64(img.height);
    int scaledWidth = std::round(img.width * scaleFactor / defaults::FONT_ASPECT_RATIO);
    int diff = termWidth - scaledWidth;

    LOG_GOOD("termWidth: {}, scaledWidth: {}, diff: {}\n", termWidth, scaledWidth, diff);
    LOG_WARN("result aspect: {}\n", f64(termWidth) / f64(termHeight));

    ChafaCanvas* canvas = createCanvas(scaledWidth, termHeight);

    paintCanvas(canvas, img.pBuff, img.width, img.height, img.eFormat);
    canvasToNcurses(pWin, canvas, scaledWidth, termHeight, diff);

    chafa_canvas_unref(canvas);
}

} /* namespace chafa */
} /* namespace platform */
