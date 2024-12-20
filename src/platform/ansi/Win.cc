#include "Win.hh"

#include "adt/logs.hh"
#include "app.hh"
#include "draw.hh"
#include "input.hh"

#include <csignal>
#include <unistd.h>
#include <sys/ioctl.h>

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

static void
sigwinchHandler([[maybe_unused]] int sig)
{
    auto* s = (Win*)app::g_pWin;

    g_termSize = getTermSize();
    LOG_GOOD("term: {}\n", g_termSize);

    /* NOTE: don't allocate / write to the buffer from here.
     * Marking things to be redrawn instead. */
    app::g_pPlayer->bSelectionChanged = true;
    s->bRedraw = true;
    s->bClear = true;
    s->lastResizeTime = utils::timeNowMS();
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
    input::procInput(this);

    common::fixFirstIdx(
        g_termSize.height - app::g_pPlayer->imgHeight - 5,
        &this->firstIdx
    );
}

void
Win::seekFromInput()
{
    common::seekFromInput(
        +[](void* pArg) { return input::readWChar((Win*)pArg); },
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
        +[](void* pArg) { return input::readWChar((Win*)pArg); },
        this,
        +[](void* pArg) { ((Win*)pArg)->bRedraw = true; draw::update((Win*)pArg); },
        this,
        &this->firstIdx
    );
}

} /* namespace ansi */
} /* namespace platform */
