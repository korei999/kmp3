#include "audio.h"

#include "klib/Ctx.h"
#include "klib/Gpa.h"

#include "app.h"

float audio_g_aDrainBuffer[audio_DRAIN_BUFFER_SIZE];

static bool audio_IMixerStartDecoderThread(audio_IMixer* s);
static K_THREAD_RESULT audio_refillRingBufferThreadTask(void* pArg);

bool
audio_RingSPSCInit(audio_RingSPSC* s, k_IAllocator* pAlloc, ssize_t cap)
{
    *s = (audio_RingSPSC){0};

    cap = k_nextPowerofTwo64(cap);
    if (cap <= 0) return false;

    float* pNew = k_IAllocatorZalloc(pAlloc, cap * sizeof(*s->pData));
    if (!pNew) return false;

    s->pData = pNew;
    s->capMinusOne = cap - 1;

    return true;
}

void
audio_RingSPSCDestroy(audio_RingSPSC* s, k_IAllocator* pAlloc)
{
    k_IAllocatorFree(pAlloc, s->pData);
    *s = (audio_RingSPSC){0};
}

bool
audio_RingSPSCPush(audio_RingSPSC* s, const float* p, ssize_t size)
{
    const uint64_t tailI = k_atomic_U64LoadRelaxed(&s->atomTailI);
    const uint64_t headI = k_atomic_U64LoadAcquire(&s->atomHeadI);
    const uint64_t availableSize = s->capMinusOne - (tailI - headI); /* Leave 1 to avoid overlapping tail with head on full queue. */

    if (availableSize < size) return false;

    const uint64_t realTailI = tailI & s->capMinusOne;
    const uint64_t nUntilEnd = K_MIN((s->capMinusOne + 1) - realTailI, size);
    memcpy(s->pData + realTailI, p, nUntilEnd);
    memcpy(s->pData, p + nUntilEnd, size - nUntilEnd);

    k_atomic_U64AddRelease(&s->atomTailI, size);

    return true;
}

ssize_t
audio_RingSPSCPop(audio_RingSPSC* s, float* pFill, ssize_t fillBuffSize)
{
    const uint64_t tailI = k_atomic_U64LoadRelaxed(&s->atomTailI);
    const uint64_t headI = k_atomic_U64LoadAcquire(&s->atomHeadI);
    const uint64_t ringSize = tailI - headI;

    if (ringSize == 0) return 0;

    fillBuffSize = K_MIN(fillBuffSize, ringSize);

    const uint64_t realHeadI = headI & s->capMinusOne;
    const uint64_t nUntilEnd = K_MIN((s->capMinusOne + 1) - realHeadI, fillBuffSize);
    memcpy(pFill, s->pData + realHeadI, nUntilEnd);
    memcpy(pFill + nUntilEnd, s->pData, fillBuffSize - nUntilEnd);

    k_atomic_U64AddRelease(&s->atomHeadI, fillBuffSize);

    return fillBuffSize;
}

static bool
audio_IMixerStartDecoderThread(audio_IMixer* s)
{
    if (!audio_RingSPSCInit(&s->ring, &k_GpaInst()->base, audio_RING_BUFFER_SIZE))
        return false;
    if (!k_ArenaInit(&s->threadArena, K_SIZE_1M, 0))
    {
        audio_RingSPSCDestroy(&s->ring, &k_GpaInst()->base);
        return false;
    }

    if (!k_ThreadInit(&s->thread, audio_refillRingBufferThreadTask, s))
    {
        audio_RingSPSCDestroy(&s->ring, &k_GpaInst()->base);
        k_ArenaDestroy(&s->threadArena);
        return false;
    }

    return true;
}

static K_THREAD_RESULT
audio_refillRingBufferThreadTask(void* pArg)
{
    audio_IMixer* s = pArg;

    k_atomic_U8StoreRelaxed(&s->atom_bLoopDone, true);
    return (K_THREAD_RESULT){0};
}

bool
audio_IMixerStart(audio_IMixer* s)
{
    if (!audio_IMixerStartDecoderThread(s))
    {
        return false;
    }

    if (!audio_IMixerInit(s))
    {
        // stop decoder thread
        return false;
    }

    return true;
}

void
audio_IMixerDestroy(audio_IMixer* s)
{
    audio_IMixerDeinit(s);
}

bool
audio_IMixerIsMuted(const audio_IMixer* s)
{
    return s->bMuted;
}

void
audio_IMixerToggleMute(audio_IMixer* s)
{
    s->bMuted ^= 1;
}

void
audio_IMixerSetVolume(audio_IMixer* s, const int volume)
{
    s->volume = K_CLAMP(volume, 0, app_g_config.maxVolume);
    K_CTX_LOG_INFO("volume: {i}", s->volume);
#ifdef OPT_MPRIS
    mpris::volumeChanged();
#endif
}

void
audio_IMixerVolumeDown(audio_IMixer* s, const int step)
{
    audio_IMixerSetVolume(s, s->volume - step);
}

void
audio_IMixerVolumeUp(audio_IMixer* s, const int step)
{
    audio_IMixerSetVolume(s, s->volume + step);
}

void
audio_IMixerRestoreSampleRate(audio_IMixer* s)
{
    audio_IMixerChangeSampleRate(s, s->sampleRate, false);
}

void
audio_IMixerChangeSampleRateDown(audio_IMixer* s, int ms, bool bSave)
{
    audio_IMixerChangeSampleRate(s, s->changedSampleRate - ms, bSave);
}

void
audio_IMixerChangeSampleRateUp(audio_IMixer* s, int ms, bool bSave)
{
    audio_IMixerChangeSampleRate(s, s->changedSampleRate + ms, bSave);
}

void
audio_IMixerSeekMS(audio_IMixer* s, double ms)
{
}

void
audio_IMixerSeekOff(audio_IMixer* s, double offset)
{
}

K_NO_DISCARD int64_t
audio_IMixerGetCurrentMS(const audio_IMixer* s)
{
    return s->currMs;
}

K_NO_DISCARD int64_t
audio_IMixerGetTotalMS(const audio_IMixer* s)
{
    return 0;
}
