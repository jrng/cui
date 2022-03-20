#! /bin/sh

compile () {
    echo "[COMPILE] $1"
    $1
}

REL_PATH=$(dirname "$0")

COMPILER="clang"

DEBUG_COMPILER_FLAGS="-Wall -std=gnu99 -g -O0"
RELDEBUG_COMPILER_FLAGS="-std=gnu99 -g -O2"
RELEASE_COMPILER_FLAGS="-std=gnu99 -O2 -DNDEBUG"

LINKER_FLAGS="-lX11 -lm -ldl -pthread"

case "$1" in
    "reldebug")
        COMPILER_FLAGS=${RELDEBUG_COMPILER_FLAGS}
        ;;

    "release")
        COMPILER_FLAGS=${RELEASE_COMPILER_FLAGS}
        ;;

    *)
        COMPILER_FLAGS=${DEBUG_COMPILER_FLAGS}
        ;;
esac

if [ ! -d "${REL_PATH}/build" ]; then
    mkdir "${REL_PATH}/build"
fi

cd "${REL_PATH}/build"

# dynamic library
compile "${COMPILER} ${COMPILER_FLAGS} -fPIC -shared -I../include -o libcui.so ../src/cui.c ${LINKER_FLAGS}"

# static library
compile "${COMPILER} ${COMPILER_FLAGS} -I../include -c -o cui.o ../src/cui.c"
compile "ar -rc libcui.a cui.o"

# examples

# interface browser
compile "${COMPILER} ${COMPILER_FLAGS} -I../include -o interface_browser ../examples/interface_browser.c ${LINKER_FLAGS} -L. -l:libcui.a"
