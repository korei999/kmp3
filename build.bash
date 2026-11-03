#!/usr/bin/env bash

set -euo pipefail

CC="gcc"

CFLAGS=" -std=c11"
CFLAGS+=" -fno-strict-aliasing -fms-extensions"
CFLAGS+=" -Wall -Wpedantic"
LDFLAGS=" -lm"

PROJECT_NAME="kmp3-c"
PROJECT_VERSION="2.0"
PROJECT_VERSION_MAJOR=2
PROJECT_VERSION_MINOR=0

DEFS=" -DPROJECT_COMMIT_HASH=\"$(git rev-parse --short HEAD)\""
DEFS+=" -DPROJECT_NAME=\"${PROJECT_NAME}\""
DEFS+=" -DPROJECT_VERSION=\"${PROJECT_VERSION}\""
DEFS+=" -DPROJECT_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}"
DEFS+=" -DPROJECT_VERSION_MINOR=${PROJECT_VERSION_MINOR}"

if command -v gcc &>/dev/null; then
    CC="gcc"
elif command -v clang &>/dev/null; then
    CC="clang"
else
    echo "no C compiler found"
    exit 1
fi

MODE="release"

PIPEWIRE=1
CHAFA=1
MPRIS=1

for arg in "$@"; do
    if [[ $arg == "gcc" ]]; then CC="gcc"
    elif [[ $arg == "clang" ]]; then CC="clang"
    elif [[ $arg == "release" ]]; then MODE="release"
    elif [[ $arg == "debug" ]]; then MODE="debug"
    elif [[ $arg == "asan" ]]; then MODE="asan"
    elif [[ $arg == "pipewire" ]]; then PIPEWIRE=1
    elif [[ $arg == "chafa" ]]; then CHAFA=1
    elif [[ $arg == "mpris" ]]; then MPRIS=1
    fi
done

if [[ $MODE == "release" ]]; then
    CFLAGS+=" -O3"
    DEFS+=" -DNDEBUG"
elif [[ $MODE == "asan" ]]; then
    CFLAGS+=" -g -O0 -fsanitize=address"
else
    CFLAGS+=" -g -O0 -Wno-unused-but-set-variable"
fi

if [[ $CC == "clang" ]]; then
    compilerVersion=$("$CC" --version | grep -oP '(?<=clang version )\d+' | head -1)
    if (( compilerVersion > 21 )); then
        CFLAGS+=" -Wno-c2y-extensions"
    fi

    CFLAGS+=" -Wno-microsoft-anon-tag"
fi

OPTS=""

applyPkgConfigFlags()
{
    if pkg-config --exists $2; then
        OPTS+=" -DOPT_$1"
        CFLAGS+=" $(pkg-config --cflags $2)"
        LDFLAGS+=" $(pkg-config --libs $2)"
    else
        if [[ $3 == 1 ]]; then
            echo "library $2 has to be installed"
            exit 1
        else
            echo "library $2 not found, skipping..."
        fi
    fi
}

if [[ $PIPEWIRE == 1 ]]; then
    applyPkgConfigFlags PIPEWIRE libpipewire-0.3 1
fi

if [[ $CHAFA == 1 ]]; then
    if pkg-config --exists libswscale; then
        applyPkgConfigFlags CHAFA chafa 0
        applyPkgConfigFlags CHAFA libswscale 0

        if pkg-config --atleast-version 1.14 chafa; then
        OPTS+=" -DOPT_CHAFA_SYMBOLS"
    fi
    else
        echo "libswscale is required to build with chafa"
    fi
else
    echo "chafa not found"
fi

if [[ $MPRIS == 1 ]]; then
    if pkg-config --exists libsystemd; then
        applyPkgConfigFlags SYSTEMD libsystemd 0
        echo "using libsystemd mpris provider"
        OPTS+=" -DOPT_MPRIS"
    elif pkg-config --exists libelogind; then
        applyPkgConfigFlags ELOGIND libelogind 0
        echo "using libelogind mpris provider"
    elif pkg-config --exists basu; then
        applyPkgConfigFlags BASU basu 0
        echo "using basu mpris provider"
    else
        echo "no mpris provider found"
    fi
fi

applyPkgConfigFlags FFMPEG libavformat libavcodec libavutil libswresample 1

COMMAND="$CC $CFLAGS $OPTS $DEFS -I src/kmp3-c/klib/ThirdParty/ -I src/kmp3-c src/kmp3-c/main.c $LDFLAGS -o $PROJECT_NAME"
echo ${COMMAND}
exec $COMMAND
