#include "Win.hh"

#include "adt/logs.hh"
#include "defaults.hh"
#include "keybinds.hh"

namespace platform
{
namespace ansi
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

[[maybe_unused, nodiscard]] static int
parseMouse([[maybe_unused]] Win* s, char* aBuff, u32 buffSize, [[maybe_unused]] ssize_t nRead)
{
    enum { TYPE_VT200 = 0, TYPE_1006, TYPE_1015, ESIZE };

    const char *cmp[ESIZE] = {//
        // X10 mouse encoding, the simplest one
        // \x1b [ M Cb Cx Cy
        [TYPE_VT200] = "\x1b[M",
        // xterm 1006 extended mode or urxvt 1015 extended mode
        // xterm: \x1b [ < Cb ; Cx ; Cy (M or m)
        [TYPE_1006] = "\x1b[<",
        // urxvt: \x1b [ Cb ; Cx ; Cy M
        [TYPE_1015] = "\x1b["
    };

    int type = 0;

    for (; type < ESIZE; ++type) {
        size_t size = strlen(cmp[type]);

        if (buffSize >= size && (strncmp(cmp[type], aBuff, size)) == 0) {
            break;
        }
    }

    LOG("type: {} '{}'\n", type, aBuff);

    if (type != 1) return {};

    return {};
}

[[nodiscard]] static int
parseSeq([[maybe_unused]] Win* s, char* aBuff, [[maybe_unused]] u32 buffSize, ssize_t nRead)
{
    /*LOG("{}, nRead: {}\n", *(int*)(aBuff + 1), nRead);*/

    /*if (nRead >= 10) return parseMouse(s, aBuff, buffSize, nRead);*/

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
    /*LOG("nRead: {}, ({}, {}): '{}'\n", nRead, *(u64*)aBuff, *(u64*)(aBuff + 8), aBuff);*/

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
Win::procInput()
{
    int wc = readFromStdin(this, defaults::UPDATE_RATE);

    for (const auto& k : keybinds::inl_aKeys)
    {
        if ((k.key > 0 && k.key == wc) || (k.ch > 0 && k.ch == (u32)wc))
        {
            keybinds::resolveKey(k.pfn, k.arg);
            app::g_pPlayer->m_bRedraw = true;
        }
    }
}


} /* namespace ansi */
} /* namespace platform */
