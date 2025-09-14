#pragma once

#include "print-inl.hh"

#include <source_location>

#define ADT_LOGGER_COL_NORM  "\x1b[0m"
#define ADT_LOGGER_COL_RED  "\x1b[31m"
#define ADT_LOGGER_COL_GREEN  "\x1b[32m"
#define ADT_LOGGER_COL_YELLOW  "\x1b[33m"
#define ADT_LOGGER_COL_BLUE  "\x1b[34m"
#define ADT_LOGGER_COL_MAGENTA  "\x1b[35m"
#define ADT_LOGGER_COL_CYAN  "\x1b[36m"
#define ADT_LOGGER_COL_WHITE  "\x1b[37m"

namespace adt
{

struct ILogger
{
    enum class LEVEL : i8 {NONE = -1, ERR = 0, WARN, INFO, DEBUG};
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

    virtual ADD_STATUS add(LEVEL eLevel, std::source_location loc, const StringView sv) noexcept = 0;
    virtual isize formatHeader(LEVEL eLevel, std::source_location loc, Span<char> spBuff) noexcept = 0;
    virtual void destroy() noexcept = 0;

    /* */

    static bool isTTY(FILE* pFile) noexcept;
    static ILogger* inst() noexcept;
    static void setGlobal(ILogger* pInst, std::source_location = std::source_location::current()) noexcept;
};

namespace print
{

inline isize format(Context* ctx, FormatArgs fmtArgs, const ILogger::LEVEL& x);

} /* namespace print */

} /* namespace adt */
