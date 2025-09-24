#pragma once

#ifdef OPT_CHAFA
    #include "platform/chafa/chafa.hh"
#endif

namespace platform::ansi
{

#ifdef OPT_CHAFA
struct TextBuffImage
{
    platform::chafa::Image img {};
    int x {};
    int y {};
};
#endif

/* ansi code */
enum class TEXT_BUFF_STYLE_CODE : int
{
    NORM = 0,
    BOLD = 1,
    DIM = 2,
    ITALIC = 3,
    UNRELINE = 4,
    BLINK = 5,
    REVERSE = 7,
    INVIS = 8,
    STRIKE = 9,

    RED = 31,
    GREEN = 32,
    YELLOW = 33,
    BLUE = 34,
    MAGENTA = 35,
    CYAN = 36,
    WHITE = 37,

    BG_RED = 41,
    BG_GREEN = 42,
    BG_YELLOW = 43,
    BG_BLUE = 44,
    BG_MAGENTA = 45,
    BG_CYAN = 46,
    BG_WHITE = 47,
};

enum class TEXT_BUFF_ARG : adt::u8
{
    TO_END, TO_BEGINNING, EVERYTHING
};

enum class TEXT_BUFF_STYLE : adt::u32
{
    NORM = 0,
    BOLD = 1,
    DIM = 1 << 1,
    ITALIC = 1 << 2,
    UNDERLINE = 1 << 3,
    BLINK = 1 << 4,
    REVERSE = 1 << 5,
    INVIS = 1 << 6,
    STRIKE = 1 << 7,

    RED = 1 << 8,
    GREEN = 1 << 9,
    YELLOW = 1 << 10,
    BLUE = 1 << 11,
    MAGENTA = 1 << 12,
    CYAN = 1 << 13,
    WHITE = 1 << 14,

    BG_RED = 1 << 15,
    BG_GREEN = 1 << 16,
    BG_YELLOW = 1 << 17,
    BG_BLUE = 1 << 18,
    BG_MAGENTA = 1 << 19,
    BG_CYAN = 1 << 20,
    BG_WHITE = 1 << 21,
};
ADT_ENUM_BITWISE_OPERATORS(TEXT_BUFF_STYLE);

/* FIXME: this is not a good approach, there are characters that can be larger in size than sizeof(wchar_t) like '/̶̢̧̠̩̠̠̪̜͚͙̏͗̏̇̑̈͛͘ͅ' (41 bytes).
 * To represent 1 terminal cell as 1 character this has to be some kind of StringView structure. */
struct TextBuffCell
{
    wchar_t wc {};
    TEXT_BUFF_STYLE eStyle {};

    /* */

    bool operator==(const TextBuffCell&) const = default;
};

struct TextBuff
{
    struct Buffer
    {
        char* pData {};
        adt::isize size {};
        adt::isize capacity {};
    };

    /* */

    adt::Arena* m_pArena {};

    adt::Arena::Ptr<Buffer> m_oBuff {};

    adt::isize m_tWidth {};
    adt::isize m_tHeight {};

    adt::isize m_newTWidth {};
    adt::isize m_newTHeight {};

    bool m_bResize {};
    bool m_bErase {};

    adt::VecM<TextBuffCell> m_vFront {}; /* what to show */
    adt::VecM<TextBuffCell> m_vBack {}; /* where to write */

#ifdef OPT_CHAFA
    /* NOTE: not using frame arena here because if SIGWINCH procs after clean() and before present()
     * the image might be forceClean()'d and gone by the next iteration.
     * A separate arena just for the image vector will be sufficient. */
    adt::Arena m_imgArena {};
    adt::Vec<TextBuffImage> m_vImages {};
#endif

    /* */

    TextBuff() = default;

    /* direct write api (cpu heavy) */
    void push(const char ch);
    void push(const char* pBuff, const adt::isize buffSize);
    void push(const adt::StringView svBuff);
    void flush();
    void moveTopLeft();
    void up(int steps);
    void down(int steps);
    void forward(int steps);
    void back(int steps);
    void move(int x, int y);
    void clearTermDown();
    void clearTermUp();
    void clearTerm();
    void clearLine(TEXT_BUFF_ARG eArg);
    void hideCursor(bool bHide);
    void pushWChar(wchar_t wc);
    void clearKittyImages();
    void setTitle(const adt::StringView svTitle);
    /* */

    /* main api (more efficient using damage tracking) */
    void start(adt::Arena* pArena, adt::isize termWidth, adt::isize termHeight);
    void destroy();
    void clean();
    void present();
    void erase();
    void resize(adt::isize width, adt::isize height);

    adt::isize string(int x, int y, TEXT_BUFF_STYLE eStyle, const adt::StringView sv, int maxSvLen = 99999);
    adt::isize wideString(int x, int y, TEXT_BUFF_STYLE eStyle, const adt::Span<const wchar_t> sp, int maxSvLen = 99999);

    adt::isize strings(int x, int y, std::initializer_list<adt::Pair<TEXT_BUFF_STYLE, const adt::StringView>> lStrings, int maxSvLen = 99999);
    adt::isize wideStrings(int x, int y, std::initializer_list<adt::Pair<TEXT_BUFF_STYLE, adt::Span<const wchar_t>>> lStrings, int maxSvLen = 99999);

#ifdef OPT_CHAFA
    void image(int x, int y, const platform::chafa::Image& img);
    void forceClean(int x, int y, int width, int height); /* remove images */
#endif
    /* */

protected:
    adt::isize styleToBuffer(adt::Span<char> spFill, TEXT_BUFF_STYLE eStyle);
    adt::Span2D<TextBuffCell> frontBufferSpan();
    adt::Span2D<TextBuffCell> backBufferSpan();
    void grow(adt::isize newCap);
    void clearBackBuffer();
    void pushDiff();
    void resetBuffers();
    void resizeBuffers(adt::isize width, adt::isize height);

    template<typename STRING_T>
    adt::isize stringFinal(int x, int y, TEXT_BUFF_STYLE eStyle, const STRING_T& s, int maxSvLen = 99999);

#ifdef OPT_CHAFA
    void showImages();
#endif
};

} /* namespace platform::ansi */
