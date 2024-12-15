#include "Win.hh"
#include "draw.hh"

#include "adt/guard.hh"
#include "common.hh"
#include "adt/logs.hh"
#include "app.hh"
#include "defaults.hh"
#include "keybinds.hh"

#include <csignal>
#include <unistd.h>
#include <sys/ioctl.h>

#define CTRL_KEY(k) ((k) & 0b11111)

namespace platform
{
namespace ansi
{

TermSize g_termSize {};

static void
disableRawMode(Win* s)
{
    TextBuffHideCursor(&s->textBuff, false);
    TextBuffMove(&s->textBuff, 0, 0);
    TextBuffClearDown(&s->textBuff);
    TextBuffPush(&s->textBuff, "\r\n", 2);
    TextBuffFlush(&s->textBuff);

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &s->termOg) == -1)
        LOG_DIE("tcsetattr\n");
}

static void
enableRawMode(Win* s)
{
    if (tcgetattr(STDIN_FILENO, &s->termOg) == -1)
        LOG_DIE("tcgetattr\n");

    struct termios raw = s->termOg;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    /*raw.c_cc[VMIN] = 0;*/
    /*raw.c_cc[VTIME] = 1;*/

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        LOG_DIE("tcsetattr\n");
}

static void
procInput(Win* s)
{
    char c {};
    if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN)
        LOG_DIE("read\n");

    if (iscntrl(c)) LOG("{}\n", (int)c);
    else LOG("{} ('{}')\r\n", (int)c, c);

    if (c == 13) c = keys::ENTER;

    for (const auto& k : keybinds::inl_aKeys)
    {
        if ((k.key > 0 && k.key == c) || (k.ch > 0 && k.ch == (u32)c))
            keybinds::resolveKey(k.pfn, k.arg);
    }

    cnd_signal(&s->cndWait);
}

static int
inputPoll(void* pArg)
{
    while (app::g_bRunning)
        procInput((Win*)pArg);

    return thrd_success;
}

static void
sigwinchHandler(int sig)
{
    auto* s = (Win*)app::g_pWin;

    g_termSize = getTermSize();
    LOG_GOOD("term: {}\n", g_termSize);

    app::g_pPlayer->bSelectionChanged = true;
    cnd_signal(&s->cndWait);
}

bool
WinStart(Win* s, Arena* pArena)
{
    s->pArena = pArena;
    s->textBuff = TextBuff(pArena);

    g_termSize = getTermSize();

    mtx_init(&s->mtxDraw, mtx_plain);
    mtx_init(&s->mtxWait, mtx_plain);
    cnd_init(&s->cndWait);
    thrd_create(&s->thrdInput, inputPoll, s);

    enableRawMode(s);
    signal(SIGWINCH, sigwinchHandler);

    TextBuffClearDown(&s->textBuff);
    TextBuffHideCursor(&s->textBuff, true);
    TextBuffFlush(&s->textBuff);

    LOG_GOOD("ansi::WinStart()\n");

    return true;
}

void
WinDestroy(Win* s)
{
    thrd_join(s->thrdInput, {});
    mtx_destroy(&s->mtxDraw);
    mtx_destroy(&s->mtxWait);
    cnd_destroy(&s->cndWait);

    disableRawMode(s);

    LOG_NOTIFY("ansi::WinDestroy()\n");
}

void
WinDraw(Win* s)
{
    common::fixFirstIdx(
        g_termSize.height - common::getHorizontalSplitPos(g_termSize.height) - 4,
        &s->firstIdx
    );

    draw::update(s);

    guard::Mtx lock(&s->mtxWait);

    timespec ts;
    timespec_get(&ts, TIME_UTC);
    utils::addNSToTimespec(&ts, defaults::UPDATE_RATE * 1000000);

    cnd_timedwait(&s->cndWait, &s->mtxWait, &ts);
}

void
WinSeekFromInput(Win* s)
{
    LOG("ansi::WinSeekFromInput(): TODO\n");
}

void
WinSubStringSearch(Win* s)
{
    LOG("ansi::WinSubStringSearch(): TODO\n");
}

} /* namespace ansi */
} /* namespace platform */
