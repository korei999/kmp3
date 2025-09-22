#pragma once

#include "print-inl.hh"

#include <source_location>
#include <utility>

namespace adt
{

struct ILogger
{
    enum class LEVEL : i8 {NONE = -1, ERR = 0, WARN = 1, INFO = 2, DEBUG = 3};
    enum class ADD_STATUS : i8 {GOOD, FAILED, DESTROYED};

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

    virtual ADD_STATUS add(LEVEL eLevel, std::source_location loc, void* pExtra, const StringView sv) noexcept = 0;
    virtual isize formatHeader(LEVEL eLevel, std::source_location loc, void* pExtra, Span<char> spBuff) noexcept = 0;
    virtual void destroy() noexcept = 0;

    /* */

    static bool isTTY(FILE* pFile) noexcept;
    static ILogger* inst() noexcept;
    static void setGlobal(ILogger* pLogger, std::source_location = std::source_location::current()) noexcept;
};

namespace print
{

inline isize format(Context* ctx, FormatArgs fmtArgs, const ILogger::LEVEL& x);

} /* namespace print */

template<typename ...ARGS>
struct Log
{
    Log(ILogger::LEVEL eLevel, ARGS&&... args, const std::source_location& loc = std::source_location::current());
    Log() noexcept = default;
};

template<typename ...ARGS>
struct LogError : Log<ARGS...>
{
#if !defined ADT_LOGGER_LEVEL || (defined ADT_LOGGER_LEVEL && ADT_LOGGER_LEVEL >= 0)
    LogError(ARGS&&... args, const std::source_location& loc = std::source_location::current())
        : Log<ARGS...>{ILogger::LEVEL::ERR, std::forward<ARGS>(args)..., loc} {}
#else
    LogError(ARGS&&..., [[maybe_unused]] const std::source_location& loc = std::source_location::current()) {}
#endif
};

template<typename ...ARGS>
struct LogWarn : Log<ARGS...>
{
#if !defined ADT_LOGGER_LEVEL || (defined ADT_LOGGER_LEVEL && ADT_LOGGER_LEVEL >= 1)
    LogWarn(ARGS&&... args, const std::source_location& loc = std::source_location::current())
        : Log<ARGS...>{ILogger::LEVEL::WARN, std::forward<ARGS>(args)..., loc} {}
#else
    LogWarn(ARGS&&..., [[maybe_unused]] const std::source_location& loc = std::source_location::current()) {}
#endif
};

template<typename ...ARGS>
struct LogInfo : Log<ARGS...>
{
#if !defined ADT_LOGGER_LEVEL || (defined ADT_LOGGER_LEVEL && ADT_LOGGER_LEVEL >= 2)
    LogInfo(ARGS&&... args, const std::source_location& loc = std::source_location::current())
        : Log<ARGS...>{ILogger::LEVEL::INFO, std::forward<ARGS>(args)..., loc} {}
#else
    LogInfo(ARGS&&..., [[maybe_unused]] const std::source_location& loc = std::source_location::current()) {}
#endif
};

template<typename ...ARGS>
struct LogDebug : Log<ARGS...>
{
#if !defined ADT_LOGGER_LEVEL || (defined ADT_LOGGER_LEVEL && ADT_LOGGER_LEVEL >= 3)
    LogDebug(ARGS&&... args, const std::source_location& loc = std::source_location::current())
        : Log<ARGS...>{ILogger::LEVEL::DEBUG, std::forward<ARGS>(args)..., loc} {}
#else
    LogDebug(ARGS&&..., [[maybe_unused]] const std::source_location& loc = std::source_location::current()) {}
#endif
};

template<typename ...ARGS>
Log(ILogger::LEVEL eLevel, ARGS&&...) -> Log<ARGS...>;

template<typename ...ARGS>
LogError(ARGS&&...) -> LogError<ARGS...>;

template<typename ...ARGS>
LogWarn(ARGS&&...) -> LogWarn<ARGS...>;

template<typename ...ARGS>
LogInfo(ARGS&&...) -> LogInfo<ARGS...>;

template<typename ...ARGS>
LogDebug(ARGS&&...) -> LogDebug<ARGS...>;

} /* namespace adt */
