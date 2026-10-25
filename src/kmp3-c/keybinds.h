#pragma once

#include "app.h"
#include "key.h"

typedef enum
{
    KEYBINDS_ARG_TYPE_NONE,
    KEYBINDS_ARG_TYPE_LONG,
    KEYBINDS_ARG_TYPE_F32,
    KEYBINDS_ARG_TYPE_F64,
    KEYBINDS_ARG_TYPE_U64,
    KEYBINDS_ARG_TYPE_U64_BOOL,
    KEYBINDS_ARG_TYPE_BOOL,
} KEYBINDS_ARG_TYPE;

typedef struct
{
    KEYBINDS_ARG_TYPE eType;
    union {
        long long l;
        float f;
        double d;
        uint64_t u;
        struct { uint64_t u; bool b; } ub;
        bool b;
    } uVal;
} keybinds_Arg;

typedef union
{
    void* ptr;
    void (*none)(void);
    void (*long_)(long long);
    void (*f32_)(float);
    void (*f64_)(double);
    void (*u64_)(uint64_t);
    void (*u64b)(uint64_t, bool);
    void (*bool_)(bool);
} keybinds_PFN;

typedef struct
{
    uint16_t key;
    uint32_t ch;
    keybinds_PFN pfn;
    keybinds_Arg arg;
} keybinds_Key;

#ifdef __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wmissing-braces"
    #pragma clang diagnostic ignored "-Wpedantic"
#else
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wmissing-braces"
    #pragma GCC diagnostic ignored "-Wpedantic"
#endif

