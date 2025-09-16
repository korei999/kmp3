#pragma once

#include "Map.hh"
#include "Vec.hh"

namespace adt
{

struct ArgvParser
{
    enum class RESULT : u8 { GOOD, FAILED, QUIT_NICELY, QUIT_BADLY, SHOW_USAGE, SHOW_ALL_USAGE };

    template<typename STRING>
    struct Arg
    {
        bool bNeedsValue {};
        STRING sOneDash {}; /* May be empty. */
        STRING sTwoDashes {}; /* May be empty (but not both). */
        STRING sUsage {};
        RESULT (*pfn)(
            ArgvParser* pSelf,
            void* pAny,
            const StringView svKey,
            const StringView svVal /* Empty if !bNeedsValue.*/
        ) {};
        void* pAnyData {};
    };

    /* */

    IAllocator* m_pAlloc {};
    FILE* m_pFile {};
    Vec<Arg<String>> m_vArgParsers {};
    Map<StringView, isize> m_mStringToArgI {};
    String m_sFirst {};
    String m_sUsage {};
    int m_argc {};
    char** m_argv {};

    /* */

    ArgvParser() noexcept = default;
    ArgvParser(
        IAllocator* pAlloc,
        FILE* pFile,
        const StringView svUsage,
        int argc,
        char** argv,
        std::initializer_list<Arg<StringView>> lParsers
    );

    /* */

