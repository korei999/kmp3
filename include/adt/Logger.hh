#pragma once

#include "ILogger.hh"
#include "IArena.hh"
#include "file.hh"

#ifdef _MSC_VER
    #include <io.h>
#endif

namespace adt::print
{

template<>
inline isize
format(Context* ctx, FormatArgs fmtArgs, const ILogger::LEVEL& x)
{
    constexpr StringView mapStrings[] {"NONE", "ERROR", "WARN", "INFO", "DEBUG"};
    ADT_ASSERT((int)x + 1 >= 0 && (int)x + 1 < utils::size(mapStrings), "{}", (int)x);
    return format(ctx, fmtArgs, mapStrings[(int)x + 1]);
}

} /* namespace adt::print */

#include "Gpa.hh"
#include "IThreadPool.hh"

namespace adt
{

inline bool
ILogger::isTTY(int fd) noexcept
{
#ifdef _MSC_VER
    return _isatty(fd);
#else
    return isatty(fd);
#endif
}

inline ILogger*
ILogger::inst() noexcept
{
    return ILogger::g_pInstance;
}

template<typename ...ARGS>
Log<ARGS...>::Log(ILogger::LEVEL eLevel, ARGS&&... args, const std::source_location& loc)
{
#if !defined ADT_LOGGER_DISABLE
    ADT_ASSERT(eLevel >= ILogger::LEVEL::NONE && eLevel <= ILogger::LEVEL::DEBUG,
        "eLevel: {}, (min: {}, max: {})", (int)eLevel, (int)ILogger::LEVEL::NONE, (int)ILogger::LEVEL::DEBUG
    );

    ILogger* pLogger = ILogger::inst();

    if (!pLogger || eLevel > pLogger->m_eLevel) return;

    IThreadPool* pTp = IThreadPool::inst();
    if (pTp)
    {
        IArena* pArena = pTp->arena();
        if (!pArena) goto fallbackToFixedBuffer;

        IArena::Scope arenaScope = pArena->restoreAfterScope();
        print::Builder pb {pArena, 512};
        StringView sv = pb.print(std::forward<ARGS>(args)...);
        const isize maxLen = utils::min(pLogger->cap(), sv.m_size);
        while (pLogger->add(eLevel, loc, nullptr, sv.subString(0, maxLen)) == ILogger::ADD_STATUS::FAILED)
            ;
    }
    else
    {
fallbackToFixedBuffer:
        StringFixed<256> msg;
        isize n = print::toSpan(msg.data(), std::forward<ARGS>(args)...);
        while (pLogger->add(eLevel, loc, nullptr, StringView{msg.data(), n}) == ILogger::ADD_STATUS::FAILED)
            ;
    }

#else
    (void)eLevel;
    ((void)args, ...);
    (void)loc;
#endif
}

inline void
ILogger::setGlobal(ILogger* pLogger, std::source_location loc) noexcept
{
    std::source_location prevLoc = ILogger::g_loc;
    bool bWasSet = ILogger::g_pInstance != nullptr;

    ILogger::g_pInstance = pLogger;
    ILogger::g_loc = loc;

    char aBuff[128] {};
    isize n = 0;
    if (bWasSet)
    {
        n = print::toSpan(aBuff, "(prev at: {}, {})",
            print::shorterSourcePath(prevLoc.file_name()), prevLoc.line()
        );
    }
    else
    {
        n = print::toSpan(aBuff, "(prev at: null)");
    }

    LogDebug("global logger set at: ({}, {}), {}\n",
        print::shorterSourcePath(loc.file_name()), loc.line(), StringView{aBuff, n}
    );
}

struct Logger : ILogger
{
    Logger() = default;
    Logger(
        int fd,
        ILogger::LEVEL,
        isize ringBufferSize, /* Preallocated storage in bytes for the whole lifetime of the logger. */
        bool bForceColor = false /* Output ANSI colors even if FILE is not stdout or stderr. */
    );

