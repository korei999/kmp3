#include "input.h"

#include "app.h"

#include "klib/Ctx.h"

#include <wctype.h>

static void input_procSeekString(const wchar_t* pWBuff, ssize_t buffSize);

ssize_t
input_ReadModePFormatter(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg)
{
    const input_READ_MODE e = (int64_t)arg;
    const k_StringView aMap[] = {
        K_SV(""),
        K_SV("searching: "),
        K_SV("time: "),
    };
    return k_print_BuilderPushSvPaddedFmtArgs(pCtx->pBuilder, pFmtArgs, aMap[e]);
}

void
input_BufferZeroOut(input_Buffer* s)
{
    memset(s->aBuff, 0, sizeof(s->aBuff));
    s->idx = 0;
}

void
input_BufferSubStringSearch(
    input_Buffer* s,
    int16_t* pFirstIdx,
    input_READ_STATUS (*pfnRead)(void* pArg),
    void* pReadArg,
    void (*pfnDraw)(void* pArg),
    void* pDrawArg
)
{
    k_Arena* pArena = k_CtxArena();
    k_ArenaState arenaState = k_ArenaStatePush(pArena);

    Player* pPl = app_g_pPlayer;

    s->eLastUsedMode = s->eCurrMode = input_READ_MODE_SEARCH;
    input_BufferZeroOut(s);

    input_READ_STATUS eRead = 0;

    if (pPl->vSearchIdxs.size <= 0)
    {
        PlayerSetDefaultSearchIdxs(pPl);
        PlayerCopySearchToSongIdxs(pPl);
    }

    pfnDraw(pDrawArg);
    do
    {
        ssize_t prevSearchSize = pPl->vSearchIdxs.size;

        eRead = pfnRead(pReadArg);
        if (eRead == input_READ_STATUS_BACKSPACE)
        {
            if (pPl->vSearchIdxs.size != pPl->vSongs.size)
            {
                PlayerSetDefaultSearchIdxs(pPl);
                PlayerCopySearchToSongIdxs(pPl);
            }
        }

        if (eRead != input_READ_STATUS_TIMEOUT && eRead != input_READ_STATUS_DONE)
        {
            PlayerSubStringSearch(pPl, s->aBuff, K_ASIZE(s->aBuff));
            if (prevSearchSize != pPl->vSearchIdxs.size)
            {
                *pFirstIdx = 0;
                pPl->focusedI = 0;
            }
        }

        pfnDraw(pDrawArg);
    }
    while (eRead != input_READ_STATUS_DONE);

    s->eCurrMode = input_READ_MODE_NONE;

    PlayerCopySearchToSongIdxs(pPl);

    /* fix focused if it ends up out of the list range */
    if (pPl->focusedI >= pPl->vSongIdxs.size)
        pPl->focusedI = 0;

    k_ArenaStateRestore(&arenaState);
}

void
input_BufferSeekFromInput(
    input_Buffer* s,
    input_READ_STATUS (*pfnRead)(void* pArg),
    void* pReadArg,
    void (*pfnDraw)(void* pArg),
    void* pDrawArg
)
{
    s->eLastUsedMode = s->eCurrMode = input_READ_MODE_SEEK;

    input_BufferZeroOut(s);
    s->idx = 0;

    do pfnDraw(pDrawArg);
    while (pfnRead(pReadArg) != input_READ_STATUS_DONE);

    input_procSeekString(s->aBuff, K_ASIZE(s->aBuff));

    s->eCurrMode = input_READ_MODE_NONE;
}

static void
input_procSeekString(const wchar_t* pWBuff, ssize_t buffSize)
{
    k_Arena* pArena = k_CtxArena();
    k_ArenaState arenaState = k_ArenaStatePush(pArena);

    bool bPercent = false;
    bool bColon = false;

    VecChar vMinutes = {0};
    VecChar vSeconds = {0};

    for (ssize_t wBuffI = 0; wBuffI < buffSize; ++wBuffI)
    {
        wchar_t wch = pWBuff[wBuffI];

        if (wch == L'%')
        {
            bPercent = true;
        }
        else if (wch == L':')
        {
            /* leave if there is one more colon or bPercent */
            if (bColon || bPercent) break;
            bColon = true;
        }
        else if (iswdigit(wch))
        {
            VecChar* pVTarget = bColon ? &vSeconds : &vMinutes;
            VecCharPushVal(pVTarget, &pArena->base, (char)(wch));  /* wdigits are equivalent to char */
        }
    }

    if (vMinutes.size == 0)
        goto done;

    if (bPercent)
    {
        int64_t maxMS = audio_IMixerGetTotalMS(app_g_pMixer);
        audio_IMixerSeekMS(app_g_pMixer, maxMS * (k_StringViewToDouble(K_NTS(vMinutes.pData)) / 100.0));
    }
    else
    {
        ssize_t sec;
        if (vSeconds.size == 0) sec = k_StringViewToI64(K_NTS(vMinutes.pData), 10);
        else sec = atoll(vSeconds.pData) + k_StringViewToI64(K_NTS(vMinutes.pData), 10) * 60;

        audio_IMixerSeekMS(app_g_pMixer, sec * 1000);
    }

done:
    k_ArenaStateRestore(&arenaState);
}
