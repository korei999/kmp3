#include "audio.h"

#include "klib/Ctx.h"

#include "app.h"

#ifdef OPT_MPRIS
    #include "platform/mpris/mpris.h"
#endif

float audio_g_aDrainBuffer[audio_DRAIN_BUFFER_SIZE];

static bool audio_IMixerStartDecoderThread(audio_IMixer* s);
static K_THREAD_RESULT audio_refillRingBufferThreadTask(void* pArg);
static void audio_IMixerFillRingBuffer(audio_IMixer* s);

bool
audio_RingBufferInit(audio_RingBuffer* s, ssize_t capacityPowOf2)
{
    s->cap = k_nextPowerofTwo64(capacityPowOf2);
    s->pData = calloc(s->cap, sizeof(*s->pData));
    k_MutexInit(&s->mtx, K_MUTEX_TYPE_PLAIN);
    k_CndVarInit(&s->cnd);

    return true;
}

void
audio_RingBufferDestroy(audio_RingBuffer* s)
{
    while (!k_atomic_U8LoadRelaxed(&s->atom_bLoopDone))
        k_CndVarSignal(&s->cnd);

    k_ThreadJoin(&s->thrd);

    k_MutexDestroy(&s->mtx);
    k_CndVarDestroy(&s->cnd);
    free(s->pData);
}

ssize_t
audio_RingBufferPush(audio_RingBuffer* s, const float* pBuff, ssize_t buffSize)
{
    k_MutexLock(&s->mtx);

    ssize_t retSize = 0;

    if ((buffSize + s->size) > s->cap)
    {
        K_CTX_LOG_WARN("dropping out of range push (buffSize: {i64}, size: {i64}, cap: {i64})", buffSize, s->size, s->cap);
        retSize = buffSize;
        goto done;
    }

    const ssize_t nextLastI = (s->lastI + buffSize) & (s->cap - 1);

    const ssize_t nUntilEnd = K_MIN(s->cap - s->lastI, buffSize);
    memcpy(s->pData + s->lastI, pBuff, nUntilEnd * sizeof(*pBuff));
    memcpy(s->pData, pBuff + nUntilEnd, sizeof(*pBuff)*(buffSize - nUntilEnd));

    s->lastI = nextLastI;
    s->size += buffSize;
    k_CndVarSignal(&s->cnd);

    retSize = s->size;

done:
    k_MutexUnlock(&s->mtx);
    return retSize;
}

ssize_t
audio_RingBufferPop(audio_RingBuffer* s, float* pFillBuff, ssize_t fillBuffSize)
{
    k_MutexLock(&s->mtx);
    ssize_t retSize = 0;

    if (fillBuffSize > s->cap)
    {
        K_CTX_LOG_WARN("dropping some size: {sz} to {sz}", fillBuffSize, s->cap);
        fillBuffSize = s->cap;
    }

    while (!s->bQuit && fillBuffSize > s->size)
    {
        K_CTX_LOG_WARN("waiting for: {sz}, (size: {sz})", fillBuffSize, s->size);
        k_CndVarWait(&s->cnd, &s->mtx);
    }

    if (s->bQuit)
    {
        retSize = 0;
        goto done;
    }

    const ssize_t nextFirstI = (s->firstI + fillBuffSize) & (s->cap - 1);

    const ssize_t nUntilEnd = K_MIN(s->cap - s->firstI, fillBuffSize);
    memcpy(pFillBuff, s->pData + s->firstI, nUntilEnd * sizeof(*pFillBuff));
    memcpy(pFillBuff + nUntilEnd, s->pData, (fillBuffSize - nUntilEnd)*sizeof(*pFillBuff));

    s->firstI = nextFirstI;
    s->size -= fillBuffSize;
    if (s->size < audio_RING_BUFFER_LOW_THRESHOLD)
        k_CndVarSignal(&s->cnd);

done:
    k_MutexUnlock(&s->mtx);
    return retSize;
}

void
audio_RingBufferClear(audio_RingBuffer* s)
{
    k_MutexLock(&s->mtx);
    s->firstI = s->lastI = s->size = 0;
    k_MutexUnlock(&s->mtx);
}

static bool
audio_IMixerStartDecoderThread(audio_IMixer* s)
{
    if (!audio_RingBufferInit(&s->ring, audio_RING_BUFFER_SIZE))
        return false;

    if (!k_ThreadInit(&s->ring.thrd, audio_refillRingBufferThreadTask, s))
    {
        audio_RingBufferDestroy(&s->ring);
        return false;
    }

    return true;
}

static K_THREAD_RESULT
audio_refillRingBufferThreadTask(void* pArg)
{
    audio_IMixer* s = pArg;
    k_CtxInitArenaForThisThread(K_SIZE_1M);

    K_CTX_LOG_DEBUG("refill loop started");

    while (s->bRunning)
    {
        k_MutexLock(&s->ring.mtx);
        {
            while (!s->ring.bQuit && s->ring.size >= audio_RING_BUFFER_LOW_THRESHOLD)
                k_CndVarWait(&s->ring.cnd, &s->ring.mtx);

            if (s->ring.bQuit)
            {
                k_MutexUnlock(&s->ring.mtx);
                break;
            }
        }
        k_MutexUnlock(&s->ring.mtx);

        audio_IMixerFillRingBuffer(s);
    }

    k_atomic_U8StoreRelaxed(&s->ring.atom_bLoopDone, true);
    K_CTX_LOG_DEBUG("refill loop done");
    k_CtxDestroyArenaForThisThread();
    return (K_THREAD_RESULT){0};
}

