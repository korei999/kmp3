#!/usr/bin/env bash
set -e
set -x

BIN=$(cat name)

_clean()
{
    rm -rf build
}

releaseCLANG()
{
    _clean

    if CC=clang CXX=clang++ cmake -GNinja -S . -B build/ -DCMAKE_BUILD_TYPE=Release "$@"
    then
        cmake --build build/ -j -v
    fi
}

release()
{
    _clean

    if cmake -GNinja -S . -B build/ -DCMAKE_BUILD_TYPE=Release "$@"
    then
        cmake --build build/ -j -v
    fi
}

default()
{
    _clean

    if cmake -G "Ninja" -S . -B build/ -DCMAKE_BUILD_TYPE=RelWithDebInfo "$@"
    then
        cmake --build build/ -j -v
    fi
}

debugCLANG()
{
    _clean

    if CC=clang CXX=clang++ cmake -G "Ninja" -S . -B build/ -DCMAKE_BUILD_TYPE=Debug "$@"
    then
        cmake --build build/ -j -v
    fi
}

debug()
{
    _clean

    if cmake -G "Ninja" -S . -B build/ -DCMAKE_BUILD_TYPE=Debug "$@"
    then
        cmake --build build/ -j -v
    fi
}

asanCLANG()
{
    _clean

    if CC=clang CXX=clang++ cmake -G "Ninja" -S . -B build/ -DCMAKE_BUILD_TYPE=Asan "$@"
    then
        cmake --build build/ -j -v
    fi
}

asan()
{
    _clean

    if cmake -G "Ninja" -S . -B build/ -DCMAKE_BUILD_TYPE=Asan "$@"
    then
        cmake --build build/ -j -v
    fi
}

build()
{
    cmake --build build/ -j
}

run()
{
    if cmake --build build/ -j
    then
        echo ""
        # ASAN_OPTIONS=detect_leaks=1 LSAN_OPTIONS=suppressions=leaks.txt ./build/$BIN "$@" # 2> /tmp/$BIN-dbg.txt
        # ASAN_OPTIONS=halt_on_error=0 ./build/$BIN "$@" # 2> /tmp/$BIN-dbg.txt
        # PIPEWIRE_DEBUG=3 ./build/$BIN "$@" 2> /tmp/$BIN-dbg.txt
        # ./build/$BIN "$@" 2> /tmp/$BIN-dbg.txt
        UBSAN_OPTIONS=print_stacktrace=1 LSAN_OPTIONS=suppressions=leaks.txt ./build/$BIN --forceLoggerColors "$@" 2> /tmp/$BIN-dbg.txt
        # UBSAN_OPTIONS=print_stacktrace=1 ./build/$BIN "$@" 2> /tmp/$BIN-dbg.txt
    fi
}

debugMINGW()
{
    _clean

    if cmake -G "Ninja" -S . -B build/ -DCMAKE_BUILD_TYPE=DebugMingw "$@"
    then
        cmake --build build/ -j -v
    fi
}

releaseMINGW()
{
    _clean

    if cmake -G "Ninja" -S . -B build/ -DCMAKE_BUILD_TYPE=ReleaseMingw "$@"
    then
        cmake --build build/ -j -v
    fi
}

_install()
{
    cmake --install build/
}

_uninstall()
{
    sudo rm $(cat "./build/install_manifest.txt")
}

_test()
{
    ./tests/test.sh
}

cd $(dirname $0)

case "$1" in
    default) default "${@:2}" ;;
    run) run "${@:2}" ;;
    debugCLANG) debugCLANG "${@:2}" ;;
    debug) debug "${@:2}" ;;
    asanCLANG) asanCLANG "${@:2}" ;;
    asan) asan "${@:2}" ;;
    releaseCLANG) releaseCLANG "${@:2}";;
    release) release "${@:2}";;
    install) _install ;;
    uninstall) _uninstall ;;
    debugMingw) debugMINGW ;;
    releaseMingw) releaseMINGW ;;
    clean) _clean ;;
    test) _test ;;
    *) build ;;
esac
