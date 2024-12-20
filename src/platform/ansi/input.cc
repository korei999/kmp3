#include "input.hh"

#include "adt/logs.hh"
#include "defaults.hh"
#include "keybinds.hh"

namespace platform
{
namespace ansi
{
namespace input
{

#define ESC_SEQ '\x1b'

[[nodiscard]] static int
parseSeq(Win* s, char* aBuff, u32 buffSize, ssize_t nRead)
{
    LOG("{}\n", *(int*)(aBuff + 1));

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
    [[maybe_unused]] ssize_t nRead = read(STDIN_FILENO, aBuff, sizeof(aBuff));
    LOG("nRead: {}, ({}, {}): '{}'\n", nRead, *(u64*)aBuff, *(u64*)(aBuff + 8), aBuff);

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
readWChar(Win* s)
{
    namespace c = common;

    int wc = readFromStdin(s, defaults::READ_TIMEOUT);

    if (wc == keys::ESC) return c::READ_STATUS::DONE; /* esc */
    else if (wc == keys::CTRL_C) return c::READ_STATUS::DONE;
    else if (wc == keys::ENTER) return c::READ_STATUS::DONE; /* enter */
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

void
procInput(Win* s)
{
    int wc = readFromStdin(s, defaults::UPDATE_RATE);

    for (const auto& k : keybinds::inl_aKeys)
    {
        if ((k.key > 0 && k.key == wc) || (k.ch > 0 && k.ch == (u32)wc))
        {
            keybinds::resolveKey(k.pfn, k.arg);
            s->bRedraw = true;
        }
    }
}


} /* namespace input */
} /* namespace ansi */
} /* namespace platform */