    /* */

    virtual ADD_STATUS add(LEVEL eLevel, std::source_location loc, void* pExtra, const StringView sv) noexcept override;
    virtual isize cap() noexcept override;
    virtual isize formatHeader(LEVEL eLevel, std::source_location loc, void* pExtra, Span<char> spBuff) noexcept override;
    virtual void destroy() noexcept override;

    /* */

    struct MsgHeader
    {
        struct LevelSize
        {
            isize m_levelSize {};

            /* */

            LevelSize() = default;
            LevelSize(LEVEL eLevel, isize size);

            /* */

            isize size() noexcept;
            LEVEL level() noexcept;
        };

        /* */

        LevelSize levelSize {}; /* Fuse msg size and level. (Total size of a msg is size() + sizeof(MsgHeader)). */
        std::source_location loc {};
    };

    struct RingBuffer
    {
        struct Popped
        {
            LEVEL eLevel {};
            std::source_location loc {};
            StringView sv0 {};
            StringView sv1 {};
        };

        /* */

        RingBuffer() = default;
        RingBuffer(isize cap);

        /* */

        void destroy() noexcept;
        ILogger::ADD_STATUS push(LEVEL eLevel, std::source_location loc, const StringView sv) noexcept;
        [[nodiscard]] Popped pop() noexcept;

        /* */

        isize m_firstI {};
        isize m_lastI {};
        isize m_size {};
        isize m_cap {};
        u8* m_pData {};
    };

    /* */

    RingBuffer m_ring {};
    char* m_pDrainBuff {};
    Mutex m_mtxRing {};
    CndVar m_cndRing {};
    Thread m_thrd {};
    bool m_bDead {};

    /* */

protected:
    THREAD_STATUS loop() noexcept;
};

inline
Logger::MsgHeader::LevelSize::LevelSize(LEVEL eLevel, isize size)
    : m_levelSize{size}
{
    ADT_ASSERT(size >= 0, "{}", size);
    m_levelSize |= ((isize)(eLevel) << 56ll);
}

inline isize
Logger::MsgHeader::LevelSize::size() noexcept
{
    return m_levelSize & ~(255ll << 56ll);
}

inline ILogger::LEVEL
Logger::MsgHeader::LevelSize::level() noexcept
{
    return (ILogger::LEVEL)(m_levelSize >> 56ll);
}

struct LoggerNoSource : Logger
{
    using Logger::Logger;

