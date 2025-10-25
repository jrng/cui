# CUI - C User Interface

CUI is basic user interface library for the c programming language.

<picture>
  <source srcset="./screenshots/color_tool_wayland.png 2x">
  <img alt="CUI Color Tool on Wayland" src="./screenshots/color_tool_wayland.png">
</picture>

## Support matrix

| operating system | backend(s)   | software renderer  | opengl es 2.0 renderer | metal renderer     | direct3d11 renderer |
| ---              | ---          | ---                | ---                    | ---                | ---                 |
| Linux            | X11, Wayland | :heavy_check_mark: | :heavy_check_mark:     | :x:                | :x:                 |
| macOS            | AppKit       | :heavy_check_mark: | :x:                    | :heavy_check_mark: | :x:                 |
| Windows          | Win32        | :heavy_check_mark: | :x:                    | :x:                | :heavy_check_mark:  |
| Android          | Android      | :x:                | :heavy_check_mark:     | :x:                | :x:                 |

## Usage

To use CUI in your project you just need the header file under `include/` and
one of the generated libraries (static or dynamic) to link against.

```c
#include <cui.h>
```

## Compile

### Linux and macOS

```shell
$ cc -o c_make c_make.c  # only needs to happen once
$ ./c_make setup build build_type=[debug|reldebug|release]
$ ./c_make build build
```

This creates a Debug (`debug`), Release Debug (`reldebug`) or Release
(`release`) build. The result can be found under `build/`. You can use the
dynamic (`libcui.so`) or the static (`libcui.a`) library.

Per default the build will use background jobs to parallelize the compilation.
This will lead to error messages getting printed out of order. To get them in
order use `./c_make build build --sequential`.

### Windows

```shell
$ cl -Fec_make.exe c_make.c  # only needs to happen once
$ c_make setup build build_type=[debug|reldebug|release]
$ c_make build build
```

This creates a Debug (`debug`), Release Debug (`reldebug`) or Release
(`release`) build. The result can be found under `build/`. You can use the
static library `cui.lib`.

Per default the build will use background jobs to parallelize the compilation.
This will lead to error messages getting printed out of order. To get them in
order use `c_make build build --sequential`.

### Android

```shell
$ cc -o c_make c_make.c  # only needs to happen once
$ ./c_make setup build target_platform=android target_architecture=[amd64|aarch64] build_type=[debug|reldebug|release]
$ ./c_make build build
```

This creates a Debug (`debug`), Release Debug (`reldebug`) or Release
(`release`) build. The result can be found under `build/`. You can use the
dynamic (`libcui.so`) or the static (`libcui.a`) library.

Per default the build will use background jobs to parallelize the compilation.
This will lead to error messages getting printed out of order. To get them in
order use `./c_make build build --sequential`.

## Configuration

If you want to configure your CUI build to your liking you can edit the `c_make.txt` file
in the build folder. You can enable and disable different rendering backends and for linux
the display protocol backends that are compiled in.

## Examples

CUI comes with a few example projects which should give you a pretty good idea on
how to use it in your own code. You can find all of these in `examples`. All examples
are build with CUI automatically. Just run one of the `build_*` files mentioned above.

  - `color_tool` - Color conversion tool
  - `file_search` - A file search utility
  - `image_viewer` - Simple image viewer which supports QOI, BMP and JPEG
  - `interface_browser` - An overview app for all the available widget types
