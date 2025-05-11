@echo off

REM ### BEGIN OF CONFIG #####

set COMPILER=cl

set CUI_RENDERER_SOFTWARE=on
set CUI_RENDERER_DIRECT3D11=on

set CUI_FRAMEBUFFER_SCREENSHOT=off

REM ### END OF CONFIG #######

REM  -Od    set the optimization level (Od, O1, O2, O3)
REM  -Z7    generate .pdb files for debug info
REM  -Oi    generate instrinsics
REM  -GR-   disable runtime information
REM  -EHa-  disable exception handling
REM  -Gm-   disable minimal rebuild
REM  -MT    static linkage of c standard library

set COMPILER_FLAGS=-MT -nologo -Gm- -GR- -EHa- -Oi -Z7 -W2 -GS- -Gs10000000
set LINKER_FLAGS=-stack:0x100000,0x100000 -subsystem:windows,6.0 -opt:ref -incremental:no user32.lib gdi32.lib shell32.lib uxtheme.lib comdlg32.lib
set CMD_LINKER_FLAGS=-stack:0x100000,0x100000 -opt:ref -incremental:no
set DEFINES=

if not "%CUI_RENDERER_SOFTWARE%"=="on" (
    if not "%CUI_RENDERER_DIRECT3D11%"=="on" (
        echo Please provide at least one renderer for cui. Software or/and Direct3D 11.
        echo Set CUI_RENDERER_SOFTWARE or CUI_RENDERER_DIRECT3D11 in 'build_windows.bat' to "on".
        exit /b 1
    )
)

if "%CUI_RENDERER_SOFTWARE%"=="on" (
    set DEFINES=%DEFINES% -DCUI_RENDERER_SOFTWARE_ENABLED=1
)

if "%CUI_RENDERER_DIRECT3D11%"=="on" (
    set DEFINES=%DEFINES% -DCUI_RENDERER_DIRECT3D11_ENABLED=1
    set LINKER_FLAGS=%LINKER_FLAGS% d3d11.lib dxguid.lib d3dcompiler.lib
)

if "%CUI_FRAMEBUFFER_SCREENSHOT%"=="on" (
    set DEFINES=%DEFINES% -DCUI_FRAMEBUFFER_SCREENSHOT_ENABLED=1
)

set BUILD_TYPE=%1

if "%BUILD_TYPE%"=="reldebug" (
    set COMPILER_FLAGS=%COMPILER_FLAGS% -O2 -DCUI_DEBUG_BUILD=1
) else if "%BUILD_TYPE%"=="release" (
    set COMPILER_FLAGS=%COMPILER_FLAGS% -O2
) else (
    set COMPILER_FLAGS=%COMPILER_FLAGS% -Od -DCUI_DEBUG_BUILD=1
)

if not exist "build" (
    mkdir "build"
)

pushd "build"

REM shape compile
echo [COMPILE] %COMPILER% %COMPILER_FLAGS% -Fe"shape_compile.exe" "../src/shape_compile.c" /link %CMD_LINKER_FLAGS%
%COMPILER% %COMPILER_FLAGS% -Fe"shape_compile.exe" "../src/shape_compile.c" /link %CMD_LINKER_FLAGS%

REM static library
echo [COMPILE] %COMPILER% %DEFINES% %COMPILER_FLAGS% -c -I"../include" -Fo"cui.obj" "../src/cui.c"
%COMPILER% %DEFINES% %COMPILER_FLAGS% -c -I"../include" -Fo"cui.obj" "../src/cui.c"
echo [COMPILE] lib -nologo "cui.obj"
lib -nologo "cui.obj"

REM interface browser
echo [COMPILE] %COMPILER% %COMPILER_FLAGS% -I"../include" -Fe"interface_browser.exe" "../examples/interface_browser.c" /link %LINKER_FLAGS% cui.lib
%COMPILER% %COMPILER_FLAGS% -I"../include" -Fe"interface_browser.exe" "../examples/interface_browser.c" /link %LINKER_FLAGS% cui.lib

REM image viewer
echo [COMPILE] %COMPILER% %COMPILER_FLAGS% -I"../include" -Fe"image_viewer.exe" "../examples/image_viewer.c" /link %LINKER_FLAGS% cui.lib
%COMPILER% %COMPILER_FLAGS% -I"../include" -Fe"image_viewer.exe" "../examples/image_viewer.c" /link %LINKER_FLAGS% cui.lib

REM file search
echo [COMPILE] %COMPILER% %COMPILER_FLAGS% -I"../include" -Fe"file_search.exe" "../examples/file_search.c" /link %LINKER_FLAGS% cui.lib
%COMPILER% %COMPILER_FLAGS% -I"../include" -Fe"file_search.exe" "../examples/file_search.c" /link %LINKER_FLAGS% cui.lib

REM color tool
echo [COMPILE] %COMPILER% %COMPILER_FLAGS% -I"../include" -Fe"color_tool.exe" "../examples/color_tool.c" /link %LINKER_FLAGS% cui.lib
%COMPILER% %COMPILER_FLAGS% -I"../include" -Fe"color_tool.exe" "../examples/color_tool.c" /link %LINKER_FLAGS% cui.lib

echo:
echo If you want to compile with the static library you need to append the following linker flags: '%LINKER_FLAGS%'

popd
