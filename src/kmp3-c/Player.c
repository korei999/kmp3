#include "Player.h"
#include "app.h"

#include "klib/Ctx.h"
#include "klib/assert.h"

#include <math.h>
#include <wctype.h>

static const char* const player_aSvAcceptedFileEndings[] = {
    ".mp2", ".mp3", ".mp4", ".m4a", ".m4b",
    ".fla", ".flac",
    ".ogg", ".opus",
    ".umx", ".s3m",
    ".wav", ".caf", ".aif",
    ".webm",
    ".mkv",
};

static bool PlayerAcceptedFormat(const k_StringView sv);
static void PlayerSelectFinal(Player* s, int64_t selI);
static int64_t PlayerNextSelectionI(Player* s, int64_t selI);
static void PlayerUpdateInfo(Player* s);

static bool
PlayerAcceptedFormat(const k_StringView sv)
{
    for (ssize_t i = 0; i < K_ASIZE(player_aSvAcceptedFileEndings); ++i)
    {
        k_StringView svEnding = K_NTS(player_aSvAcceptedFileEndings[i]);
        if (k_StringViewEndsWith(sv, svEnding))
            return true;
    }

    return false;
}

static void
PlayerSelectFinal(Player* s, int64_t selI)
{
    k_Arena* pArena = k_CtxArena();
    k_ArenaState arenaState = k_ArenaStatePush(pArena);

    int64_t nFailed = 0;

    while (true)
    {
        if (!app_g_vol_bRunning) goto done;
        if (audio_IMixerPlay(app_g_pMixer, VecSVGet(&s->vSongs, selI))) break;

        K_CTX_LOG_WARN("failed to open: '{PSv}', selI: {i64}", VecSVGetP(&s->vSongs, selI), selI);

        if (++nFailed >= s->vSearchIdxs.size)
        {
            K_CTX_LOG_INFO("QUIT (nFailed: {i64}, size: {i64})", nFailed, s->vSearchIdxs.size);
            app_quit();
            goto done;
        }

        PlayerMsg msg = {
            .time = K_TIME_MSEC * 5000,
            .eType = PLAYER_MSG_TYPE_ERROR,
        };
        k_print_Builder pb = {0};
        if (!k_print_BuilderInit(&pb, (k_print_BuilderInitOpts){.pAllocOrNull = &pArena->base}))
        {
            K_CTX_LOG_ERROR("can't even create a print builder...");
            goto done;
        }

        k_StringView svEnding = k_StringViewPathEnding(VecSVGet(&s->vSongs, selI));
        k_print_BuilderPrint(&pb, "failed to open \"{PSv}\"", &svEnding);
        msg.sMsg = k_StringCreate(s->pAlloc, pb.pData, pb.size);

        if (!k_StringEq(&msg.sMsg, &s->sLastMessage) || k_time_diff(k_time_now(), s->lastMessageTime) >= msg.time)
            PlayerPushErrorMsg(s, &msg);

        selI = PlayerNextSelectionI(s, selI); /* Might set g_bRunning. */
    }

    s->bSelectionChanged = true;
    s->selectedI = selI;
    PlayerUpdateInfo(s);

done:
    k_ArenaStateRestore(&arenaState);
}

static int64_t
PlayerNextSelectionI(Player* s, int64_t selI)
{
    const int64_t currI = PlayerFindSongI(s, selI);
    int64_t nextI = currI + 1;
    int64_t retI = 0;

    if (s->eRepeatMethod == PLAYER_REPEAT_METHOD_TRACK)
    {
        nextI = currI;
    }
    else if (nextI >= s->vSongIdxs.size)
    {
        if (s->eRepeatMethod == PLAYER_REPEAT_METHOD_PLAYLIST)
        {
            nextI = 0;
        }
        else
        {
            app_g_vol_bRunning = false;
            retI = VecU16Get(&s->vSongIdxs, currI);
            goto done;
        }
    }

    retI = VecU16Get(&s->vSongIdxs, nextI);

done:
    K_CTX_LOG_DEBUG("currI: {i64}, nextI: {i64}\n", currI, nextI);
    return retI;
}