    virtual isize
    formatHeader(LEVEL eLevel, std::source_location, void*, Span<char> spBuff) noexcept override
    {
        if (eLevel == LEVEL::NONE) return 0;

        StringView svCol0 {};
        StringView svCol1 {};

        if (m_bTTY)
        {
            switch (eLevel)
            {
                case LEVEL::NONE:
                return 0;

                case LEVEL::ERR:
                svCol0 = ADT_LOGGER_COL_RED;
                break;

                case LEVEL::WARN:
                svCol0 = ADT_LOGGER_COL_YELLOW;
                break;

                case LEVEL::INFO:
                svCol0 = ADT_LOGGER_COL_BLUE;
                break;

                case LEVEL::DEBUG:
                svCol0 = ADT_LOGGER_COL_GREEN;
                break;
            }
            svCol1 = ADT_LOGGER_COL_NORM;
        }

        return print::toSpan(spBuff, "({}{}{}): ", svCol0, eLevel, svCol1);
    }
};

inline
Logger::Logger(int fd, ILogger::LEVEL eLevel, isize ringBufferSize, bool bForceColor)
    : ILogger{fd, eLevel, bForceColor},
      m_ring{ringBufferSize},
      m_pDrainBuff{Gpa::inst()->zallocV<char>(m_ring.m_cap)},
      m_mtxRing{INIT}, m_cndRing{INIT},
      m_thrd{(ThreadFn)methodPointerNonVirtual(&Logger::loop), this}
{
}

inline ILogger::ADD_STATUS
Logger::add(LEVEL eLevel, std::source_location loc, void*, const StringView sv) noexcept
{
    ADD_STATUS eStatus;
    {
        LockScope lock {&m_mtxRing};
        if (m_bDead) return ADD_STATUS::DESTROYED;
        eStatus = m_ring.push(eLevel, loc, sv);
    }
    if (eStatus == ADD_STATUS::GOOD) m_cndRing.signal();
    return eStatus;
}

inline isize
Logger::cap() noexcept
{
    return m_ring.m_cap - sizeof(MsgHeader);;
}

inline isize
Logger::formatHeader(LEVEL eLevel, std::source_location loc, void*, Span<char> spBuff) noexcept
{
    if (eLevel == LEVEL::NONE) return 0;

    StringView svCol0 {};
    StringView svCol1 {};

    if (m_bTTY)
    {
        switch (eLevel)
        {
            case LEVEL::NONE:
            return 0;

            case LEVEL::ERR:
            svCol0 = ADT_LOGGER_COL_RED;
            break;

            case LEVEL::WARN:
            svCol0 = ADT_LOGGER_COL_YELLOW;
            break;

            case LEVEL::INFO:
            svCol0 = ADT_LOGGER_COL_BLUE;
            break;

            case LEVEL::DEBUG:
            svCol0 = ADT_LOGGER_COL_CYAN;
            break;
        }
        svCol1 = ADT_LOGGER_COL_NORM;
    }

    char aTimeBuff[64] {};
    time_t now = ::time(nullptr);

#ifdef _WIN32
    tm* pTm = ::localtime(&now);
#else
    tm timeStruct {};
    tm* pTm = ::localtime_r(&now, &timeStruct);
#endif

    const isize n = strftime(aTimeBuff, sizeof(aTimeBuff), "%Y-%m-%d %I:%M:%S%p", pTm);

    if (loc.line() != 0)
        return print::toSpan(spBuff, "({}{}{}: {}, {}, {}): ", svCol0, eLevel, svCol1, StringView{aTimeBuff, n}, print::shorterSourcePath(loc.file_name()), loc.line());
    else return print::toSpan(spBuff, "({}{}{}: {}): ", svCol0, eLevel, svCol1, StringView{aTimeBuff, n});
}

inline void
Logger::destroy() noexcept
{
    {
        IThreadPool* pTp = IThreadPool::inst();
        if (pTp) pTp->wait(true);
    }

    LogDebug{"destroying logger...\n"};
    {
        {
            LockScope lock {&m_mtxRing};
            m_bDead = true;
            m_cndRing.signal();
        }
    }

    m_thrd.join();

    m_mtxRing.destroy();
    m_cndRing.destroy();
    m_ring.destroy();
    Gpa::inst()->free(m_pDrainBuff);
}

inline THREAD_STATUS
Logger::loop() noexcept
{
    char aHeaderBuff[512] {};

    while (true)
    {
        RingBuffer::Popped p {};
        {
            LockScope lock {&m_mtxRing};

            while (!m_bDead && m_ring.m_size <= 0)
                m_cndRing.wait(&m_mtxRing);

            if (m_bDead && m_ring.m_size <= 0) break;

            p = m_ring.pop();

            if (p.sv0) ::memcpy(m_pDrainBuff, p.sv0.data(), p.sv0.size());
            if (p.sv1) ::memcpy(m_pDrainBuff + p.sv0.size(), p.sv1.data(), p.sv1.size());
        }

        const isize n = formatHeader(p.eLevel, p.loc, nullptr, aHeaderBuff);
        file::writeToFd(m_fd, aHeaderBuff, n);
        file::writeToFd(m_fd, m_pDrainBuff, p.sv0.m_size + p.sv1.m_size);
    }

    return THREAD_STATUS(0);
}

inline
Logger::RingBuffer::RingBuffer(isize cap)
    : m_cap{nextPowerOf2(cap)},
      m_pData{Gpa::inst()->zallocV<u8>(m_cap)}
{
}

inline ILogger::ADD_STATUS
Logger::RingBuffer::push(LEVEL eLevel, std::source_location loc, const StringView sv) noexcept
{
    MsgHeader msg {.levelSize {eLevel, sv.size()}, .loc = loc};
    const isize payloadSize = sizeof(msg) + sv.size();

    if (m_size + payloadSize > m_cap) return ILogger::ADD_STATUS::FAILED;

    const isize nextLastI = (m_lastI + payloadSize) & (m_cap - 1);

    if (m_lastI >= m_firstI) /* Append + (opt) wrap case. */
    {
        const isize nTailRange = utils::min(payloadSize, m_cap - m_lastI);
        if ((isize)sizeof(msg) >= nTailRange)
        {
            const isize nMsgFirst = utils::min(nTailRange, (isize)sizeof(msg));
            ::memcpy(m_pData + m_lastI, &msg, nMsgFirst);
            ::memcpy(m_pData, (u8*)(&msg) + nMsgFirst, sizeof(msg) - nMsgFirst);
            ::memcpy(m_pData + sizeof(msg) - nMsgFirst, sv.data(), sv.size());
        }
        else
        {
            ::memcpy(m_pData + m_lastI, &msg, sizeof(msg));
            const isize nTailLeft = utils::min(nTailRange - (isize)sizeof(msg), sv.size());
            ::memcpy(m_pData + m_lastI + sizeof(msg), sv.data(), nTailLeft);
            ::memcpy(m_pData, sv.data() + nTailLeft, sv.size() - nTailLeft);
        }
    }
    else
    {
        ::memcpy(m_pData + m_lastI, &msg, sizeof(msg));
        ::memcpy(m_pData + m_lastI + sizeof(msg), sv.data(), sv.size());
    }

    m_lastI = nextLastI;
    m_size += payloadSize;

    return ADD_STATUS::GOOD;
}

inline Logger::RingBuffer::Popped
Logger::RingBuffer::pop() noexcept
{
    if (m_size <= 0) return {};

    Popped p {};
    MsgHeader msg {};

    if (m_firstI >= m_lastI)
    {
        const isize nUntilEnd = m_cap - m_firstI;
        if ((isize)sizeof(msg) >= nUntilEnd)
        {
            ::memcpy(&msg, m_pData + m_firstI, nUntilEnd);
            ::memcpy(((u8*)&msg) + nUntilEnd, m_pData, sizeof(msg) - nUntilEnd);

            p.sv0.m_pData = (char*)(m_pData + sizeof(msg) - nUntilEnd);
            p.sv0.m_size = msg.levelSize.size();
        }
        else
        {
            ::memcpy(&msg, m_pData + m_firstI, sizeof(msg));
            p.sv0.m_pData = (char*)(m_pData + m_firstI + sizeof(msg));
            p.sv0.m_size = nUntilEnd - sizeof(msg);

            p.sv1 = (char*)m_pData;
            p.sv1.m_size = msg.levelSize.size() - p.sv0.m_size;
        }
    }
    else
    {
        ::memcpy(&msg, m_pData + m_firstI, sizeof(msg));
        p.sv0.m_pData = (char*)(m_pData + m_firstI + sizeof(msg));
        p.sv0.m_size = msg.levelSize.size();
    }

    p.eLevel = msg.levelSize.level();
    p.loc = msg.loc;

    const isize msgSize = sizeof(msg) + msg.levelSize.size();
    m_firstI = (m_firstI + msgSize) & (m_cap - 1);
    m_size -= msgSize;
    ADT_ASSERT(m_size >= 0, "m_size: {}", m_size);

    return p;
}

inline void
Logger::RingBuffer::destroy() noexcept
{
    Gpa::inst()->free(m_pData);
    m_pData = nullptr;
    m_firstI = m_lastI = m_size = m_cap = 0;
}

} /* namespace adt */
