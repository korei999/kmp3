#pragma once

#include "app.hh"
#include "platform/termbox2/window.hh"
#include "termbox2/termbox2.h"

namespace keybinds
{

enum ARG_TYPE : u8
{
    NONE, LONG, F32, U64, U64_BOOL, BOOL, PASS_PALLOC
};

struct Arg
{
    ARG_TYPE eType {};
    union {
        null nil;
        long l;
        f32 f;
        u64 u;
        struct {
            u64 u;
            bool b;
        } u64Bool;
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
    void (*u64Bool)(u64, bool);
    void (*bool_)(bool);
    void (*pass)(Allocator*); /* used for subStringSearch/seekFromInput to pass allocator */
};

/* using termbox keys for now */
struct key
{
    u8 mod {}; /* bitwise `TB_MOD_*` constants */
    u16 key {}; /* one of `TB_KEY_*` constants */
    u32 ch {}; /* a Unicode codepoint */
    PFN pfn {};
    Arg arg {};
};

static const key gc_aKeys[] {
/* mod(unused rn)  key                char   function                          arg */
    {{},           {},                L'q',  (void*)app::quit,                 NONE},
    {{},           {},                L'/',  (void*)platform::termbox2::window::subStringSearch, PASS_PALLOC},
    {{},           TB_KEY_ARROW_DOWN, L'j',  (void*)app::focusNext,            NONE},
    {{},           TB_KEY_ARROW_UP,   L'k',  (void*)app::focusPrev,            NONE},
    {{},           {},                L'g',  (void*)app::focusFirst,           NONE},
    {{},           {},                L'G',  (void*)app::focusLast,            NONE},
    {{},           TB_KEY_CTRL_D,     {},    (void*)app::focusDown,            {.eType = LONG, .uVal.l = 22}},
    {{},           TB_KEY_CTRL_U,     {},    (void*)app::focusUp,              {.eType = LONG, .uVal.l = 22}},
    {{},           TB_KEY_ENTER,      {},    (void*)app::selectFocused,        NONE},
    {{},           {},                L'z',  (void*)app::focusSelected,        NONE},
    {{},           {},                L' ',  (void*)app::togglePause,          NONE},
    {{},           {},                L'9',  (void*)app::volumeDown,           {.eType = F32, .uVal.f = 0.1f}},
    {{},           {},                L'(',  (void*)app::volumeDown,           {.eType = F32, .uVal.f = 0.01f}},
    {{},           {},                L'0',  (void*)app::volumeUp,             {.eType = F32, .uVal.f = 0.1f}},
    {{},           {},                L')',  (void*)app::volumeUp,             {.eType = F32, .uVal.f = 0.01f}},
    {{},           {},                L'[',  (void*)app::changeSampleRateDown, {.eType = U64_BOOL, .uVal.u64Bool {1000, false}}},
    {{},           {},                L'{',  (void*)app::changeSampleRateDown, {.eType = U64_BOOL, .uVal.u64Bool {100, false}}},
    {{},           {},                L']',  (void*)app::changeSampleRateUp,   {.eType = U64_BOOL, .uVal.u64Bool {1000, false}}},
    {{},           {},                L'}',  (void*)app::changeSampleRateUp,   {.eType = U64_BOOL, .uVal.u64Bool {100, false}}},
    {{},           {},                L'\\', (void*)app::restoreSampleRate,    NONE},
    {{},           {},                L'h',  (void*)app::seekLeftMS,           {.eType = U64, .uVal.u = 5000}},
    {{},           {},                L'H',  (void*)app::seekLeftMS,           {.eType = U64, .uVal.u = 1000}},
    {{},           {},                L'l',  (void*)app::seekRightMS,          {.eType = U64, .uVal.u = 5000}},
    {{},           {},                L'L',  (void*)app::seekRightMS,          {.eType = U64, .uVal.u = 1000}},
    {{},           {},                L'r',  (void*)app::cycleRepeatMethods,   {.eType = BOOL, .uVal.b = true}},
    {{},           {},                L'R',  (void*)app::toggleMute,           NONE},
    {{},           {},                L't',  (void*)platform::termbox2::window::seekFromInput, PASS_PALLOC},
    {{},           {},                L'i',  (void*)app::selectPrev,           NONE},
    {{},           {},                L'o',  (void*)app::selectNext,           NONE},
};

} /* namespace keybinds */
