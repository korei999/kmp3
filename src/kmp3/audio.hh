#pragma once

#include "Image.hh"

namespace audio
{

constexpr adt::isize RING_BUFFER_SIZE = 1 << 16; /* Big enough. */
constexpr adt::isize RING_BUFFER_LOW_THRESHOLD = RING_BUFFER_SIZE * 0.25; /* Minimal size after which refillRingBufferLoop() kicks in. */
constexpr adt::isize RING_BUFFER_HIGH_THRESHOLD = RING_BUFFER_SIZE * 0.75; /* refillRingBufferLoop() will keep filling until this. */
constexpr adt::isize DRAIN_BUFFER_SIZE = 1 << 15; /* Usually its 1-2K frames*nChannels. */
extern adt::f32 g_aDrainBuffer[DRAIN_BUFFER_SIZE];

enum class PCM_TYPE : adt::u8 { S16, F32 };

/* NOTE: refillRingBufferLoop() thread will lock, push() and signal if pop() thread sits on the m_cnd (waiting for more data).
 * While pop() thread will signal if refillRingBufferLoop() thread is waiting on the same m_cnd. */
struct RingBuffer
{
    adt::isize m_firstI {};
    adt::isize m_lastI {};
    adt::isize m_size {};
    adt::isize m_cap {};
    adt::f32* m_pData {};

    adt::Mutex m_mtx {};
    adt::CndVar m_cnd {};
    adt::Thread m_thrd {};
    adt::atomic::Bool m_atom_bLoopDone {false};
    bool m_bQuit = false;

    /* */

    RingBuffer() = default;
    RingBuffer(adt::isize capacityPowOf2); /* capacityPowOf2 is rounded to next power of two */

    /* */

    void destroy() noexcept;
    adt::isize push(const adt::Span<const adt::f32> sp) noexcept; /* Returns size after push. */
    adt::isize pop(adt::Span<adt::f32> sp) noexcept;
    void clear() noexcept;
};

/* Platrform abstracted audio interface */
struct IMixer
{
    adt::atomic::Bool m_atom_bPaused {false};
    adt::atomic::Bool m_atom_bSongEnd {false};
    adt::atomic::Bool m_atom_bDecodes {false};

    bool m_bMuted = false;
    bool m_bRunning = true;
    adt::u32 m_sampleRate = 48000;
    adt::u32 m_changedSampleRate = 48000;
    adt::u8 m_nChannels = 2;
    int m_volume = 40;
    PCM_TYPE m_ePcmType = PCM_TYPE::F32;
    adt::i64 m_currentTimeStamp {};
    adt::i64 m_nTotalSamples {};
    adt::f64 m_currMs {};

    RingBuffer m_ringBuff {};

    /* */

    virtual IMixer& init() = 0;
    virtual void deinit() = 0;
    virtual bool play(adt::StringView svPath) = 0;
    virtual void pause(bool bPause) = 0;
    virtual void togglePause() = 0;
    virtual void changeSampleRate(adt::u64 sampleRate, bool bSave) = 0;

    /* */

    IMixer& start();
    void destroy() noexcept;
    void fillRingBuffer();
    bool isMuted() const;
    void toggleMute();
    adt::u32 getSampleRate() const;
    adt::u32 getChangedSampleRate() const;
    adt::u8 getNChannels() const;
    adt::u64 getTotalSamplesCount() const;
    adt::u64 getCurrentTimeStamp() const;
    const adt::atomic::Bool& isPaused() const;
    int getVolume() const;
    void volumeDown(const int step);
    void volumeUp(const int step);
    void setVolume(const int volume);
    [[nodiscard]] adt::f64 calcCurrentMS();
    [[nodiscard]] adt::f64 calcTotalMS();
    void changeSampleRateDown(int ms, bool bSave);
    void changeSampleRateUp(int ms, bool bSave);
    void restoreSampleRate();
    void seekMS(adt::f64 ms);
    void seekOff(adt::f64 offset);
    [[nodiscard]] adt::i64 getCurrentMS();
    [[nodiscard]] adt::i64 getTotalMS();

protected:
    IMixer& startDecoderThread();
    adt::THREAD_STATUS refillRingBufferLoop();
    bool play2(adt::StringView svPath);
};

struct DummyMixer : public IMixer
{
    virtual DummyMixer& init() override final { return *this; }
    virtual void deinit() override final {}
    virtual bool play(adt::StringView) override final { return true; }
    virtual void pause(bool) override final {}
    virtual void togglePause() override final {}
    virtual void changeSampleRate(adt::u64, bool) override final {}
};

enum class ERROR : adt::u8
{
    OK_ = 0,
    END_OF_FILE,
    DONE,
    FAIL,
};

constexpr adt::StringView mapERRORToString[] {
    "OK_",
    "EOF_OF_FILE",
    "DONE",
    "FAIL",
};

struct IDecoder
{

    adt::Mutex m_mtx {};

    /* */

    [[nodiscard]] virtual ERROR writeToRingBuffer(
        audio::RingBuffer* pRingBuff,
        const int nChannels,
        const PCM_TYPE ePcmType,
        long* pSamplesWritten,
        adt::isize* pPcmPos
    ) = 0;

    virtual IDecoder& init() = 0;
    virtual void destroy() = 0;
    [[nodiscard]] virtual adt::u32 getSampleRate() = 0;
    virtual void seekMS(adt::f64 ms) = 0;
    [[nodiscard]] virtual adt::i64 getCurrentSamplePos() = 0;
    [[nodiscard]] virtual adt::i64 getCurrentMS() = 0;
    [[nodiscard]] virtual adt::i64 getTotalMS() = 0;
    [[nodiscard]] virtual adt::i64 getTotalSamplesCount() = 0;
    [[nodiscard]] virtual int getChannelsCount() = 0;
    [[nodiscard]] virtual adt::StringView getMetadata(const adt::StringView sKey) = 0;
    [[nodiscard]] virtual Image getCoverImage() = 0;
    [[nodiscard]] virtual ERROR open(adt::StringView sPath) = 0;
    virtual void close() = 0;
};

} /* namespace audio */
