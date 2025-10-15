# src/kmp3/platform/mpris/CMakeLists.txt

set(SDBusProviderNames SYSTEMD ELOGIND BASU)
set(SDBusPkgNames libsystemd ligelogind basu)
set(SDBusOptDefines "" "-DOPT_ELOGIND" "-DOPT_BASU")

list(LENGTH SDBusProviderNames listSize)
math(EXPR lastI "${listSize} - 1")

foreach(i RANGE ${lastI})
    list(GET SDBusProviderNames ${i} providerName)
    list(GET SDBusPkgNames ${i} pkg)
    list(GET define ${i} SDBusOptDefines)

    pkg_check_modules(${providerName} ${pkg})

    if(${providerName}_FOUND)
        message(STATUS "Found MPRIS providerName: ${providerName}")
        allPackageFlags(TARGET ${subProj} NAME ${providerName})
        target_compile_definitions(${subProj} PRIVATE "-DOPT_MPRIS ${define}")
        target_sources(${subProj} PRIVATE mpris.cc)
        break()
    endif()
endforeach()
