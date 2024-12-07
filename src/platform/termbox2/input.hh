#pragma once

struct tb_event;

namespace platform
{
namespace termbox2
{
namespace input
{

void procKey(tb_event* pEv);
void procMouse(tb_event* pEv);
void fillInputMap();

} /* namespace input */
} /* namespace termbox2 */
} /* namespace platform */
