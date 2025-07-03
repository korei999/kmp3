#include "chafa.hh"

#include "adt/defer.hh"
#include "adt/logs.hh"
#include "defaults.hh"

#include <cmath>

#include <sys/ioctl.h>

using namespace adt;

/* https://github.com/hpjansson/chafa/blob/master/examples/adaptive.c */

union StringLines
{
    GString* pGStr;
    Span<gchar*> sppLines;
};

namespace platform::chafa
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
        return CHAFA_PIXEL_RGBA8_UNASSOCIATED;
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

void
detectTerminal(ChafaTermInfo** ppTermInfo, ChafaCanvasMode* pMode, ChafaPixelMode* pPixelMode)
{
    ChafaCanvasMode mode;
    ChafaPixelMode pixelMode;
    ChafaTermInfo* pTermInfo;
    // ChafaTermInfo* pFallbackInfo;
    ChafaTermDb* pTermDb = chafa_term_db_new();
    defer( chafa_term_db_unref(pTermDb) );
    gchar** ppEnv;

    /* Examine the environment variables and guess what the terminal can do */
    ppEnv = g_get_environ();
    defer( g_strfreev(ppEnv) );

    pTermInfo = chafa_term_db_detect(pTermDb, ppEnv);

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
}

static StringLines
getString(
    const IMAGE_LAYOUT eLayout,
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

    defer(
        chafa_canvas_unref(pCanvas);
        chafa_canvas_config_unref(pConfig);
        chafa_symbol_map_unref(pSymbolMap);

        /* struct ChafaTermInfo
         * {
         *     gint refs;
         *     gchar *name;
         *     gchar seq_str [CHAFA_TERM_SEQ_MAX] [CHAFA_TERM_SEQ_LENGTH_MAX];
         *     SeqArgInfo seq_args [CHAFA_TERM_SEQ_MAX] [CHAFA_TERM_SEQ_ARGS_MAX];
         *     gchar *unparsed_str [CHAFA_TERM_SEQ_MAX];
         *     guint8 pixel_passthrough_needed [CHAFA_PIXEL_MODE_MAX];
         *     guint8 inherit_seq [CHAFA_TERM_SEQ_MAX];
         *     ChafaSymbolTags safe_symbol_tags;
         * }; */

        /* BUG: stupid fix for the chafa leak: https://github.com/hpjansson/chafa/commit/05e76092c459421131cca8d512df693d3fd98b99 */
        /* first 4 bytes is the ref count */
        int refs = *reinterpret_cast<int*>(pTermInfo);
        chafa_term_info_unref(pTermInfo);
        if (refs >= 2) chafa_term_info_unref(pTermInfo);
    );

    if (eLayout == IMAGE_LAYOUT::RAW)
    {
        /* Build printable strings */
        auto* pGStr = chafa_canvas_print(pCanvas, pTermInfo);

        return {.pGStr = pGStr};
    }
    else
    {
#ifdef OPT_CHAFA_SYMBOLS

        gchar** ppRows = chafa_canvas_print_rows_strv(pCanvas, pTermInfo);
        /* https://github.com/hpjansson/chafa/pull/240/commits/28ba1760b4aa9626cef38887479f22266ab1cad9 */
        /* defer( chafa_term_info_unref(pTermInfo) ); */

        gint len = 0;
        for (; ppRows[len]; ++len)
            ;

        return {.sppLines = {ppRows, len}};
#else
        return {};
#endif
    }
}

Image
allocImage(IAllocator* pAlloc, IMAGE_LAYOUT eLayout, const ::Image img, int termHeight, int termWidth)
{
    if (img.height <= 0 || img.width <= 0) return {};

    TermSize termSize {};
    getTTYSize(&termSize);

    f64 fontRatio = defaults::FONT_ASPECT_RATIO;
    int cellWidth = -1, cellHeight = -1; /* Size of each character cell, in pixels */
    int widthCells {}, heightCells {}; /* Size of output image, in cells */

    if (termWidth > 0 && termHeight > 0 && termSize.widthPixels > 0 && termSize.heightPixels > 0)
    {
        cellWidth = termSize.widthPixels / termSize.widthCells;
        cellHeight = termSize.heightPixels / termSize.heightCells;
        fontRatio = f64(cellWidth) / f64(cellHeight);
    }

    widthCells = termWidth;
    heightCells = termHeight;

    chafa_calc_canvas_geometry(
        img.width, img.height,
        &widthCells, &heightCells,
        fontRatio, true, false
    );

    LOG_GOOD("formatSize: {}\n", getFormatChannelNumber(img.eFormat));

    if (eLayout == IMAGE_LAYOUT::RAW)
    {
        auto* pGStr = getString(
            eLayout,
            img.pBuff,
            img.width,
            img.height,
            img.width * getFormatChannelNumber(img.eFormat),
            static_cast<ChafaPixelType>(formatToPixelType(img.eFormat)),
            widthCells,
            heightCells,
            cellWidth,
            cellHeight
        ).pGStr;

        defer( g_string_free(pGStr, true) );

        auto sRet = String(pAlloc, pGStr->str, pGStr->len);
        ADT_ASSERT(sRet.size() == (isize)pGStr->len, " ");

        return {
            .eLayout = IMAGE_LAYOUT::RAW,
            .uData {.sRaw = sRet},
            .width = widthCells,
            .height = heightCells,
        };
    }
    else
    {
        Span<gchar*> ppGStr = getString(
            eLayout,
            img.pBuff,
            img.width,
            img.height,
            img.width * getFormatChannelNumber(img.eFormat),
            static_cast<ChafaPixelType>(formatToPixelType(img.eFormat)),
            widthCells,
            heightCells,
            cellWidth,
            cellHeight
        ).sppLines;

        defer( g_strfreev(ppGStr.data()) );

        Vec<String> vLines(pAlloc, ppGStr.size());
        vLines.setSize(pAlloc, ppGStr.size());

        for (isize i = 0; i < vLines.size(); ++i)
            vLines[i] = String(pAlloc, ppGStr[i], strlen(ppGStr[i]));

        return {
            .eLayout = IMAGE_LAYOUT::LINES,
            .uData {.vLines = vLines},
            .width = widthCells,
            .height = heightCells,
        };
    }
}

} /* namespace platform::chafa */
