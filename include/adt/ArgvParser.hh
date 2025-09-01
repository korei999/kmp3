#pragma once

#include "Map.hh"
#include "Vec.hh"

namespace adt
{

struct ArgvParser
{
    template<typename STRING>
    struct Arg
    {
        bool bNeedsValue {};
        STRING sOneDash {}; /* May be empty. */
        STRING sTwoDashes {}; /* May be empty (but not both). */
        STRING sUsage {};
        bool (*pfn)(
            void* pAny,
            const StringView svKey,
            const StringView svVal /* Empty if !bNeedsValue.*/
        ) {};
        void* pAnyData {};
    };

    /* */

    IAllocator* m_pAlloc {};
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
        const StringView svUsage,
        int argc,
        char** argv,
        std::initializer_list<Arg<StringView>> lParsers
    );

    /* */

    void destroy() noexcept;
    bool parse();
    void printUsage();

protected:
    Pair<bool /*bSuccess*/, bool /* bIncArgc */> parseArg(isize i, const StringView svKey);
};

inline
ArgvParser::ArgvParser(IAllocator* pAlloc, const StringView svUsage, int argc, char** argv, std::initializer_list<Arg<StringView>> lParsers)
    : m_pAlloc{pAlloc},
      m_vArgParsers{pAlloc, argc},
      m_mStringToArgI{pAlloc, argc},
      m_sUsage{pAlloc, svUsage},
      m_argc{argc},
      m_argv{argv}
{
    if (argc > 0) m_sFirst = {pAlloc, argv[0]};

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

inline bool
ArgvParser::parse()
{
    bool bAllSuccess = true;

    for (isize i = 1; i < m_argc; ++i)
    {
        const StringView svArg = m_argv[i];

        if (svArg.beginsWith("--"))
        {
            const StringView svTwoDash = svArg.subString(2, svArg.size() - 2);
            auto [bSuccess, bInc] = parseArg(i, svTwoDash);
            if (!bSuccess)
            {
                bAllSuccess = false;
                goto done;
            }
            if (bInc) ++i;
        }
        else if (svArg.beginsWith("-"))
        {
            const StringView svKey = svArg.subString(1, svArg.size() - 1);
                
            for (const char& rChar : svKey)
            {
                auto [bSuccess, bInc] = parseArg(i, Span<const char>{&rChar, 1});
                if (!bSuccess)
                {
                    bAllSuccess = false;
                }

                if (bInc)
                {
                    if (svKey.size() != 1)
                    {
                        print::err("expected value after '{}' but its used in a pack\n", rChar);
                        bAllSuccess = false;
                        goto done;
                    }

                    ++i;
                }
            }
        }
    }

done:
    if (!bAllSuccess) printUsage();

    return bAllSuccess;
}

inline void
ArgvParser::printUsage()
{
    print::err("Usage: {} <args>... {}\n\n", m_sFirst, m_sUsage);
    for (auto& p : m_vArgParsers)
    {
        print::err("    {}{}" "{}" "{}{}\n        {}\n\n",
            p.sOneDash ? "-" : "", p.sOneDash,
            p.sOneDash && p.sTwoDashes ? ", " : "",
            p.sTwoDashes ? "--" : "", p.sTwoDashes,
            p.sUsage
        );
    }
}

inline Pair<bool, bool>
ArgvParser::parseArg(isize i, const StringView svKey)
{
    MapResult res = m_mStringToArgI.search(svKey);

    if (!res)
    {
        print::err("unhandled argument: '{}'\n", svKey);
        return {false, false};
    }

    const isize idx = res.value();
    auto& parser = m_vArgParsers[idx];
    bool bSuccess = true;

    if (parser.bNeedsValue)
    {
        if (i + 1 >= m_argc)
        {
            print::err("expected value after '{}'\n", svKey);
            bSuccess = false;
            goto done;
        }

        const StringView svVal = m_argv[i + 1];
        if (!parser.pfn(parser.pAnyData, svKey, svVal))
            bSuccess = false;
    }
    else
    {
        if (!parser.pfn(parser.pAnyData, svKey, {}))
            bSuccess = false;
    }

done:
    if (!bSuccess) print::err("failed to parse '{}' argument (usage: '{}')\n", svKey, parser.sUsage);

    return {bSuccess, parser.bNeedsValue};
}

} /* namespace adt */
