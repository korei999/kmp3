#include "Win.hh"

#include "adt/logs.hh"
#include "app.hh"
#include "common.hh"

#include <csignal>
#include <unistd.h>
#include <sys/ioctl.h>

namespace platform::ansi
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

    s.adjustListHeight();
    s.m_textBuff.resizeBuffers(g_termSize.width, g_termSize.height);

    pl.m_bSelectionChanged = true;
    s.m_bRedraw = true;

    s.m_bClear = true;
    s.m_lastResizeTime = utils::timeNowMS();
}

bool
Win::start(Arena* pArena)
{
    m_pArena = pArena;
    g_termSize = getTermSize();

    mtx_init(&m_mtxUpdate, mtx_plain);

    enableRawMode();
    signal(SIGWINCH, sigwinchHandler);

    m_textBuff.start(m_pArena, g_termSize.width, g_termSize.height);

    adjustListHeight();

    LOG_GOOD("ansi::WinStart()\n");

    return true;
}

void
Win::destroy()
{
    m_textBuff.hideCursor(false);
    m_textBuff.clear();
    m_textBuff.clearKittyImages();
    m_textBuff.push(TEXT_BUFF_KEYPAD_DISABLE);
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
        &m_firstIdx
    );
}

void
Win::seekFromInput()
{
    common::seekFromInput<
        [](void* pArg) { return ((Win*)pArg)->readWChar(); },
        [](void* pArg) { ((Win*)pArg)->m_bRedraw = true; ((Win*)pArg)->update(); }
    >(this, this);
}

void
Win::subStringSearch()
{
    common::subStringSearch<
        [](void* pArg) { return ((Win*)pArg)->readWChar(); },
        [](void* pArg) { auto& self = *((Win*)pArg); self.m_bRedraw = true; self.update(); }
    >(m_pArena, &m_firstIdx, this, this);
}

void
Win::centerAroundSelection()
{
}

void
Win::adjustListHeight()
{
    const int split = app::g_pPlayer->m_imgHeight + 1;
    const int height = g_termSize.height;
    m_listHeight = height - split - 2;
}

} /* namespace platform::ansi */
