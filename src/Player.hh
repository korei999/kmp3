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
    IAllocator* pAlloc {};
    struct {
        String time {};
        String volume {};
        String total {};
    } status {};
    struct {
        String title {};
        String album {};
        String artist {};
    } info {};
    struct {
        //
    } bottomBar {};
    u8 imgHeight {};
    f64 statusToInfoWidthRatio {};
    VecBase<String> aShortArgvs {}; /* only the name of the file, without full path */
    VecBase<u16> aSongIdxs {}; /* index buffer for aShortArgvs */
    long focused {};
    long selected {};
    u32 longestStringSize {};
    PLAYER_REPEAT_METHOD eReapetMethod {};
    bool bSelectionChanged {};

    Player() = delete;
    Player(IAllocator* p, int nArgs, [[maybe_unused]] char** ppArgs)
        : pAlloc(p), aShortArgvs(p, nArgs), aSongIdxs(p, nArgs) {}

    bool acceptedFormat(const String s);
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
};
