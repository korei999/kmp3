#include "Win.hh"
#include "draw.hh"

#include "common.hh"
#include "adt/logs.hh"
#include "app.hh"
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

static common::READ_STATUS
readWChar([[maybe_unused]] Win* s)
{
    namespace c = common;

    wint_t wc = getwchar();
    LOG_WARN("readWChar(): {}\n", wc);

    if (wc == keys::ESC) return c::READ_STATUS::DONE; /* esc */
    else if (wc == keys::CTRL_C) return c::READ_STATUS::DONE;
    else if (wc == 13) return c::READ_STATUS::DONE; /* enter */
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
    wint_t wc = getwchar();

    LOG("wc: '{}' ({})\n", (wchar_t)wc, (int)wc);

    if (wc == 13) wc = keys::ENTER;

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
    TextBuffFlush(&s->textBuff);

    LOG_GOOD("ansi::WinStart()\n");

    return true;
}

void
WinDestroy(Win* s)
{
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
        +[](void* pArg) { WinDraw((Win*)pArg); },
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
        +[](void* pArg) { WinDraw((Win*)pArg); },
        s,
        &s->firstIdx
    );
}

} /* namespace ansi */
} /* namespace platform */