static void
audio_IMixerFillRingBuffer(audio_IMixer* s)
{
    K_MUTEX_LOCK_SCOPE(&app_g_decoder.mtx)
    {
        if (!k_atomic_U8LoadRelaxed(&s->atom_bDecodes))
            continue;

        ssize_t nSamplesWritten = 0;
        FFMPEG_ERROR err = ffmpeg_DecoderWriteToRing(
            &app_g_decoder,
            &s->ring,
            s->nChannels,
            &nSamplesWritten,
            &s->currentTimeStamp
        );
        s->currMs = ffmpeg_DecoderGetCurrentMS(&app_g_decoder);

        if (err == FFMPEG_ERROR_END_OF_FILE)
        {
            audio_IMixerPause(s, true);
            ffmpeg_DecoderClose(&app_g_decoder);
            audio_RingBufferClear(&s->ring);
            k_atomic_U8StoreRelaxed(&s->atom_bDecodes, false);
            k_atomic_U8StoreRelease(&s->atom_bSongEnd, true);
        }
    }
}

bool
audio_IMixerPlayFinal(audio_IMixer* s, k_StringView svPath)
{
    k_MutexLock(&app_g_decoder.mtx);

    bool bRet = true;
    FFMPEG_ERROR err;

    if (k_atomic_U8LoadAcquire(&s->atom_bDecodes))
        ffmpeg_DecoderClose(&app_g_decoder);

    audio_RingBufferClear(&s->ring);
    s->currentTimeStamp = s->nTotalSamples = s->currMs = 0;

    if ((err = ffmpeg_DecoderOpen(&app_g_decoder, svPath)) != FFMPEG_ERROR_OK)
    {
        bRet = false;
        goto done;
    }

    s->nTotalSamples = ffmpeg_DecoderGetTotalSamplesCount(&app_g_decoder);
    k_atomic_U8StoreRelaxed(&s->atom_bDecodes, true);
    k_CndVarSignal(&s->ring.cnd);

done:
    k_MutexUnlock(&app_g_decoder.mtx);
    return bRet;
}

bool
audio_IMixerStart(audio_IMixer* s)
{
    s->bMuted = false;
    s->bRunning = true;
    s->sampleRate = s->changedSampleRate = 48000;
    s->nChannels = 2;

    s->volume = app_g_config.volume;

    if (!audio_IMixerStartDecoderThread(s))
    {
        return false;
    }

    if (!audio_IMixerInit(s))
    {
        return false;
    }

    return true;
}

void
audio_IMixerDestroy(audio_IMixer* s)
{
    if (!s) return;

    s->bRunning = false;

    k_MutexLock(&s->ring.mtx);
    {
        s->ring.bQuit = true;
        k_CndVarSignal(&s->ring.cnd);
    }
    k_MutexUnlock(&s->ring.mtx);
    audio_RingBufferDestroy(&s->ring);
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

uint8_t
audio_IMixerGetNChannels(const audio_IMixer* s)
{
    return s->nChannels;
}

uint64_t
audio_IMixerGetTotalSamplesCount(const audio_IMixer* s)
{
    return s->nTotalSamples;
}

int
audio_IMixerGetVolume(const audio_IMixer* s)
{
    return s->volume;
}

void
audio_IMixerSetVolume(audio_IMixer* s, const int volume)
{
    s->volume = K_CLAMP(volume, 0, app_g_config.maxVolume);
    K_CTX_LOG_INFO("volume: {i}", s->volume);

#ifdef OPT_MPRIS
    platform_mpris_volumeChanged();
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

double
audio_IMixerCalcCurrentMS(const audio_IMixer* s)
{
    return ((double)s->currentTimeStamp / (double)s->sampleRate / (double)s->nChannels) * 1000.0;
}

double
audio_IMixerCalcTotalMS(const audio_IMixer* s)
{
    return ((double)s->nTotalSamples / (double)s->sampleRate / (double)s->nChannels) * 1000.0;
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
    K_MUTEX_LOCK_SCOPE(&app_g_decoder.mtx)
    {
        if (!k_atomic_U8LoadAcquire(&s->atom_bDecodes))
            continue;

        const double totalMS = ffmpeg_DecoderGetTotalMS(&app_g_decoder);
        ms = K_CLAMP(ms, 0.0, totalMS);
        audio_RingBufferClear(&s->ring);
        k_CndVarSignal(&s->ring.cnd);
        ffmpeg_DecoderSeekMS(&app_g_decoder, ms);

        s->currMs = ms;
        s->currentTimeStamp = (ms * s->sampleRate * s->nChannels) / 1000.0;
        s->nTotalSamples = ffmpeg_DecoderGetTotalSamplesCount(&app_g_decoder);
    }

#ifdef OPT_MPRIS
    platform_mpris_seeked();
#endif
}

void
audio_IMixerSeekOff(audio_IMixer* s, double offset)
{
    double time = ffmpeg_DecoderGetCurrentMS(&app_g_decoder) + offset;
    audio_IMixerSeekMS(s, time);
}

int64_t
audio_IMixerGetCurrentMS(const audio_IMixer* s)
{
    return s->currMs;
}

int64_t
audio_IMixerGetTotalMS(const audio_IMixer* s)
{
    k_MutexLock(&app_g_decoder.mtx);
    int64_t ret = ffmpeg_DecoderGetTotalMS(&app_g_decoder);
    k_MutexUnlock(&app_g_decoder.mtx);
    return ret;
}
