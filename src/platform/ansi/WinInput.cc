#include "Win.hh"

#include "defaults.hh"
#include "keybinds.hh"

namespace platform::ansi
{

#define ESC_SEQ '\x1b'

enum class MOUSE : u16
{
    LEFT = 48,
    MID = 49,
    RIGHT = 50,
    SCROLL = 54,
};

enum class SCROLL_DIR : char
{
    UP = 52,
    DOWN = 53,
};

enum class BUTTON_STATE : u16
{
    PRESS = 19764,
    RELEASE = 27961,
};

[[nodiscard]] ADT_NO_UB static int
parseSeq([[maybe_unused]] Win* s, char* aBuff, [[maybe_unused]] u32 buffSize, ssize_t nRead)
{
    if (nRead <= 5)
    {
        switch (*(int*)(aBuff + 1))
        {
            case 17487:
            return keys::ARROWLEFT;

            case 17231:
            return keys::ARROWRIGHT;

            case 16719:
            return keys::ARROWUP;

            case 16975:
            return keys::ARROWDOWN;

            case 8271195:
            return keys::PGUP;

            case 8271451:
            return keys::PGDN;

            case 18511:
            return keys::HOME;

            case 17999:
            return keys::END;
        }
    }

    return 0;
}

static int
readFromStdin(Win* s, const int timeoutMS)
{
    char aBuff[128] {};
    fd_set fds {};
    FD_SET(STDIN_FILENO, &fds);

    timeval tv;
    tv.tv_sec = timeoutMS / 1000;
    tv.tv_usec = (timeoutMS - (tv.tv_sec * 1000)) * 1000;

    select(1, &fds, {}, {}, &tv);
    ssize_t nRead = read(STDIN_FILENO, aBuff, sizeof(aBuff));

    if (nRead > 1)
    {
        int k = parseSeq(s, aBuff, sizeof(aBuff), nRead);
        if (k != 0) return k;
    }

    wchar_t wc {};
    mbtowc(&wc, aBuff, sizeof(aBuff));

    return wc;
}

common::READ_STATUS
Win::readWChar()
{
    namespace c = common;

    int wc = readFromStdin(this, defaults::READ_TIMEOUT);

    if (wc == keys::ESC)
        return c::READ_STATUS::DONE; /* esc */
    else if (wc == keys::CTRL_C)
        return c::READ_STATUS::DONE;
    else if (wc == keys::ENTER)
        return c::READ_STATUS::DONE; /* enter */
    else if (wc == keys::CTRL_W)
    {
        if (c::g_input.m_idx > 0)
        {
            c::g_input.m_idx = 0;
            c::g_input.zeroOutBuff();
        }
    }
    else if (wc == 127) /* backspace */
    {
        if (c::g_input.m_idx > 0)
            c::g_input.m_aBuff[--c::g_input.m_idx] = L'\0';
        else
            return c::READ_STATUS::BACKSPACE;
    }
    else if (wc)
    {
        if (c::g_input.m_idx < utils::size(c::g_input.m_aBuff) - 1)
            c::g_input.m_aBuff[c::g_input.m_idx++] = wc;
    }

    return c::READ_STATUS::OK_;
}

void
Win::procInput()
{
    int wc = readFromStdin(this, defaults::UPDATE_RATE);

    for (const auto& k : keybinds::inl_aKeys)
    {
        if ((k.key > 0 && k.key == wc) || (k.ch > 0 && k.ch == (u32)wc))
        {
            keybinds::exec(k.pfn, k.arg);
            m_bRedraw = true;
        }
    }
}


} /* namespace platform::ansi */
