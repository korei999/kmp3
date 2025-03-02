#pragma once

#include "adt/types.hh"

using namespace adt;

namespace defaults
{

constexpr f32 MAX_VOLUME = 1.5f; /* (0.0f, 1.0f], > 1.0f might cause distortion */
constexpr f32 VOLUME = 0.4f; /* startup volume */
constexpr int UPDATE_RATE = 500; /* ui update rate (ms) */
constexpr int IMAGE_UPDATE_RATE_LIMIT = 300; /* (ms). Is also limited by UPDATE_RATE. */
constexpr int READ_TIMEOUT = 5000; /* string input timeout (ms) */
constexpr f64 MPRIS_UPDATE_RATE = 100.0; /* delay between mpris polls (ms) */
constexpr u64 MIN_SAMPLE_RATE = 1000;
constexpr u64 MAX_SAMPLE_RATE = 9999999;
constexpr f64 FONT_ASPECT_RATIO = 1.0 / 2.0; /* typical monospaced font is 1/2 or 3/5 (width/height) */
constexpr int MOUSE_STEP = 4;
constexpr u8 IMAGE_HEIGHT = 11; /* terminal rows height */
constexpr u8 MIN_IMAGE_HEIGHT = 10;
constexpr u8 MAX_IMAGE_HEIGHT = 30;
constexpr f64 DOUBLE_CLICK_DELAY = 300.0;

} /* namespace defaults */
