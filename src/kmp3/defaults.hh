#pragma once

namespace defaults
{

constexpr adt::f32 MAX_VOLUME = 1.5f; /* (0.0f, 1.0f], > 1.0f might cause distortion. */
constexpr adt::f32 VOLUME = 0.4f; /* Startup volume. */
constexpr int UPDATE_RATE = 500; /* Ui update rate (ms). */
constexpr int IMAGE_UPDATE_RATE_LIMIT = 100; /* (ms). Is also limited by UPDATE_RATE. */
constexpr int READ_TIMEOUT = 5000; /* String input timeout (ms). */
constexpr adt::u64 MIN_SAMPLE_RATE = 1000;
constexpr adt::u64 MAX_SAMPLE_RATE = 9999999;
constexpr adt::f64 FONT_ASPECT_RATIO = 1.0 / 2.0; /* Typical monospaced font is 1/2 or 3/5 (width/height). */
constexpr int MOUSE_STEP = 4;
constexpr adt::u8 IMAGE_HEIGHT = 11; /* Terminal rows height. */
constexpr adt::u8 MIN_IMAGE_HEIGHT = 10;
constexpr adt::u8 MAX_IMAGE_HEIGHT = 30;
constexpr adt::f64 DOUBLE_CLICK_DELAY = 350.0;
constexpr const char* MPRIS_NAME = "a_kmp3"; /* Using 'a' to top kmp3 instance in playerctl. */

} /* namespace defaults */
