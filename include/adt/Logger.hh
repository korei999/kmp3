#pragma once

#include "print-inl.hh"
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

struct ILogger
{
    enum class LEVEL : i8 {NONE = -1, ERR = 0, WARN, INFO, DEBUG};

    /* */

    static inline ILogger* g_pInstance;
    static inline std::source_location g_loc;

    /* */

    FILE* m_pFile {};
    LEVEL m_eLevel = LEVEL::WARN;
    bool m_bTTY = false;
    bool m_bForceColor = false;

    /* */

    ILogger() noexcept = default;
    ILogger(FILE* pFile, LEVEL eLevel, bool bForceColor = false) noexcept
        : m_pFile {pFile}, m_eLevel {eLevel}, m_bTTY {bForceColor || isTTY(pFile)}, m_bForceColor {bForceColor} {}

    /* */

    virtual bool add(LEVEL eLevel, std::source_location loc, const StringView sv) noexcept = 0;
    virtual isize formatHeader(LEVEL eLevel, std::source_location loc, Span<char> spBuff) noexcept = 0;

    /* */

    static bool isTTY(FILE* pFile) noexcept;
    static ILogger* inst() noexcept;
    static void setGlobal(ILogger* pInst, std::source_location = std::source_location::current()) noexcept;
};

namespace print
{

inline isize
format(Context* ctx, FormatArgs fmtArgs, const ILogger::LEVEL& x)
{
    constexpr StringView mapStrings[] {"", "ERROR", "WARN", "INFO", "DEBUG"};
    ADT_ASSERT((int)x + 1 >= 0 && (int)x + 1 < utils::size(mapStrings), "{}", (int)x);
    return format(ctx, fmtArgs, mapStrings[(int)x + 1]);
}

} /* namespace print */

} /* namespace adt */

#include "Queue.hh"
#include "Thread.hh"

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

template<isize SIZE = 512, typename ...ARGS>
struct Log
{    
#ifndef ADT_LOGGER_DISABLE
    Log(ILogger::LEVEL eLevel, ARGS&&... args, const std::source_location& loc = std::source_location::current())
    {
        ADT_ASSERT(eLevel >= ILogger::LEVEL::NONE && eLevel <= ILogger::LEVEL::DEBUG,
            "eLevel: {}, (min: {}, max: {})", (int)eLevel, (int)ILogger::LEVEL::NONE, (int)ILogger::LEVEL::DEBUG
        );

        ILogger* pLogger = ILogger::inst();

        if (eLevel > pLogger->m_eLevel || !pLogger) return;

        StringFixed<SIZE> msg;
        isize n = print::toSpan(msg.data(), std::forward<ARGS>(args)...);
        while (!pLogger->add(eLevel, loc, StringView{msg.data(), n}))
            ;
    }
#else
    Log(ILogger::LEVEL, ARGS&&..., [[maybe_unused]] const std::source_location& loc = std::source_location::current())
    {}
#endif
};

template<isize SIZE = 512, typename ...ARGS>
struct LogError : Log<SIZE, ARGS...>
{
    LogError(ARGS&&... args, const std::source_location& loc = std::source_location::current())
        : Log<SIZE, ARGS...>{ILogger::LEVEL::ERR, std::forward<ARGS>(args)..., loc} {}
};

template<isize SIZE = 512, typename ...ARGS>
struct LogWarn : Log<SIZE, ARGS...>
{
    LogWarn(ARGS&&... args, const std::source_location& loc = std::source_location::current())
        : Log<SIZE, ARGS...>{ILogger::LEVEL::WARN, std::forward<ARGS>(args)..., loc} {}
};

template<isize SIZE = 512, typename ...ARGS>
struct LogInfo : Log<SIZE, ARGS...>
{
    LogInfo(ARGS&&... args, const std::source_location& loc = std::source_location::current())
        : Log<SIZE, ARGS...>{ILogger::LEVEL::INFO, std::forward<ARGS>(args)..., loc} {}
};

template<isize SIZE = 512, typename ...ARGS>
struct LogDebug : Log<SIZE, ARGS...>
{
    LogDebug(ARGS&&... args, const std::source_location& loc = std::source_location::current())
        : Log<SIZE, ARGS...>{ILogger::LEVEL::DEBUG, std::forward<ARGS>(args)..., loc} {}
};

