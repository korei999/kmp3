#pragma once

#include "adt/types.hh"
#include "adt/String.hh"
#include "adt/Vec.hh"

using namespace adt;

struct Player;

void PlayerNext(Player* s);
void PlayerPrev(Player* s);
void PlayerFocus(Player* s, long i);
void PlayerFocusFirst(Player* s);
void PlayerSetDefaultIdxs(Player* s);
void PlayerFocusLast(Player* s);
void PlayerFocusSelected(Player* s);
void PlayerSubStringSearch(Player* s, Allocator* pAlloc, wchar_t* pWBuff, u32 size);
void PlayerSelectFocused(Player* s);

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
    VecBase<String> aShortArgvs {};
    VecBase<u16> aSongIdxs {};
    long focused {};
    long selected  {};
    u32 longestStringSize {};

    Player() = delete;
    Player(Allocator* p, int nArgs, [[maybe_unused]] char** ppArgs)
        : pAlloc(p), aShortArgvs(p, nArgs), aSongIdxs(p, nArgs) {}
};
