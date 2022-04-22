@echo off

set CompilerFlags=-Od -MT -nologo -Gm- -GR- -EHa- -Oi -Z7 -W2 -GS- -Gs9999999
set LinkerFlags=-stack:0x100000,0x100000 -subsystem:windows,6.0 -opt:ref -incremental:no user32.lib gdi32.lib comdlg32.lib

if not exist build mkdir build
pushd build

REM static library
cl %CompilerFlags% -c -I"../include" -Fo"cui.obj" "../src/cui.c"
lib -nologo "cui.obj"

REM shape compile
cl %CompilerFlags% -Fe"shape_compile.exe" "../src/shape_compile.c"

REM interface browser
cl %CompilerFlags% -I"../include" -Fe"interface_browser.exe" "../examples/interface_browser.c" /link %LinkerFlags% cui.lib

popd
