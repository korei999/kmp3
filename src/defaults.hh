#pragma once

namespace defaults
{

constexpr adt::f32 MAX_VOLUME = 1.5f; /* (0.0f, 1.0f], > 1.0f might cause distortion */
constexpr adt::f32 VOLUME = 0.4f; /* startup volume */
constexpr int UPDATE_RATE = 500; /* ui update rate (ms) */
constexpr int IMAGE_UPDATE_RATE_LIMIT = 100; /* (ms). Is also limited by UPDATE_RATE. */
constexpr int READ_TIMEOUT = 5000; /* string input timeout (ms) */
constexpr adt::f64 MPRIS_UPDATE_RATE = 100.0; /* delay between mpris polls (ms) */
constexpr adt::u64 MIN_SAMPLE_RATE = 1000;
constexpr adt::u64 MAX_SAMPLE_RATE = 9999999;
constexpr adt::f64 FONT_ASPECT_RATIO = 1.0 / 2.0; /* typical monospaced font is 1/2 or 3/5 (width/height) */
constexpr int MOUSE_STEP = 4;
constexpr adt::u8 IMAGE_HEIGHT = 11; /* terminal rows height */
constexpr adt::u8 MIN_IMAGE_HEIGHT = 10;
constexpr adt::u8 MAX_IMAGE_HEIGHT = 30;
constexpr adt::f64 DOUBLE_CLICK_DELAY = 350.0;

} /* namespace defaults */
