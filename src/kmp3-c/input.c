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
    input_BufferZeroOut(s);
}

static void
input_procSeekString(const wchar_t* pWBuff, ssize_t buffSize)
{
    k_Arena* pArena = k_CtxArena();
    k_ArenaState arenaState = k_ArenaStatePush(pArena);

    bool bPercent = false;
    bool bColon = false;

    VecChar vMinutes = {0}; VecCharInit(&vMinutes, &pArena->base, 32);
    VecChar vSeconds = {0}; VecCharInit(&vSeconds, &pArena->base, 32);

    for (ssize_t buffI = 0; buffI < buffSize; ++buffI)
    {
        const wchar_t wc = pWBuff[buffI];

        if (wc == L'%')
        {
            bPercent = true;
        }
        else if (wc == L':')
        {
            /* leave if there is one more colon or bPercent */
            if (bColon || bPercent) break;
            bColon = true;
        }
        else if (iswdigit(wc))
        {
            VecChar* pTargetArray = bColon ? &vSeconds : &vMinutes;
            VecCharPushVal(pTargetArray, &pArena->base, (char)wc);  /* wdigits are equivalent to char */
        }
    }

    if (vMinutes.size <= 0) goto done;

    /* Null terminate. */
    VecCharPushVal(&vSeconds, &pArena->base, '\0');
    VecCharPushVal(&vMinutes, &pArena->base, '\0');

    k_StringView svMinutes = {.pData = vMinutes.pData, vMinutes.size - 1};
    k_StringView svSeconds = {.pData = vSeconds.pData, vSeconds.size - 1};

    if (bPercent)
    {
        const int64_t maxMS = audio_IMixerGetTotalMS(app_g_pMixer);
        double seekMS = k_StringViewToI64(svMinutes, 10);
        seekMS = maxMS * (seekMS / 100.0);

        audio_IMixerSeekMS(app_g_pMixer, seekMS);
    }
    else
    {
        ssize_t sec;
        if (svSeconds.size <= 0) sec = k_StringViewToI64(svMinutes, 10);
        else sec = k_StringViewToI64(svSeconds, 10) + k_StringViewToI64(svMinutes, 10)*60;

        audio_IMixerSeekMS(app_g_pMixer, sec * 1000);
    }

done:
    k_ArenaStateRestore(&arenaState);
}
