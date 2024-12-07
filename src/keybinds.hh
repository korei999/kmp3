#pragma once

#include "app.hh"
#include "platform/termbox2/window.hh"
#include "termbox2/termbox2.h"

namespace keybinds
{

enum ARG_TYPE : u8 { NONE, LONG, F32, U64, U64_BOOL, BOOL };

struct Arg
{
    ARG_TYPE eType {};
    union {
        null nil;
        long l;
        f32 f;
        u64 u;
        struct { u64 u; bool b; } ub;
        bool b;
    } uVal {};
};

union PFN
{
    void* ptr;
    void (*none)();
    void (*long_)(long);
    void (*f32_)(f32);
    void (*u64_)(u64);
    void (*u64b)(u64, bool);
    void (*bool_)(bool);
};

/* using termbox keys for now */
struct Key
{
    // u8 mod {}; /* bitwise `TB_MOD_*` constants */
    u16 key {}; /* one of `TB_KEY_*` constants */
    u32 ch {}; /* a Unicode codepoint */
    PFN pfn {};
    Arg arg {};
};

/* match key OR char (mods are ignored) */
inline const Key gc_aKeys[] {
/*  key                  char   function                          arg */
    {{},                 L'q',  (void*)app::quit,                 NONE                           },
    {{},                 L'/',  (void*)app::subStringSearch,      NONE                           },
    {TB_KEY_ARROW_DOWN,  L'j',  (void*)app::focusNext,            NONE                           },
    {TB_KEY_ARROW_UP,    L'k',  (void*)app::focusPrev,            NONE                           },
    {{},                 L'g',  (void*)app::focusFirst,           NONE                           },
    {TB_KEY_HOME,        {},    (void*)app::focusFirst,           NONE                           },
    {{},                 L'G',  (void*)app::focusLast,            NONE                           },
    {TB_KEY_END,         {},    (void*)app::focusLast,            NONE                           },
    {TB_KEY_CTRL_D,      {},    (void*)app::focusDown,            {LONG, {.l = 22}}              },
    {TB_KEY_PGDN,        {},    (void*)app::focusDown,            {LONG, {.l = 22}}              },
    {TB_KEY_CTRL_U,      {},    (void*)app::focusUp,              {LONG, {.l = 22}}              },
    {TB_KEY_PGUP,        {},    (void*)app::focusUp,              {LONG, {.l = 22}}              },
    {TB_KEY_ENTER,       {},    (void*)app::selectFocused,        NONE                           },
    {{},                 L'z',  (void*)app::focusSelected,        NONE                           },
    {{},                 L' ',  (void*)app::togglePause,          NONE                           },
    {{},                 L'9',  (void*)app::volumeDown,           {F32, {.f = 0.1f}}             },
    {{},                 L'(',  (void*)app::volumeDown,           {F32, {.f = 0.01f}}            },
    {{},                 L'0',  (void*)app::volumeUp,             {F32, {.f = 0.1f}}             },
    {{},                 L')',  (void*)app::volumeUp,             {F32, {.f = 0.01f}}            },
    {{},                 L'[',  (void*)app::changeSampleRateDown, {U64_BOOL, {.ub {1000, false}}}},
    {{},                 L'{',  (void*)app::changeSampleRateDown, {U64_BOOL, {.ub {100, false}}} },
    {{},                 L']',  (void*)app::changeSampleRateUp,   {U64_BOOL, {.ub {1000, false}}}},
    {{},                 L'}',  (void*)app::changeSampleRateUp,   {U64_BOOL, {.ub {100, false}}} },
    {{},                 L'\\', (void*)app::restoreSampleRate,    NONE                           },
    {TB_KEY_ARROW_LEFT,  L'h',  (void*)app::seekLeftMS,           {U64, {.u = 5000}}             },
    {{},                 L'H',  (void*)app::seekLeftMS,           {U64, {.u = 1000}}             },
    {TB_KEY_ARROW_RIGHT, L'l',  (void*)app::seekRightMS,          {U64, {.u = 5000}}             },
    {{},                 L'L',  (void*)app::seekRightMS,          {U64, {.u = 1000}}             },
    {{},                 L'r',  (void*)app::cycleRepeatMethods,   {BOOL, {.b = true}}            },
    {{},                 L'R',  (void*)app::cycleRepeatMethods,   {BOOL, {.b = false}}           },
    {{},                 L'm',  (void*)app::toggleMute,           NONE                           },
    {{},                 L't',  (void*)app::seekFromInput,        NONE                           },
    {{},                 L'i',  (void*)app::selectPrev,           NONE                           },
    {{},                 L'o',  (void*)app::selectNext,           NONE                           },
};

} /* namespace keybinds */
