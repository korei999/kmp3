# src/kmp3/platform/chafa/CMakeLists.txt

pkg_check_modules(CHAFA chafa)
pkg_check_modules(SWSCALE libswscale)

if (CHAFA_FOUND AND SWSCALE_FOUND)
    message(STATUS "CHAFA_VERSION: '${CHAFA_VERSION}'")
    if (${CHAFA_VERSION} VERSION_GREATER_EQUAL 1.14)
        target_compile_definitions(${kmp3} PRIVATE "-DOPT_CHAFA_SYMBOLS")
    endif()

    allPackageFlags(TARGET ${kmp3} NAME CHAFA)
    allPackageFlags(TARGET ${kmp3} NAME SWSCALE)

    target_compile_definitions(${kmp3} PRIVATE "-DOPT_CHAFA")
    target_sources(${kmp3} PRIVATE
        chafa.cc
    )
endif()
