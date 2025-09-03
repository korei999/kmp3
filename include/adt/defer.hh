#pragma once

/* https://www.gingerbill.org/article/2015/08/19/defer-in-cpp/ */

namespace adt
{

template<typename LAMBDA_T>
struct Defer
{
    Defer(LAMBDA_T f) : m_onScopeExit(f) {}
    ~Defer() { m_onScopeExit(); }

    /* */

private:
    LAMBDA_T m_onScopeExit;
};

} /* namespace adt */

/* create unique name with ## and __COUNTER__ */
#define ADT_DEFER_GLUE2(x, y) x##y##_
#define ADT_DEFER_GLUE1(x, y) ADT_DEFER_GLUE2(x, y)

#define ADT_DEFER(...) auto ADT_DEFER_GLUE1(_clDefer, __COUNTER__) = adt::Defer([&] { __VA_ARGS__; })

#ifdef ADT_DEFER_LESS_TYPING
    #define defer(...) ADT_DEFER(__VA_ARGS__)
#endif
