#pragma once

#include <type_traits>

/* https://voithos.io/articles/enum-class-bitmasks/ */
/* Define bitwise operators for an enum class, allowing usage as bitmasks. */
#define ADT_ENUM_BITWISE_OPERATORS(ENUM)                                                                               \
    inline constexpr ENUM operator|(ENUM Lhs, ENUM Rhs)                                                                \
    {                                                                                                                  \
        return static_cast<ENUM>(                                                                                      \
            static_cast<std::underlying_type_t<ENUM>>(Lhs) | static_cast<std::underlying_type_t<ENUM>>(Rhs)            \
        );                                                                                                             \
    }                                                                                                                  \
    inline constexpr ENUM operator&(ENUM Lhs, ENUM Rhs)                                                                \
    {                                                                                                                  \
        return static_cast<ENUM>(                                                                                      \
            static_cast<std::underlying_type_t<ENUM>>(Lhs) & static_cast<std::underlying_type_t<ENUM>>(Rhs)            \
        );                                                                                                             \
    }                                                                                                                  \
    inline constexpr ENUM operator^(ENUM Lhs, ENUM Rhs)                                                                \
    {                                                                                                                  \
        return static_cast<ENUM>(                                                                                      \
            static_cast<std::underlying_type_t<ENUM>>(Lhs) ^ static_cast<std::underlying_type_t<ENUM>>(Rhs)            \
        );                                                                                                             \
    }                                                                                                                  \
    inline constexpr ENUM operator~(ENUM E)                                                                            \
    {                                                                                                                  \
        return static_cast<ENUM>(~static_cast<std::underlying_type_t<ENUM>>(E));                                       \
    }                                                                                                                  \
    inline constexpr ENUM& operator|=(ENUM& Lhs, ENUM Rhs)                                                             \
    {                                                                                                                  \
        return Lhs = static_cast<ENUM>(                                                                                \
                   static_cast<std::underlying_type_t<ENUM>>(Lhs) | static_cast<std::underlying_type_t<ENUM>>(Lhs)     \
               );                                                                                                      \
    }                                                                                                                  \
    inline constexpr ENUM& operator&=(ENUM& Lhs, ENUM Rhs)                                                             \
    {                                                                                                                  \
        return Lhs = static_cast<ENUM>(                                                                                \
                   static_cast<std::underlying_type_t<ENUM>>(Lhs) & static_cast<std::underlying_type_t<ENUM>>(Lhs)     \
               );                                                                                                      \
    }                                                                                                                  \
    inline constexpr ENUM& operator^=(ENUM& Lhs, ENUM Rhs)                                                             \
    {                                                                                                                  \
        return Lhs = static_cast<ENUM>(                                                                                \
                   static_cast<std::underlying_type_t<ENUM>>(Lhs) ^ static_cast<std::underlying_type_t<ENUM>>(Lhs)     \
               );                                                                                                      \
    }                                                                                                                  \
    inline constexpr ENUM operator+(ENUM Lhs, int Rhs)                                                                 \
    {                                                                                                                  \
        return Lhs = static_cast<ENUM>(                                                                                \
                   static_cast<std::underlying_type_t<ENUM>>(Lhs) + static_cast<std::underlying_type_t<ENUM>>(Lhs)     \
               );                                                                                                      \
    }                                                                                                                  \
    inline constexpr ENUM operator+(ENUM Lhs, ENUM Rhs)                                                                \
    {                                                                                                                  \
        return Lhs = static_cast<ENUM>(                                                                                \
                   static_cast<std::underlying_type_t<ENUM>>(Lhs) + static_cast<std::underlying_type_t<ENUM>>(Lhs)     \
               );                                                                                                      \
    }                                                                                                                  \
    inline constexpr ENUM operator-(ENUM Lhs, int Rhs)                                                                 \
    {                                                                                                                  \
        return Lhs = static_cast<ENUM>(                                                                                \
                   static_cast<std::underlying_type_t<ENUM>>(Lhs) - static_cast<std::underlying_type_t<ENUM>>(Lhs)     \
               );                                                                                                      \
    }                                                                                                                  \
    inline constexpr ENUM operator-(ENUM Lhs, ENUM Rhs)                                                                \
    {                                                                                                                  \
        return Lhs = static_cast<ENUM>(                                                                                \
                   static_cast<std::underlying_type_t<ENUM>>(Lhs) - static_cast<std::underlying_type_t<ENUM>>(Lhs)     \
               );                                                                                                      \
    }                                                                                                                  \
    inline constexpr ENUM operator%(ENUM Lhs, ENUM Rhs)                                                                \
    {                                                                                                                  \
        return Lhs = static_cast<ENUM>(                                                                                \
                   static_cast<std::underlying_type_t<ENUM>>(Lhs) % static_cast<std::underlying_type_t<ENUM>>(Lhs)     \
               );                                                                                                      \
    }
