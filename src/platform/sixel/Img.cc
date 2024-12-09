#include "Img.hh"

#include "adt/defer.hh"
#include "adt/logs.hh"

using namespace adt;

namespace platform
{
namespace sixel
{

static void
printError(int status)
{
    LOG_BAD("{}\n{}\n",
        sixel_helper_format_error(status),
        sixel_helper_get_additional_message()
    );
};

static int
convertFormat(ffmpeg::PIXEL_FORMAT eFormat)
{
    switch (eFormat)
    {
        default: return -1;

        case ffmpeg::PIXEL_FORMAT::RGB8: return SIXEL_PIXELFORMAT_RGB888;
        /*case ffmpeg::PIXEL_FORMAT::RGB555: return SIXEL_PIXELFORMAT_RGB555;*/
        case ffmpeg::PIXEL_FORMAT::RGBA8_UNASSOCIATED: return SIXEL_PIXELFORMAT_RGBA8888;
        /*case ffmpeg::PIXEL_FORMAT::RGB565: return SIXEL_PIXELFORMAT_RGB565;*/
    }
}

static int
bitSizeOfFormat(int format)
{
    switch (format)
    {
        default: return 0;
        case SIXEL_PIXELFORMAT_RGB888: return 24;
        case SIXEL_PIXELFORMAT_RGB555: return 15;
        case SIXEL_PIXELFORMAT_RGBA8888: return 32;
    }
}

static int
write(char* data, int size, void* priv)
{
    return fwrite(data, 1, size, (FILE*)priv);
}

void
ImgInit(Img* s)
{
}

void
ImgDestroy(Img* s)
{
}

void
ImgPrintBytes(Img* s, ffmpeg::Image img)
{
    SIXELSTATUS status = SIXEL_FALSE;

    status = sixel_encoder_new(&s->pEncoder, {});
    if (SIXEL_FAILED(status)) printError(status);

    int format = convertFormat(img.eFormat);
    if (format == -1) return;

    // sixel_encoder_encode_bytes(
    //     s->pEncoder,
    //     img.pBuff,
    //     img.width,
    //     img.height,
    //     format,
    //     SIXEL_PALETTETYPE_AUTO,
    //     -1
    // );

    u32 bitSize = bitSizeOfFormat(format);
    u8* pp = (u8*)::calloc(1, (img.height * img.width * bitSize) / 8);
    memcpy(pp, img.pBuff, (img.width * img.height * bitSize) / 8);

    sixel_dither_new(&s->pDither, -1, {});
    /*sixel_dither_set_pixelformat(s->pDither, format);*/

    sixel_dither_initialize(
        s->pDither,
        pp,
        img.width,
        img.height,
        format,
        SIXEL_LARGE_AUTO,
        SIXEL_REP_AUTO,
        SIXEL_QUALITY_AUTO
    );

    /*sixel_output_set_palette_type(s->pOutput, PALETTETYPE_RGB);*/
    /*sixel_output_set_8bit_availability(s->pOutput, 0);*/
    /*sixel_output_set_skip_dcs_envelope(s->pOutput, 0);*/
    /*sixel_output_set_encode_policy(s->pOutput, SIXEL_ENCODEPOLICY_FAST);*/

    sixel_frame_t* pFrame {};
    sixel_frame_new(&pFrame, {});
    defer( sixel_frame_unref(pFrame) );

    sixel_frame_init(
        pFrame,
        pp,
        img.width,
        img.height,
        format,
        SIXEL_PALETTETYPE_AUTO,
        -1
    );

    /*sixel_frame_resize(pFrame, img.width / 2, img.height / 2, SIXEL_RES_NEAREST);*/

    auto* pPix = sixel_frame_get_pixels(pFrame);
    auto width = sixel_frame_get_width(pFrame);
    auto height = sixel_frame_get_height(pFrame);

    sixel_output_new(&s->pOutput, write, stdout, {});

    sixel_encode(
        pPix,
        width,
        height,
        0,
        s->pDither,
        s->pOutput
    );

    sixel_output_destroy(s->pOutput);
    sixel_dither_destroy(s->pDither);
    sixel_encoder_unref(s->pEncoder);
}

} /* namespace sixel */
} /* namespace platform */
