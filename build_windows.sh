#! /bin/sh

### BEGIN OF CONFIG #########

COMPILER="x86_64-w64-mingw32-gcc"

CUI_RENDERER_SOFTWARE="on"
CUI_RENDERER_DIRECT3D11="on"

CUI_FRAMEBUFFER_SCREENSHOT="off"

### END OF CONFIG ###########

REL_PATH=$(dirname "$0")

if [ -f "${REL_PATH}/build_config_windows.sh" ]; then
    . "${REL_PATH}/build_config_windows.sh"
fi

compile () {
    echo "[COMPILE] $1"
    if [ "${NO_JOBS}" = "-no_jobs" ]; then
        $1
    else
        $1 &
    fi
}

compile_no_jobs () {
    echo "[COMPILE] $1"
    $1
}

COMPILER_FLAGS="-std=c99 -municode -Wno-missing-field-initializers -Wno-cast-function-type"
LINKER_FLAGS="-luser32 -lgdi32 -lshell32 -luxtheme"
DEFINES=""

if [ "$(uname)" != "Linux" ]; then
    echo "It doesn't look like you are running on linux. You might want to check that."
fi

if [ "${CUI_RENDERER_SOFTWARE}" != "on" ] && [ "${CUI_RENDERER_DIRECT3D11}" != "on" ]; then
    echo "Please provide at least one renderer for cui. Software or/and Direct3D 11."
    echo "Set CUI_RENDERER_SOFTWARE or CUI_RENDERER_DIRECT3D11 in 'build_windows.sh' to \"on\"."
    exit 1
fi

if [ "${CUI_RENDERER_SOFTWARE}" = "on" ]; then
    DEFINES="${DEFINES} -DCUI_RENDERER_SOFTWARE_ENABLED=1"
fi

if [ "${CUI_RENDERER_DIRECT3D11}" = "on" ]; then
    DEFINES="${DEFINES} -DCUI_RENDERER_DIRECT3D11_ENABLED=1"
    LINKER_FLAGS="${LINKER_FLAGS} -ld3d11 -ldxguid -ld3dcompiler"
fi

if [ "${CUI_FRAMEBUFFER_SCREENSHOT}" = "on" ]; then
    DEFINES="${DEFINES} -DCUI_FRAMEBUFFER_SCREENSHOT_ENABLED=1"
fi

NO_JOBS="$1"
BUILD_TYPE="$1"

if [ "${NO_JOBS}" = "-no_jobs" ]; then
    BUILD_TYPE="$2"
fi

case "${BUILD_TYPE}" in
    "reldebug")
        COMPILER_FLAGS="${COMPILER_FLAGS} -g -O2 -DCUI_DEBUG_BUILD=1"
        ;;

    "release")
        COMPILER_FLAGS="${COMPILER_FLAGS} -O2 -DNDEBUG"
        ;;

    *)
        COMPILER_FLAGS="${COMPILER_FLAGS} -pedantic -Wall -Wextra -g -O0 -DCUI_DEBUG_BUILD=1"
        ;;
esac

if [ ! -d "${REL_PATH}/build" ]; then
    mkdir "${REL_PATH}/build"
fi

cd "${REL_PATH}/build"

# shape compile
compile "${COMPILER} ${COMPILER_FLAGS} -o shape_compile ../src/shape_compile.c"

# dynamic library
# compile "${COMPILER} ${DEFINES} ${COMPILER_FLAGS} -fPIC -shared -I../include -o libcui.so ../src/cui.c ${LINKER_FLAGS}"

# static library
compile_no_jobs "${COMPILER} ${DEFINES} ${COMPILER_FLAGS} -I../include -c -o cui.obj ../src/cui.c"
compile_no_jobs "ar -rc libcui.a cui.obj"

wait

# examples

# interface browser
compile "${COMPILER} ${COMPILER_FLAGS} -I../include -o interface_browser ../examples/interface_browser.c libcui.a ${LINKER_FLAGS}"

# image viewer
compile "${COMPILER} ${COMPILER_FLAGS} -I../include -o image_viewer ../examples/image_viewer.c libcui.a ${LINKER_FLAGS}"

# file search
compile "${COMPILER} ${COMPILER_FLAGS} -I../include -o file_search ../examples/file_search.c libcui.a ${LINKER_FLAGS}"

# color tool
compile "${COMPILER} ${COMPILER_FLAGS} -I../include -o color_tool ../examples/color_tool.c libcui.a ${LINKER_FLAGS}"

wait

echo
echo "If you want to compile with the static library you need to append the following linker flags: '${LINKER_FLAGS}'"
