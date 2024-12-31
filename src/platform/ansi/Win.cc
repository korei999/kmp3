#include "Win.hh"

#include "adt/logs.hh"
#include "app.hh"
#include "common.hh"

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

void
Win::disableRawMode()
{
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &m_termOg) == -1)
        LOG_DIE("tcsetattr\n");
}

void
Win::enableRawMode()
{
    if (tcgetattr(STDIN_FILENO, &m_termOg) == -1)
        LOG_DIE("tcgetattr\n");

    struct termios raw = m_termOg;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG);
    raw.c_cc[VMIN] = 0; /* disable blocking on read() */
    /*raw.c_cc[VTIME] = 1;*/

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        LOG_DIE("tcsetattr\n");
}

void
sigwinchHandler([[maybe_unused]] int sig)
{
    auto& s = *(Win*)app::g_pWin;
    auto& pl = *app::g_pPlayer;

    g_termSize = getTermSize();
    LOG_GOOD("term: {}\n", g_termSize);

    s.m_textBuff.resizeBuffers(g_termSize.width, g_termSize.height);

    pl.m_bSelectionChanged = true;
    s.m_bRedraw = true;

    s.m_bClear = true;
    s.m_lastResizeTime = utils::timeNowMS();
}

bool
Win::start(Arena* pArena)
{
    this->m_pArena = pArena;
    m_textBuff = TextBuff(pArena);
    g_termSize = getTermSize();

    mtx_init(&m_mtxUpdate, mtx_plain);

    enableRawMode();
    signal(SIGWINCH, sigwinchHandler);

    m_textBuff.clear();
    m_textBuff.hideCursor(true);
    m_textBuff.push(MOUSE_ENABLE KEYPAD_ENABLE);
    m_textBuff.flush();

    m_textBuff.resizeBuffers(g_termSize.width, g_termSize.height);

    LOG_GOOD("ansi::WinStart()\n");

    return true;
}

void
Win::destroy()
{
    m_textBuff.hideCursor(false);
    m_textBuff.clear();
    m_textBuff.clearKittyImages();
    m_textBuff.push(MOUSE_DISABLE KEYPAD_DISABLE);
    m_textBuff.moveTopLeft();
    m_textBuff.push("\n", 2);
    m_textBuff.flush();

    disableRawMode();
    mtx_destroy(&m_mtxUpdate);

    m_textBuff.destroy();

    LOG_GOOD("ansi::WinDestroy()\n");
}

void
Win::draw()
{
    update();
}

void
Win::procEvents()
{
    procInput();

    common::fixFirstIdx(
        g_termSize.height - app::g_pPlayer->m_imgHeight - 5,
        &this->m_firstIdx
    );
}

void
Win::seekFromInput()
{
    common::seekFromInput(
        +[](void* pArg) { return ((Win*)pArg)->readWChar(); },
        this,
        +[](void* pArg) { ((Win*)pArg)->m_bRedraw = true; ((Win*)pArg)->update(); },
        this
    );
}

void
Win::subStringSearch()
{
    common::subStringSearch(
        this->m_pArena,
        +[](void* pArg) { return ((Win*)pArg)->readWChar(); },
        this,
        +[](void* pArg) { ((Win*)pArg)->m_bRedraw = true; ((Win*)pArg)->update(); },
        this,
        &this->m_firstIdx
    );
}

} /* namespace ansi */
} /* namespace platform */
