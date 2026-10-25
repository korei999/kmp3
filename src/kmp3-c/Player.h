#pragma once

#include "klib/String.h"
#include "klib/time.h"
#include "klib/QueueMPMC.h"

#include "generated.h"

#include <float.h>

typedef enum
{
    PLAYER_REPEAT_METHOD_NONE,
    PLAYER_REPEAT_METHOD_TRACK,
    PLAYER_REPEAT_METHOD_PLAYLIST,
    PLAYER_REPEAT_METHOD_ESIZE,
} PLAYER_REPEAT_METHOD;

typedef enum
{
    PLAYER_MSG_TYPE_NOTIFY,
    PLAYER_MSG_TYPE_WARNING,
    PLAYER_MSG_TYPE_ERROR,
    PLAYER_MSG_TYPE_ESIZE,
} PLAYER_MSG_TYPE;

static const double PLAYER_MSG_UNTIL_NEXT = DBL_MAX;

typedef struct
{
    k_String sMsg;
    k_time_Type time;
    PLAYER_MSG_TYPE eType;
} PlayerMsg;

typedef struct
{
    k_IAllocator* pAlloc;

    struct {
        k_String sTitle;
        k_String sAlbum;
        k_String sArtist;
    } info;

    uint8_t imgHeight;
    uint8_t imgWidth;
    VecSV vSongs; /* full path */
    VecSV vShortSongs; /* file name only */

    /* two index buffers for recursive filtering */
    VecU16 vSongIdxs;
    VecU16 vSearchIdxs;

    int64_t focusedI;
    int64_t selectedI;
    ssize_t longestString;
    PLAYER_REPEAT_METHOD eRepeatMethod;
    k_QueueMPMC qMPMCErrorMessages;
    k_time_Type lastMessageTime;
    k_String sLastMessage;
    bool bSelectionChanged;
    bool bRedrawImage; /* FIXME: belongs to Window? */
    bool bQuitOnSongEnd;
} Player;

bool PlayerInit(Player* s, k_IAllocator* pAlloc, int argc, char** argv);
void PlayerDestroy(Player* s);
void PlayerSetDefaultIdxs(Player* s, VecU16* pvIdxs);
void PlayerSetDefaultSongIdxs(Player* s);
void PlayerSetDefaultSearchIdxs(Player* s);
void PlayerSetAllDefaultIdxs(Player* s);

void PlayerFocusNext(Player* s);
void PlayerFocusPrev(Player* s);
void PlayerFocus(Player* s, int64_t i);
void PlayerFocusFirst(Player* s);
void PlayerFocusLast(Player* s);

int64_t PlayerFindSongI(Player* s, int64_t selI);
void PlayerFocusSelected(Player* s);
void PlayerFocusSelectedAtCenter(Player* s);
void PlayerSubStringSearch(Player* s, const wchar_t* pBuff, ssize_t buffSize);
void PlayerSelectFocused(Player* s); /* starts playing the focused song */
void PlayerTogglePause(Player* s);
void PlayerNextSongIfPrevEnded(Player* s);
PLAYER_REPEAT_METHOD PlayerCycleRepeatMethods(Player* s, bool bForward);
void PlayerSelect(Player* s, int64_t i);
void PlayerSelectNext(Player* s);
void PlayerSelectPrev(Player* s);
void PlayerCopySearchToSongIdxs(Player* s);
void PlayerSetImgSize(Player* s, int64_t height);
void PlayerAdjustImgWidth(Player* s);
void PlayerPushErrorMsg(Player* s, const PlayerMsg* pMsg);
PlayerMsg PlayerPopErrorMsg(Player* s);
