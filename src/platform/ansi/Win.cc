#include "Win.hh"

#include "app.hh"
#include "common.hh"

#include "adt/logs.hh"

#include <csignal>
#include <unistd.h>
#include <sys/ioctl.h>

using namespace adt;

namespace platform::ansi
{

TermSize g_termSize {};

void
Win::disableRawMode()
{
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &m_termOg) == -1)
        throw RuntimeException("tcsetattr() failed");
}

void
Win::enableRawMode()
{
    if (tcgetattr(STDIN_FILENO, &m_termOg) == -1)
        throw RuntimeException("tcsetattr() failed");

    struct termios raw = m_termOg;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG);
    raw.c_cc[VMIN] = 0; /* disable blocking on read() */
    /*raw.c_cc[VTIME] = 1;*/

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        throw RuntimeException("tcsetattr() failed");
}

void
sigwinchHandler(int)
{
    auto& s = *(Win*)app::g_pWin;
    auto& pl = *app::g_pPlayer;

    g_termSize = getTermSize();
    LOG_GOOD("term: {}\n", g_termSize);

    s.adjustListHeight();
    s.m_textBuff.resize(g_termSize.width, g_termSize.height);

    pl.m_bSelectionChanged = true;
    s.m_bRedraw = true;

    s.m_bClear = true;
    s.m_lastResizeTime = utils::timeNowMS();

    /* adapt list height if window size increased */
    const int listHeight = s.m_listHeight - 2;
    if (pl.m_focused > pl.m_vSearchIdxs.size() - listHeight && pl.m_vSearchIdxs.size() > listHeight)
        s.m_firstIdx = pl.m_vSearchIdxs.size() - listHeight - 1;
}

bool
Win::start(Arena* pArena)
{
    m_pArena = pArena;
    g_termSize = getTermSize();

    m_mtxUpdate = Mutex(Mutex::TYPE::PLAIN);

    enableRawMode();
    m_textBuff.start(m_pArena, g_termSize.width, g_termSize.height);

    adjustListHeight();

    signal(SIGWINCH, sigwinchHandler);

    LOG_GOOD("start()\n");

    return true;
}

void
Win::destroy()
{
    disableRawMode();
    m_mtxUpdate.destroy();

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
        m_listHeight - 2,
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
