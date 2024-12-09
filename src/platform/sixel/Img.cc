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

        case ffmpeg::PIXEL_FORMAT::RGB888: return SIXEL_PIXELFORMAT_RGB888;
        case ffmpeg::PIXEL_FORMAT::RGB555: return SIXEL_PIXELFORMAT_RGB555;
        case ffmpeg::PIXEL_FORMAT::RGBA8888: return SIXEL_PIXELFORMAT_RGBA8888;
        /*case ffmpeg::PIXEL_FORMAT::RGB565: return SIXEL_PIXELFORMAT_RGB565;*/
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
    SIXELSTATUS status = SIXEL_FALSE;

    status = sixel_encoder_new(&s->pEncoder, {});
    if (SIXEL_FAILED(status)) printError(status);

    sixel_output_new(&s->pOutput, write, stdout, {});
    s->pDither = sixel_dither_get(SIXEL_BUILTIN_XTERM16);

    sixel_output_set_palette_type(s->pOutput, PALETTETYPE_RGB);
    sixel_output_set_8bit_availability(s->pOutput, 0);
    sixel_output_set_skip_dcs_envelope(s->pOutput, 0);
    sixel_output_set_encode_policy(s->pOutput, SIXEL_ENCODEPOLICY_FAST);
}

void
ImgDestroy(Img* s)
{
    sixel_encoder_unref(s->pEncoder);
    sixel_output_destroy(s->pOutput);
    sixel_dither_destroy(s->pDither);
}

void
ImgPrintBytes(Img* s, ffmpeg::Image img)
{
    auto format = convertFormat(img.eFormat);
    if (format == -1) return;

    /*sixel_encoder_encode_bytes(*/
    /*    s->pEncoder,*/
    /*    img.pBuff,*/
    /*    img.width,*/
    /*    img.height,*/
    /*    format,*/
    /*    SIXEL_PALETTETYPE_AUTO,*/
    /*    -1*/
    /*);*/

    sixel_frame_t* pFrame {};
    sixel_frame_new(&pFrame, {});
    /*defer( sixel_frame_unref(pFrame) );*/

    /*auto* pp = ::calloc(1, img.height * img.width * 8);*/
    /*defer( ::free(pp) );*/
    /*memcpy(pp, img.pBuff, img.width * img.height);*/

    sixel_frame_init(
        pFrame,
        img.pBuff,
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

    sixel_frame_clip(pFrame, 0, 0, width, height);

    sixel_dither_set_pixelformat(s->pDither, format);

    sixel_encode(
        pPix,
        width,
        height,
        0,
        s->pDither,
        s->pOutput
    );

    /*defer( sixel_frame_unref(pFrame) );*/
}

} /* namespace sixel */
} /* namespace platform */
