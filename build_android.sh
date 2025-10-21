#! /bin/sh

### BEGIN OF CONFIG #########

ANDROID_ROOT="/opt/android"
JAVA_HOME="/usr/lib64/openjdk-11/bin"

CUI_RENDERER_OPENGLES2_RENDER_TIMES="off"

### END OF CONFIG ###########

REL_PATH=$(dirname "$0")

if [ -f "${REL_PATH}/build_config_android.sh" ]; then
    . "${REL_PATH}/build_config_android.sh"
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
    $1
}

build_android_app () {
    APK_NAME="${2/ /}"

    # delete previous build directory
    rm -rf "android/$1"
    rm "android/${APK_NAME}.apk"

    # create new build directory structure
    mkdir -p "android/$1/apk_build/lib/arm64-v8a"
    mkdir -p "android/$1/res/values"

    if [ ! -f "android/$1.keystore" ]; then
        "${JAVA_HOME}/keytool" -genkey -keystore "android/$1.keystore" -storepass android -alias androiddebugkey -dname "CN=company name, OU=0, O=Dream, L=City, S=StateProvince, C=Country" -keyalg RSA -keysize 2048 -validity 20000
    fi

    cp "android/$1.keystore" "android/$1/debug.keystore"

    cat <<EOF > "android/$1/AndroidManifest.xml"
<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
          package="com.$1"
          android:versionCode="1"
          android:versionName="1.0"
          android:debuggable="true">
  <uses-sdk android:minSdkVersion="24" />
  <application android:label="@string/app_name"
               android:theme="@style/CustomStyle"
               android:hasCode="false">
    <activity android:name="android.app.NativeActivity"
              android:label="@string/app_name"
              android:configChanges="orientation|screenSize|screenLayout|keyboardHidden|density">
      <meta-data android:name="android.app.lib_name"
                 android:value="$1" />
      <intent-filter>
        <action android:name="android.intent.action.MAIN" />
        <category android:name="android.intent.category.LAUNCHER" />
      </intent-filter>
    </activity>
  </application>
</manifest>
EOF

    cat <<EOF > "android/$1/res/values/strings.xml"
<?xml version="1.0" encoding="utf-8"?>
<resources>
  <string name="app_name">$2</string>
</resources>
EOF

    cat <<EOF > "android/$1/res/values/styles.xml"
<resources>
  <style name="CustomStyle">
    <item name="android:windowActionBar">false</item>
    <item name="android:windowNoTitle">true</item>
  </style>
</resources>
EOF

    compile_no_jobs "${COMPILER} ${COMPILER_FLAGS} -I../include -o android/$1/apk_build/lib/arm64-v8a/lib$1.so ../examples/$1.c android/libcui.a ${LINKER_FLAGS}"
    package "${BUILD_TOOLS}/aapt package --debug-mode -f -M android/$1/AndroidManifest.xml -S android/$1/res -I ${ANDROID_PLATFORM_JAR} -F android/$1/${APK_NAME}.unaligned.apk android/$1/apk_build"
    package "${JAVA_HOME}/jarsigner -sigalg SHA1withRSA -digestalg SHA1 -storepass android -keypass android -keystore android/$1/debug.keystore -signedjar android/$1/${APK_NAME}.signed.apk android/$1/${APK_NAME}.unaligned.apk androiddebugkey"
    package "${BUILD_TOOLS}/zipalign 4 android/$1/${APK_NAME}.signed.apk android/${APK_NAME}.apk"
}

NDK_PATH="${ANDROID_ROOT}/ndk/21.4.7075529"
BUILD_TOOLS="${ANDROID_ROOT}/build-tools/24.0.3"
ANDROID_PLATFORM_JAR="${ANDROID_ROOT}/platforms/android-24/android.jar"

COMPILER="${NDK_PATH}/toolchains/llvm/prebuilt/linux-x86_64/bin/clang"

COMMON_COMPILER_FLAGS="-std=c99"

ARM64_COMPILER_FLAGS="-target aarch64-linux-android24"

COMPILER_FLAGS="${COMMON_COMPILER_FLAGS} ${ARM64_COMPILER_FLAGS}"
LINKER_FLAGS="-shared -static-libgcc -lm -llog -landroid -lEGL -lGLESv2"
DEFINES="-DCUI_RENDERER_OPENGLES2_ENABLED=1"

if [ "${CUI_RENDERER_OPENGLES2_RENDER_TIMES}" = "on" ]; then
    DEFINES="${DEFINES} -DCUI_RENDERER_OPENGLES2_RENDER_TIMES_ENABLED=1"
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

if [ ! -d "${REL_PATH}/build/android" ]; then
    mkdir -p "${REL_PATH}/build/android"
fi

cd "${REL_PATH}/build"

# dynamic library
# compile "${COMPILER} ${DEFINES} ${COMPILER_FLAGS} -I../include -o android/libcui.so ../src/cui.c ${LINKER_FLAGS}"

# static library
compile_no_jobs "${COMPILER} ${DEFINES} ${COMPILER_FLAGS} -I../include -c -o android/cui.o ../src/cui.c"
compile_no_jobs "ar -rc android/libcui.a android/cui.o"

wait

# examples

# interface browser
build_android_app "interface_browser" "Interface Browser"

# image viewer
build_android_app "image_viewer" "Image Viewer"

# file search
build_android_app "file_search" "File Search"

# color tool
build_android_app "color_tool" "Color Tool"

echo
echo "If you want to compile with the static library you need to append the following linker flags: '${LINKER_FLAGS}'"
