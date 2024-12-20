#pragma once

#include "Win.hh"
#include "common.hh"

namespace platform
{
namespace ansi
{
namespace input
{

common::READ_STATUS readWChar(Win* s);
void procInput(Win* s);

} /* namespace input */
} /* namespace ansi */
} /* namespace platform */
