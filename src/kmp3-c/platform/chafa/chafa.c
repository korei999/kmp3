#include "chafa.h"

#include "Window.h"
#include "app.h"

#include "klib/Ctx.h"
#include "klib/assert.h"

/* https://github.com/hpjansson/chafa/blob/master/examples/adaptive.c */

typedef union
{
    GString* pGStr;
    struct
    {
        gchar** ppLines;
        ssize_t nLines;
    } sppLines;
} platform_chafa_StringLines;

K_NO_DISCARD static int
platform_chafa_formatToPixelType(const IMAGE_PIXEL_FORMAT eFormat)
{
    switch (eFormat)
    {
        case IMAGE_PIXEL_FORMAT_NONE: return -1;

        case IMAGE_PIXEL_FORMAT_RGB8: return CHAFA_PIXEL_RGB8;

        case IMAGE_PIXEL_FORMAT_RGBA8_PREMULTIPLIED:
        case IMAGE_PIXEL_FORMAT_RGBA8_UNASSOCIATED:
        return CHAFA_PIXEL_RGBA8_UNASSOCIATED;
    }

    return -1;
}

K_NO_DISCARD static int
platform_chafa_getFormatChannelNumber(const IMAGE_PIXEL_FORMAT eFormat)
{
    switch (eFormat)
    {
        case IMAGE_PIXEL_FORMAT_NONE: return 0;

        case IMAGE_PIXEL_FORMAT_RGBA8_UNASSOCIATED:
        case IMAGE_PIXEL_FORMAT_RGBA8_PREMULTIPLIED:
        return 4;

        case IMAGE_PIXEL_FORMAT_RGB8: return 3;
    }

    return 0;
}

K_NO_DISCARD static platform_chafa_StringLines
platform_chafa_getString(
    const platform_chafa_IMAGE_LAYOUT eLayout,
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

    platform_chafa_StringLines ret = {0};

    platform_chafa_detectTerminal(&pTermInfo, &mode, &ePixelMode);

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
    chafa_canvas_draw_all_pixels(pCanvas, ePixelType, (uint8_t*)pPixels, pixWidth, pixHeight, pixRowStride);

    if (eLayout == platform_chafa_IMAGE_LAYOUT_RAW)
    {
        /* Build printable strings */
        GString* pGStr = chafa_canvas_print(pCanvas, pTermInfo);

        ret.pGStr = pGStr;
    }
    else
    {
#ifdef OPT_CHAFA_SYMBOLS

        gchar** ppRows = chafa_canvas_print_rows_strv(pCanvas, pTermInfo);

        gint len = 0;
        for (; ppRows[len]; ++len)
            ;

        ret.sppLines.ppLines = ppRows;
        ret.sppLines.nLines = len;
#endif
    }

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

    /* BUG: fix for the chafa leak: https://github.com/hpjansson/chafa/commit/05e76092c459421131cca8d512df693d3fd98b99 */
    /* first 4 bytes is the ref count */
    int refs;
    memcpy(&refs, pTermInfo, sizeof(refs));
    chafa_term_info_unref(pTermInfo);
    if (refs >= 2) chafa_term_info_unref(pTermInfo);

    return ret;
}

platform_chafa_Image
platform_chafa_allocImage(k_IAllocator* pAlloc, platform_chafa_IMAGE_LAYOUT eLayout, const Image img, int termHeight, int termWidth)
{
    if (img.height <= 0 || img.width <= 0) return (platform_chafa_Image){0};

    WindowTermSize termSize = WindowTermSizeGet();

    platform_chafa_Image ret = {0};

    double fontRatio = app_g_config.fontAspectRatio;
    int cellWidth = -1, cellHeight = -1; /* Size of each character cell, in pixels */
    int widthCells = 0, heightCells = 0; /* Size of output image, in cells */

    if (termWidth > 0 && termHeight > 0 && termSize.pixWidth > 0 && termSize.pixHeight > 0)
    {
        cellWidth = termSize.pixWidth / termSize.width;
        cellHeight = termSize.pixHeight / termSize.height;
        fontRatio = (double)cellWidth / (double)cellHeight;
    }

    widthCells = termWidth;
    heightCells = termHeight;

    chafa_calc_canvas_geometry(
        img.width, img.height,
        &widthCells, &heightCells,
        fontRatio, true, false
    );
    if (eLayout == platform_chafa_IMAGE_LAYOUT_RAW)
    {
        GString* pGStr = platform_chafa_getString(
            eLayout,
            img.pBuff,
            img.width,
            img.height,
            img.width * platform_chafa_getFormatChannelNumber(img.eFormat),
            platform_chafa_formatToPixelType(img.eFormat),
            widthCells,
            heightCells,
            cellWidth,
            cellHeight
        ).pGStr;

        k_String sRet = k_StringCreate(pAlloc, pGStr->str, pGStr->len);
        K_ASSERT(k_StringSize(&sRet) == pGStr->len, " ");

        ret.eLayout = platform_chafa_IMAGE_LAYOUT_RAW;
        ret.uData.sRaw = sRet;
        ret.width = widthCells;
        ret.height = heightCells;

        g_string_free(pGStr, true);
    }
    else
    {
        platform_chafa_StringLines lines = platform_chafa_getString(
            eLayout,
            img.pBuff,
            img.width,
            img.height,
            img.width * platform_chafa_getFormatChannelNumber(img.eFormat),
            platform_chafa_formatToPixelType(img.eFormat),
            widthCells,
            heightCells,
            cellWidth,
            cellHeight
        );

        VecString vLines = {0};
        VecStringInit(&vLines, pAlloc, lines.sppLines.nLines);
        VecStringSetSize(&vLines, pAlloc, lines.sppLines.nLines);

        for (ssize_t i = 0; i < vLines.size; ++i)
        {
            k_String s = k_StringCreate(pAlloc, lines.sppLines.ppLines[i], strlen(lines.sppLines.ppLines[i]));
            VecStringSet(&vLines, i, &s);
        }

        ret.eLayout = platform_chafa_IMAGE_LAYOUT_LINES;
        ret.uData.vLines = vLines;
        ret.width = widthCells;
        ret.height = heightCells;

        g_strfreev(lines.sppLines.ppLines);
    }

    return ret;
}

void
platform_chafa_detectTerminal(ChafaTermInfo** ppTermInfo, ChafaCanvasMode* pMode, ChafaPixelMode* pPixelMode)
{
    ChafaCanvasMode mode;
    ChafaPixelMode pixelMode;
    ChafaTermInfo* pTermInfo;
    // ChafaTermInfo* pFallbackInfo;
    ChafaTermDb* pTermDb = chafa_term_db_new();

    gchar** ppEnv;

    /* Examine the environment variables and guess what the terminal can do */
    ppEnv = g_get_environ();

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

    chafa_term_db_unref(pTermDb);
    g_strfreev(ppEnv);
}
