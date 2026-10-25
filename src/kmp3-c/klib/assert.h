#pragma once

#include "common.h" /* IWYU pragma: keep */

_Noreturn void k_assert_die(const char* ntsFile, const char* ntsFunc, ssize_t line, const char* ntsFmt, ...);

#define K_ASSERT_ALWAYS(cnd, ...)                                                                                      \
    ((cnd) ? (void)0 : k_assert_die(__FILE__, __func__, __LINE__, "[assertion( '" #cnd "' ) failed]: " __VA_ARGS__))

#define K_PANIC(...) (k_assert_die(__FILE__, __func__, __LINE__, "[panic]: " __VA_ARGS__))

#ifndef NDEBUG
    #define K_ASSERT(...) K_ASSERT_ALWAYS(__VA_ARGS__)
#else
    #define K_ASSERT(...) (void)0
#endif
