#pragma once

#include "adt/Arena.hh"
#include "adt/types.hh"
#include "adt/String.hh"
#include "adt/Vec.hh"
#include "adt/enum.hh"
#include "adt/Span.hh"

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

    u8 m_imgHeight {};
    u8 m_imgWidth {};
    VecBase<String> m_aSongs {}; /* full path */
    VecBase<String> m_aShortSongs {}; /* file name only */
    VecBase<u16> m_aSongIdxs {}; /* index buffer */
    VecBase<u16> m_aSearchIdxs {}; /* search index buffer */
    long m_focused {};
    long m_selected {};
    ssize m_longestString {};
    PLAYER_REPEAT_METHOD m_eReapetMethod {};
    bool m_bSelectionChanged {};

    /* */

    Player() = default;
    Player(IAllocator* p, int nArgs, char** ppArgs);

    /* */

    static bool acceptedFormat(const String s);
    void focusNext();
    void focusPrev();
    void focus(long i);
    void focusFirst() { focus(0); }
    void setDefaultIdxs(VecBase<u16>* pIdxs);
    void focusLast();
    u16 findSongIdxFromSelected();
    void focusSelected();
    void subStringSearch(Arena* pAlloc, Span<wchar_t> pBuff);
    void selectFocused(); /* starts playing focused song */
    void pause(bool bPause);
    void togglePause();
    void onSongEnd();
    PLAYER_REPEAT_METHOD cycleRepeatMethods(bool bForward);
    void selectNext();
    void selectPrev();
    void copySearchIdxs();
    void destroy();

    /* */

private:
    void updateInfo();
};
