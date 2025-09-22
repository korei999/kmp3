#include "Win.hh"

#include "app.hh"
#include "common.hh"

#include <csignal>
#include <unistd.h>
#include <sys/ioctl.h>

using namespace adt;

namespace platform::ansi
{

int
Win::calcImageHeightSplit()
{
    const Image img = app::decoder().getCoverImage();

    if (img.height > 0 && img.width > 0)
        return app::player().m_imgHeight + 1;
    else return 12; /* Default offset from the top to the start of the list. */
}

void
Win::disableRawMode()
{
    ADT_RUNTIME_EXCEPTION(tcsetattr(STDIN_FILENO, TCSAFLUSH, &m_termOg) != -1);
}

void
Win::enableRawMode()
{
    ADT_RUNTIME_EXCEPTION(tcgetattr(STDIN_FILENO, &m_termOg) != -1);

    struct termios raw = m_termOg;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG);
    raw.c_cc[VMIN] = 0; /* disable blocking on read() */
    /*raw.c_cc[VTIME] = 1;*/

    ADT_RUNTIME_EXCEPTION(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != -1);
}

void
sigwinchHandler(int)
{
    auto& win = *(Win*)app::g_pWin;
    win.m_bNeedsResize = true;
}

bool
Win::start(Arena* pArena)
{
    m_pArena = pArena;
    m_termSize = getTermSize();

#ifdef OPT_MPRIS
    m_fdWakeUp = eventfd(0, EFD_NONBLOCK);
#endif

    new(&m_mtxUpdate) Mutex(Mutex::TYPE::PLAIN);

    enableRawMode();
    m_textBuff.start(m_pArena, m_termSize.width, m_termSize.height);

    adjustListHeight();

    signal(SIGWINCH, sigwinchHandler);

    LogDebug("start()\n");

    return true;
}

void
Win::destroy()
{
    disableRawMode();
    m_mtxUpdate.destroy();

    m_textBuff.destroy();

    LogDebug("ansi::WinDestroy()\n");
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

    adjustListHeight();
    if (m_bUpdateFirstIdx)
    {
        common::fixFirstIdx(m_listHeight - 2, &m_firstIdx);
        m_bUpdateFirstIdx = false;
    }
}

void
Win::seekFromInput()
{
    common::seekFromInput(
        [&] { return readWChar(); },
        [&] { update(); }
    );
}

void
Win::subStringSearch()
{
    common::subStringSearch(m_pArena, &m_firstIdx,
        [&] { return readWChar(); },
        [&] { update(); }
    );
}

void
Win::wakeUp()
{
#ifdef OPT_MPRIS
    eventfd_write(m_fdWakeUp, 1);
#endif
}

void
Win::adjustListHeight()
{
    const int split = calcImageHeightSplit();
    const int height = m_termSize.height;
    m_listHeight = height - split - 2;
}

void
Win::resizeHandler()
{
    Player& pl = app::player();
    pl.m_bRedrawImage = true;

    m_termSize = getTermSize();
    LogDebug("term: {}\n", m_termSize);

    m_textBuff.resize(m_termSize.width, m_termSize.height);
    m_timerResize.reset();

    m_bClear = true;

    adjustListHeight();
    common::fixFirstIdx(m_listHeight - 2, &m_firstIdx);
}

} /* namespace platform::ansi */