static void
PlayerUpdateInfo(Player* s)
{
    // m_info.sTitle.reallocWith(m_pAlloc, app::decoder().getMetadata("title"));
    // m_info.sAlbum.reallocWith(m_pAlloc, app::decoder().getMetadata("album"));
    // m_info.sArtist.reallocWith(m_pAlloc, app::decoder().getMetadata("artist"));

    // if (m_info.sTitle.empty()) m_info.sTitle.reallocWith(m_pAlloc, m_vShortSongs[m_selectedI]);

    // m_bSelectionChanged = true;

    s->bSelectionChanged = true;
}

bool
PlayerInit(Player* s, k_IAllocator* pAlloc, int argc, char** argv)
{
    *s = (Player){0};

    s->pAlloc = pAlloc;
    if (!VecSVInit(&s->vSongs, pAlloc, argc)) goto failed;
    if (!VecSVInit(&s->vShortSongs, pAlloc, argc)) goto failed;
    if (!VecU16Init(&s->vSearchIdxs, pAlloc, argc)) goto failed;

    if (!k_QueueMPMCInit(&s->qMPMCErrorMessages, pAlloc,
            (k_QueueMPMCInitOpts){.slotSize = sizeof(PlayerMsg), .cap = 1 << 8}
        )
    )
    {
        goto failed;
    }

    for (ssize_t i = 1; i < argc; ++i)
    {
        k_StringView svArg = K_NTS(argv[i]);
        if (PlayerAcceptedFormat(svArg))
        {
            ssize_t lastI = VecSVPush(&s->vSongs, s->pAlloc, &svArg);
            k_StringView svShortFileName = k_StringViewPathEnding(svArg);
            VecSVPush(&s->vShortSongs, s->pAlloc, &svShortFileName);

            if (lastI >= 0 && lastI < s->vSongs.size && VecSVGetP(&s->vSongs, lastI)->size > s->longestString)
                s->longestString = VecSVGetP(&s->vSongs, lastI)->size;
        }
    }

    PlayerSetDefaultSongIdxs(s);
    PlayerSetDefaultSearchIdxs(s);

    return true;

failed:
    PlayerDestroy(s);
    return false;
}

void
PlayerDestroy(Player* s)
{
    if (!s->pAlloc) return;

    VecSVDestroy(&s->vSongs, s->pAlloc);
    VecSVDestroy(&s->vShortSongs, s->pAlloc);
    VecU16Destroy(&s->vSongIdxs, s->pAlloc);
    VecU16Destroy(&s->vSearchIdxs, s->pAlloc);
    k_QueueMPMCDestroy(&s->qMPMCErrorMessages, s->pAlloc);
}

void
PlayerSetDefaultIdxs(Player* s, VecU16* pvIdxs)
{
    VecU16SetSize(pvIdxs, s->pAlloc, s->vSongs.size);
    for (ssize_t i = 0; i < pvIdxs->size; ++i) pvIdxs->pData[i] = i;
}

void
PlayerSetDefaultSongIdxs(Player* s)
{
    PlayerSetDefaultIdxs(s, &s->vSongIdxs);
}

void
PlayerSetDefaultSearchIdxs(Player* s)
{
    PlayerSetDefaultIdxs(s, &s->vSearchIdxs);
}

void
PlayerSetAllDefaultIdxs(Player* s)
{
    PlayerSetDefaultSongIdxs(s);
    PlayerSetDefaultSearchIdxs(s);
}

void
PlayerFocusNext(Player* s)
{
    s->focusedI = s->vSongIdxs.size > 0 ? (s->focusedI + 1) % s->vSongIdxs.size : 0;
    app_g_pWin->bUpdateFirstIdx = true;
}

void
PlayerFocusPrev(Player* s)
{
    s->focusedI = s->vSongIdxs.size > 0 ? (s->focusedI + (s->vSongIdxs.size - 1)) % s->vSongIdxs.size : 0;
    app_g_pWin->bUpdateFirstIdx = true;
}

void
PlayerFocus(Player* s, int64_t i)
{
    s->focusedI = K_CLAMP(i, 0l, (s->vSongIdxs.size - 1));
    app_g_pWin->bUpdateFirstIdx = true;
}

void
PlayerFocusFirst(Player* s)
{
    PlayerFocus(s, 0);
}

