#include "Img.hh"

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
    }
}

void
ImgInit(Img* s)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_encoder_t *encoder = NULL;

    status = sixel_encoder_new(&encoder, NULL);
    if (SIXEL_FAILED(status)) printError(status);

    s->pEncoder = encoder;
}

void
ImgDestroy(Img* s)
{
    sixel_encoder_unref(s->pEncoder);
}

void
ImgPrintBytes(Img* s, ffmpeg::Image img)
{
    auto format = convertFormat(img.eFormat);
    if (format == -1) return;

    sixel_encoder_encode_bytes(
        s->pEncoder,
        img.pBuff,
        img.width,
        img.height,
        format,
        SIXEL_QUALITY_AUTO,
        0
    );
}

} /* namespace sixel */
} /* namespace platform */
