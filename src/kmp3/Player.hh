#pragma once

#include <limits>

enum class PLAYER_REPEAT_METHOD: adt::u8 { NONE, TRACK, PLAYLIST, ESIZE };
ADT_ENUM_BITWISE_OPERATORS(PLAYER_REPEAT_METHOD);

constexpr adt::StringView mapPlayerRepeatMethodStrings[] {"None", "Track", "Playlist"};

constexpr adt::StringView
repeatMethodToString(PLAYER_REPEAT_METHOD e)
{
    return mapPlayerRepeatMethodStrings[int(e)];
}

struct Player
{
    struct Msg
    {
        enum class TYPE : adt::u8 { NOTIFY, WARNING, ERROR };
        using String64 = adt::StringFixed<128>;
        static constexpr adt::f64 UNTIL_NEXT = std::numeric_limits<adt::f64>::max();

        /* */

        String64 sfMsg {};
        adt::i64 timeMS {};
        TYPE eType {};

        /* */

        explicit operator bool() const { return bool(sfMsg); }
    };

    /* */

    adt::IAllocator* m_pAlloc {};

    struct {
        adt::StringFixed<128> sfTitle {};
        adt::StringFixed<128> sfAlbum {};
        adt::StringFixed<128> sfArtist {};
    } m_info {};

    adt::u8 m_imgHeight {};
    adt::u8 m_imgWidth {};
    adt::Vec<adt::StringView> m_vSongs {}; /* full path */
    adt::Vec<adt::StringView> m_vShortSongs {}; /* file name only */
    /* two index buffers for recursive filtering */
    adt::Vec<adt::u16> m_vSongIdxs {}; /* index buffer */
    adt::Vec<adt::u16> m_vSearchIdxs {}; /* search index buffer */
    long m_focusedI {};
    long m_selectedI {};
    adt::isize m_longestString {};
    PLAYER_REPEAT_METHOD m_eRepeatMethod {};
    bool m_bSelectionChanged {};
    bool m_bRedrawImage {};
    adt::Mutex m_mtxQ {};
    adt::QueueArray<Msg, 16> m_qErrorMsgs {};
    Msg::String64 m_sfLastMessage {};
    // adt::u64 m_lastPushedMessageTime {};
    adt::Timer m_messageTimer {};

    /* */

    Player() = default;
    Player(adt::IAllocator* p, int nArgs, char** ppArgs);

    /* */

    static bool acceptedFormat(const adt::StringView s) noexcept;

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
    void subStringSearch(adt::Arena* pAlloc, adt::Span<const wchar_t> pBuff);
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
    void setDefaultIdxs(adt::Vec<adt::u16>* pIdxs);
};
