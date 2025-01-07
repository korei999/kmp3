#pragma once

struct tb_event;

namespace platform::termbox2::input
{

void procKey(tb_event* pEv);
void procMouse(tb_event* pEv);
void fillInputMap();

} /* namespace platform::termbox2::input */