    void destroy() noexcept;
    RESULT parse();
    void printUsage(IAllocator* pAlloc);
    void printFullUsage(IAllocator* pAlloc);

protected:
    Pair<RESULT, bool /* bIncArgc */> parseArg(isize i, bool bTwoDash, const StringView svKey);
};

inline
ArgvParser::ArgvParser(IAllocator* pAlloc, FILE* pFile, const StringView svUsage, int argc, char** argv, std::initializer_list<Arg<StringView>> lParsers)
    : m_pAlloc{pAlloc},
      m_pFile{pFile},
      m_vArgParsers{pAlloc, argc},
      m_mStringToArgI{pAlloc, argc},
      m_sUsage{pAlloc, svUsage},
      m_argc{argc},
      m_argv{argv}
{
    if (argc > 0) m_sFirst = {pAlloc, argv[0]};

    m_vArgParsers.setCap(m_pAlloc, lParsers.size());
    for (auto& e : lParsers)
    {
        ADT_ASSERT(!e.sOneDash.empty() || !e.sTwoDashes.empty(),
            "should have at least one arg, oneDash: '{}', sTwoDashes: '{}'",
            e.sOneDash, e.sTwoDashes
        );

        const isize idx = m_vArgParsers.push(m_pAlloc, {
            .bNeedsValue = e.bNeedsValue,
            .sOneDash = {m_pAlloc, e.sOneDash},
            .sTwoDashes = {m_pAlloc, e.sTwoDashes},
            .sUsage = {m_pAlloc, e.sUsage},
            .pfn = e.pfn
        });

        if (!e.sOneDash.empty())
            m_mStringToArgI.insert(m_pAlloc, e.sOneDash, idx);
        if (!e.sTwoDashes.empty())
            m_mStringToArgI.insert(m_pAlloc, m_vArgParsers[idx].sTwoDashes, idx);
    }
}

inline void
ArgvParser::destroy() noexcept
{
    for (auto& e : m_vArgParsers)
    {
        e.sOneDash.destroy(m_pAlloc);
        e.sTwoDashes.destroy(m_pAlloc);
        e.sUsage.destroy(m_pAlloc);
    }
    m_vArgParsers.destroy(m_pAlloc);
    m_mStringToArgI.destroy(m_pAlloc);
    m_sFirst.destroy(m_pAlloc);
    m_sUsage.destroy(m_pAlloc);
}

inline ArgvParser::RESULT
ArgvParser::parse()
{
    RESULT eFinalResult = RESULT::GOOD;

    for (isize i = 1; i < m_argc; ++i)
    {
        const StringView svArg = m_argv[i];

        const bool bTwoDash = svArg.beginsWith("--");
        const bool bOneDash = svArg.beginsWith("-");
        const bool bHasEquals = svArg.contains('=');

        if (bTwoDash || (bOneDash && bHasEquals))
        {
            const StringView svRealArg = [&] {
                if (bTwoDash) return svArg.subString(2);
                else return svArg.subString(1);
            }();

            auto [eResult, bInc] = parseArg(i, true, svRealArg);
            if (eResult != RESULT::GOOD)
            {
                eFinalResult = eResult;
                goto done;
            }
            if (bInc) ++i;
        }
        else if (bOneDash)
        {
            const StringView svKey = svArg.subString(1);
                
            for (const char& rChar : svKey)
            {
                auto [eResult, bInc] = parseArg(i, false, Span<const char>{&rChar, 1});
                eFinalResult = eResult;

                if (bInc)
                {
                    if (svKey.size() != 1)
                    {
                        print::toFILE(m_pFile, "'{}' expects value but it was used in a pack\n", rChar);
                        eFinalResult = RESULT::QUIT_BADLY;
                        goto done;
                    }

                    ++i;
                }
            }
        }
        else
        {
            break;
        }
    }

done:
    return eFinalResult;
}

inline void
ArgvParser::printUsage(IAllocator* pAlloc)
{
    print::toFILE(pAlloc, m_pFile, "Usage: {} {}\n", m_sFirst, m_sUsage);
}

inline void
ArgvParser::printFullUsage(IAllocator* pAlloc)
{
    print::toFILE(pAlloc, m_pFile, "Usage: {} {}\n\n", m_sFirst, m_sUsage);
    for (auto& p : m_vArgParsers)
    {
        print::toFILE(pAlloc, m_pFile,
            "    {}{}" "{}" "{}{}{}\n        {}\n\n",
            p.sOneDash ? "-" : "", p.sOneDash,
            p.sOneDash && p.sTwoDashes ? ", " : "",
            p.sTwoDashes ? "--" : "", p.sTwoDashes,
            p.bNeedsValue ? "=[value]" : "",
            p.sUsage
        );
    }
}

inline Pair<ArgvParser::RESULT, bool>
ArgvParser::parseArg(isize i, bool bTwoDash, const StringView svKey)
{
    const isize equalsI = svKey.charAt('=');
    bool bInc = false;
    const StringView svRealKey = [&] {
        return (equalsI == -1 ? svKey : svKey.subString(0, equalsI));
    }();
    MapResult<StringView, isize> res = m_mStringToArgI.search(svRealKey);

    if (!res)
    {
        print::toFILE(m_pAlloc, m_pFile, "unknown argument key: '{}'\n", svRealKey);
        return {RESULT::FAILED, false};
    }

    const isize idx = res.value();
    auto& parser = m_vArgParsers[idx];
    RESULT eResult = RESULT::GOOD;

    if (parser.bNeedsValue)
    {
        if (bTwoDash)
        {
            if (equalsI == -1)
            {
                print::toFILE(m_pAlloc, m_pFile, "no '=' sign after '{}'\n", svRealKey);
                eResult = RESULT::QUIT_BADLY;
                goto done;
            }
            else
            {
                if ((svKey.size() - (equalsI + 1)) <= 0)
                {
                    print::toFILE(m_pAlloc, m_pFile, "no value after '{}'\n", svKey);
                    eResult = RESULT::QUIT_BADLY;
                    goto done;
                }

                const StringView svVal = svKey.subString(equalsI + 1);
                eResult = parser.pfn(this, parser.pAnyData, svRealKey, svVal);
            }
        }
        else
        {
            if (equalsI == -1 && i + 1 >= m_argc)
            {
                print::toFILE(m_pAlloc, m_pFile, "no value after '{}'\n", svKey);
                eResult = RESULT::QUIT_BADLY;
                goto done;
            }

            if (equalsI > 0)
            {
                if (equalsI > 0 && (svKey.size() - (equalsI + 1)) <= 0)
                {
                    print::toFILE(m_pAlloc, m_pFile, "no value after '{}'\n", svKey);
                    eResult = RESULT::QUIT_BADLY;
                    goto done;
                }

                const StringView svVal = svKey.subString(equalsI + 1);
                eResult = parser.pfn(this, parser.pAnyData, svRealKey, svVal);
            }
            else
            {
                const StringView svVal = m_argv[i + 1];
                eResult = parser.pfn(this, parser.pAnyData, svRealKey, svVal);
                bInc = true;
            }
        }
    }
    else
    {
        eResult = parser.pfn(this, parser.pAnyData, svRealKey, {});
    }

done:
    if (eResult == RESULT::FAILED || eResult == RESULT::QUIT_BADLY)
        print::toFILE(m_pAlloc, m_pFile, "failed to parse '{}' argument (usage: '{}')\n", svKey, parser.sUsage);
    else if (eResult == RESULT::SHOW_USAGE)
        print::toFILE(m_pAlloc, m_pFile, "{}\n", parser.sUsage);
    else if (eResult == RESULT::SHOW_ALL_USAGE)
        printFullUsage(m_pAlloc);

    return {eResult, bInc};
}

} /* namespace adt */
