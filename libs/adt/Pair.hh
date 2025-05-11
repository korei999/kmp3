#pragma once

#include "PairDecl.hh"

#include "print.hh"

namespace adt
{

namespace print
{

template<typename A, typename B>
inline u32
formatToContext(Context ctx, [[maybe_unused]] FormatArgs fmtArgs, const Pair<A, B>& x)
{
    ctx.fmt = "{}, {}";
    ctx.fmtIdx = 0;
    return printArgs(ctx, x.first, x.second);
}

} /* namespace print */

} /* namespace adt */