template<isize SIZE = 512, typename ...ARGS>
Log(ILogger::LEVEL eLevel, ARGS&&...) -> Log<SIZE, ARGS...>;

template<isize SIZE = 512, typename ...ARGS>
LogError(ARGS&&...) -> LogError<SIZE, ARGS...>;

template<isize SIZE = 512, typename ...ARGS>
LogWarn(ARGS&&...) -> LogWarn<SIZE, ARGS...>;

template<isize SIZE = 512, typename ...ARGS>
LogInfo(ARGS&&...) -> LogInfo<SIZE, ARGS...>;

template<isize SIZE = 512, typename ...ARGS>
LogDebug(ARGS&&...) -> LogDebug<SIZE, ARGS...>;

inline void
ILogger::setGlobal(ILogger* pInst, std::source_location loc) noexcept
{
    std::source_location prevLoc = ILogger::g_loc;
    bool bWasSet = ILogger::g_pInstance != nullptr;

    ILogger::g_pInstance = pInst;
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
    struct Msg
    {
        isize msgSize;
        LEVEL eLevel;
        std::source_location loc;
        StringFixed<488> sf;
    };

    /* */

    QueueM<Msg> m_q;
    Mutex m_mtxQ;
    CndVar m_cnd;
    bool m_bDone;
    Thread m_thrd;

    /* */

    Logger(FILE* pFile, ILogger::LEVEL eLevel, isize maxQueueSize, bool bForceColor = false);
    Logger() noexcept : m_q {}, m_mtxQ {}, m_cnd {}, m_bDone {}, m_thrd {} {}
    Logger(UninitFlag) noexcept {}

    /* */

    virtual bool add(LEVEL eLevel, std::source_location loc, const StringView sv) noexcept override;
    virtual isize formatHeader(LEVEL eLevel, std::source_location loc, Span<char> spBuff) noexcept override;

    /* */

    void destroy() noexcept;

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
                svCol0 = ADT_LOGGER_COL_CYAN;
                break;
            }
            svCol1 = ADT_LOGGER_COL_NORM;
        }

        return print::toSpan(spBuff, "({}{}{}): ", svCol0, eLevel, svCol1);
    }
};

inline
Logger::Logger(FILE* pFile, ILogger::LEVEL eLevel, isize maxQueueSize, bool bForceColor)
    : ILogger {pFile, eLevel, bForceColor},
      m_q {maxQueueSize},
      m_mtxQ {Mutex::TYPE::PLAIN},
      m_cnd {INIT},
      m_bDone {false},
      m_thrd {(ThreadFn)methodPointerNonVirtual(&Logger::loop), this}
{
}

inline THREAD_STATUS
Logger::loop() noexcept
{
    char aBuff[256] {};

    while (true)
    {
        Msg msg;
        {
            LockGuard lock {&m_mtxQ};
            while (!m_bDone && m_q.empty())
                m_cnd.wait(&m_mtxQ);

            if (m_bDone && m_q.empty()) break;

            msg = m_q.popFront();
        }

        isize n = formatHeader(msg.eLevel, msg.loc, aBuff);

        fwrite(aBuff, n, 1, m_pFile);
        fwrite(msg.sf.data(), msg.msgSize, 1, m_pFile);
    }

    return THREAD_STATUS(0);
}

inline bool
Logger::add(LEVEL eLevel, std::source_location loc, const StringView sv) noexcept
{
    isize i;
    {
        LockGuard lock {&m_mtxQ};
        i = m_q.emplaceBackNoGrow(sv.size(), eLevel, loc, sv);
    }
    m_cnd.signal();
    return i != -1;
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
            svCol0 = ADT_LOGGER_COL_CYAN;
            break;
        }
        svCol1 = ADT_LOGGER_COL_NORM;
    }

    return print::toSpan(spBuff, "({}{}{}: {}, {}): ", svCol0, eLevel, svCol1, print::shorterSourcePath(loc.file_name()), loc.line());
}

inline void
Logger::destroy() noexcept
{
    LogDebug{"destroying logger...\n"};

    {
        LockGuard lock {&m_mtxQ};
        m_bDone = true;
        m_cnd.signal();
    }

    m_thrd.join();

    m_q.destroy();
    m_mtxQ.destroy();
    m_cnd.destroy();
}

} /* namespace adt */
