#pragma once

#include "Player.hh"
#include "audio.hh"
#include "IWindow.hh"

using namespace adt;

namespace app
{

enum class UI_FRONTEND : u8 { ANSI, TERMBOX, NCURSES, NOTCURSES };

extern UI_FRONTEND g_eUIFrontend;
extern IWindow* g_pWin;
extern bool g_bRunning;
extern int g_argc;
extern char** g_argv;
extern VecBase<String> g_aArgs;

extern Player* g_pPlayer;
extern audio::IMixer* g_pMixer;

IWindow* allocWindow(IAllocator* pArena);

inline void quit() { g_bRunning = false; }
inline void focusNext() { g_pPlayer->focusNext(); }
inline void focusPrev() { g_pPlayer->focusPrev(); }
inline void focusFirst() { g_pPlayer->focusFirst(); }
inline void focusLast() { g_pPlayer->focusLast(); }
inline void focus(long i) { g_pPlayer->focus(i); }
inline void focusUp(long step) { focus(g_pPlayer->focused - step); }
inline void focusDown(long step) { focus(g_pPlayer->focused + step); }
inline void selectFocused() { g_pPlayer->selectFocused(); }
inline void focusSelected() { g_pPlayer->focusSelected(); }
inline void togglePause() { g_pPlayer->togglePause(); }
inline void volumeDown(const f32 step) { g_pMixer->volumeDown(step); }
inline void volumeUp(const f32 step) { g_pMixer->volumeUp(step); }
inline void changeSampleRateDown(u64 ms, bool bSave) { g_pMixer->changeSampleRateDown(ms, bSave); }
inline void changeSampleRateUp(u64 ms, bool bSave) { g_pMixer->changeSampleRateUp(ms, bSave); }
inline void restoreSampleRate() { g_pMixer->restoreSampleRate(); }
inline void seekLeftMS(u64 ms) { g_pMixer->seekLeftMS(ms); }
inline void seekRightMS(u64 ms) { g_pMixer->seekRightMS(ms); }
inline PLAYER_REPEAT_METHOD cycleRepeatMethods(bool bForward) { return g_pPlayer->cycleRepeatMethods(bForward); }
inline void selectPrev() { g_pPlayer->selectPrev(); }
inline void selectNext() { g_pPlayer->selectNext(); }
inline void toggleMute() { utils::toggle(&g_pMixer->m_bMuted); }
inline void seekFromInput() { g_pWin->seekFromInput(); }
inline void subStringSearch() { g_pWin->subStringSearch(); }

} /* namespace app */
