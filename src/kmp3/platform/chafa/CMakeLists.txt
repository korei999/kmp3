# src/kmp3/platform/chafa/CMakeLists.txt

pkg_check_modules(CHAFA chafa)
pkg_check_modules(SWSCALE libswscale)

if (CHAFA_FOUND AND SWSCALE_FOUND)
    message(STATUS "CHAFA_VERSION: '${CHAFA_VERSION}'")
    if (${CHAFA_VERSION} VERSION_GREATER_EQUAL 1.14)
        target_compile_definitions(${subProj} PRIVATE "-DOPT_CHAFA_SYMBOLS")
    endif()

    allPackageFlags(TARGET ${subProj} NAME CHAFA)
    # allPackageFlags(TARGET ${subProj} NAME SWSCALE)

    target_include_directories(${subProj} PRIVATE ${SWSCALE_INCLUDE_DIRS})
    target_compile_options(${subProj} PRIVATE ${SWSCALE_CFLAGS})

    target_compile_definitions(${subProj} PRIVATE "-DOPT_CHAFA")
    target_sources(${subProj} PRIVATE
        chafa.cc
    )
endif()