/* match key OR char (mods are ignored) */
static const keybinds_Key keybinds_aKeys[] = {
    /* key           char   function                          arg */
    {KEY_CTRL_C,     L'q',  app_quit,                  KEYBINDS_ARG_TYPE_NONE                             },
    {0,              L'й',  app_quit,                  KEYBINDS_ARG_TYPE_NONE                             },
    {0,              L'Q',  app_quitOnSongEnd,         KEYBINDS_ARG_TYPE_NONE                             },
    {0,              L'Й',  app_quit,                  KEYBINDS_ARG_TYPE_NONE                             },
    {KEY_CTRL_L,     0,     app_cleanRedraw,           KEYBINDS_ARG_TYPE_NONE                             },
    {0,              L'/',  app_subStringSearch,       KEYBINDS_ARG_TYPE_NONE                             },
    {0,              L'j',  app_focusNext,             KEYBINDS_ARG_TYPE_NONE                             },
    {KEY_ARROWDOWN,  L'о',  app_focusNext,             KEYBINDS_ARG_TYPE_NONE                             },
    {0,              L'k',  app_focusPrev,             KEYBINDS_ARG_TYPE_NONE                             },
    {KEY_ARROWUP,    L'л',  app_focusPrev,             KEYBINDS_ARG_TYPE_NONE                             },
    {KEY_HOME,       L'g',  app_focusFirst,            KEYBINDS_ARG_TYPE_NONE                             },
    {0,              L'п',  app_focusFirst,            KEYBINDS_ARG_TYPE_NONE                             },
    {KEY_END,        L'G',  app_focusLast,             KEYBINDS_ARG_TYPE_NONE                             },
    {0,              L'П',  app_focusLast,             KEYBINDS_ARG_TYPE_NONE                             },
    {KEY_CTRL_D,     0,     app_focusDown,             {KEYBINDS_ARG_TYPE_LONG, {.l = 22}}                },
    {KEY_PGDN,       0,     app_focusDown,             {KEYBINDS_ARG_TYPE_LONG, {.l = 22}}                },
    {KEY_CTRL_U,     0,     app_focusUp,               {KEYBINDS_ARG_TYPE_LONG, {.l = 22}}                },
    {KEY_PGUP,       0,     app_focusUp,               {KEYBINDS_ARG_TYPE_LONG, {.l = 22}}                },
    {KEY_ENTER,      0,     app_selectFocused,         KEYBINDS_ARG_TYPE_NONE                             },
    {0,              L'z',  app_focusSelectedAtCenter, KEYBINDS_ARG_TYPE_NONE                             },
    {0,              L'я',  app_focusSelectedAtCenter, KEYBINDS_ARG_TYPE_NONE                             },
    {0,              L'Z',  app_focusSelected,         KEYBINDS_ARG_TYPE_NONE                             },
    {0,              L'Я',  app_focusSelected,         KEYBINDS_ARG_TYPE_NONE                             },
    {0,              L' ',  app_togglePause,           KEYBINDS_ARG_TYPE_NONE                             },
    {0,              L'9',  app_volumeDown,            {KEYBINDS_ARG_TYPE_LONG, {.l = 10}}                },
    {0,              L'(',  app_volumeDown,            {KEYBINDS_ARG_TYPE_LONG, {.l = 1}}                 },
    {0,              L'0',  app_volumeUp,              {KEYBINDS_ARG_TYPE_LONG, {.l = 10}}                },
    {0,              L')',  app_volumeUp,              {KEYBINDS_ARG_TYPE_LONG, {.l = 1}}                 },
    {0,              L'[',  app_changeSampleRateDown,  {KEYBINDS_ARG_TYPE_U64_BOOL, {.ub = {1000, false}}}},
    {0,              L'{',  app_changeSampleRateDown,  {KEYBINDS_ARG_TYPE_U64_BOOL, {.ub = {100, false}}} },
    {0,              L']',  app_changeSampleRateUp,    {KEYBINDS_ARG_TYPE_U64_BOOL, {.ub = {1000, false}}}},
    {0,              L'}',  app_changeSampleRateUp,    {KEYBINDS_ARG_TYPE_U64_BOOL, {.ub = {100, false}}} },
    {0,              L'\\', app_restoreSampleRate,     KEYBINDS_ARG_TYPE_NONE                             },
    {KEY_ARROWLEFT,  L'h',  app_seekOff,               {KEYBINDS_ARG_TYPE_F64, {.d = -10000.0}}           },
    {KEY_ARROWLEFT,  L'р',  app_seekOff,               {KEYBINDS_ARG_TYPE_F64, {.d = -10000.0}}           },
    {0,              L'H',  app_seekOff,               {KEYBINDS_ARG_TYPE_F64, {.d = -1000.0}}            },
    {0,              L'Р',  app_seekOff,               {KEYBINDS_ARG_TYPE_F64, {.d = -1000.0}}            },
    {KEY_ARROWRIGHT, L'l',  app_seekOff,               {KEYBINDS_ARG_TYPE_F64, {.d = 10000.0}}            },
    {KEY_ARROWRIGHT, L'д',  app_seekOff,               {KEYBINDS_ARG_TYPE_F64, {.d = 10000.0}}            },
    {0,              L'L',  app_seekOff,               {KEYBINDS_ARG_TYPE_F64, {.d = 1000.0}}             },
    {0,              L'Д',  app_seekOff,               {KEYBINDS_ARG_TYPE_F64, {.d = 1000.0}}             },
    {0,              L'r',  app_cycleRepeatMethods,    {KEYBINDS_ARG_TYPE_BOOL, {.b = true}}              },
    {0,              L'к',  app_cycleRepeatMethods,    {KEYBINDS_ARG_TYPE_BOOL, {.b = true}}              },
    {0,              L'R',  app_cycleRepeatMethods,    {KEYBINDS_ARG_TYPE_BOOL, {.b = false}}             },
    {0,              L'К',  app_cycleRepeatMethods,    {KEYBINDS_ARG_TYPE_BOOL, {.b = false}}             },
    {0,              L'm',  app_toggleMute,            KEYBINDS_ARG_TYPE_NONE                             },
    {0,              L'ь',  app_toggleMute,            KEYBINDS_ARG_TYPE_NONE                             },
    {0,              L't',  app_seekFromInput,         KEYBINDS_ARG_TYPE_NONE                             },
    {0,              L'Е',  app_seekFromInput,         KEYBINDS_ARG_TYPE_NONE                             },
    {0,              L'p',  app_selectPrev,            KEYBINDS_ARG_TYPE_NONE                             },
    {0,              L'з',  app_selectPrev,            KEYBINDS_ARG_TYPE_NONE                             },
    {0,              L'n',  app_selectNext,            KEYBINDS_ARG_TYPE_NONE                             },
    {0,              L'т',  app_selectNext,            KEYBINDS_ARG_TYPE_NONE                             },
    {0,              L'i',  app_increaseImageSize,     {KEYBINDS_ARG_TYPE_LONG, {.l = 1}}                 },
    {0,              L'ш',  app_increaseImageSize,     {KEYBINDS_ARG_TYPE_LONG, {.l = 1}}                 },
    {0,              L'I',  app_increaseImageSize,     {KEYBINDS_ARG_TYPE_LONG, {.l = -1}}                },
    {0,              L'Ш',  app_increaseImageSize,     {KEYBINDS_ARG_TYPE_LONG, {.l = -1}}                },
    {0,              L'o',  app_restoreImageSize,      KEYBINDS_ARG_TYPE_NONE                             },
    {0,              L'щ',  app_restoreImageSize,      KEYBINDS_ARG_TYPE_NONE                             },
#ifndef NDEBUG
    {0,              L'b',  app_testMsg,               KEYBINDS_ARG_TYPE_NONE                             },
#endif
};

#ifdef __clang__
    #pragma clang diagnostic pop
#else
    #pragma GCC diagnostic pop
#endif

K_NO_UB static inline void /* triggers ubsan */
keybinds_exec(const keybinds_PFN pfn, const keybinds_Arg arg)
{
    switch (arg.eType)
    {
        case KEYBINDS_ARG_TYPE_NONE:
        pfn.none();
        break;

        case KEYBINDS_ARG_TYPE_LONG:
        pfn.long_(arg.uVal.l);
        break;

        case KEYBINDS_ARG_TYPE_F32:
        pfn.f32_(arg.uVal.f);
        break;

        case KEYBINDS_ARG_TYPE_F64:
        pfn.f64_(arg.uVal.d);
        break;

        case KEYBINDS_ARG_TYPE_U64:
        pfn.u64_(arg.uVal.u);
        break;

        case KEYBINDS_ARG_TYPE_U64_BOOL:
        pfn.u64b(arg.uVal.ub.u, arg.uVal.ub.b);
        break;

        case KEYBINDS_ARG_TYPE_BOOL:
        pfn.bool_(arg.uVal.b);
        break;
    }
}
