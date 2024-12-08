#pragma once

#include <type_traits>

/* https://voithos.io/articles/enum-class-bitmasks/ */
/* Define bitwise operators for an enum class, allowing usage as bitmasks. */
#define ADT_ENUM_BITWISE_OPERATORS(ENUM)                                                                               \
    inline constexpr ENUM operator|(ENUM l, ENUM r)                                                                    \
    {                                                                                                                  \
        return static_cast<ENUM>(                                                                                      \
            static_cast<std::underlying_type_t<ENUM>>(l) | static_cast<std::underlying_type_t<ENUM>>(r)                \
        );                                                                                                             \
    }                                                                                                                  \
    inline constexpr ENUM operator&(ENUM l, ENUM r)                                                                    \
    {                                                                                                                  \
        return static_cast<ENUM>(                                                                                      \
            static_cast<std::underlying_type_t<ENUM>>(l) & static_cast<std::underlying_type_t<ENUM>>(r)                \
        );                                                                                                             \
    }                                                                                                                  \
    inline constexpr ENUM operator^(ENUM l, ENUM r)                                                                    \
    {                                                                                                                  \
        return static_cast<ENUM>(                                                                                      \
            static_cast<std::underlying_type_t<ENUM>>(l) ^ static_cast<std::underlying_type_t<ENUM>>(r)                \
        );                                                                                                             \
    }                                                                                                                  \
    inline constexpr ENUM operator~(ENUM e)                                                                            \
    {                                                                                                                  \
        return static_cast<ENUM>(~static_cast<std::underlying_type_t<ENUM>>(e));                                       \
    }                                                                                                                  \
    inline constexpr ENUM& operator|=(ENUM& l, ENUM r)                                                                 \
    {                                                                                                                  \
        return l = static_cast<ENUM>(                                                                                  \
                   static_cast<std::underlying_type_t<ENUM>>(l) | static_cast<std::underlying_type_t<ENUM>>(r)         \
               );                                                                                                      \
    }                                                                                                                  \
    inline constexpr ENUM& operator&=(ENUM& l, ENUM r)                                                                 \
    {                                                                                                                  \
        return l = static_cast<ENUM>(                                                                                  \
                   static_cast<std::underlying_type_t<ENUM>>(l) & static_cast<std::underlying_type_t<ENUM>>(r)         \
               );                                                                                                      \
    }                                                                                                                  \
    inline constexpr ENUM& operator^=(ENUM& l, ENUM r)                                                                 \
    {                                                                                                                  \
        return l = static_cast<ENUM>(                                                                                  \
                   static_cast<std::underlying_type_t<ENUM>>(l) ^ static_cast<std::underlying_type_t<ENUM>>(r)         \
               );                                                                                                      \
    }
