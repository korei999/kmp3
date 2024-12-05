#pragma once

#include "Player.hh"
#include "audio.hh"

using namespace adt;

namespace app
{

extern bool g_bRunning;
extern int g_argc;
extern char** g_argv;
extern VecBase<String> g_aArgs;

extern Player* g_pPlayer;
extern audio::IMixer* g_pMixer;

inline void quit() { g_bRunning = false; }
inline void focusNext() { PlayerFocusNext(g_pPlayer); }
inline void focusPrev() { PlayerFocusPrev(g_pPlayer); }
inline void focusFirst() { PlayerFocusFirst(g_pPlayer); }
inline void focusLast() { PlayerFocusLast(g_pPlayer); }
inline void focus(long i) { PlayerFocus(g_pPlayer, i); }
inline void focusUp(long step) { focus(g_pPlayer->focused - step); }
inline void focusDown(long step) { focus(g_pPlayer->focused + step); }
inline void selectFocused() { PlayerSelectFocused(g_pPlayer); }
inline void focusSelected() { PlayerFocusSelected(g_pPlayer); }
inline void togglePause() { PlayerTogglePause(g_pPlayer); }
inline void volumeDown(const f32 step) { audio::MixerVolumeDown(g_pMixer, step); }
inline void volumeUp(const f32 step) { audio::MixerVolumeUp(g_pMixer, step); }
inline void changeSampleRateDown(u64 ms, bool bSave) { audio::MixerChangeSampleRateDown(g_pMixer, ms, bSave); }
inline void changeSampleRateUp(u64 ms, bool bSave) { audio::MixerChangeSampleRateUp(g_pMixer, ms, bSave); }
inline void restoreSampleRate() { audio::MixerRestoreSampleRate(g_pMixer); }
inline void seekLeftMS(u64 ms) { audio::MixerSeekLeftMS(g_pMixer, ms); }
inline void seekRightMS(u64 ms) { audio::MixerSeekRightMS(g_pMixer, ms); }
inline PLAYER_REPEAT_METHOD cycleRepeatMethods(bool bForward) { return PlayerCycleRepeatMethods(g_pPlayer, bForward); }
inline void selectPrev() { PlayerSelectPrev(g_pPlayer); }
inline void selectNext() { PlayerSelectNext(g_pPlayer); }
inline void toggleMute() { utils::toggle(&g_pMixer->bMuted); }

} /* namespace app */
