#pragma once

#include <threads.h>

namespace mpris
{

extern bool g_bReady;
extern mtx_t g_mtx;
extern cnd_t g_cnd;

void init();
void proc();
void destroy();

inline void initMutexes() { mtx_init(&g_mtx, mtx_plain); cnd_init(&g_cnd); }
inline void destroyMutexes() { mtx_destroy(&g_mtx); cnd_destroy(&g_cnd); }

} /* namespace mpris */
