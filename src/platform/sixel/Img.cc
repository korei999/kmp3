#include "Img.hh"

#include "adt/logs.hh"
#include "adt/defer.hh"

#include <sixel.h>

using namespace adt;

namespace platform
{
namespace sixel
{

void
ImgDisplay(Img* s, const char* path)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_encoder_t *encoder = NULL;

    auto printError = [](int status) {
        LOG_BAD("{}\n{}\n",
            sixel_helper_format_error(status),
            sixel_helper_get_additional_message()
        );
    };

    status = sixel_encoder_new(&encoder, NULL);
    if (SIXEL_FAILED(status)) printError(status);
    defer( sixel_encoder_unref(encoder) );

    status = sixel_encoder_encode(encoder, path);
    if (SIXEL_FAILED(status)) printError(status);
}

} /* namespace sixel */
} /* namespace platform */
