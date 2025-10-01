#pragma once

#include <limits>

enum class PLAYER_REPEAT_METHOD: u8 { NONE, TRACK, PLAYLIST, ESIZE };
ADT_ENUM_BITWISE_OPERATORS(PLAYER_REPEAT_METHOD);

constexpr StringView mapPlayerRepeatMethodStrings[] {"None", "Track", "Playlist"};

constexpr StringView
repeatMethodToString(PLAYER_REPEAT_METHOD e)
{
    return mapPlayerRepeatMethodStrings[int(e)];
}

struct Player
{
    struct Msg
    {
        enum class TYPE : u8 { NOTIFY, WARNING, ERROR };
        using String128 = StringFixed<128>;
        static constexpr f64 UNTIL_NEXT = std::numeric_limits<f64>::max();

        /* */

        String128 sfMsg {};
        i64 timeMS {};
        TYPE eType {};

        /* */

        explicit operator bool() const { return bool(sfMsg); }
    };

    /* */

    IAllocator* m_pAlloc {};

    struct {
        VString sTitle {};
        VString sAlbum {};
        VString sArtist {};
    } m_info {};

    u8 m_imgHeight {};
    u8 m_imgWidth {};
    Vec<StringView> m_vSongs {}; /* full path */
    Vec<StringView> m_vShortSongs {}; /* file name only */
    /* two index buffers for recursive filtering */
    Vec<u16> m_vSongIdxs {}; /* index buffer */
    Vec<u16> m_vSearchIdxs {}; /* search index buffer */
    long m_focusedI {};
    long m_selectedI {};
    isize m_longestString {};
    PLAYER_REPEAT_METHOD m_eRepeatMethod {};
    Mutex m_mtxQ {};
    QueueArray<Msg, 16> m_qErrorMsgs {};
    Msg::String128 m_sfLastMessage {};
    time::Type m_lastMessageTime {};
    bool m_bSelectionChanged {};
    bool m_bRedrawImage {};
    bool m_bQuitOnSongEnd {};

    /* */

    Player() = default;
    Player(IAllocator* p, int nArgs, char** ppArgs);

    /* */

    static bool acceptedFormat(const StringView s) noexcept;

    /* */

    void focusNext() noexcept;
    void focusPrev() noexcept;
    void focus(long i) noexcept;
    void focusFirst() { focus(0); }
    void setDefaultSongIdxs() { setDefaultIdxs(&m_vSongIdxs); }
    void setDefaultSearchIdxs() { setDefaultIdxs(&m_vSearchIdxs); }
    void setAllDefaultIdxs() { setDefaultSongIdxs(); setDefaultSearchIdxs(); }
    void focusLast() noexcept;
    long findSongI(long selI);
    void focusSelected();
    void focusSelectedAtCenter();
    void subStringSearch(Arena* pAlloc, Span<const wchar_t> pBuff);
    void selectFocused(); /* starts playing focused song */
    void pause(bool bPause);
    void togglePause();
    void nextSongIfPrevEnded();
    PLAYER_REPEAT_METHOD cycleRepeatMethods(bool bForward);
    void select(long i);
    void selectNext();
    void selectPrev();
    void copySearchToSongIdxs();
    void setImgSize(long height);
    void adjustImgWidth() noexcept;
    void destroy();
    void pushErrorMsg(const Msg& msg);
    Msg popErrorMsg();

    /* */

protected:
    long nextSelectionI(long selI);
    void updateInfo() noexcept;
    void selectFinal(long selI);
    void setDefaultIdxs(Vec<u16>* pIdxs);
};
