# CUI - C User Interface

CUI is a basic user interface library for the c programming language. It currently supports
Linux with X11 and Microsoft Windows.

## Usage

To use CUI in your project you just need the header file under `include/` and
one of the generated libraries (static or dynamic) to link against.

```c
#include <cui.h>
```

## Compile

### Linux

```
./build.sh [debug|reldebug|release]
```

This creates a Debug (`debug`), Release Debug (`reldebug`) or Release
(`release`) build.  The result can be found under `build/`. You can use the
dynamic (`libcui.so`) or the static (`libcui.a`) library.

### Windows

```
./build.bat
```

This creates a Debug Release. The result can be found under `build/`. You can
use the static library `cui.lib`.
