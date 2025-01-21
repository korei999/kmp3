#pragma once

#if defined __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnonnull"
#endif

namespace adt
{

/* reverse iterator adapter for auto loops */
template<typename ITERABLE_T>
struct ReverseIt
{
    using Iter = decltype(((ITERABLE_T*)nullptr)->begin());

    /* */

    ITERABLE_T& s;

    /* */

    ReverseIt(ITERABLE_T& _s) : s(_s) {};

    /* */

    struct It
    {
        Iter it;

        constexpr It(Iter _it) : it(_it) {};

        constexpr auto& operator*() { return *it; }
        constexpr auto* operator->() { return it; }

        constexpr Iter operator++() { return --it; }
        constexpr Iter operator++(int) { auto tmp = it; --it; return tmp; }
        constexpr Iter operator--() { return ++it; }
        constexpr Iter operator--(int) { auto tmp = it; ++it; return tmp; }

        friend constexpr bool operator==(It l, It r) { return l.it == r.it; }
        friend constexpr bool operator!=(It l, It r) { return l.it != r.it; }
    };

    constexpr It begin() { return s.rbegin(); }
    constexpr It end() { return s.rend(); }
    constexpr It rbegin() { return s.begin(); }
    constexpr It rend() { return s.end(); }

    constexpr const It begin() const { return s.rbegin(); }
    constexpr const It end() const { return s.rend(); }
    constexpr const It rbegin() const { return s.begin(); }
    constexpr const It rend() const { return s.end(); }
};

} /* namespace adt */

#if defined __GNUC__
#pragma GCC diagnostic pop
#endif
