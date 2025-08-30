#pragma once

namespace mpris
{

extern bool g_bReady;
extern adt::Mutex g_mtx;
extern adt::CndVar g_cnd;

#ifdef OPT_MPRIS

void init();
void proc();
void wakeUp() noexcept; /* Wake up from sd_bus_wait() */
void destroy();
adt::THREAD_STATUS pollLoop(void*) noexcept;

inline void initLocks() { g_mtx = adt::Mutex {adt::Mutex::TYPE::RECURSIVE}; g_cnd = adt::CndVar {adt::INIT}; }
inline void destroyLocks() { g_mtx.destroy(); g_cnd.destroy(); }

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
