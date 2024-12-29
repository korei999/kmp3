#pragma once

#include "adt/Arena.hh"
#include "adt/types.hh"
#include "adt/String.hh"
#include "adt/Vec.hh"
#include "adt/enum.hh"

using namespace adt;

enum class PLAYER_REPEAT_METHOD: u8 { NONE, TRACK, PLAYLIST, ESIZE };
ADT_ENUM_BITWISE_OPERATORS(PLAYER_REPEAT_METHOD);

constexpr String mapPlayerRepeatMethodStrings[] {"None", "Track", "Playlist"};

constexpr String
repeatMethodToString(PLAYER_REPEAT_METHOD e)
{
    return mapPlayerRepeatMethodStrings[int(e)];
}

struct Player
{
    IAllocator* m_pAlloc {};
    struct {
        String time {};
        String volume {};
        String total {};
    } m_status {};
    struct {
        String title {};
        String album {};
        String artist {};
    } m_info {};
    struct {
        //
    } m_bottomBar {};
    u8 m_imgHeight {};
    f64 m_statusToInfoWidthRatio {};
    VecBase<String> m_aShortArgvs {}; /* only the name of the file, without full path */
    VecBase<u16> m_aSongIdxs {}; /* index buffer for aShortArgvs */
    long m_focused {};
    long m_selected {};
    u32 m_longestStringSize {};
    PLAYER_REPEAT_METHOD m_eReapetMethod {};
    bool m_bSelectionChanged {};

    /* */

    Player() = delete;
    Player(IAllocator* p, int nArgs, [[maybe_unused]] char** ppArgs)
        : m_pAlloc(p), m_aShortArgvs(p, nArgs), m_aSongIdxs(p, nArgs) {}

    /* */

    static bool acceptedFormat(const String s);
    void focusNext();
    void focusPrev();
    void focus(long i);
    void focusFirst() { focus(0); }
    void setDefaultIdxs();
    void focusLast();
    u16 findSongIdxFromSelected();
    void focusSelected();
    void subStringSearch(Arena* pAlloc, wchar_t* pWBuff, u32 size);
    void selectFocused(); /* starts playing focused song */
    void pause(bool bPause);
    void togglePause();
    void onSongEnd();
    PLAYER_REPEAT_METHOD cycleRepeatMethods(bool bForward);
    void selectNext();
    void selectPrev();
    void destroy();

    /* */

private:
    void updateInfo();
};
