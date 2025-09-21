#pragma once

#include "Logger-inl.hh"
#include "String.hh" /* IWYU pragma: keep */

#define ADT_LOGGER_COL_NORM  "\x1b[0m"
#define ADT_LOGGER_COL_RED  "\x1b[31m"
#define ADT_LOGGER_COL_GREEN  "\x1b[32m"
#define ADT_LOGGER_COL_YELLOW  "\x1b[33m"
#define ADT_LOGGER_COL_BLUE  "\x1b[34m"
#define ADT_LOGGER_COL_MAGENTA  "\x1b[35m"
#define ADT_LOGGER_COL_CYAN  "\x1b[36m"
#define ADT_LOGGER_COL_WHITE  "\x1b[37m"

#ifdef _MSC_VER
    #include <io.h>
#endif

namespace adt
{

namespace print
{

inline isize
format(Context* ctx, FormatArgs fmtArgs, const ILogger::LEVEL& x)
{
    constexpr StringView mapStrings[] {"NONE", "ERROR", "WARN", "INFO", "DEBUG"};
    ADT_ASSERT((int)x + 1 >= 0 && (int)x + 1 < utils::size(mapStrings), "{}", (int)x);
    return format(ctx, fmtArgs, mapStrings[(int)x + 1]);
}

} /* namespace print */

} /* namespace adt */

#include "StdAllocator.hh"
#include "IThreadPool.hh"
#include "Arena.hh"

namespace adt
{

inline bool
ILogger::isTTY(FILE* pFile) noexcept
{
#ifdef _MSC_VER
    return _isatty(_fileno(pFile));
#else
    return isatty(fileno(pFile));
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
#ifndef ADT_LOGGER_DISABLE
    ADT_ASSERT(eLevel >= ILogger::LEVEL::NONE && eLevel <= ILogger::LEVEL::DEBUG,
        "eLevel: {}, (min: {}, max: {})", (int)eLevel, (int)ILogger::LEVEL::NONE, (int)ILogger::LEVEL::DEBUG
    );

    ILogger* pLogger = ILogger::inst();

    if (!pLogger || eLevel > pLogger->m_eLevel) return;

    IThreadPool* pTp = IThreadPool::inst();
    if (pTp)
    {
        Arena* pArena = pTp->arena();
        if (pArena->memoryReserved() <= 0) goto fallbackToFixedBuffer;

        ArenaPushScope arenaScope {pArena};
        print::Builder pb {pArena, 512};
        StringView sv = pb.print(std::forward<ARGS>(args)...);
        while (pLogger->add(eLevel, loc, sv) == ILogger::ADD_STATUS::FAILED)
            ;
    }
    else
    {
fallbackToFixedBuffer:
        StringFixed<512> msg;
        isize n = print::toSpan(msg.data(), std::forward<ARGS>(args)...);
        while (pLogger->add(eLevel, loc, StringView{msg.data(), n}) == ILogger::ADD_STATUS::FAILED)
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
        FILE* pFile,
        ILogger::LEVEL,
        isize ringBufferSize, /* Preallocated storage in bytes for the whole lifetime of the logger. */
        bool bForceColor = false /* Output ANSI colors even if FILE is not stdout or stderr. */
    );

    /* */

    virtual ADD_STATUS add(LEVEL eLevel, std::source_location loc, const StringView sv) noexcept override;
    virtual isize formatHeader(LEVEL eLevel, std::source_location loc, Span<char> spBuff) noexcept override;
    virtual void destroy() noexcept override;

    /* */

    struct MsgHeader
    {
        LEVEL eLevel {};
        std::source_location loc {};
        isize size {}; /* Bytes after this header, so that the total msg size is sizeof(MsgHeader) + size. */
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

struct LoggerNoSource : Logger
{
    using Logger::Logger;

    virtual isize
    formatHeader(LEVEL eLevel, std::source_location, Span<char> spBuff) noexcept override
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
Logger::Logger(FILE* pFile, ILogger::LEVEL eLevel, isize ringBufferSize, bool bForceColor)
    : ILogger{pFile, eLevel, bForceColor},
      m_ring{ringBufferSize},
      m_pDrainBuff{StdAllocator::inst()->zallocV<char>(m_ring.m_cap)},
      m_mtxRing{INIT}, m_cndRing{INIT},
      m_thrd{(ThreadFn)methodPointerNonVirtual(&Logger::loop), this}
{
}

inline ILogger::ADD_STATUS
Logger::add(LEVEL eLevel, std::source_location loc, const StringView sv) noexcept
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
Logger::formatHeader(LEVEL eLevel, std::source_location loc, Span<char> spBuff) noexcept
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

    if (loc.line() != 0)
        return print::toSpan(spBuff, "({}{}{}: {}, {}): ", svCol0, eLevel, svCol1, print::shorterSourcePath(loc.file_name()), loc.line());
    else return print::toSpan(spBuff, "({}{}{}): ", svCol0, eLevel, svCol1);
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
    StdAllocator::inst()->free(m_pDrainBuff);
}

inline THREAD_STATUS
Logger::loop() noexcept
{
    char aHeaderBuff[256] {};

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

        const isize n = formatHeader(p.eLevel, p.loc, aHeaderBuff);
        fwrite(aHeaderBuff, n, 1, m_pFile);
        fwrite(m_pDrainBuff, p.sv0.m_size + p.sv1.m_size, 1, m_pFile);
    }

    return THREAD_STATUS(0);
}

inline
Logger::RingBuffer::RingBuffer(isize cap)
    : m_cap{nextPowerOf2(cap)},
      m_pData{StdAllocator::inst()->zallocV<u8>(m_cap)}
{
}

inline ILogger::ADD_STATUS
Logger::RingBuffer::push(LEVEL eLevel, std::source_location loc, const StringView sv) noexcept
{
    MsgHeader msg {.eLevel = eLevel, .loc = loc, .size = sv.size()};
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
            p.sv0.m_size = msg.size;
        }
        else
        {
            ::memcpy(&msg, m_pData + m_firstI, sizeof(msg));
            p.sv0.m_pData = (char*)(m_pData + m_firstI + sizeof(msg));
            p.sv0.m_size = nUntilEnd - sizeof(msg);

            p.sv1 = (char*)m_pData;
            p.sv1.m_size = msg.size - p.sv0.m_size;
        }
    }
    else
    {
        ::memcpy(&msg, m_pData + m_firstI, sizeof(msg));
        p.sv0.m_pData = (char*)(m_pData + m_firstI + sizeof(msg));
        p.sv0.m_size = msg.size;
    }

    p.eLevel = msg.eLevel;
    p.loc = msg.loc;

    const isize msgSize = sizeof(msg) + msg.size;
    m_firstI = (m_firstI + msgSize) & (m_cap - 1);
    m_size -= msgSize;
    ADT_ASSERT(m_size >= 0, "m_size: {}", m_size);

    return p;
}

inline void
Logger::RingBuffer::destroy() noexcept
{
    StdAllocator::inst()->free(m_pData);
    m_pData = nullptr;
    m_firstI = m_lastI = m_size = m_cap = 0;
}

} /* namespace adt */
