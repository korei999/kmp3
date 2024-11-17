#pragma once

#include "adt/Arena.hh"
#include "adt/types.hh"
#include "adt/String.hh"
#include "adt/Vec.hh"

using namespace adt;

enum PLAYER_REPEAT_METHOD: u8 { NONE, TRACK, PLAYLIST, ESIZE };

constexpr String mapPlayerRepeatMethodStrings[] {"None", "Track", "Playlist"};

constexpr String
PlayerRepeatMethodToString(PLAYER_REPEAT_METHOD e)
{
    return mapPlayerRepeatMethodStrings[e];
}

struct Player;

bool PlayerAcceptedFormat(const String s);
void PlayerFocusNext(Player* s);
void PlayerFocusPrev(Player* s);
void PlayerFocus(Player* s, long i);
inline void PlayerFocusFirst(Player* s) { PlayerFocus(s, 0); }
void PlayerSetDefaultIdxs(Player* s);
void PlayerFocusLast(Player* s);
u16 PlayerFindSongIdxFromSelected(Player* s);
void PlayerFocusSelected(Player* s);
void PlayerSubStringSearch(Player* s, Arena* pAlloc, wchar_t* pWBuff, u32 size);
void PlayerSelectFocused(Player* s); /* starts playing focused song */
void PlayerPause(Player* s, bool bPause);
void PlayerTogglePause(Player* s);
void PlayerOnSongEnd(Player* s);
PLAYER_REPEAT_METHOD PlayerCycleRepeatMethods(Player* s, bool bForward);
void PlayerSelectNext(Player* s);
void PlayerSelectPrev(Player* s);

struct Player
{
    Allocator* pAlloc {};
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
    u8 statusAndInfoHeight {};
    f64 statusToInfoWidthRatio {};
    VecBase<String> aShortArgvs {}; /* only the name of the file, without full path */
    VecBase<u16> aSongIdxs {}; /* index buffer for aShortArgvs */
    long focused {};
    long selected {};
    u32 longestStringSize {};
    PLAYER_REPEAT_METHOD eReapetMethod {};

    Player() = delete;
    Player(Allocator* p, int nArgs, [[maybe_unused]] char** ppArgs)
        : pAlloc(p), aShortArgvs(p, nArgs), aSongIdxs(p, nArgs) {}
};