void
PlayerFocusLast(Player* s)
{
    PlayerFocus(s, s->vSongIdxs.size - 1);
}

int64_t
PlayerFindSongI(Player* s, int64_t selI)
{
    if (s->vSongs.size <= 0) return 0;

again:
    ssize_t res = K_NPOS;
    for (ssize_t i = 0; i < s->vSongIdxs.size; ++i)
    {
        if (s->vSongIdxs.pData[i] == selI)
        {
            res = i;
            break;
        }
    }

    if (res <= K_NPOS)
    {
        PlayerSetDefaultSearchIdxs(s);
        PlayerSetDefaultSongIdxs(s);
        goto again;
    }

    return res;
}

void
PlayerFocusSelected(Player* s)
{
    PlayerFocus(s, PlayerFindSongI(s, s->selectedI));
}

void
PlayerFocusSelectedAtCenter(Player* s)
{
    PlayerFocusSelected(s);

    const ssize_t height = app_g_pWin->listHeight;

    if (s->vSearchIdxs.size <= height) return;

    const ssize_t half = (height-2) / 2;
    app_g_pWin->firstIdx = K_MAX(0ll, s->focusedI - half);
}

void
PlayerSubStringSearch(Player* s, const wchar_t* pBuff, ssize_t buffSize)
{
    if (!pBuff || buffSize <= 0 || wcsnlen(pBuff, buffSize) == 0) return;

    k_Arena* pArena = k_CtxArena();
    k_ArenaState arenaState = k_ArenaStatePush(pArena);

    /* Convert both to upper case. */
    VecWChar vNeedle = {0};
    VecWCharInit(&vNeedle, &pArena->base, buffSize + 1);
    VecWCharSetSize(&vNeedle, &pArena->base, vNeedle.cap);

    for (ssize_t i = 0; i < buffSize && pBuff[i]; ++i)
        vNeedle.pData[i] = towupper(pBuff[i]);

    VecWChar vHaystack = {0};
    VecWCharInit(&vHaystack, &pArena->base, s->longestString + 1);
    VecWCharSetSize(&vHaystack, &pArena->base, vHaystack.cap);

    VecU16SetSize(&s->vSearchIdxs, &pArena->base, 0);

    for (ssize_t i = 0; i < s->vSearchIdxs.size; ++i)
    {
        uint16_t songIdx = VecU16Get(&s->vSongIdxs, i);

        const k_StringView song = VecSVGet(&s->vShortSongs, songIdx);

        memset(&vHaystack.pData, 0, vHaystack.size * sizeof(vHaystack.pData[0]));

        mbstowcs(vHaystack.pData, song.pData, song.size);
        for (ssize_t i = 0; i < vHaystack.size; ++i)
            vHaystack.pData[i] = towupper(vHaystack.pData[i]);

        if (wcsstr(vHaystack.pData, vNeedle.pData) != NULL)
            VecU16PushVal(&s->vSearchIdxs, &pArena->base, songIdx);
    }

    k_ArenaStateRestore(&arenaState);
}

void
PlayerSelectFocused(Player* s)
{
    if (s->vSongIdxs.size <= s->focusedI)
    {
        K_CTX_LOG_WARN("out of range selection: (vec.size: {sz})", s->vSongIdxs.size);
        return;
    }

    // K_CTX_LOG_DEBUG("idxs:  {[({sz}) {sz}]PU16}", sizeof(s->vSongIdxs.pData[0]), s->vSongIdxs.size, s->vSongIdxs.pData);
    K_CTX_LOG_DEBUG("selected: {PSv}", VecSVGetP(&s->vSongs, VecU16Get(&s->vSongIdxs, s->focusedI)));
    PlayerSelectFinal(s, VecU16Get(&s->vSongIdxs, s->focusedI));
}

void
PlayerTogglePause(Player* s)
{
    audio_IMixer* pMix = app_g_pMixer;
    audio_IMixerPause(pMix, !k_atomic_U8LoadRelaxed(&pMix->atom_bPaused));
}

void
PlayerNextSongIfPrevEnded(Player* s)
{
    uint8_t bExpected = true;
    if (k_atomic_U8CASAcquire(&app_g_pMixer->atom_bSongEnd, &bExpected, false))
    {
        if (s->bQuitOnSongEnd) app_quit();
        PlayerSelectFinal(s, PlayerNextSelectionI(s, s->selectedI));
    }
}

