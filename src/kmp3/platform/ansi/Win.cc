#include "Win.hh"

#include "app.hh"
#include "common.hh"

#include <csignal>
#include <unistd.h>

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

    {
        ADT_RUNTIME_EXCEPTION_FMT(pipe(m_aFdsWakeUp) >= 0, "{}", strerror(errno));
        int flags = fcntl(m_aFdsWakeUp[0], F_GETFL, 0);
        fcntl(m_aFdsWakeUp[0], F_SETFL, flags | O_NONBLOCK);
    }

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

    close(m_aFdsWakeUp[0]);
    close(m_aFdsWakeUp[1]);

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
    u64 t = 1;
    [[maybe_unused]] auto _ = write(m_aFdsWakeUp[1], &t, sizeof(t));
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
