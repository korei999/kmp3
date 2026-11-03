#pragma once

#include "klib/Thread.h"

extern bool platform_mpris_g_bReady;
extern k_Mutex platform_mpris_g_mtx;
extern k_CndVar platform_mpris_g_cnd;

bool platform_mpris_init(void);
void platform_mpris_deinit(void);
void platform_mpris_proc(void);
void platform_mpris_wakeUp(void); /* Wake up from sd_bus_wait() */
K_THREAD_RESULT platform_mpris_pollLoop(void*);

void platform_mpris_initLocks(void);
void platform_mpris_destroyLocks(void);

void platform_mpris_playbackStatusChanged(void);
void platform_mpris_loopStatusChanged(void);
void platform_mpris_shuffleChanged(void);
void platform_mpris_volumeChanged(void);
void platform_mpris_seeked(void);
void platform_mpris_metadataChanged(void);
