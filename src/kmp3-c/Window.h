#pragma once

#include "klib/String.h"
#include "klib/print.h"
#include "klib/time.h"

#include "TextBuff.h"
#include "Player.h"

#include <termios.h>

typedef enum
{
    WINDOW_READ_MODE_NONE,
    WINDOW_READ_MODE_SEARCH,
    WINDOW_READ_MODE_SEEK
} WINDOW_READ_MODE;

typedef enum
{
    WINDOW_MOUSE_KEY_NONE,
    WINDOW_MOUSE_KEY_WHEEL_UP,
    WINDOW_MOUSE_KEY_WHEEL_DOWN,
    WINDOW_MOUSE_KEY_LEFT,
    WINDOW_MOUSE_KEY_MIDDLE,
    WINDOW_MOUSE_KEY_RIGHT,
    WINDOW_MOUSE_KEY_RELEASE,
} WINDOW_MOUSE_KEY;

typedef struct
{
    int width;
    int height;
    int pixWidth;
    int pixHeight;
} WindowTermSize;

WindowTermSize WindowTermSizeGet(void);
ssize_t WindowTermSizePFormatter(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* arg);

typedef struct
{
    WINDOW_MOUSE_KEY eKey;
    int x;
    int y;
} WindowMouseInput;

#define WINDOW_INVALID_MOUSE (WindowMouseInput){.eKey = WINDOW_MOUSE_KEY_NONE, .x = -1, .y = -1}

static inline bool WindowMouseInputEq(const WindowMouseInput* pL, const WindowMouseInput* pR);

typedef enum
{
    WINDOW_INPUT_TYPE_KB,
    WINDOW_INPUT_TYPE_MOUSE,
    WINDOW_INPUT_TYPE_TIMEOUT,
} WINDOW_INPUT_TYPE;

typedef struct
{
    union
    {
        WindowMouseInput mouse;
        int key;
    };
    WINDOW_INPUT_TYPE eType;
} WindowInput;

typedef struct
{
    k_Arena* pArena;

    int16_t firstIdx;
    int16_t listHeight;
    bool bUpdateFirstIdx;
    bool bClear;
    volatile bool vol_bRunning;

    TextBuff textBuff;
    struct termios termiosOg;
    WindowTermSize termSize;
    int prevImgWidth;
    k_time_Type time;
    WindowInput lastInput;
    int lastMouseSelection;
    k_time_Type lastMouseSelectionTime;
    k_time_Type lastImageRedrawTime;
    k_time_Type lastResizeTime;
    bool bNeedsResize;
    bool bImageJustRedrawn;
    k_String sTitle; /* Gpa. */

    int aFdsWakeUp[2];

    bool bPressed;
    bool bHoldTimeSlider;
    bool bHoldScrollBar;

    PlayerMsg currMsg;
    k_time_Type currMsgTime;
} Window;

bool WindowInit(Window* s, k_Arena* pArena);
void WindowDestroy(Window* s);
void WindowDraw(Window* s);
void WindowProcEvents(Window* s);
void WindowSeekFromInput(Window* s);
void WindowSubStringSearch(Window* s);
void WindowWakeUp(Window* s);

static inline bool
WindowMouseInputEq(const WindowMouseInput* pL, const WindowMouseInput* pR)
{
    return pL->eKey == pR->eKey && pL->x == pR->x && pL->y == pR->y;
}
