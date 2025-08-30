#include "Win.hh"

#include "defaults.hh"
#include "keybinds.hh"

#include <sys/select.h>

using namespace adt;

namespace platform::ansi
{

/* https://github.com/termbox/termbox2/blob/master/termbox2.h */
Win::MouseInput
Win::parseMouse(Span<char> spBuff, ssize_t nRead)
{
    enum
    {
        TYPE_VT200 = 0,
        TYPE_1006,
        TYPE_1015,
        TYPE_MAX
    };

    const char* aCmpMap[TYPE_MAX] {
        "\x1b[M",
        "\x1b[<",
        "\x1b["
    };

    int type = 0;
    for (; type < TYPE_MAX; type++)
    {
        isize size = strlen(aCmpMap[type]);

        if (spBuff.size() >= size && (strncmp(aCmpMap[type], spBuff.data(), size)) == 0)
            break;
    }

    if (type == TYPE_MAX)
        return INVALID_MOUSE;

    MouseInput ret = INVALID_MOUSE;

    switch (type)
    {
        case TYPE_VT200:
        {
            if (nRead >= 6)
            {
                int b = spBuff[3] - 0x20;
                int fail = 0;

                switch (b & 3)
                {
                    case 0:
                    ret.eKey = ((b & 64) != 0) ? MouseInput::KEY::WHEEL_UP : MouseInput::KEY::LEFT;
                    break;

                    case 1:
                    ret.eKey = ((b & 64) != 0) ? MouseInput::KEY::WHEEL_DOWN : MouseInput::KEY::MIDDLE;
                    break;

                    case 2:
                    ret.eKey = MouseInput::KEY::RIGHT;
                    break;

                    case 3:
                    ret.eKey = MouseInput::KEY::RELEASE;
                    break;

                    default:
                    ret = INVALID_MOUSE;
                    fail = 1;
                    break;
                }

                if (!fail)
                {
                    /* the coord is 1,1 for upper left */
                    ret.x = ((u8)spBuff[4]) - 0x21;
                    ret.y = ((u8)spBuff[5]) - 0x21;
                }
            }
        }
        break; /* case TYPE_VT200: */

        case TYPE_1006:
        [[fallthrough]];
        case TYPE_1015:
        {
            isize indexFail = -1;

            enum
            {
                FIRST_M = 0,
                FIRST_SEMICOLON,
                LAST_SEMICOLON,
                FIRST_LAST_MAX
            };

            isize aIndices[FIRST_LAST_MAX] = {indexFail, indexFail, indexFail};
            bool bCapital = 0;

            for (isize i = 0; i < spBuff.size(); ++i)
            {
                if (spBuff[i] == ';')
                {
                    if (aIndices[FIRST_SEMICOLON] == indexFail)
                    {
                        aIndices[FIRST_SEMICOLON] = i;
                    }
                    else
                    {
                        aIndices[LAST_SEMICOLON] = i;
                    }
                }
                else if (aIndices[FIRST_M] == indexFail)
                {
                    if (spBuff[i] == 'm' || spBuff[i] == 'M')
                    {
                        bCapital = (spBuff[i] == 'M');
                        aIndices[FIRST_M] = i;
                    }
                }
            }

            if (aIndices[FIRST_M] == indexFail || aIndices[FIRST_SEMICOLON] == indexFail ||
                aIndices[LAST_SEMICOLON] == indexFail)
            {
                return INVALID_MOUSE;
            }
            else
            {
                int start = (type == TYPE_1015 ? 2 : 3);

                unsigned n1 = strtoul(&spBuff[start], nullptr, 10);
                unsigned n2 = strtoul(&spBuff[aIndices[FIRST_SEMICOLON] + 1], nullptr, 10);
                unsigned n3 = strtoul(&spBuff[aIndices[LAST_SEMICOLON] + 1], nullptr, 10);

                if (type == TYPE_1015)
                {
                    n1 -= 0x20;
                }

                int fail = 0;

                switch (n1 & 3)
                {
                    case 0:
                    ret.eKey = ((n1 & 64) != 0) ? MouseInput::KEY::WHEEL_UP : MouseInput::KEY::LEFT;
                    break;

                    case 1:
                    ret.eKey = ((n1 & 64) != 0) ? MouseInput::KEY::WHEEL_DOWN : MouseInput::KEY::MIDDLE;
                    break;

                    case 2:
                    ret.eKey = MouseInput::KEY::RIGHT;
                    break;

                    case 3:
                    ret.eKey = MouseInput::KEY::RELEASE;
                    break;

                    default:
                    fail = 1;
                    break;
                }

                if (!fail)
                {
                    if (!bCapital)
                    {
                        ret.eKey = MouseInput::KEY::RELEASE;
                    }

                    ret.x = (n2) - 1;
                    ret.y = (n3) - 1;
                }
            }
        }
        break; /* case TYPE_1015: */
    }

    return ret;
}

void
Win::procMouse(MouseInput in)
{
    static bool s_bPressed = false;
    static bool s_bHoldTimeSlider = false;
    static bool s_bHoldScrollBar = false;

    if (in.eKey == MouseInput::KEY::RELEASE)
    {
        s_bPressed = false;
        s_bHoldTimeSlider = false;
        s_bHoldScrollBar = false;
        return;
    }

    auto& pl = app::player();
    const long width = m_termSize.width;
    const long height = m_termSize.height;

    const long xOff = m_prevImgWidth + 2;
    const int yOff = calcImageHeightSplit();

    const long listOff = yOff + 1;
    const long timeSliderOff = 10;
    const long volumeOff = 6;

    auto clHoldScrollBar = [&]
    {
        s_bHoldScrollBar = true;

        const f32 listSizeFactor = m_listHeight / f32(pl.m_vSearchIdxs.size() - 0.9999f);
        const int barHeight = utils::max(1, static_cast<int>(m_listHeight * listSizeFactor));

        const f32 sizeToListSize = f32((pl.m_vSearchIdxs.size()) + listOff) / (m_listHeight - 0.9999f + barHeight);
        const int tar = std::round(((in.y - listOff)) * sizeToListSize);
        m_firstIdx = utils::clamp(tar, 0, int(pl.m_vSearchIdxs.size() - m_listHeight) + 1);
    };

    auto clHoldTimeSlider = [&]
    {
        s_bHoldTimeSlider = true;

        f64 target = f64(in.x - xOff - 2) / f64(width - xOff - 4);
        target = utils::clamp(target, 0.0, static_cast<f64>(width - xOff - 4));
        target *= app::mixer().getTotalMS();
        app::mixer().seekMS(target);
    };

    if (s_bHoldScrollBar)
    {
        clHoldScrollBar();
        return;
    }

    if (s_bHoldTimeSlider)
    {
        clHoldTimeSlider();
        return;
    }

    /* click on time slider line */
    if (in.y == timeSliderOff && in.x >= xOff)
    {
        defer( s_bPressed = true );

        if (in.eKey == MouseInput::KEY::LEFT || in.eKey == MouseInput::KEY::RIGHT)
        {
            /* click on indicator */
            if (in.x >= xOff && in.x <= xOff + 2)
            {
                if (s_bPressed || s_bHoldTimeSlider) return;

                app::togglePause();
                return;
            }

            /* click on slider */
            if (width - xOff - 4 > 0)
            {
                if (s_bPressed && !s_bHoldTimeSlider) return;

                clHoldTimeSlider();
            }

            return;
        }
        else if (in.eKey == MouseInput::KEY::WHEEL_UP)
        {
            app::seekOff(10000);
        }
        else if (in.eKey == MouseInput::KEY::WHEEL_DOWN)
        {
            app::seekOff(-10000);
        }

        return;
    }

    /* scroll ontop of volume */
    if (in.y == volumeOff && in.x >= xOff)
    {
        if (in.eKey == MouseInput::KEY::WHEEL_DOWN)
        {
            app::volumeDown(0.1f);
        }
        else if (in.eKey == MouseInput::KEY::WHEEL_UP)
        {
            app::volumeUp(0.1f);
        }
        else if (in.eKey == MouseInput::KEY::LEFT || in.eKey == MouseInput::KEY::RIGHT)
        {
            if (s_bPressed) return;

            s_bPressed = true;
            app::toggleMute();
        }

        return;
    }

    /* click on song list */
    if (in.y < listOff || in.y >= height - 2) return;

    long target = m_firstIdx + in.y - listOff;
    target = utils::clamp(
        target,
        long(m_firstIdx),
        long(m_firstIdx + height - listOff - 3)
    );

    if (in.eKey == MouseInput::KEY::LEFT || in.eKey == MouseInput::KEY::RIGHT)
    {
        /* click on list scrollbar */
        if (in.x == m_termSize.width - 1)
        {
            clHoldScrollBar();
            return;
        }
        else
        {
            if (s_bPressed) return;
            else s_bPressed = true;

            f64 time = time::nowMS();

            {
                pl.focus(target);

                if (target == m_lastMouseSelection &&
                    time < m_lastMouseSelectionTime + defaults::DOUBLE_CLICK_DELAY
                )
                {
                    pl.selectFocused();
                }
            }

            m_lastMouseSelection = target;
            m_lastMouseSelectionTime = time;
        }
    }
    else if (in.eKey == MouseInput::KEY::WHEEL_UP)
    {
        m_firstIdx -= defaults::MOUSE_STEP;
        if (m_firstIdx < 0) m_firstIdx = 0;
    }
    else if (in.eKey == MouseInput::KEY::WHEEL_DOWN)
    {
        m_firstIdx = utils::clamp(
            i16(m_firstIdx + defaults::MOUSE_STEP),
            i16(0),
            i16((pl.m_vSearchIdxs.size() - m_listHeight) + 1)
        );
    }
}

int
Win::parseSeq(Span<char> spBuff, ssize_t nRead)
{
    if (nRead <= 5)
    {
        switch (*(int*)(&spBuff[1]))
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

Win::Input
Win::readFromStdin(const int timeoutMS)
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
        MouseInput mouse = parseMouse({aBuff, sizeof(aBuff)}, nRead);
        if (mouse != INVALID_MOUSE)
        {
            return {
                .mouse = mouse,
                .key = 0,
                .eType = Input::TYPE::MOUSE,
            };
        }

        int k = parseSeq({aBuff, sizeof(aBuff)}, nRead);
        if (k != 0)
        {
            return {
                .key = k,
                .eType = Input::TYPE::KB,
            };
        }
    }

    wchar_t wc {};
    mbtowc(&wc, aBuff, sizeof(aBuff));

    return {
        .mouse = INVALID_MOUSE,
        .key = static_cast<int>(wc),
        .eType = Input::TYPE::KB,
    };
}

common::READ_STATUS
Win::readWChar()
{
    namespace c = common;

    Input in = readFromStdin(defaults::READ_TIMEOUT);

    int wc = in.key;
    if (wc == 0 /* timeout */ || wc == keys::CTRL_C || wc == keys::ESC)
    {
        c::g_input.zeroOut();
        return c::READ_STATUS::DONE;
    }
    else if (wc == keys::ENTER)
    {
        return c::READ_STATUS::DONE;
    }
    else if (wc == keys::CTRL_W)
    {
        if (c::g_input.m_idx > 0)
        {
            c::g_input.m_idx = 0;
            c::g_input.zeroOut();
        }
    }
    else if (wc == keys::BACKSPACE)
    {
        if (c::g_input.m_idx > 0)
            c::g_input.m_aBuff[--c::g_input.m_idx] = L'\0';
        else return c::READ_STATUS::BACKSPACE;
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
    const Input in = readFromStdin(defaults::UPDATE_RATE);

    m_bUpdateFirstIdx = false;

    switch (in.eType)
    {
        case Input::TYPE::KB:
        {
            int wc = in.key;
            for (const auto& k : keybinds::inl_aKeys)
            {
                if ((k.key > 0 && k.key == wc) || (k.ch > 0 && k.ch == u32(wc)))
                {
                    keybinds::exec(k.pfn, k.arg);
                    m_bRedraw = true;
                    m_bUpdateFirstIdx = true;
                }
            }
        }
        break;

        case Input::TYPE::MOUSE:
        procMouse(in.mouse);
        break;
    }
}

} /* namespace platform::ansi */
