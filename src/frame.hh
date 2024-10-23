#pragma once

#include <threads.h>

namespace frame
{

extern cnd_t g_cndUpdate;

void run();

} /* namespace frame */
