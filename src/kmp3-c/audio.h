#pragma once

#include "klib/Arena.h"
#include "klib/IAllocator.h"
#include "klib/StringView.h"
#include "klib/Thread.h"

static const ssize_t audio_RING_BUFFER_SIZE = 1 << 16; /* Big enough. */
static const ssize_t audio_RING_BUFFER_LOW_THRESHOLD = audio_RING_BUFFER_SIZE * 0.25; /* Minimal size after which refillRingBufferLoop() kicks in. */
static const ssize_t audio_RING_BUFFER_HIGH_THRESHOLD = audio_RING_BUFFER_SIZE * 0.75; /* refillRingBufferLoop() will keep filling until this. */
#define audio_DRAIN_BUFFER_SIZE (1 << 15) /* Usually its 1-2K frames*nChannels. */

extern float audio_g_aDrainBuffer[audio_DRAIN_BUFFER_SIZE];

typedef struct
{
    _Alignas(K_CACHE_SIZE) k_atomic_U64 atomHeadI;
    uint64_t capMinusOne;
    float* pData;
    _Alignas(K_CACHE_SIZE) k_atomic_U64 atomTailI;
} audio_RingSPSC;

bool audio_RingSPSCInit(audio_RingSPSC* s, k_IAllocator* pAlloc, ssize_t cap); /* False on allocation failure. */
void audio_RingSPSCDestroy(audio_RingSPSC* s, k_IAllocator* pAlloc);
bool audio_RingSPSCPush(audio_RingSPSC* s, const float* p, ssize_t size); /* False if atomic push failed. */
ssize_t audio_RingSPSCPop(audio_RingSPSC* s, float* pFill, ssize_t fillBuffSize); /* Returns nFloats written. */

typedef struct audio_IMixer audio_IMixer;
struct audio_IMixer
{
    struct
    {
        bool (*init)(audio_IMixer* s);
        void (*deinit)(audio_IMixer* s);
        bool (*play)(audio_IMixer* s, k_StringView svPath);
        void (*pause)(audio_IMixer* s, bool bPause);
        void (*changeSampleRate)(audio_IMixer* s, uint64_t sampleRate, bool bSave);
    } vtable;

    audio_RingSPSC ring;
    k_Thread thread;
    k_Arena threadArena; /* Used only by 'thread'. */

    k_atomic_U8 atom_bPaused;
    k_atomic_U8 atom_bSongEnd;
    k_atomic_U8 atom_bDecodes;
    k_atomic_U8 atom_bLoopDone;

    bool bMuted;
    bool bRunning;
    uint32_t sampleRate;
    uint32_t changedSampleRate;
    uint8_t nChannels;
    int volume;
    int64_t currentTimeStamp;
    int64_t nTotalSamples;
    double currMs;
};

static inline bool audio_IMixerInit(audio_IMixer* s) { return s->vtable.init(s); }
static inline void audio_IMixerDeinit(audio_IMixer* s) { s->vtable.deinit(s); }
static inline bool audio_IMixerPlay(audio_IMixer* s, k_StringView svPath) { return s->vtable.play(s, svPath); }
static inline void audio_IMixerPause(audio_IMixer* s, bool bPause) { s->vtable.pause(s, bPause); }
static inline void audio_IMixerChangeSampleRate(audio_IMixer* s, uint64_t sampleRate, bool bSave) { s->vtable.changeSampleRate(s, sampleRate, bSave); }

bool audio_IMixerStart(audio_IMixer* s); /* vtable.init(); */
void audio_IMixerDestroy(audio_IMixer* s); /* vtable.deinit(). */
bool audio_IMixerIsMuted(const audio_IMixer* s);
void audio_IMixerToggleMute(audio_IMixer* s);
uint8_t audio_IMixerGetNChannels(const audio_IMixer* s);
uint64_t audio_IMixerGetTotalSamplesCount(const audio_IMixer* s);
int audio_IMixerGetVolume(const audio_IMixer* s);
void audio_IMixerSetVolume(audio_IMixer* s, const int volume);
void audio_IMixerVolumeDown(audio_IMixer* s, const int step);
void audio_IMixerVolumeUp(audio_IMixer* s, const int step);
K_NO_DISCARD double audio_IMixerCalcCurrentMS(const audio_IMixer* s);
K_NO_DISCARD double audio_IMixerCalcTotalMS(const audio_IMixer* s);
void audio_IMixerRestoreSampleRate(audio_IMixer* s);
void audio_IMixerChangeSampleRateDown(audio_IMixer* s, int ms, bool bSave);
void audio_IMixerChangeSampleRateUp(audio_IMixer* s, int ms, bool bSave);
void audio_IMixerSeekMS(audio_IMixer* s, double ms);
void audio_IMixerSeekOff(audio_IMixer* s, double offset);
K_NO_DISCARD int64_t audio_IMixerGetCurrentMS(const audio_IMixer* s);
K_NO_DISCARD int64_t audio_IMixerGetTotalMS(const audio_IMixer* s);

typedef struct audio_DummyMixer audio_DummyMixer;
struct audio_DummyMixer
{
    union
    {
        audio_IMixer;
        audio_IMixer base;
    };
};

static inline audio_DummyMixer audio_DummyMixerCreate(void);
static inline bool audio_DummyMixerInit(audio_DummyMixer* s) { *s = audio_DummyMixerCreate(); return true; }
static inline void audio_DummyMixerDeinit(audio_DummyMixer* s) { ; }
static inline bool audio_DummyMixerPlay(audio_DummyMixer* s, k_StringView svPath) { (void)s, (void)svPath; return true; }
static inline void audio_DummyMixerPause(audio_DummyMixer* s, bool bPause) { (void)s, (void)bPause; }
static inline void audio_DummyMixerChangeSampleRate(audio_DummyMixer* s, uint64_t sampleRate, bool bSave) { (void)s, (void)sampleRate, (void)bSave; }

static inline audio_DummyMixer
audio_DummyMixerCreate(void)
{
    audio_DummyMixer s = {0};
    s.vtable.init = (K_TYPEOF(s.base.vtable.init))audio_DummyMixerInit;
    s.vtable.deinit = (K_TYPEOF(s.base.vtable.deinit))audio_DummyMixerDeinit;
    s.vtable.play = (K_TYPEOF(s.base.vtable.play))audio_DummyMixerPlay;
    s.vtable.pause = (K_TYPEOF(s.base.vtable.pause))audio_DummyMixerPause;
    s.vtable.changeSampleRate = (K_TYPEOF(s.base.vtable.changeSampleRate))audio_DummyMixerChangeSampleRate;
    return s;
}
