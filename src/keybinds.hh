#pragma once

#include "app.hh"
#include "keys.hh"

namespace keybinds
{

enum ARG_TYPE : u8 { NONE, LONG, F32, F64, U64, U64_BOOL, BOOL };

struct Arg
{
    ARG_TYPE eType {};
    union {
        null nil;
        long l;
        f32 f;
        f64 d;
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
    void (*f64_)(f64);
    void (*u64_)(u64);
    void (*u64b)(u64, bool);
    void (*bool_)(bool);
};

struct Key
{
    u16 key {};
    u32 ch {};
    PFN pfn {};
    Arg arg {};
};

/* match key OR char (mods are ignored) */
inline const Key inl_aKeys[] {
/*  key                char   function                          arg */
    {keys::CTRL_C,     L'q',  (void*)app::quit,                 NONE                           },
    {{},               L'/',  (void*)app::subStringSearch,      NONE                           },
    {keys::ARROWDOWN,  L'j',  (void*)app::focusNext,            NONE                           },
    {keys::ARROWUP,    L'k',  (void*)app::focusPrev,            NONE                           },
    {{},               L'g',  (void*)app::focusFirst,           NONE                           },
    {keys::HOME,       {},    (void*)app::focusFirst,           NONE                           },
    {{},               L'G',  (void*)app::focusLast,            NONE                           },
    {keys::END,        {},    (void*)app::focusLast,            NONE                           },
    {keys::END,        {},    (void*)app::focusLast,            NONE                           },
    {keys::CTRL_D,     {},    (void*)app::focusDown,            {LONG, {.l = 22}}              },
    {keys::PGDN,       {},    (void*)app::focusDown,            {LONG, {.l = 22}}              },
    {keys::CTRL_U,     {},    (void*)app::focusUp,              {LONG, {.l = 22}}              },
    {keys::PGUP,       {},    (void*)app::focusUp,              {LONG, {.l = 22}}              },
    {keys::ENTER,      {},    (void*)app::selectFocused,        NONE                           },
    {{},               L'z',  (void*)app::focusSelected,        NONE                           },
    {{},               L' ',  (void*)app::togglePause,          NONE                           },
    {{},               L'9',  (void*)app::volumeDown,           {F32, {.f = 0.1f}}             },
    {{},               L'(',  (void*)app::volumeDown,           {F32, {.f = 0.01f}}            },
    {{},               L'0',  (void*)app::volumeUp,             {F32, {.f = 0.1f}}             },
    {{},               L')',  (void*)app::volumeUp,             {F32, {.f = 0.01f}}            },
    {{},               L'[',  (void*)app::changeSampleRateDown, {U64_BOOL, {.ub {1000, false}}}},
    {{},               L'{',  (void*)app::changeSampleRateDown, {U64_BOOL, {.ub {100, false}}} },
    {{},               L']',  (void*)app::changeSampleRateUp,   {U64_BOOL, {.ub {1000, false}}}},
    {{},               L'}',  (void*)app::changeSampleRateUp,   {U64_BOOL, {.ub {100, false}}} },
    {{},               L'\\', (void*)app::restoreSampleRate,    NONE                           },
    {keys::ARROWLEFT,  L'h',  (void*)app::seekOff,              {F64, {.d = -10000.0}}         },
    {{},               L'H',  (void*)app::seekOff,              {F64, {.d = -1000.0}}          },
    {keys::ARROWRIGHT, L'l',  (void*)app::seekOff,              {F64, {.d = 10000.0}}          },
    {{},               L'L',  (void*)app::seekOff,              {F64, {.d = 1000.0}}           },
    {{},               L'r',  (void*)app::cycleRepeatMethods,   {BOOL, {.b = true}}            },
    {{},               L'R',  (void*)app::cycleRepeatMethods,   {BOOL, {.b = false}}           },
    {{},               L'm',  (void*)app::toggleMute,           NONE                           },
    {{},               L't',  (void*)app::seekFromInput,        NONE                           },
    {{},               L'p',  (void*)app::selectPrev,           NONE                           },
    {{},               L'n',  (void*)app::selectNext,           NONE                           },
};

inline void
resolveKey(const keybinds::PFN pfn, const keybinds::Arg arg)
{
    switch (arg.eType)
    {
        case keybinds::ARG_TYPE::NONE:
        pfn.none();
        break;

        case keybinds::ARG_TYPE::LONG:
        pfn.long_(arg.uVal.l);
        break;

        case keybinds::ARG_TYPE::F32:
        pfn.f32_(arg.uVal.f);
        break;

        case keybinds::ARG_TYPE::F64:
        pfn.f64_(arg.uVal.d);
        break;

        case keybinds::ARG_TYPE::U64:
        pfn.u64_(arg.uVal.u);
        break;

        case keybinds::ARG_TYPE::U64_BOOL:
        pfn.u64b(arg.uVal.ub.u, arg.uVal.ub.b);
        break;

        case keybinds::ARG_TYPE::BOOL:
        pfn.bool_(arg.uVal.b);
        break;
    }
}

} /* namespace keybinds */
