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
    auto& win = *(Win*)app::g_pWin;

    win.resizeHandler();
}

bool
Win::start(Arena* pArena)
{
    m_pArena = pArena;
    m_termSize = getTermSize();

    m_mtxUpdate = Mutex(Mutex::TYPE::PLAIN);

    enableRawMode();
    m_textBuff.start(m_pArena, m_termSize.width, m_termSize.height);

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
    if (app::mixer().m_bSongEnd.load(atomic::ORDER::ACQUIRE))
    {
        app::mixer().m_bSongEnd.store(false, atomic::ORDER::RELEASE);
        app::player().onSongEnd();
    }
    app::player().nextSongIfPrevEnded();

    update();

    m_firstIdxPrev = m_firstIdx;
}

void
Win::procEvents()
{
    procInput();

    if (m_bUpdateFirst)
    {
        common::fixFirstIdx(
            m_listHeight - 2,
            &m_firstIdx
        );
    }
}

void
Win::seekFromInput()
{
    common::seekFromInput(
        [&] { return readWChar(); },
        [&] { m_bRedraw = true; update(); }
    );
}

void
Win::subStringSearch()
{
    common::subStringSearch(m_pArena, &m_firstIdx,
        [&] { return readWChar(); },
        [&] { m_bRedraw = true; update(); }
    );
}

void
Win::centerAroundSelection()
{
}

void
Win::adjustListHeight()
{
    const int split = app::g_pPlayer->m_imgHeight + 1;
    const int height = m_termSize.height;
    m_listHeight = height - split - 2;
}

void
Win::resizeHandler()
{
    Player& pl = app::player();

    m_termSize = getTermSize();
    LOG_GOOD("term: {}\n", m_termSize);

    adjustListHeight();
    m_textBuff.resize(m_termSize.width, m_termSize.height);

    pl.m_bSelectionChanged = true;
    m_bRedraw = true;

    m_bClear = true;
    m_lastResizeTime = utils::timeNowMS();

    /* adapt list height if window size increased */
    const int listHeight = m_listHeight - 2;
    if (pl.m_focused > pl.m_vSearchIdxs.size() - listHeight && pl.m_vSearchIdxs.size() > listHeight)
        m_firstIdx = pl.m_vSearchIdxs.size() - listHeight - 1;
}

} /* namespace platform::ansi */
