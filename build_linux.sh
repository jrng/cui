#! /bin/sh

### BEGIN OF CONFIG #########

COMPILER="clang"

CROSS_COMPILER=""
CROSS_COMPILE_SYSROOT=""

CUI_BACKEND_X11="on"
CUI_BACKEND_WAYLAND="on"

CUI_RENDERER_SOFTWARE="on"
CUI_RENDERER_OPENGLES2="on"

CUI_FRAMEBUFFER_SCREENSHOT="off"
CUI_RENDERER_OPENGLES2_RENDER_TIMES="off"

### END OF CONFIG ###########

REL_PATH=$(dirname "$0")

if [ -f "${REL_PATH}/build_config_linux.sh" ]; then
    . "${REL_PATH}/build_config_linux.sh"
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

COMPILER_FLAGS="-std=gnu99"
LINKER_FLAGS="-lm -pthread"
DEFINES=""

if [ "$(uname)" != "Linux" ]; then
    echo "It doesn't look like you are running on linux. You might want to check that."
fi

if [ "${CUI_BACKEND_X11}" != "on" ] && [ "${CUI_BACKEND_WAYLAND}" != "on" ]; then
    echo "Please provide at least one backend for cui. X11 or/and Wayland."
    echo "Set CUI_BACKEND_X11 or CUI_BACKEND_WAYLAND in 'build_linux.sh' to \"on\"."
    exit 1
fi

if [ "${CUI_BACKEND_X11}" = "on" ]; then
    DEFINES="${DEFINES} -DCUI_BACKEND_X11_ENABLED=1"
    LINKER_FLAGS="${LINKER_FLAGS} -lX11 -lXext -lXrandr"
fi

if [ "${CUI_BACKEND_WAYLAND}" = "on" ]; then
    DEFINES="${DEFINES} -DCUI_BACKEND_WAYLAND_ENABLED=1"
    LINKER_FLAGS="${LINKER_FLAGS} -lwayland-client -lwayland-cursor -lxkbcommon"
fi

if [ "${CUI_RENDERER_SOFTWARE}" != "on" ] && [ "${CUI_RENDERER_OPENGLES2}" != "on" ]; then
    echo "Please provide at least one renderer for cui. Software or/and OpenGL ES."
    echo "Set CUI_RENDERER_SOFTWARE or CUI_RENDERER_OPENGLES2 in 'build_linux.sh' to \"on\"."
    exit 1
fi

if [ "${CUI_RENDERER_SOFTWARE}" = "on" ]; then
    DEFINES="${DEFINES} -DCUI_RENDERER_SOFTWARE_ENABLED=1"
fi

if [ "${CUI_RENDERER_OPENGLES2}" = "on" ]; then
    DEFINES="${DEFINES} -DCUI_RENDERER_OPENGLES2_ENABLED=1"
    LINKER_FLAGS="${LINKER_FLAGS} -lEGL -lGLESv2"

    if [ "${CUI_BACKEND_WAYLAND}" = "on" ]; then
        LINKER_FLAGS="${LINKER_FLAGS} -lwayland-egl"
    fi

    if [ "${CUI_RENDERER_OPENGLES2_RENDER_TIMES}" = "on" ]; then
        DEFINES="${DEFINES} -DCUI_RENDERER_OPENGLES2_RENDER_TIMES_ENABLED=1"
    fi
fi

if [ "${CUI_FRAMEBUFFER_SCREENSHOT}" = "on" ]; then
    DEFINES="${DEFINES} -DCUI_FRAMEBUFFER_SCREENSHOT_ENABLED=1"
fi

NO_JOBS="$1"
CROSS_COMPILE="$2"
BUILD_TYPE="$3"

if [ "${NO_JOBS}" != "-no_jobs" ]; then
    BUILD_TYPE="${CROSS_COMPILE}"
    CROSS_COMPILE="${NO_JOBS}"
fi

if [ "${CROSS_COMPILE}" = "-cross_compile" ]; then
    if [ -z "${CROSS_COMPILER}" ] || [ -z "${CROSS_COMPILE_SYSROOT}" ]; then
        echo "Compiling with option '-cross_compile' but no cross compiler or sysroot given."
        echo "Set CROSS_COMPILER and CROSS_COMPILE_SYSROOT to useful values in 'build_linux.sh'."
        exit 1
    fi

    COMPILER="${CROSS_COMPILER}"
    COMPILER_FLAGS="${COMPILER_FLAGS} --sysroot=${CROSS_COMPILE_SYSROOT}"
else
    BUILD_TYPE="${CROSS_COMPILE}"
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
compile "${COMPILER} ${DEFINES} ${COMPILER_FLAGS} -fPIC -shared -I../include -o libcui.so ../src/cui.c ${LINKER_FLAGS}"

# static library
compile_no_jobs "${COMPILER} ${DEFINES} ${COMPILER_FLAGS} -I../include -c -o cui.o ../src/cui.c"
compile_no_jobs "ar -rc libcui.a cui.o"

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
