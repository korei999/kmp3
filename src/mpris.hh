#pragma once

#include <threads.h>

namespace mpris
{

extern bool g_bReady;
extern mtx_t g_mtx;
extern cnd_t g_cnd;

#ifdef USE_MPRIS

void init();
void proc();
void destroy();

inline void initLocks() { mtx_init(&g_mtx, mtx_recursive); cnd_init(&g_cnd); }
inline void destroyLocks() { mtx_destroy(&g_mtx); cnd_destroy(&g_cnd); }

void playbackStatusChanged();
void loopStatusChanged();
void shuffleChanged();
void volumeChanged();
void seeked();
void metadataChanged();

#else

constexpr void init() {};
constexpr void proc() {};
constexpr void destroy() {};

constexpr void initMutexes() {}
constexpr void destroyMutexes() {}

constexpr void playbackStatusChanged() {};
constexpr void loopStatusChanged() {};
constexpr void shuffleChanged() {};
constexpr void volumeChanged() {};
constexpr void seeked() {};
constexpr void metadataChanged() {};

#endif


} /* namespace mpris */
