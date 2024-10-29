#pragma once

#include "adt/types.hh"

using namespace adt;

namespace defaults
{

constexpr f32 MAX_VOLUME = 1.2f; /* [0.0f, 1.0f], > 1.0f will cause distortion */
constexpr f32 VOLUME = 0.4f; /* startup volume */
constexpr int UPDATE_RATE = 500; /* ui update rate, in ms */
constexpr int READ_TIMEOUT = 5000; /* string input timeout, in ms */

} /* namespace defaults */