PLAYER_REPEAT_METHOD
PlayerCycleRepeatMethods(Player* s, bool bForward)
{
    PLAYER_REPEAT_METHOD eRM;
    if (bForward) eRM = (s->eRepeatMethod + 1) % PLAYER_REPEAT_METHOD_ESIZE;
    else eRM = (s->eRepeatMethod + (PLAYER_REPEAT_METHOD_ESIZE - 1)) % PLAYER_REPEAT_METHOD_ESIZE;

    s->eRepeatMethod = eRM;

#ifdef OPT_MPRIS
    mpris_loopStatusChanged();
#endif

    return eRM;
}

void
PlayerSelect(Player* s, int64_t i)
{
    if (s->vSongs.size <= 0 || s->vSearchIdxs.size <= 0)
        return;

    const int64_t idx = K_CLAMP(i, 0L, s->vSearchIdxs.size - 1);
    PlayerSelectFinal(s, VecU16Get(&s->vSearchIdxs, idx));
}

void
PlayerSelectNext(Player* s)
{
    if (s->vSongs.size <= 0 || s->vSearchIdxs.size <= 0)
    {
        PlayerSetAllDefaultIdxs(s);
        PlayerFocusSelectedAtCenter(s);
    }

    K_ASSERT(s->vSearchIdxs.size > 0, "size: {sz}", s->vSearchIdxs.size);
    PlayerSelect(s, PlayerFindSongI(s, (s->selectedI + 1) % s->vSearchIdxs.size));
}

void
PlayerSelectPrev(Player* s)
{
    if (s->vSongs.size <= 0 || s->vSearchIdxs.size <= 0)
    {
        PlayerSetAllDefaultIdxs(s);
        PlayerFocusSelectedAtCenter(s);
    }

    K_ASSERT(s->vSearchIdxs.size > 0, "size: {sz}", s->vSearchIdxs.size);
    PlayerSelect(s, (PlayerFindSongI(s, (s->selectedI + (s->vSearchIdxs.size - 1)) % s->vSearchIdxs.size)));
}

void
PlayerCopySearchToSongIdxs(Player* s)
{
    VecU16SetSize(&s->vSongIdxs, s->pAlloc, s->vSearchIdxs.size);
    memcpy(s->vSongIdxs.pData, s->vSearchIdxs.pData, s->vSearchIdxs.size * sizeof(s->vSearchIdxs.pData[0]));
}

void
PlayerSetImgSize(Player* s, int64_t height)
{
    height = K_CLAMP(height, app_g_config.minImageHeight, app_g_config.maxImageHeight);
    if (s->imgHeight == height) return;

    s->imgHeight = height;
    PlayerAdjustImgWidth(s);
    s->bSelectionChanged = true;

    if (app_g_pWin) app_g_pWin->bClear = true;
}

void
PlayerAdjustImgWidth(Player* s)
{
    s->imgWidth = round((s->imgHeight * (1920.0/1080.0)) / app_g_config.fontAspectRatio);
}

void
PlayerPushErrorMsg(Player* s, const PlayerMsg* pMsg)
{
    {
        k_StringView sv = k_StringToSv(&pMsg->sMsg);
        K_CTX_LOG_DEBUG("new msg: '{PSv}' (type: {i}, time: {sz})", &sv, pMsg->eType, pMsg->time);
        (void)sv;
    }

    if (!k_QueueMPMCPush(&s->qMPMCErrorMessages, pMsg, sizeof(*pMsg)))
    {
        const k_StringView svMsg = k_StringToSv(&pMsg->sMsg);
        K_CTX_LOG_ERROR("Message queue is empty. Skipped pushing this message: '{PSv}', time: {i64}", &svMsg);
        return;
    }

    s->sLastMessage = k_StringCreateSv(s->pAlloc, k_StringToSv(&pMsg->sMsg));
    s->lastMessageTime = k_time_now();
}

PlayerMsg
PlayerPopErrorMsg(Player* s)
{
    PlayerMsg msg = {0};
    k_QueueMPMCPop(&s->qMPMCErrorMessages, &msg, sizeof(msg));
    return msg;
}
