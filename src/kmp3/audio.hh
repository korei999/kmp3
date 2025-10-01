#pragma once

#include "Image.hh"

namespace audio
{

constexpr isize RING_BUFFER_SIZE = 1 << 16; /* Big enough. */
constexpr isize RING_BUFFER_LOW_THRESHOLD = RING_BUFFER_SIZE * 0.25; /* Minimal size after which refillRingBufferLoop() kicks in. */
constexpr isize RING_BUFFER_HIGH_THRESHOLD = RING_BUFFER_SIZE * 0.75; /* refillRingBufferLoop() will keep filling until this. */
constexpr isize DRAIN_BUFFER_SIZE = 1 << 15; /* Usually its 1-2K frames*nChannels. */
extern f32 g_aDrainBuffer[DRAIN_BUFFER_SIZE];

enum class PCM_TYPE : u8 { S16, F32 };

/* NOTE: refillRingBufferLoop() thread will lock, push() and signal if pop() thread sits on the m_cnd (waiting for more data).
 * While pop() thread will signal if refillRingBufferLoop() thread is waiting on the same m_cnd. */
struct RingBuffer
{
    isize m_firstI {};
    isize m_lastI {};
    isize m_size {};
    isize m_cap {};
    f32* m_pData {};

    Mutex m_mtx {};
    CndVar m_cnd {};
    Thread m_thrd {};
    atomic::Bool m_atom_bLoopDone {false};
    bool m_bQuit = false;

    /* */

    RingBuffer() = default;
    RingBuffer(isize capacityPowOf2); /* capacityPowOf2 is rounded to next power of two */

    /* */

    void destroy() noexcept;
    isize push(const Span<const f32> sp) noexcept; /* Returns size after push. */
    isize pop(Span<f32> sp) noexcept;
    void clear() noexcept;
};

/* Platrform abstracted audio interface */
struct IMixer
{
    atomic::Bool m_atom_bPaused {false};
    atomic::Bool m_atom_bSongEnd {false};
    atomic::Bool m_atom_bDecodes {false};

    bool m_bMuted = false;
    bool m_bRunning = true;
    u32 m_sampleRate = 48000;
    u32 m_changedSampleRate = 48000;
    u8 m_nChannels = 2;
    int m_volume = 40;
    PCM_TYPE m_ePcmType = PCM_TYPE::F32;
    i64 m_currentTimeStamp {};
    i64 m_nTotalSamples {};
    f64 m_currMs {};

    RingBuffer m_ringBuff {};

    /* */

    virtual IMixer& init() = 0;
    virtual void deinit() = 0;
    virtual bool play(StringView svPath) = 0;
    virtual void pause(bool bPause) = 0;
    virtual void changeSampleRate(u64 sampleRate, bool bSave) = 0;

    /* */

    IMixer& start();
    void destroy() noexcept;
    void fillRingBuffer();
    bool isMuted() const;
    void toggleMute();
    void togglePause();
    u32 getSampleRate() const;
    u32 getChangedSampleRate() const;
    u8 getNChannels() const;
    u64 getTotalSamplesCount() const;
    u64 getCurrentTimeStamp() const;
    const atomic::Bool& isPaused() const;
    int getVolume() const;
    void volumeDown(const int step);
    void volumeUp(const int step);
    void setVolume(const int volume);
    [[nodiscard]] f64 calcCurrentMS();
    [[nodiscard]] f64 calcTotalMS();
    void changeSampleRateDown(int ms, bool bSave);
    void changeSampleRateUp(int ms, bool bSave);
    void restoreSampleRate();
    void seekMS(f64 ms);
    void seekOff(f64 offset);
    [[nodiscard]] i64 getCurrentMS();
    [[nodiscard]] i64 getTotalMS();

protected:
    IMixer& startDecoderThread();
    THREAD_STATUS refillRingBufferLoop();
    bool playFinal(StringView svPath);
};

struct DummyMixer : public IMixer
{
    virtual DummyMixer& init() override final { return *this; }
    virtual void deinit() override final {}
    virtual bool play(StringView) override final { return true; }
    virtual void pause(bool) override final {}
    virtual void changeSampleRate(u64, bool) override final {}
};

enum class ERROR : u8
{
    OK_ = 0,
    END_OF_FILE,
    DONE,
    FAIL,
};

constexpr StringView mapERRORToString[] {
    "OK_",
    "EOF_OF_FILE",
    "DONE",
    "FAIL",
};

struct IDecoder
{

    Mutex m_mtx {};

    /* */

    [[nodiscard]] virtual ERROR writeToRingBuffer(
        audio::RingBuffer* pRingBuff,
        const int nChannels,
        const PCM_TYPE ePcmType,
        long* pSamplesWritten,
        isize* pPcmPos
    ) = 0;

    virtual IDecoder& init() = 0;
    virtual void destroy() = 0;
    [[nodiscard]] virtual u32 getSampleRate() = 0;
    virtual void seekMS(f64 ms) = 0;
    [[nodiscard]] virtual i64 getCurrentSamplePos() = 0;
    [[nodiscard]] virtual i64 getCurrentMS() = 0;
    [[nodiscard]] virtual i64 getTotalMS() = 0;
    [[nodiscard]] virtual i64 getTotalSamplesCount() = 0;
    [[nodiscard]] virtual int getChannelsCount() = 0;
    [[nodiscard]] virtual StringView getMetadata(const StringView sKey) = 0;
    [[nodiscard]] virtual Image getCoverImage() = 0;
    [[nodiscard]] virtual ERROR open(StringView sPath) = 0;
    virtual void close() = 0;
};

} /* namespace audio */
