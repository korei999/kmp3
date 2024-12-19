#include "Win.hh"

#include "adt/logs.hh"
#include "app.hh"
#include "common.hh"
#include "defaults.hh"
#include "draw.hh"
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
    LOG("nRead: {}, ({}, {}): '{}'\n", nRead, *(u64*)aBuff, *(u64*)(aBuff + 8), aBuff);

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
        {
            keybinds::resolveKey(k.pfn, k.arg);
            s->bRedraw = true;
        }
    }
}

static void
sigwinchHandler(int sig)
{
    auto* s = (Win*)app::g_pWin;

    g_termSize = getTermSize();
    LOG_GOOD("term: {}\n", g_termSize);

    /* NOTE: don't allocate / write to the buffer from here.
     * Marking things to be redrawn instead. */
    app::g_pPlayer->bSelectionChanged = true;
    s->bRedraw = true;
    s->bClear = true;
}

bool
Win::start(Arena* pArena)
{
    this->pArena = pArena;
    this->textBuff = TextBuff(pArena);
    g_termSize = getTermSize();

    mtx_init(&this->mtxUpdate, mtx_plain);

    enableRawMode(this);
    signal(SIGWINCH, sigwinchHandler);

    this->textBuff.clear();
    this->textBuff.hideCursor(true);
    this->textBuff.push(MOUSE_ENABLE KEYPAD_ENABLE);
    this->textBuff.flush();

    LOG_GOOD("ansi::WinStart()\n");

    return true;
}

void
Win::destroy()
{
    this->textBuff.hideCursor(false);
    this->textBuff.clear();
    this->textBuff.clearKittyImages();
    this->textBuff.push(MOUSE_DISABLE KEYPAD_DISABLE);
    this->textBuff.moveTopLeft();
    this->textBuff.push("\n", 2);
    this->textBuff.flush();

    disableRawMode(this);
    mtx_destroy(&this->mtxUpdate);

    LOG_GOOD("ansi::WinDestroy()\n");
}

void
Win::draw()
{
    draw::update(this);
}

void
Win::procEvents()
{
    procInput(this);

    common::fixFirstIdx(
        g_termSize.height - app::g_pPlayer->imgHeight - 5,
        &this->firstIdx
    );
}

void
Win::seekFromInput()
{
    common::seekFromInput(
        +[](void* pArg) { return readWChar((Win*)pArg); },
        this,
        +[](void* pArg) { ((Win*)pArg)->bRedraw = true; draw::update((Win*)pArg); },
        this
    );
}

void
Win::subStringSearch()
{
    common::subStringSearch(
        this->pArena,
        +[](void* pArg) { return readWChar((Win*)pArg); },
        this,
        +[](void* pArg) { ((Win*)pArg)->bRedraw = true; draw::update((Win*)pArg); },
        this,
        &this->firstIdx
    );
}

} /* namespace ansi */
} /* namespace platform */
