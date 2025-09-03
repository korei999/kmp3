#pragma once

#include "IWindow.hh"
#include "Player.hh"
#include "audio.hh"
#include "defaults.hh"
#include "config.hh"

#include "platform/ffmpeg/Decoder.hh"

namespace app
{

enum class UI : adt::u8
{
    DUMMY,
    ANSI,
};

enum class MIXER : adt::u8
{
    DUMMY,

    ALSA,
    PIPEWIRE,
    COREAUDIO,
    SNDIO,
};

enum class TERM : adt::u8 { ELSE, XTERM, XTERM_256COLOR, KITTY, FOOT, GHOSTTY, ALACRITTY };


extern adt::ILogger::LEVEL g_eLogLevel;
extern bool g_bForceLoggerColors;

extern UI g_eUIFrontend;
extern MIXER g_eMixer;
extern adt::StringView g_svTerm;
extern TERM g_eTerm;
extern volatile bool g_bRunning;
extern bool g_bNoImage;
extern bool g_bSixelOrKitty;
extern bool g_bChafaSymbols;

extern IWindow* g_pWin;
extern Config g_config;
extern Player* g_pPlayer;
extern audio::IMixer* g_pMixer;
extern platform::ffmpeg::Decoder g_decoder;

inline Player& player() { return *g_pPlayer; }
inline audio::IMixer& mixer() { return *g_pMixer; }
inline platform::ffmpeg::Decoder& decoder() { return g_decoder; }
inline IWindow& window() { return *g_pWin; }

IWindow* allocWindow(adt::IAllocator* pArena);
audio::IMixer* allocMixer(adt::IAllocator* pAlloc);

inline void quit() { g_bRunning = false; }
inline void focusNext() { player().focusNext(); }
inline void focusPrev() { player().focusPrev(); }
inline void focusFirst() { player().focusFirst(); }
inline void focusLast() { player().focusLast(); }
inline void focus(long i) { player().focus(i); }
inline void focusUp(long step) { focus(player().m_focusedI - step); }
inline void focusDown(long step) { focus(player().m_focusedI + step); }
inline void selectFocused() { player().selectFocused(); }
inline void focusSelected() { player().focusSelected(); }
inline void focusSelectedAtCenter() { player().focusSelectedAtCenter(); }
inline void togglePause() { player().togglePause(); }
inline void volumeDown(const adt::f32 step) { g_pMixer->volumeDown(step); }
inline void volumeUp(const adt::f32 step) { g_pMixer->volumeUp(step); }
inline void changeSampleRateDown(adt::u64 ms, bool bSave) { g_pMixer->changeSampleRateDown(ms, bSave); }
inline void changeSampleRateUp(adt::u64 ms, bool bSave) { g_pMixer->changeSampleRateUp(ms, bSave); }
inline void restoreSampleRate() { g_pMixer->restoreSampleRate(); }
inline void seekOff(adt::f64 ms) { g_pMixer->seekOff(ms); }
inline PLAYER_REPEAT_METHOD cycleRepeatMethods(bool bForward) { return player().cycleRepeatMethods(bForward); }
inline void selectPrev() { player().selectPrev(); }
inline void selectNext() { player().selectNext(); }
inline void toggleMute() { g_pMixer->toggleMute(); }
inline void seekFromInput() { g_pWin->seekFromInput(); }
inline void subStringSearch() { g_pWin->subStringSearch(); }
inline void increaseImageSize(long i) { player().setImgSize(player().m_imgHeight + i); }
inline void restoreImageSize() { player().setImgSize(defaults::IMAGE_HEIGHT); }

} /* namespace app */
