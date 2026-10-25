#pragma once

#include "audio.h"
#include "Config.h"
#include "Player.h"
#include "Window.h"

typedef enum
{
    app_MIXER_DUMMY,
    app_MIXER_ALSA,
    app_MIXER_PIPEWIRE,
    app_MIXER_COREAUDIO,
    app_MIXER_SNDIO,
} app_MIXER;

typedef enum
{
    app_TERM_OTHER,
    app_TERM_XTERM,
    app_TERM_XTERM_256COLOR,
    app_TERM_KITTY,
    app_TERM_FOOT,
    app_TERM_GHOSTTY,
    app_TERM_ALACRITTY
} app_TERM;

extern bool app_g_bForceLoggerColors;

extern app_MIXER app_g_eMixer;
extern k_StringView app_g_svTerm;
extern app_TERM app_g_eTerm;
extern volatile bool app_g_vol_bRunning;
extern bool app_g_bNoImage;
extern bool app_g_bSixelOrKitty;
extern bool app_g_bChafaSymbols;

extern Window* app_g_pWin;
extern Config app_g_config;
extern Player* app_g_pPlayer;
extern audio_IMixer* app_g_pMixer;
// extern platform::ffmpeg::Decoder g_decoder;

audio_IMixer* app_allocMixer(k_IAllocator* pAlloc);

static inline void app_quit(void) { app_g_vol_bRunning = false; }
static inline void app_focusNext(void) { PlayerFocusNext(app_g_pPlayer); }
static inline void app_focusPrev(void) { PlayerFocusPrev(app_g_pPlayer); }
static inline void app_focusFirst(void) { PlayerFocusFirst(app_g_pPlayer); }
static inline void app_focusLast(void) { PlayerFocusLast(app_g_pPlayer); }
static inline void app_focus(long i) { PlayerFocus(app_g_pPlayer, i); }
static inline void app_focusUp(long step) { app_focus(app_g_pPlayer->focusedI - step); }
static inline void app_focusDown(long step) { app_focus(app_g_pPlayer->focusedI + step); }
static inline void app_selectFocused(void) { PlayerSelectFocused(app_g_pPlayer); }
static inline void app_focusSelected(void) { PlayerFocusSelected(app_g_pPlayer); }
static inline void app_focusSelectedAtCenter(void) { PlayerFocusSelectedAtCenter(app_g_pPlayer); }
static inline void app_togglePause(void) { PlayerTogglePause(app_g_pPlayer); }
static inline void app_volumeDown(const long step) { audio_IMixerVolumeDown(app_g_pMixer, step); }
static inline void app_volumeUp(const long step) { audio_IMixerVolumeUp(app_g_pMixer, step); }
static inline void app_changeSampleRateDown(uint64_t ms, bool bSave) { audio_IMixerChangeSampleRateDown(app_g_pMixer, ms, bSave); }
static inline void app_changeSampleRateUp(uint64_t ms, bool bSave) { audio_IMixerChangeSampleRateUp(app_g_pMixer, ms, bSave); }
static inline void app_restoreSampleRate(void) { audio_IMixerRestoreSampleRate(app_g_pMixer); }
static inline void app_seekOff(double ms) { audio_IMixerSeekOff(app_g_pMixer, ms); }
static inline PLAYER_REPEAT_METHOD app_cycleRepeatMethods(bool bForward) { app_g_pPlayer->bQuitOnSongEnd = false; return PlayerCycleRepeatMethods(app_g_pPlayer, bForward); }
static inline void app_selectPrev(void) { PlayerSelectPrev(app_g_pPlayer); }
static inline void app_selectNext(void) { PlayerSelectNext(app_g_pPlayer); }
static inline void app_toggleMute(void) { audio_IMixerToggleMute(app_g_pMixer); }
static inline void app_seekFromInput(void) { WindowSeekFromInput(app_g_pWin); }
static inline void app_subStringSearch(void) { WindowSubStringSearch(app_g_pWin); }
static inline void app_increaseImageSize(long i) { PlayerSetImgSize(app_g_pPlayer, app_g_pPlayer->imgHeight + i); }
static inline void app_restoreImageSize(void) { PlayerSetImgSize(app_g_pPlayer, app_g_config.imageHeight); }
static inline void app_cleanRedraw(void) { app_g_pWin->bClear = true; app_g_pPlayer->bRedrawImage = true; }
static inline void app_quitOnSongEnd(void) { app_g_pPlayer->bQuitOnSongEnd = !app_g_pPlayer->bQuitOnSongEnd; }

#ifndef NDEBUG
static inline void
app_testMsg(void)
{
    int i = rand() % PLAYER_MSG_TYPE_ESIZE;;
    PlayerPushErrorMsg(app_g_pPlayer, &(PlayerMsg){
        .eType = i,
        .time = 3000 * K_TIME_MSEC,
        .sMsg = k_StringCreateSmallSv(K_SV("TEST MESSAGE")),
    });
}
#endif
