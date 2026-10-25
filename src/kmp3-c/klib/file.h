#pragma once

#include "IAllocator.h"
#include "Span.h"
#include "String.h"

bool k_file_isatty(int fd);
K_NO_DISCARD k_Span k_file_load(k_IAllocator* pAlloc, const char* ntsPath);
ssize_t k_file_write(int fd, void* pBuff, ssize_t buffSize);
k_String k_file_cwd(k_IAllocator* pAlloc);
