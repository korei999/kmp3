#pragma once

#include "adt/Allocator.hh"

using namespace adt;

namespace platform
{

void TermboxInit();
void TermboxStop();
void TermboxProcEvents(Allocator* pAlloc);
void TermboxRender(Allocator* pAlloc);

}
