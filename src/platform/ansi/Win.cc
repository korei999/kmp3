#include "Win.hh"
#include "defaults.hh"
#include "draw.hh"

#include "common.hh"
#include "adt/logs.hh"
#include "app.hh"
#include "keybinds.hh"

#include <csignal>
#include <unistd.h>
#include <sys/ioctl.h>

#define CTRL_KEY(k) ((k) & 0b11111)
#define MOUSE_ENABLE "\x1b[?1000h\x1b[?1002h\x1b[?1015h\x1b[?1006h"
#define MOUSE_DISABLE "\x1b[?1006l\x1b[?1015l\x1b[?1002l\x1b[?1000l"
#define KEYPAD_ENABLE "\033[?1h\033="
#define KEYPAD_DISABLE "\033[?1l\033>"

namespace platform
{
namespace ansi
{

TermSize g_termSize {};

static void
disableRawMode(Win* s)
{
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &s->termOg) == -1)
        LOG_DIE("tcsetattr\n");
}

static void
enableRawMode(Win* s)
{
    if (tcgetattr(STDIN_FILENO, &s->termOg) == -1)
        LOG_DIE("tcgetattr\n");

    struct termios raw = s->termOg;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG);
    raw.c_cc[VMIN] = 0; /* disable blocking on read() */
    /*raw.c_cc[VTIME] = 1;*/

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        LOG_DIE("tcsetattr\n");
}

static int
readFromStdin([[maybe_unused]] Win* s, const int timeoutMS)
{
    char aBuff[128] {};
    fd_set fds {};
    FD_SET(STDIN_FILENO, &fds);

    timeval tv;
    tv.tv_sec = timeoutMS / 1000;
    tv.tv_usec = (timeoutMS - (tv.tv_sec * 1000)) * 1000;

    select(1, &fds, {}, {}, &tv);
    ssize_t nRead = read(STDIN_FILENO, aBuff, sizeof(aBuff));
    LOG("nRead: {}, ({}): '{}'\n", nRead, *(int*)aBuff, aBuff);

    wchar_t wc {};
    mbtowc(&wc, aBuff, sizeof(aBuff));

    return wc;
}

static common::READ_STATUS
readWChar(Win* s)
{
    namespace c = common;

    int wc = readFromStdin(s, defaults::READ_TIMEOUT);

    if (wc == keys::ESC) return c::READ_STATUS::DONE; /* esc */
    else if (wc == keys::CTRL_C) return c::READ_STATUS::DONE;
    else if (wc == keys::ENTER) return c::READ_STATUS::DONE; /* enter */
    else if (wc == keys::CTRL_W)
    {
        if (c::g_input.idx > 0)
        {
            c::g_input.idx = 0;
            c::g_input.zeroOutBuff();
        }
    }
    else if (wc == 127) /* backspace */
    {
        if (c::g_input.idx > 0)
            c::g_input.aBuff[--c::g_input.idx] = L'\0';
    }
    else if (wc)
    {
        if (c::g_input.idx < utils::size(c::g_input.aBuff) - 1)
            c::g_input.aBuff[c::g_input.idx++] = wc;
    }

    return c::READ_STATUS::OK_;
}

static void
procInput(Win* s)
{
    int wc = readFromStdin(s, defaults::UPDATE_RATE);

    for (const auto& k : keybinds::inl_aKeys)
    {
        if ((k.key > 0 && k.key == wc) || (k.ch > 0 && k.ch == (u32)wc))
            keybinds::resolveKey(k.pfn, k.arg);
    }
}

static void
sigwinchHandler(int sig)
{
    auto* s = (Win*)app::g_pWin;

    g_termSize = getTermSize();
    LOG_GOOD("term: {}\n", g_termSize);

    app::g_pPlayer->bSelectionChanged = true;
    WinDraw(s);
}

bool
WinStart(Win* s, Arena* pArena)
{
    s->pArena = pArena;
    s->textBuff = TextBuff(pArena);

    g_termSize = getTermSize();

    enableRawMode(s);
    signal(SIGWINCH, sigwinchHandler);

    TextBuffClearDown(&s->textBuff);
    TextBuffHideCursor(&s->textBuff, true);
    TextBuffPush(&s->textBuff, MOUSE_ENABLE KEYPAD_ENABLE);
    TextBuffFlush(&s->textBuff);

    LOG_GOOD("ansi::WinStart()\n");

    return true;
}

void
WinDestroy(Win* s)
{
    TextBuffHideCursor(&s->textBuff, false);
    TextBuffClear(&s->textBuff);
    TextBuffClearKittyImages(&s->textBuff);
    TextBuffPush(&s->textBuff, MOUSE_DISABLE KEYPAD_DISABLE);
    TextBuffMoveTopLeft(&s->textBuff);
    TextBuffPush(&s->textBuff, "\n", 2);
    TextBuffFlush(&s->textBuff);

    disableRawMode(s);

    LOG_NOTIFY("ansi::WinDestroy()\n");
}

void
WinDraw(Win* s)
{
    draw::update(s);
}

void
WinProcEvents(Win* s)
{
    procInput(s);

    common::fixFirstIdx(
        g_termSize.height - common::getHorizontalSplitPos(g_termSize.height) - 4,
        &s->firstIdx
    );
}

void
WinSeekFromInput(Win* s)
{
    common::seekFromInput(
        +[](void* pArg) { return readWChar((Win*)pArg); },
        s,
        +[](void* pArg) { draw::update((Win*)pArg); },
        s
    );
}

void
WinSubStringSearch(Win* s)
{
    common::subStringSearch(
        s->pArena,
        +[](void* pArg) { return readWChar((Win*)pArg); },
        s,
        +[](void* pArg) { draw::update((Win*)pArg); },
        s,
        &s->firstIdx
    );
}

} /* namespace ansi */
} /* namespace platform */
