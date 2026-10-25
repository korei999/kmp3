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

if command -v gcc &>/dev/null; then
    CC="gcc"
elif command -v clang &>/dev/null; then
    CC="clang"
else
    echo "no C compiler found"
    exit 1
fi

MODE="release"

PIPEWIRE=0

for arg in "$@"; do
    if [[ $arg == "gcc" ]]; then CC="gcc"
    elif [[ $arg == "clang" ]]; then CC="clang"
    elif [[ $arg == "release" ]]; then MODE="release"
    elif [[ $arg == "debug" ]]; then MODE="debug"
    elif [[ $arg == "asan" ]]; then MODE="asan"
    elif [[ $arg == "pipewire" ]]; then PIPEWIRE=1
    fi
done

if [[ $MODE == "release" ]]; then
    CFLAGS+=" -O3"
elif [[ $MODE == "asan" ]]; then
    CFLAGS+=" -g -O0 -fsanitize=address"
else
    CFLAGS+=" -g -O0"
fi

if [[ $CC == "clang" ]]; then
    compilerVersion=$("$CC" --version | grep -oP '(?<=clang version )\d+' | head -1)
    if (( compilerVersion > 21 )); then
        CFLAGS+=" -Wno-c2y-extensions"
    fi

    CFLAGS+=" -Wno-microsoft-anon-tag"
fi

DEFS=" -DPROJECT_COMMIT_HASH=\"$(git rev-parse --short HEAD)\""
DEFS+=" -DPROJECT_NAME=\"${PROJECT_NAME}\""
DEFS+=" -DPROJECT_VERSION=\"${PROJECT_VERSION}\""
DEFS+=" -DPROJECT_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}"
DEFS+=" -DPROJECT_VERSION_MINOR=${PROJECT_VERSION_MINOR}"

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

COMMAND="$CC $CFLAGS $OPTS $DEFS -I src/kmp3-c/klib/ThirdParty/ -I src/kmp3-c src/kmp3-c/main.c $LDFLAGS -o $PROJECT_NAME"
echo ${COMMAND}
exec $COMMAND
