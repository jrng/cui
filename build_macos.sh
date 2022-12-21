#! /bin/sh

### BEGIN OF CONFIG #########

COMPILER="clang"

CUI_RENDERER_SOFTWARE="on"
CUI_RENDERER_METAL="on"

CUI_FRAMEBUFFER_SCREENSHOT="off"

### END OF CONFIG ###########

REL_PATH=$(dirname "$0")

if [ -f "${REL_PATH}/build_config_macos.sh" ]; then
    source "${REL_PATH}/build_config_macos.sh"
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

package () {
    echo "[PACKAGE] $1"
    rm -rf "$1.app"

    mkdir -p "$1.app/Contents/MacOS"
    mkdir -p "$1.app/Contents/Resources"

    cp "$2" "$1.app/Contents/MacOS/"
    cp "../examples/$2.Info.plist" "$1.app/Contents/Info.plist"
}

COMPILER_FLAGS="-ObjC -std=c99 -Wno-nullability-completeness"
LINKER_FLAGS="-framework AppKit"
DEFINES=""

if [ "$(uname)" != "Darwin" ]; then
    echo "It doesn't look like your running on macos. You might want to check that."
fi

if [ "${CUI_RENDERER_SOFTWARE}" != "on" ] && [ "${CUI_RENDERER_METAL}" != "on" ]; then
    echo "Please provide at least one renderer for cui. Software or/and Metal."
    echo "Set CUI_RENDERER_SOFTWARE or CUI_RENDERER_METAL in 'build_macos.sh' to \"on\"."
    exit 1
fi

if [ "${CUI_RENDERER_SOFTWARE}" = "on" ]; then
    DEFINES="${DEFINES} -DCUI_RENDERER_SOFTWARE_ENABLED=1"
fi

if [ "${CUI_RENDERER_METAL}" = "on" ]; then
    DEFINES="${DEFINES} -DCUI_RENDERER_METAL_ENABLED=1"
    LINKER_FLAGS="${LINKER_FLAGS} -framework Quartz -framework Metal"
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

if [ "$(uname -m)" = "arm64" ]; then

    # dynamic library
    compile "${COMPILER} ${DEFINES} ${COMPILER_FLAGS} -fPIC -shared -I../include -o libcui-arm64.so -target arm64-apple-macos11 ../src/cui.c ${LINKER_FLAGS}"
    compile "${COMPILER} ${DEFINES} ${COMPILER_FLAGS} -fPIC -shared -I../include -o libcui-x86_64.so -target x86_64-apple-macos10.14 ../src/cui.c ${LINKER_FLAGS}"

    # static library
    compile "${COMPILER} ${DEFINES} ${COMPILER_FLAGS} -I../include -c -o cui-arm64.o -target arm64-apple-macos11 ../src/cui.c"
    compile "${COMPILER} ${DEFINES} ${COMPILER_FLAGS} -I../include -c -o cui-x86_64.o -target x86_64-apple-macos10.14 ../src/cui.c"

    wait

    # dynamic library
    compile "lipo -create -output libcui.so libcui-arm64.so libcui-x86_64.so"

    # static library
    compile "ar -rc libcui-arm64.a cui-arm64.o"
    compile "ar -rc libcui-x86_64.a cui-x86_64.o"

    wait

    # static library
    compile_no_jobs "lipo -create -output libcui.a libcui-arm64.a libcui-x86_64.a"

    # examples

    # interface browser
    compile "${COMPILER} ${COMPILER_FLAGS} -I../include -o interface_browser-arm64 -target arm64-apple-macos11 ../examples/interface_browser.c libcui.a ${LINKER_FLAGS}"
    compile "${COMPILER} ${COMPILER_FLAGS} -I../include -o interface_browser-x86_64 -target x86_64-apple-macos10.14 ../examples/interface_browser.c libcui.a ${LINKER_FLAGS}"

    # image viewer
    compile "${COMPILER} ${COMPILER_FLAGS} -I../include -o image_viewer-arm64 -target arm64-apple-macos11 ../examples/image_viewer.c libcui.a ${LINKER_FLAGS}"
    compile "${COMPILER} ${COMPILER_FLAGS} -I../include -o image_viewer-x86_64 -target x86_64-apple-macos10.14 ../examples/image_viewer.c libcui.a ${LINKER_FLAGS}"

    # color tool
    compile "${COMPILER} ${COMPILER_FLAGS} -I../include -o color_tool-arm64 -target arm64-apple-macos11 ../examples/color_tool.c libcui.a ${LINKER_FLAGS}"
    compile "${COMPILER} ${COMPILER_FLAGS} -I../include -o color_tool-x86_64 -target x86_64-apple-macos10.14 ../examples/color_tool.c libcui.a ${LINKER_FLAGS}"

    wait

    compile "lipo -create -output interface_browser interface_browser-arm64 interface_browser-x86_64"
    compile "lipo -create -output image_viewer image_viewer-arm64 image_viewer-x86_64"
    compile "lipo -create -output color_tool color_tool-arm64 color_tool-x86_64"

elif [ "$(uname -m)" = "x86_64" ]; then

    echo "This machine can't compile for aarch64"

    # dynamic library
    compile "${COMPILER} ${DEFINES} ${COMPILER_FLAGS} -fPIC -shared -I../include -o libcui.so -target x86_64-apple-macos10.14 ../src/cui.c ${LINKER_FLAGS}"

    # static library
    compile_no_jobs "${COMPILER} ${DEFINES} ${COMPILER_FLAGS} -I../include -c -o cui.o -target x86_64-apple-macos10.14 ../src/cui.c"
    compile_no_jobs "ar -rc libcui.a cui.o"

    wait

    # examples

    # interface browser
    compile "${COMPILER} ${COMPILER_FLAGS} -I../include -o interface_browser -target x86_64-apple-macos10.14 ../examples/interface_browser.c libcui.a ${LINKER_FLAGS}"

    # image viewer
    compile "${COMPILER} ${COMPILER_FLAGS} -I../include -o image_viewer -target x86_64-apple-macos10.14 ../examples/image_viewer.c libcui.a ${LINKER_FLAGS}"

    # color tool
    compile "${COMPILER} ${COMPILER_FLAGS} -I../include -o color_tool -target x86_64-apple-macos10.14 ../examples/color_tool.c libcui.a ${LINKER_FLAGS}"

fi

wait

# # code signing is needed for some debugging functionalities in macos
# # so if you run into issues with debugging enable these commands:
# compile "codesign -s - -v -f --entitlements ../src/macos_signing.plist interface_browser"
# compile "codesign -s - -v -f --entitlements ../src/macos_signing.plist image_viewer"
# compile "codesign -s - -v -f --entitlements ../src/macos_signing.plist color_tool"
# wait

package "Interface Browser" "interface_browser"
package "Image Viewer" "image_viewer"
package "Color Tool" "color_tool"

echo
echo "If you want to compile with the static library you need to append the following linker flags: '${LINKER_FLAGS}'"
