#!/usr/bin/env bash

set -euo pipefail
# set -x

PROJECT_NAME="kmp3-c"
PROJECT_VERSION="2.0"
PROJECT_VERSION_MAJOR=2
PROJECT_VERSION_MINOR=0

CC="gcc"

CFLAGS=" -std=c11"
CFLAGS+=" -fno-strict-aliasing -fms-extensions"
CFLAGS+=" -Wall -Wpedantic"
LDFLAGS=" -lm"

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

ALSA=1
PIPEWIRE=1
SNDIO=1
CHAFA=1
MPRIS=1
USER_KEYBINDS=1
LOGS=4

SSE4_2=0
AVX2=0

for arg in "$@"; do
    if [[ $arg == "gcc" ]]; then CC="gcc"
    elif [[ $arg == "clang" ]]; then CC="clang"
    elif [[ $arg == "release" ]]; then MODE="release"
    elif [[ $arg == "debug" ]]; then MODE="debug"
    elif [[ $arg == "asan" ]]; then MODE="asan"
    elif [[ $arg == "alsa" ]]; then ALSA=1
    elif [[ $arg == "no-alsa" ]]; then ALSA=0
    elif [[ $arg == "pipewire" ]]; then PIPEWIRE=1
    elif [[ $arg == "no-pipewire" ]]; then PIPEWIRE=0
    elif [[ $arg == "sndio" ]]; then SNDIO=1
    elif [[ $arg == "no-sndio" ]]; then SNDIO=0
    elif [[ $arg == "chafa" ]]; then CHAFA=1
    elif [[ $arg == "no-chafa" ]]; then CHAFA=0
    elif [[ $arg == "mpris" ]]; then MPRIS=1
    elif [[ $arg == "no-mpris" ]]; then MPRIS=0
    elif [[ $arg == "no-user-keybinds" ]]; then USER_KEYBINDS=0
    elif [[ $arg == "sse4_2" ]]; then SSE4_2=1
    elif [[ $arg == "avx2" ]]; then AVX2=1
    elif [[ $arg == "logs0" ]]; then LOGS=0
    elif [[ $arg == "logs1" ]]; then LOGS=1
    elif [[ $arg == "logs2" ]]; then LOGS=2
    elif [[ $arg == "logs3" ]]; then LOGS=3
    elif [[ $arg == "logs4" ]]; then LOGS=4
    fi
done

DBG_CFLAGS+=" -Wno-unused-but-set-variable -Wno-unused-function"

if [[ $MODE == "release" ]]; then
    CFLAGS+=" -O3"
    DEFS+=" -DNDEBUG"
    echo "building release"
elif [[ $MODE == "asan" ]]; then
    CFLAGS+=" -g -O0 -fsanitize=address $DBG_CFLAGS"
    echo "building asan"
else
    CFLAGS+=" -g -O0 $DBG_CFLAGS"
    echo "building debug"
fi

if [[ $USER_KEYBINDS == 1 ]]; then
    DEFS+=" -DOPT_USER_KEYBINDS"
    echo "adding user keybinds"
fi

if [[ $CC == "clang" ]]; then
    compilerVersion=$("$CC" --version | grep -oP '(?<=clang version )\d+' | head -1)
    if (( compilerVersion > 21 )); then
        CFLAGS+=" -fcolor-diagnostics -Wno-c2y-extensions"
    fi

    CFLAGS+=" -Wno-microsoft-anon-tag"
fi

if [[ $SSE4_2 == 1 ]]; then
    CFLAGS+=" -msse4.2"
    DEFS+=" -DOPT_SSE4_2"
    echo "using sse4.2"
fi

if [[ $AVX2 == 1 ]]; then
    CFLAGS+=" -mavx2 -msse4.2"
    DEFS+=" -DOPT_AVX2 -DOPT_SSE4_2"
    echo "using avx2"
fi

if [[ $LOGS -ge 0 && $LOGS -le 4 ]]; then
    DEFS+=" -DK_CTX_LOG_LEVEL=$LOGS"
    echo "setting compile time log level to $LOGS"
fi

OPTS=""

applyPkgConfigFlags()
{
    if pkg-config --exists $1; then
        CFLAGS+=" $(pkg-config --cflags $1)"
        LDFLAGS+=" $(pkg-config --libs $1)"
        if [[ $# -ge 3 ]]; then
            OPTS+=" -DOPT_$3"
        fi

        return 0
    else
        if [[ $2 == 1 ]]; then
            echo "library $1 has to be installed"
            exit 1
        else
            echo "library $1 not found, skipping..."
            return 1
        fi
    fi
}

applyPkgConfigFlagsAndDontLink()
{
    if pkg-config --exists $1; then
        CFLAGS+=" $(pkg-config --cflags $1)"
        if [[ $# -ge 3 ]]; then
            OPTS+=" -DOPT_$3"
        fi

        return 0
    else
        if [[ $2 == 1 ]]; then
            echo "headers for library $1 have to be installed"
            exit 1
        else
            echo "headers for library $1 not found, skipping..."
            return 1
        fi
    fi
}

GOT_SOUND=0

if [[ $ALSA == 1 ]]; then
    if applyPkgConfigFlags alsa 0 ALSA; then
        GOT_SOUND=1
        echo "adding alsa sound backend"
    fi
fi

if [[ $PIPEWIRE == 1 ]]; then
    if applyPkgConfigFlags libpipewire-0.3 0 PIPEWIRE; then
        GOT_SOUND=1
        echo "adding pipewire sound backend"
    fi
fi

if [[ $SNDIO == 1 ]]; then
    if applyPkgConfigFlags sndio 0 SNDIO; then
        GOT_SOUND=1
        echo "adding sndio sound backend"
    fi
fi

if [[ $GOT_SOUND == 0 ]]; then
    echo "we need at least one of these sound backends to build this program: pipewire (linux), alsa (linux), sndio (openbsd), coreaudio (mac)"
    exit 1
fi

if [[ $CHAFA == 1 ]]; then
    if pkg-config --exists libswscale; then
        applyPkgConfigFlags chafa 0 CHAFA
        applyPkgConfigFlagsAndDontLink libswscale 0

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
        applyPkgConfigFlags libsystemd 0 SYSTEMD
        echo "using libsystemd mpris provider"
        OPTS+=" -DOPT_MPRIS"
    elif pkg-config --exists libelogind; then
        applyPkgConfigFlags libelogind 0 ELOGIND
        echo "using libelogind mpris provider"
    elif pkg-config --exists basu; then
        applyPkgConfigFlags basu 0 BASU
        echo "using basu mpris provider"
    else
        echo "no mpris provider found"
    fi
fi

applyPkgConfigFlagsAndDontLink libavcodec 1
applyPkgConfigFlagsAndDontLink libavutil 1
applyPkgConfigFlagsAndDontLink libavutil 1

COMMAND="$CC $CFLAGS $OPTS $DEFS -I src/kmp3-c/klib/ThirdParty/ -I src/kmp3-c src/kmp3-c/main.c $LDFLAGS -o $PROJECT_NAME"
echo ${COMMAND}
exec $COMMAND
