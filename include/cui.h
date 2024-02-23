#ifndef _CUI_H
#define _CUI_H

#define CUI_PLATFORM_ANDROID 0
#define CUI_PLATFORM WINDOWS 0
#define CUI_PLATFORM_LINUX 0
#define CUI_PLATFORM_MACOS 0

#if defined(__ANDROID__)
#  undef CUI_PLATFORM_ANDROID
#  define CUI_PLATFORM_ANDROID 1
#elif defined(_WIN32)
#  undef CUI_PLATFORM_WINDOWS
#  define CUI_PLATFORM_WINDOWS 1
#elif defined(__linux__)
#  undef CUI_PLATFORM_LINUX
#  define CUI_PLATFORM_LINUX 1
#elif defined(__APPLE__) && defined(__MACH__)
#  undef CUI_PLATFORM_MACOS
#  define CUI_PLATFORM_MACOS 1
#endif

#define CUI_ARCH_ARM64 0
#define CUI_ARCH_X86_64 0

#if CUI_PLATFORM_WINDOWS
#  if defined(_M_AMD64)
#    undef CUI_ARCH_X86_64
#    define CUI_ARCH_X86_64 1
#  endif
#elif CUI_PLATFORM_ANDROID || CUI_PLATFORM_LINUX || CUI_PLATFORM_MACOS
#  if defined(__x86_64__)
#    undef CUI_ARCH_X86_64
#    define CUI_ARCH_X86_64 1
#  elif defined(__aarch64__)
#    undef CUI_ARCH_ARM64
#    define CUI_ARCH_ARM64 1
#  endif
#endif

#if CUI_PLATFORM_WINDOWS
#  define CUI_PLATFORM_MAIN                                 \
    int CALLBACK wWinMain(HINSTANCE __instance,             \
                          HINSTANCE __prev_instance,        \
                          PWSTR __command_line,             \
                          int __show_code)
#  define CUI_PLATFORM_INIT cui_init(0, 0)
#else
#  define CUI_PLATFORM_MAIN int main(int argc, char **argv)
#  define CUI_PLATFORM_INIT cui_init(argc, argv)
#endif

#if !defined(CUI_DEBUG_BUILD)
#  define CUI_DEBUG_BUILD 0
#endif

#if CUI_DEBUG_BUILD
#  define CuiAssert(expression) if (!(expression)) { _cui_assert(#expression, __FILE__, __LINE__); }
#else
#  define CuiAssert(expression)
#endif

#define CuiNArgs(...) __CuiNArgs(_CuiNArgs(__VA_ARGS__, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0))
#define __CuiNArgs(x) x
#define _CuiNArgs(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, n, ...) n

#define CuiOffsetOf(type, member) (intptr_t) &((type *) 0)->member
#define CuiContainerOf(ptr, type, member) (type *) ((uint8_t *) (ptr) - CuiOffsetOf(type, member))
#define CuiArrayCount(array) (sizeof(array)/sizeof((array)[0]))
#define CuiStringLiteral(str) cui_make_string((char *) (str), sizeof(str) - 1)
#define CuiCString(str) cui_make_string((char *) (str), cui_string_length(str))
#define CuiClearStruct(inst) cui_clear_memory(&(inst), sizeof(inst))

#define CuiKiB(value) (      (value) * (int64_t) 1024)
#define CuiMiB(value) (CuiKiB(value) * (int64_t) 1024)
#define CuiGiB(value) (CuiMiB(value) * (int64_t) 1024)

// NOTE: this only works for powers of two
#define CuiAlign(value, alignment) (((value) + (alignment) - (int64_t) 1) & ~((alignment) - (int64_t) 1))

#define CuiAlignUp(value, alignment) ((((value) + (alignment) - 1ll) / (alignment)) * (alignment))
#define CuiAlignDown(value, alignment) (((value) / (alignment)) * (alignment))

#define CuiForEachWidget(iter, sentinel)                    \
    for (CuiWidget *iter =                                  \
         CuiContainerOf((sentinel)->next, CuiWidget, list); \
         &iter->list != (sentinel);                         \
         iter =                                             \
         CuiContainerOf(iter->list.next, CuiWidget, list))

#define CuiForEachWidgetReversed(iter, sentinel)            \
    for (CuiWidget *iter =                                  \
         CuiContainerOf((sentinel)->prev, CuiWidget, list); \
         &iter->list != (sentinel);                         \
         iter =                                             \
         CuiContainerOf(iter->list.prev, CuiWidget, list))

#define CuiDListInit(sentinel)                              \
    (sentinel)->prev = (sentinel)->next = (sentinel);

#define CuiDListIsEmpty(sentinel)                           \
    (((sentinel)->prev == (sentinel)) ? true : false)

#define CuiDListInsertBefore(sentinel, element)             \
    (element)->prev = (sentinel)->prev;                     \
    (element)->next = (sentinel);                           \
    (sentinel)->prev->next = (element);                     \
    (sentinel)->prev = (element);

#define CuiDListInsertAfter(sentinel, element)              \
    (element)->prev = (sentinel);                           \
    (element)->next = (sentinel)->next;                     \
    (sentinel)->next->prev = (element);                     \
    (sentinel)->next = (element);

#define CuiDListRemove(element)                             \
    (element)->prev->next = (element)->next;                \
    (element)->next->prev = (element)->prev;                \
    (element)->prev = (element)->next = (element);

#ifdef __cplusplus

#  define CuiHexColor(color) (CuiColor { (float) (((color) >> 16) & 0xFF) / 255.0f,     \
                                         (float) (((color) >>  8) & 0xFF) / 255.0f,     \
                                         (float) (((color) >>  0) & 0xFF) / 255.0f,     \
                                         (float) (((color) >> 24) & 0xFF) / 255.0f })

#  define CuiHexColorLiteral(color) { (float) (((color) >> 16) & 0xFF) / 255.0f,    \
                                      (float) (((color) >>  8) & 0xFF) / 255.0f,    \
                                      (float) (((color) >>  0) & 0xFF) / 255.0f,    \
                                      (float) (((color) >> 24) & 0xFF) / 255.0f }

#  define _cui_array_header(array) ((CuiArrayHeader *) (array) - 1)
#  define _cui_array_ensure_space(array)                                                                                        \
              (((cui_array_count(array) + 1) > cui_array_allocated(array)) ?                                                    \
                  (array) = (decltype(+(array))) _cui_array_grow(array, 2 * cui_array_allocated(array), sizeof(*(array)), 0) : 0)

#  define cui_array_init(array, allocated, arena) ((array) = (decltype(+(array))) _cui_array_grow(array, allocated, sizeof(*(array)), arena))
#  define cui_array_count(array) ((array) ? _cui_array_header(array)->count : 0)
#  define cui_array_allocated(array) ((array) ? _cui_array_header(array)->allocated : 0)
#  define cui_array_append(array) ((array) ? (_cui_array_ensure_space(array), (array) + _cui_array_header(array)->count++) : 0)

#else

#  define CuiHexColor(color) ((CuiColor) { (float) ((color >> 16) & 0xFF) / 255.0f,   \
                                           (float) ((color >>  8) & 0xFF) / 255.0f,   \
                                           (float) ((color >>  0) & 0xFF) / 255.0f,   \
                                           (float) ((color >> 24) & 0xFF) / 255.0f })

#  define CuiHexColorLiteral(color) { (float) ((color >> 16) & 0xFF) / 255.0f,   \
                                      (float) ((color >>  8) & 0xFF) / 255.0f,   \
                                      (float) ((color >>  0) & 0xFF) / 255.0f,   \
                                      (float) ((color >> 24) & 0xFF) / 255.0f }

#  define _cui_array_header(array) ((CuiArrayHeader *) (array) - 1)
#  define _cui_array_ensure_space(array)                                                                                        \
              (((cui_array_count(array) + 1) > cui_array_allocated(array)) ?                                                    \
                  (array) = _cui_array_grow(array, 2 * cui_array_allocated(array), sizeof(*(array)), 0) : 0)

#  define cui_array_init(array, allocated, arena) ((array) = _cui_array_grow(array, allocated, sizeof(*(array)), arena))
#  define cui_array_count(array) ((array) ? _cui_array_header(array)->count : 0)
#  define cui_array_allocated(array) ((array) ? _cui_array_header(array)->allocated : 0)
#  define cui_array_append(array) ((array) ? (_cui_array_ensure_space(array), (array) + _cui_array_header(array)->count++) : 0)

#endif

#include <math.h>
#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>

#if CUI_PLATFORM_WINDOWS
#  include <intrin.h>
#endif

#if CUI_DEBUG_BUILD

#  if CUI_PLATFORM_ANDROID
#    include <android/log.h>
#  endif

static inline void
_cui_assert(const char *expression, const char *file, int line_number)
{
#if CUI_PLATFORM_WINDOWS
    (void) expression;
    (void) file;
    (void) line_number;
    __debugbreak();
#else
#  if CUI_PLATFORM_ANDROID
    __android_log_print(ANDROID_LOG_INFO, "cui_android", "assertion failed: '%s', %s:%d\n", expression, file, line_number);
#  else
    printf("assertion failed: '%s', %s:%d\n", expression, file, line_number);
#  endif
    __builtin_abort();
#endif
}

#endif

static inline void
cui_clear_memory(void *_dst, uint64_t size)
{
    uint8_t *dst = (uint8_t *) _dst;
    while (size--) *dst++ = 0;
}

static inline void
cui_copy_memory(void *_dst, void *_src, uint64_t size)
{
    if (_dst < _src)
    {
        uint8_t *src = (uint8_t *) _src;
        uint8_t *dst = (uint8_t *) _dst;
        while (size--) *dst++ = *src++;
    }
    else if (_dst > _src)
    {
        uint8_t *src = (uint8_t *) _src + size;
        uint8_t *dst = (uint8_t *) _dst + size;
        while (size--) *--dst = *--src;
    }
}

static inline bool
cui_count_trailing_zeros_u32(uint32_t *index, uint32_t mask)
{
    // TODO: this should be better separated by compiler
#if CUI_PLATFORM_WINDOWS
    return _BitScanForward((unsigned long *) index, mask);
#else
    if (mask)
    {
        *index = __builtin_ctz(mask);
        return true;
    }

    return false;
#endif
}

static inline void
cui_swap_int32(int32_t *a, int32_t *b)
{
    int32_t val = *a;
    *a = *b;
    *b = val;
}

static inline uint16_t
cui_max_u16(uint16_t a, uint16_t b)
{
    return (a > b) ? a : b;
}

static inline uint32_t
cui_min_uint32(uint32_t a, uint32_t b)
{
    return (a < b) ? a : b;
}

static inline int32_t
cui_min_int32(int32_t a, int32_t b)
{
    return (a < b) ? a : b;
}

static inline int32_t
cui_max_int32(int32_t a, int32_t b)
{
    return (a > b) ? a : b;
}

static inline int64_t
cui_min_int64(int64_t a, int64_t b)
{
    return (a < b) ? a : b;
}

static inline int64_t
cui_max_int64(int64_t a, int64_t b)
{
    return (a > b) ? a : b;
}

static inline float
cui_min_float(float a, float b)
{
    return (a < b) ? a : b;
}

static inline float
cui_max_float(float a, float b)
{
    return (a > b) ? a : b;
}

static inline float
cui_lerp_float(float a, float b, float t)
{
    return a + (b - a) * t;
}

static inline uint16_t
cui_read_u16(uint8_t *data, int64_t offset, uint16_t little_endian_mask)
{
    CuiAssert((little_endian_mask == 0) || (little_endian_mask == 0xFFFF));

    uint8_t *buffer = data + offset;
    return (((buffer[1] << 8) | buffer[0]) & little_endian_mask) |
           (((buffer[0] << 8) | buffer[1]) & ~little_endian_mask);
}

static inline uint16_t
cui_read_u16_le(uint8_t *data, int64_t offset)
{
    uint8_t *buffer = data + offset;
    return (buffer[1] << 8) | buffer[0];
}

static inline uint16_t
cui_read_u16_be(uint8_t *data, int64_t offset)
{
    uint8_t *buffer = data + offset;
    return (buffer[0] << 8) | buffer[1];
}

static inline uint32_t
cui_read_u32(uint8_t *data, int64_t offset, uint32_t little_endian_mask)
{
    CuiAssert((little_endian_mask == 0) || (little_endian_mask == 0xFFFFFFFF));

    uint8_t *buffer = data + offset;
    return (((buffer[3] << 24) | (buffer[2] << 16) | (buffer[1] << 8) | buffer[0]) & little_endian_mask) |
           (((buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3]) & ~little_endian_mask);
}

static inline uint32_t
cui_read_u32_le(uint8_t *data, int64_t offset)
{
    uint8_t *buffer = data + offset;
    return (buffer[3] << 24) | (buffer[2] << 16) | (buffer[1] << 8) | buffer[0];
}

static inline uint32_t
cui_read_u32_be(uint8_t *data, int64_t offset)
{
    uint8_t *buffer = data + offset;
    return (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3];
}

static inline void
cui_write_u16_le(uint8_t *data, int64_t offset, uint16_t value)
{
    uint8_t *buffer = data + offset;

    buffer[0] = (uint8_t) (value >>  0);
    buffer[1] = (uint8_t) (value >>  8);
}

static inline void
cui_write_u32_le(uint8_t *data, int64_t offset, uint32_t value)
{
    uint8_t *buffer = data + offset;

    buffer[0] = (uint8_t) (value >>  0);
    buffer[1] = (uint8_t) (value >>  8);
    buffer[2] = (uint8_t) (value >> 16);
    buffer[3] = (uint8_t) (value >> 24);
}

typedef struct CuiColor
{
    float r;
    float g;
    float b;
    float a;
} CuiColor;

typedef struct CuiPoint
{
    int32_t x;
    int32_t y;
} CuiPoint;

typedef struct CuiRect
{
    CuiPoint min;
    CuiPoint max;
} CuiRect;

typedef struct CuiFloatPoint
{
    float x;
    float y;
} CuiFloatPoint;

typedef struct CuiFloatRect
{
    CuiFloatPoint min;
    CuiFloatPoint max;
} CuiFloatRect;

typedef struct CuiBitmap
{
    int32_t width;
    int32_t height;
    int32_t stride;
    void *pixels;
} CuiBitmap;

typedef struct CuiString
{
    int64_t count;
    uint8_t *data;
} CuiString;

typedef struct CuiSizedFontSpec
{
    CuiString name;
    float size;
    float line_height;
} CuiSizedFontSpec;

typedef enum CuiFileAttributeFlags
{
    CUI_FILE_ATTRIBUTE_IS_DIRECTORY = (1 << 0),
} CuiFileAttributeFlags;

typedef struct CuiFileAttributes
{
    uint32_t flags;
    uint64_t size;
} CuiFileAttributes;

typedef struct CuiFileInfo
{
    CuiString name;
    CuiFileAttributes attr;
} CuiFileInfo;

#ifndef CUI_NO_BACKEND

typedef enum CuiColorThemeId
{
    CUI_COLOR_WINDOW_TITLEBAR_BACKGROUND           = 0,
    CUI_COLOR_WINDOW_TITLEBAR_BORDER               = 1,
    CUI_COLOR_WINDOW_TITLEBAR_TEXT                 = 2,
    CUI_COLOR_WINDOW_TITLEBAR_ICON                 = 3,
    CUI_COLOR_WINDOW_DROP_SHADOW                   = 4,
    CUI_COLOR_WINDOW_OUTLINE                       = 5,
    CUI_COLOR_DEFAULT_BG                           = 6,
    CUI_COLOR_DEFAULT_FG                           = 7,
    CUI_COLOR_DEFAULT_BORDER                       = 8,
    CUI_COLOR_DEFAULT_BUTTON_NORMAL_BACKGROUND     = 9,
    CUI_COLOR_DEFAULT_BUTTON_NORMAL_BOX_SHADOW     = 10,
    CUI_COLOR_DEFAULT_BUTTON_NORMAL_BORDER         = 11,
    CUI_COLOR_DEFAULT_BUTTON_NORMAL_TEXT           = 12,
    CUI_COLOR_DEFAULT_BUTTON_NORMAL_ICON           = 13,
    CUI_COLOR_DEFAULT_TEXTINPUT_NORMAL_BACKGROUND  = 14,
    CUI_COLOR_DEFAULT_TEXTINPUT_NORMAL_BOX_SHADOW  = 15,
    CUI_COLOR_DEFAULT_TEXTINPUT_NORMAL_BORDER      = 16,
    CUI_COLOR_DEFAULT_TEXTINPUT_NORMAL_TEXT        = 17,
    CUI_COLOR_DEFAULT_TEXTINPUT_NORMAL_PLACEHOLDER = 18,
    CUI_COLOR_DEFAULT_TEXTINPUT_NORMAL_ICON        = 19,
} CuiColorThemeId;

typedef struct CuiColorTheme
{
    CuiColor window_titlebar_background;
    CuiColor window_titlebar_border;
    CuiColor window_titlebar_text;
    CuiColor window_titlebar_icon;
    CuiColor window_drop_shadow;
    CuiColor window_outline;
    CuiColor default_bg;
    CuiColor default_fg;
    CuiColor default_border;
    CuiColor default_button_normal_background;
    CuiColor default_button_normal_box_shadow;
    CuiColor default_button_normal_border;
    CuiColor default_button_normal_text;
    CuiColor default_button_normal_icon;
    CuiColor default_textinput_normal_background;
    CuiColor default_textinput_normal_box_shadow;
    CuiColor default_textinput_normal_border;
    CuiColor default_textinput_normal_text;
    CuiColor default_textinput_normal_placeholder;
    CuiColor default_textinput_normal_icon;
} CuiColorTheme;

static const CuiColorTheme cui_color_theme_default_dark = {
    /* window_titlebar_background           */ CuiHexColorLiteral(0xFF2F333D),
    /* window_titlebar_border               */ CuiHexColorLiteral(0x7F000000),
    /* window_titlebar_text                 */ CuiHexColorLiteral(0xFFD7DAE0),
    /* window_titlebar_icon                 */ CuiHexColorLiteral(0xFFD7DAE0),
    /* window_drop_shadow                   */ CuiHexColorLiteral(0x6F000000),
    /* window_outline                       */ CuiHexColorLiteral(0xAF050607),
    /* default_bg                           */ CuiHexColorLiteral(0xFF282C34),
    /* default_fg                           */ CuiHexColorLiteral(0xFFD7DAE0),
    /* default_border                       */ CuiHexColorLiteral(0xFF585C64),
    /* default_button_normal_background     */ CuiHexColorLiteral(0xFF2F333D),
    /* default_button_normal_box_shadow     */ CuiHexColorLiteral(0x3F000000),
    /* default_button_normal_border         */ CuiHexColorLiteral(0xFF1E1E1E),
    /* default_button_normal_text           */ CuiHexColorLiteral(0xFFD7DAE0),
    /* default_button_normal_icon           */ CuiHexColorLiteral(0xFFB7BAC0),
    /* default_textinput_normal_background  */ CuiHexColorLiteral(0xFF20232A),
    /* default_textinput_normal_box_shadow  */ CuiHexColorLiteral(0x3F000000),
    /* default_textinput_normal_border      */ CuiHexColorLiteral(0xFF3A3D45),
    /* default_textinput_normal_text        */ CuiHexColorLiteral(0xFFD7DAE0),
    /* default_textinput_normal_placeholder */ CuiHexColorLiteral(0xFF585C64),
    /* default_textinput_normal_icon        */ CuiHexColorLiteral(0xFFB7BAC0),
};

static inline CuiColor
cui_color_theme_get_color(const CuiColorTheme *color_theme, CuiColorThemeId color_id)
{
    return *((CuiColor *) color_theme + color_id);
}

#endif

typedef struct CuiUnicodeResult
{
    uint32_t codepoint;
    int32_t  byte_count;
} CuiUnicodeResult;

static inline CuiColor
cui_make_color(float r, float g, float b, float a)
{
    CuiColor result;
    result.r = r;
    result.g = g;
    result.b = b;
    result.a = a;
    return result;
}

static inline CuiPoint
cui_make_point(int32_t x, int32_t y)
{
    CuiPoint result;
    result.x = x;
    result.y = y;
    return result;
}

static inline CuiRect
cui_make_rect(int32_t x_min, int32_t y_min, int32_t x_max, int32_t y_max)
{
    CuiRect result;
    result.min = cui_make_point(x_min, y_min);
    result.max = cui_make_point(x_max, y_max);
    return result;
}

static inline CuiFloatPoint
cui_make_float_point(float x, float y)
{
    CuiFloatPoint result;
    result.x = x;
    result.y = y;
    return result;
}

static inline CuiFloatRect
cui_make_float_rect(float x_min, float y_min, float x_max, float y_max)
{
    CuiFloatRect result;
    result.min = cui_make_float_point(x_min, y_min);
    result.max = cui_make_float_point(x_max, y_max);
    return result;
}

static inline CuiString
cui_make_string(void *data, int64_t count)
{
    CuiString result;
    result.count = count;
    result.data = (uint8_t *) data;
    return result;
}

static inline CuiSizedFontSpec
cui_make_sized_font_spec(CuiString name, float size, float line_height)
{
    CuiSizedFontSpec result;
    result.name = name;
    result.size = size;
    result.line_height = line_height;
    return result;
}

static inline CuiPoint
cui_point_add(CuiPoint a, CuiPoint b)
{
    CuiPoint result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    return result;
}

static inline CuiPoint
cui_point_sub(CuiPoint a, CuiPoint b)
{
    CuiPoint result;
    result.x = a.x - b.x;
    result.y = a.y - b.y;
    return result;
}

static inline int32_t
cui_rect_get_width(CuiRect rect)
{
    return (rect.max.x - rect.min.x);
}

static inline int32_t
cui_rect_get_height(CuiRect rect)
{
    return (rect.max.y - rect.min.y);
}

static inline bool
cui_rect_equals(CuiRect a, CuiRect b)
{
    return ((a.min.x == b.min.x) && (a.min.y == b.min.y) &&
            (a.max.x == b.max.x) && (a.max.y == b.max.y));
}

static inline bool
cui_rect_overlap(CuiRect a, CuiRect b)
{
    return ((a.min.x < b.max.x) &&
            (a.max.x > b.min.x) &&
            (a.min.y < b.max.y) &&
            (a.max.y > b.min.y));
}

static inline bool
cui_rect_has_area(CuiRect rect)
{
    return ((rect.max.x > rect.min.x) && (rect.max.y > rect.min.y));
}

static inline bool
cui_rect_has_point_inside(CuiRect rect, CuiPoint point)
{
    return ((rect.min.x <= point.x) && (rect.max.x > point.x) &&
            (rect.min.y <= point.y) && (rect.max.y > point.y));
}

static inline CuiRect
cui_rect_get_union(CuiRect a, CuiRect b)
{
    CuiRect result;
    result.min.x = cui_min_int32(a.min.x, b.min.x);
    result.min.y = cui_min_int32(a.min.y, b.min.y);
    result.max.x = cui_max_int32(a.max.x, b.max.x);
    result.max.y = cui_max_int32(a.max.y, b.max.y);
    return result;
}

static inline CuiRect
cui_rect_get_intersection(CuiRect a, CuiRect b)
{
    CuiRect result;
    result.min.x = cui_max_int32(a.min.x, b.min.x);
    result.min.y = cui_max_int32(a.min.y, b.min.y);
    result.max.x = cui_min_int32(a.max.x, b.max.x);
    result.max.y = cui_min_int32(a.max.y, b.max.y);
    return result;
}

static inline float
cui_float_rect_get_width(CuiFloatRect rect)
{
    return (rect.max.x - rect.min.x);
}

static inline float
cui_float_rect_get_height(CuiFloatRect rect)
{
    return (rect.max.y - rect.min.y);
}

static inline int64_t
cui_string_length(const char *str)
{
    int64_t length = 0;
    while (*str++) length += 1;
    return length;
}

static inline int64_t
cui_wide_string_length(const uint16_t *str)
{
    int64_t length = 0;
    while (*str++) length += 1;
    return length;
}

static inline void
cui_string_advance(CuiString *str, int64_t count)
{
    if (count > str->count)
    {
        count = str->count;
    }

    str->count -= count;
    str->data  += count;
}

static inline bool
cui_string_equals(CuiString a, CuiString b)
{
    if (a.count != b.count) return false;

    for (int64_t index = 0; index < a.count; index += 1)
    {
        if (a.data[index] != b.data[index]) return false;
    }

    return true;
}

static inline bool
cui_string_starts_with(CuiString str, CuiString start)
{
    if (str.count < start.count) return false;
    return cui_string_equals(cui_make_string(str.data, start.count), start);
}

static inline bool
cui_string_ends_with(CuiString str, CuiString end)
{
    if (str.count < end.count) return false;
    return cui_string_equals(cui_make_string(str.data + (str.count - end.count), end.count), end);
}

#define cui_string_concat(arena, ...) cui_string_concat_n(arena, CuiNArgs(__VA_ARGS__), __VA_ARGS__)

typedef struct CuiAllocationParams
{
    bool clear;
    uint32_t alignment;
} CuiAllocationParams;

static inline CuiAllocationParams
cui_make_allocation_params(bool clear, uint32_t alignment)
{
    CuiAllocationParams result;
    result.clear = clear;
    result.alignment = alignment;
    return result;
}

#define CuiDefaultAllocationParams() (cui_make_allocation_params(false, 8))
#define cui_alloc_type(arena, type, params) (type *) cui_alloc(arena, sizeof(type), params)
#define cui_alloc_array(arena, type, count, params) (type *) cui_alloc(arena, (count) * sizeof(type), params)

typedef struct CuiArena
{
    uint8_t *base;
    uint64_t capacity;
    uint64_t occupied;

    int32_t temporary_memory_count;
} CuiArena;

typedef struct CuiTemporaryMemory
{
    CuiArena *arena;
    uint64_t occupied;
    int32_t index;
} CuiTemporaryMemory;

typedef struct CuiTextBuffer
{
    struct CuiTextBuffer *next;
    uint64_t occupied;
    uint8_t data[4096];
} CuiTextBuffer;

typedef struct CuiStringBuilder
{
    CuiArena *arena;
    CuiTextBuffer *write_buffer;
    CuiTextBuffer base_buffer;
} CuiStringBuilder;

typedef struct CuiTextInput
{
    int64_t cursor_start;
    int64_t cursor_end;

    int64_t count;
    int64_t capacity;
    uint8_t *data;
} CuiTextInput;

#define cui_text_input_to_string(text_input) cui_make_string((text_input).data, (text_input).count)

typedef enum CuiFileMode
{
    CUI_FILE_MODE_READ  = (1 << 0),
    CUI_FILE_MODE_WRITE = (1 << 1),
} CuiFileMode;

typedef struct CuiFile CuiFile;

typedef struct CuiFontId { uint16_t value; } CuiFontId;

#define CUI_ICON_SIZE_SHIFT 24

typedef enum CuiIconType
{
    CUI_ICON_NONE             = 0,
    CUI_ICON_WINDOWS_MINIMIZE = (12 << CUI_ICON_SIZE_SHIFT) | 1,
    CUI_ICON_WINDOWS_MAXIMIZE = (12 << CUI_ICON_SIZE_SHIFT) | 2,
    CUI_ICON_WINDOWS_CLOSE    = (12 << CUI_ICON_SIZE_SHIFT) | 3,
    CUI_ICON_ANGLE_UP_12      = (12 << CUI_ICON_SIZE_SHIFT) | 4,
    CUI_ICON_ANGLE_RIGHT_12   = (12 << CUI_ICON_SIZE_SHIFT) | 5,
    CUI_ICON_ANGLE_DOWN_12    = (12 << CUI_ICON_SIZE_SHIFT) | 6,
    CUI_ICON_ANGLE_LEFT_12    = (12 << CUI_ICON_SIZE_SHIFT) | 7,
    CUI_ICON_INFO_12          = (12 << CUI_ICON_SIZE_SHIFT) | 8,
    CUI_ICON_EXPAND_12        = (12 << CUI_ICON_SIZE_SHIFT) | 9,
    CUI_ICON_SEARCH_12        = (12 << CUI_ICON_SIZE_SHIFT) | 10,
    CUI_ICON_UPPERCASE_A_12   = (12 << CUI_ICON_SIZE_SHIFT) | 11,
    CUI_ICON_UPPERCASE_B_12   = (12 << CUI_ICON_SIZE_SHIFT) | 12,
    CUI_ICON_UPPERCASE_G_12   = (12 << CUI_ICON_SIZE_SHIFT) | 13,
    CUI_ICON_UPPERCASE_H_12   = (12 << CUI_ICON_SIZE_SHIFT) | 14,
    CUI_ICON_UPPERCASE_L_12   = (12 << CUI_ICON_SIZE_SHIFT) | 15,
    CUI_ICON_UPPERCASE_R_12   = (12 << CUI_ICON_SIZE_SHIFT) | 16,
    CUI_ICON_UPPERCASE_S_12   = (12 << CUI_ICON_SIZE_SHIFT) | 17,
    CUI_ICON_UPPERCASE_V_12   = (12 << CUI_ICON_SIZE_SHIFT) | 18,
    CUI_ICON_PLUS_12          = (12 << CUI_ICON_SIZE_SHIFT) | 19,
} CuiIconType;

typedef enum CuiShapeType
{
    CUI_SHAPE_ROUNDED_CORNER            = 0,
    CUI_SHAPE_INVERTED_ROUNDED_CORNER   = 1,
    CUI_SHAPE_SHADOW_CORNER             = 2,
    CUI_SHAPE_SHADOW_HORIZONTAL         = 3,
    CUI_SHAPE_SHADOW_VERTICAL           = 4,
    CUI_SHAPE_WINDOWS_MINIMIZE          = 5,
    CUI_SHAPE_WINDOWS_MAXIMIZE          = 6,
    CUI_SHAPE_WINDOWS_CLOSE             = 7,
    CUI_SHAPE_CHECKBOX_OUTER_16         = 8,
    CUI_SHAPE_CHECKBOX_INNER_16         = 9,
    CUI_SHAPE_CHECKMARK_16              = 10,
    CUI_SHAPE_ANGLE_UP_12               = 11,
    CUI_SHAPE_ANGLE_RIGHT_12            = 12,
    CUI_SHAPE_ANGLE_DOWN_12             = 13,
    CUI_SHAPE_ANGLE_LEFT_12             = 14,
    CUI_SHAPE_INFO_12                   = 15,
    CUI_SHAPE_EXPAND_12                 = 16,
    CUI_SHAPE_SEARCH_12                 = 17,
    CUI_SHAPE_UPPERCASE_A_12            = 18,
    CUI_SHAPE_UPPERCASE_B_12            = 19,
    CUI_SHAPE_UPPERCASE_G_12            = 20,
    CUI_SHAPE_UPPERCASE_H_12            = 21,
    CUI_SHAPE_UPPERCASE_L_12            = 22,
    CUI_SHAPE_UPPERCASE_R_12            = 23,
    CUI_SHAPE_UPPERCASE_S_12            = 24,
    CUI_SHAPE_UPPERCASE_V_12            = 25,
    CUI_SHAPE_PLUS_12                   = 26,
} CuiShapeType;

typedef enum CuiAxis
{
    CUI_AXIS_X = 0,
    CUI_AXIS_Y = 1,
} CuiAxis;

typedef enum CuiGravity
{
    CUI_GRAVITY_START  = 0,
    CUI_GRAVITY_CENTER = 1,
    CUI_GRAVITY_END    = 2,
} CuiGravity;

typedef enum CuiDirection
{
    CUI_DIRECTION_NORTH = 0,
    CUI_DIRECTION_EAST  = 1,
    CUI_DIRECTION_SOUTH = 2,
    CUI_DIRECTION_WEST  = 3,
} CuiDirection;

#ifndef CUI_NO_BACKEND

typedef struct CuiBackgroundTask { uint32_t state; } CuiBackgroundTask;

typedef enum CuiEventType
{
    CUI_EVENT_TYPE_MOUSE_ENTER  = 0,
    CUI_EVENT_TYPE_MOUSE_LEAVE  = 1,
    CUI_EVENT_TYPE_MOUSE_MOVE   = 2,
    CUI_EVENT_TYPE_MOUSE_DRAG   = 3,
    CUI_EVENT_TYPE_MOUSE_WHEEL  = 4,
    CUI_EVENT_TYPE_LEFT_DOWN    = 5,
    CUI_EVENT_TYPE_LEFT_UP      = 6,
    CUI_EVENT_TYPE_DOUBLE_CLICK = 7,
    CUI_EVENT_TYPE_RIGHT_DOWN   = 8,
    CUI_EVENT_TYPE_RIGHT_UP     = 9,
    CUI_EVENT_TYPE_KEY_DOWN     = 10,
    CUI_EVENT_TYPE_KEY_UP       = 11,
    CUI_EVENT_TYPE_FOCUS        = 12,
    CUI_EVENT_TYPE_UNFOCUS      = 13,
    CUI_EVENT_TYPE_POINTER_DOWN = 14,
    CUI_EVENT_TYPE_POINTER_UP   = 15,
    CUI_EVENT_TYPE_POINTER_MOVE = 16,
} CuiEventType;

typedef enum CuiKeyId
{
    CUI_KEY_UNKNOWN,

    CUI_KEY_ALT = 1,
    CUI_KEY_CTRL = 2,
    CUI_KEY_SHIFT = 3,

    CUI_KEY_UP = 4,
    CUI_KEY_DOWN = 5,
    CUI_KEY_LEFT = 6,
    CUI_KEY_RIGHT = 7,

    CUI_KEY_BACKSPACE = 8,
    CUI_KEY_TAB = 9,
    CUI_KEY_LINEFEED = 10,
    CUI_KEY_ENTER = 13,

    CUI_KEY_F1  = 14,
    CUI_KEY_F2  = 15,
    CUI_KEY_F3  = 16,
    CUI_KEY_F4  = 17,
    CUI_KEY_F5  = 18,
    CUI_KEY_F6  = 19,
    CUI_KEY_F7  = 20,
    CUI_KEY_F8  = 21,
    CUI_KEY_F9  = 22,
    CUI_KEY_F10 = 23,
    CUI_KEY_F11 = 24,
    CUI_KEY_F12 = 25,

    CUI_KEY_ESCAPE = 27,
    CUI_KEY_SPACE = 32,

    // rest of ascii table

    CUI_KEY_DELETE = 127,
} CuiKeyId;

// TODO: maybe make this a union
// TODO: do we need this to be public?
typedef struct CuiEvent
{
    CuiEventType type;

    CuiPoint mouse;

    struct {
        bool is_precise_scrolling;
        float dx;
        float dy;
    } wheel;

    struct {
        uint32_t codepoint;
        bool alt_is_down;
        bool ctrl_is_down;
        bool shift_is_down;
        bool command_is_down;
    } key;

    struct {
        int32_t index;
        CuiPoint position;
    } pointer;
} CuiEvent;

typedef enum CuiCursorType
{
    CUI_CURSOR_NONE                = 1,
    CUI_CURSOR_ARROW               = 2,
    CUI_CURSOR_HAND                = 3,
    CUI_CURSOR_TEXT                = 4,
    CUI_CURSOR_MOVE_ALL            = 5,
    CUI_CURSOR_MOVE_LEFT_RIGHT     = 6,
    CUI_CURSOR_MOVE_UP_DOWN        = 7,
    CUI_CURSOR_RESIZE_TOP          = 8,
    CUI_CURSOR_RESIZE_BOTTOM       = 9,
    CUI_CURSOR_RESIZE_LEFT         = 10,
    CUI_CURSOR_RESIZE_RIGHT        = 11,
    CUI_CURSOR_RESIZE_TOP_LEFT     = 12,
    CUI_CURSOR_RESIZE_TOP_RIGHT    = 13,
    CUI_CURSOR_RESIZE_BOTTOM_LEFT  = 14,
    CUI_CURSOR_RESIZE_BOTTOM_RIGHT = 15,
} CuiCursorType;

typedef enum CuiWidgetType
{
    CUI_WIDGET_TYPE_BOX       = 0,
    CUI_WIDGET_TYPE_STACK     = 1,
    CUI_WIDGET_TYPE_LABEL     = 2,
    CUI_WIDGET_TYPE_BUTTON    = 3,
    CUI_WIDGET_TYPE_CHECKBOX  = 4,
    CUI_WIDGET_TYPE_TEXTINPUT = 5,

    CUI_WIDGET_TYPE_CUSTOM = 100,
} CuiWidgetType;

typedef enum CuiWidgetFlags
{
    CUI_WIDGET_FLAG_DRAW_BACKGROUND = (1 << 0),
    CUI_WIDGET_FLAG_FIXED_WIDTH     = (1 << 1),
    CUI_WIDGET_FLAG_FIXED_HEIGHT    = (1 << 2),
    CUI_WIDGET_FLAG_CLIP_CONTENT    = (1 << 3),
} CuiWidgetFlags;

typedef enum CuiWidgetStateFlags
{
    CUI_WIDGET_STATE_HOVERED = (1 << 0),
    CUI_WIDGET_STATE_PRESSED = (1 << 1),
    CUI_WIDGET_STATE_FOCUSED = (1 << 2),
} CuiWidgetStateFlags;

typedef enum CuiWindowCreationFlags
{
    // all platforms
    CUI_WINDOW_CREATION_FLAG_PREFER_SYSTEM_DECORATION = (1 << 0),
    CUI_WINDOW_CREATION_FLAG_NOT_USER_RESIZABLE       = (1 << 1),

    // macos
    /* This creates a full height titlebar without a title
     * and puts your apps content inside the titlebar area.
     * Use cui_window_get_titlebar_height() to adjust your content.
     */
    CUI_WINDOW_CREATION_FLAG_MACOS_UNIFIED_TITLEBAR   = (1 << 2),
} CuiWindowCreationFlags;

typedef enum CuiWindowFrameActions
{
    CUI_WINDOW_FRAME_ACTION_CLOSE           = (1 << 0),
    CUI_WINDOW_FRAME_ACTION_SET_FULLSCREEN  = (1 << 1),
} CuiWindowFrameActions;

typedef struct CuiWindowFrameResult
{
    uint32_t window_frame_actions;

    bool should_be_fullscreen;
} CuiWindowFrameResult;

typedef struct CuiWindow CuiWindow;
typedef struct CuiGraphicsContext CuiGraphicsContext;

typedef struct CuiWidget CuiWidget;
typedef struct CuiWidgetList CuiWidgetList;

struct CuiWidgetList
{
    CuiWidgetList *prev;
    CuiWidgetList *next;
};

struct CuiWidget
{
    CuiWidgetList list;
    CuiWidgetList children;

    CuiWidget *parent;
    CuiWindow *window;

    uint32_t type;
    uint32_t flags;
    uint32_t state;

    uint32_t value;
    uint32_t old_value;

    float ui_scale;

    CuiFontId font_id;
    const CuiColorTheme *color_theme;

    CuiString label; // NOTE: This is also the placeholder text for text inputs
    CuiIconType icon_type;

    CuiTextInput text_input;

    CuiColorThemeId color_normal_background;
    CuiColorThemeId color_normal_box_shadow;
    CuiColorThemeId color_normal_border;
    CuiColorThemeId color_normal_text;
    CuiColorThemeId color_normal_placeholder;
    CuiColorThemeId color_normal_icon;

    CuiAxis main_axis;
    CuiGravity x_gravity;
    CuiGravity y_gravity;

    CuiRect rect;

    CuiFloatRect padding;
    CuiFloatRect border_width;
    CuiFloatRect border_radius;
    CuiFloatPoint preferred_size;
    CuiFloatPoint shadow_offset;
    float inline_padding;
    float blur_radius;

    CuiPoint effective_icon_size;

    CuiRect effective_padding;
    CuiRect effective_border_width;
    CuiRect effective_border_radius;
    CuiPoint effective_preferred_size;
    CuiPoint effective_shadow_offset;
    int32_t effective_inline_padding;
    int32_t effective_blur_radius;

    CuiFloatPoint label_offset;
    CuiFloatPoint text_offset;
    CuiFloatPoint icon_offset;

    // event callbacks
    void     (*on_action)          (CuiWidget *widget);
    void     (*on_changed)         (CuiWidget *widget);

    // custom widget functions
    void     (*set_ui_scale)       (CuiWidget *widget, float ui_scale);
    CuiPoint (*get_preferred_size) (CuiWidget *widget);
    void     (*layout)             (CuiWidget *widget, CuiRect rect);
    void     (*draw)               (CuiWidget *widget, CuiGraphicsContext *ctx, const CuiColorTheme *color_theme);
    bool     (*handle_event)       (CuiWidget *widget, CuiEventType event_type);
};

#define CuiWidgetInitCustomFunctions(widget, function_prefix)                   \
    (widget)->set_ui_scale       = function_prefix##set_ui_scale;               \
    (widget)->get_preferred_size = function_prefix##get_preferred_size;         \
    (widget)->layout             = function_prefix##layout;                     \
    (widget)->draw               = function_prefix##draw;                       \
    (widget)->handle_event       = function_prefix##handle_event;

#endif

typedef enum CuiImageMetaDataType
{
    CUI_IMAGE_META_DATA_CAMERA_MAKER = 0,
    CUI_IMAGE_META_DATA_CAMERA_MODEL = 1,
} CuiImageMetaDataType;

typedef struct CuiImageMetaData
{
    CuiImageMetaDataType type;
    CuiString value;
} CuiImageMetaData;

#ifdef __cplusplus
extern "C" {
#endif

int32_t cui_parse_int32(CuiString str);

CuiString cui_string_trim(CuiString str);
CuiString cui_copy_string(CuiArena *arena, CuiString str);
CuiString cui_string_concat_n(CuiArena *arena, int32_t n, ...);
CuiString cui_path_concat(CuiArena *arena, CuiString a, CuiString b);
void cui_path_remove_last_directory(CuiString *dir);

char * cui_to_c_string(CuiArena *arena, CuiString str);

//
// unicode
//

bool cui_unicode_is_digit(uint32_t c);
bool cui_unicode_is_whitespace(uint32_t c);
bool cui_unicode_is_printable(uint32_t c);

CuiUnicodeResult cui_utf8_decode(CuiString str, int64_t index);
int64_t cui_utf8_encode(CuiString str, int64_t index, uint32_t codepoint);
CuiUnicodeResult cui_utf16le_decode(CuiString str, int64_t index);
int64_t cui_utf16le_encode(CuiString str, int64_t index, uint32_t codepoint);
CuiString cui_utf8_to_utf16le(CuiArena *arena, CuiString utf8_str);
CuiString cui_utf16le_to_utf8(CuiArena *arena, CuiString utf16_str);
int64_t cui_utf8_get_character_count(CuiString str);
int64_t cui_utf8_get_character_byte_offset(CuiString str, int64_t character_index);

//
// text input
//

void cui_text_input_allocate(CuiTextInput *input, int64_t capacity);
void cui_text_input_clear(CuiTextInput *input);
void cui_text_input_set_string_value(CuiTextInput *input, CuiString str, int64_t cursor_start, int64_t cursor_end);
void cui_text_input_select_all(CuiTextInput *input);
void cui_text_input_delete_range(CuiTextInput *input, int64_t start, int64_t end);
void cui_text_input_delete_selected_range(CuiTextInput *input);
CuiString cui_text_input_get_selected_text(CuiTextInput *input);
void cui_text_input_insert_string(CuiTextInput *input, CuiString str);
void cui_text_input_insert_codepoint(CuiTextInput *input, uint32_t codepoint);
bool cui_text_input_backspace(CuiTextInput *input);
void cui_text_input_move_left(CuiTextInput *input, bool shift_is_down);
void cui_text_input_move_right(CuiTextInput *input, bool shift_is_down);

//
// platform api
//

CuiString cui_platform_get_environment_variable(CuiArena *temporary_memory, CuiArena *arena, CuiString name);
int32_t cui_platform_get_environment_variable_int32(CuiArena *temporary_memory, CuiString name);

void *cui_platform_allocate(uint64_t size);
void cui_platform_deallocate(void *ptr, uint64_t size);

void cui_platform_set_clipboard_text(CuiArena *temporary_memory, CuiString text);
CuiString cui_platform_get_clipboard_text(CuiArena *arena);

uint64_t cui_platform_get_performance_counter(void);
uint64_t cui_platform_get_performance_frequency(void);

uint32_t cui_platform_get_core_count(void);
uint32_t cui_platform_get_performance_core_count(void);
uint32_t cui_platform_get_efficiency_core_count(void);

bool cui_platform_directory_exists(CuiArena *temporary_memory, CuiString directory);
void cui_platform_directory_create(CuiArena *temporary_memory, CuiString directory);

bool cui_platform_file_exists(CuiArena *temporary_memory, CuiString filename);
CuiFile *cui_platform_file_open(CuiArena *temporary_memory, CuiString filename, uint32_t mode);
CuiFile *cui_platform_file_create(CuiArena *temporary_memory, CuiString filename);
CuiFileAttributes cui_platform_file_get_attributes(CuiFile *file);
CuiFileAttributes cui_platform_get_file_attributes(CuiArena *temporary_memory, CuiString filename);
void cui_platform_file_truncate(CuiFile *file, uint64_t size);
void cui_platform_file_read(CuiFile *file, void *buffer, uint64_t offset, uint64_t size);
void cui_platform_file_write(CuiFile *file, void *buffer, uint64_t offset, uint64_t size);
void cui_platform_file_close(CuiFile *file);

static inline uint64_t
cui_platform_file_get_size(CuiFile *file)
{
    return cui_platform_file_get_attributes(file).size;
}

CuiString cui_platform_get_canonical_filename(CuiArena *temporary_memory, CuiArena *arena, CuiString filename);
void cui_platform_get_files(CuiArena *temporary_memory, CuiArena *arena, CuiString directory, CuiFileInfo **file_list);
void cui_platform_get_font_directories(CuiArena *temporary_memory, CuiArena *arena, CuiString **font_dirs);
CuiString cui_platform_get_data_directory(CuiArena *temporary_memory, CuiArena *arena);
CuiString cui_platform_get_current_working_directory(CuiArena *temporary_memory, CuiArena *arena);

//
// memory arena
//

void cui_arena_allocate(CuiArena *arena, uint64_t capacity);
void cui_arena_deallocate(CuiArena *arena);
void cui_arena_initialize_with_memory(CuiArena *arena, void *memory, uint64_t size);
void cui_arena_clear(CuiArena *arena);

void *cui_alloc(CuiArena *arena, uint64_t size, CuiAllocationParams params);
void *cui_realloc(CuiArena *arena, void *old_pointer, uint64_t old_size, uint64_t size, CuiAllocationParams params);

CuiTemporaryMemory cui_begin_temporary_memory(CuiArena *arena);
void cui_end_temporary_memory(CuiTemporaryMemory temp_memory);

//
// string builder
//

void cui_string_builder_init(CuiStringBuilder *builder, CuiArena *arena);
int64_t cui_string_builder_get_size(CuiStringBuilder *builder);
CuiString cui_string_builder_to_string(CuiStringBuilder *builder, CuiArena *arena);
CuiString cui_sprint(CuiArena *arena, CuiString format, ...);
void cui_string_builder_write_to_file(CuiStringBuilder *builder, CuiFile *file, uint64_t offset);
void cui_string_builder_print(CuiStringBuilder *builder, CuiString format, ...);
void cui_string_builder_print_valist(CuiStringBuilder *builder, CuiString format, va_list args);
void cui_string_builder_append_character(CuiStringBuilder *builder, uint8_t c);
void cui_string_builder_append_string(CuiStringBuilder *builder, CuiString str);
void cui_string_builder_append_number(CuiStringBuilder *builder, int64_t value, int64_t leading_zeros);
void cui_string_builder_append_unsigned_number(CuiStringBuilder *builder, uint64_t value, int64_t leading_zeros, int64_t base, bool uppercase_digits);
void cui_string_builder_append_float(CuiStringBuilder *builder, double value, int32_t digits_after_decimal_point);
void cui_string_builder_append_builder(CuiStringBuilder *builder, CuiStringBuilder *append);

//
// colors
//

uint32_t cui_color_pack_rgba(CuiColor color);
CuiColor cui_color_unpack_rgba(uint32_t color);
uint32_t cui_color_pack_bgra(CuiColor color);
CuiColor cui_color_unpack_bgra(uint32_t color);
CuiColor cui_color_rgb_to_hsv(CuiColor rgb_color);
CuiColor cui_color_hsv_to_rgb(CuiColor hsv_color);
CuiColor cui_color_rgb_to_hsl(CuiColor rgb_color);
CuiColor cui_color_hsl_to_rgb(CuiColor hsl_color);

//
// bitmap drawing
//

void cui_bitmap_clear(CuiBitmap *bitmap, CuiColor clear_color);

//
// images
//

CuiString cui_image_encode_bmp(CuiBitmap bitmap, bool top_to_bottom, bool bgra, CuiArena *arena);

bool cui_image_decode_pgm(CuiBitmap *bitmap, CuiString data, CuiArena *arena);
bool cui_image_decode_pbm(CuiBitmap *bitmap, CuiString data, CuiArena *arena);
bool cui_image_decode_qoi(CuiBitmap *bitmap, CuiString data, CuiArena *arena);
bool cui_image_decode_bmp(CuiBitmap *bitmap, CuiString data, CuiArena *arena);
bool cui_image_decode_jpeg(CuiArena *temporary_memory, CuiBitmap *bitmap, CuiString data, CuiArena *arena, CuiImageMetaData **meta_data, CuiArena *meta_data_arena);
bool cui_image_decode(CuiArena *temporary_memory, CuiBitmap *bitmap, CuiString data, CuiArena *arena, CuiImageMetaData **meta_data, CuiArena *meta_data_arena);

#ifndef CUI_NO_BACKEND

//
// job system
//

bool cui_background_task_start(CuiBackgroundTask *task, void (*task_func)(CuiBackgroundTask *, void *), void *data, bool is_interactive);
void cui_background_task_cancel(CuiBackgroundTask *task);
bool cui_background_task_has_finished(CuiBackgroundTask *task);

//
// window
//

CuiWindow *cui_window_create(uint32_t creation_flags);
void cui_window_set_title(CuiWindow *window, CuiString title);
void cui_window_resize(CuiWindow *window, int32_t width, int32_t height);
void cui_window_show(CuiWindow *window);
void cui_window_pack(CuiWindow *window);
void cui_window_set_fullscreen(CuiWindow *window, bool fullscreen); // TODO: remove
bool cui_window_is_maximized(CuiWindow *window);
bool cui_window_is_fullscreen(CuiWindow *window);
bool cui_window_is_tiled(CuiWindow *window);
float cui_window_get_ui_scale(CuiWindow *window);
float cui_window_get_titlebar_height(CuiWindow *window);
void cui_window_close(CuiWindow *window); // TODO: remove
void cui_window_set_root_widget(CuiWindow *window, CuiWidget *widget);
bool cui_window_handle_event(CuiWindow *window, CuiEventType event_type);
void cui_window_set_hovered(CuiWindow *window, CuiWidget *widget);
void cui_window_set_pressed(CuiWindow *window, CuiWidget *widget);
void cui_window_set_focused(CuiWindow *window, CuiWidget *widget);
void cui_window_request_redraw(CuiWindow *window);
void cui_window_set_color_theme(CuiWindow *window, const CuiColorTheme *color_theme);
int32_t cui_window_allocate_texture_id(CuiWindow *window);
void cui_window_deallocate_texture_id(CuiWindow *window, int32_t texture_id);
void cui_window_set_cursor(CuiWindow *window, CuiCursorType cursor_type);

#define cui_window_find_font(window, ...) cui_window_find_font_n(window, CuiNArgs(__VA_ARGS__), __VA_ARGS__)

CuiFontId cui_window_find_font_n(CuiWindow *window, const uint32_t n, ...);
void cui_window_update_font(CuiWindow *window, CuiFontId font_id, float size, float line_height);
int32_t cui_window_get_font_line_height(CuiWindow *window, CuiFontId font_id);
int32_t cui_window_get_font_cursor_offset(CuiWindow *window, CuiFontId font_id);
int32_t cui_window_get_font_cursor_height(CuiWindow *window, CuiFontId font_id);
float cui_window_get_font_baseline_offset(CuiWindow *window, CuiFontId font_id);
float cui_window_get_codepoint_width(CuiWindow *window, CuiFontId font_id, uint32_t codepoint);
float cui_window_get_string_width(CuiWindow *window, CuiFontId font_id, CuiString str);
float cui_window_get_string_width_until_character(CuiWindow *window, CuiFontId font_id, CuiString str, int64_t character_index);

// TODO: change prefix to 'cui_window_event_'
// TODO: group by event type
CuiPoint cui_window_get_mouse_position(CuiWindow *window);
bool cui_window_is_precise_scrolling(CuiWindow *window);
float cui_window_get_wheel_dx(CuiWindow *window);
float cui_window_get_wheel_dy(CuiWindow *window);
int32_t cui_window_get_pointer_index(CuiWindow *window);
CuiPoint cui_window_get_pointer_position(CuiWindow *window);
void cui_window_set_pointer_capture(CuiWindow *window, CuiWidget *widget, int32_t pointer_index);

bool cui_event_is_inside_widget(CuiWindow *window, CuiWidget *widget);
bool cui_event_pointer_is_inside_widget(CuiWindow *window, CuiWidget *widget);

//
// key event
//

bool cui_window_event_is_alt_down(CuiWindow *window);
bool cui_window_event_is_ctrl_down(CuiWindow *window);
bool cui_window_event_is_shift_down(CuiWindow *window);
bool cui_window_event_is_command_down(CuiWindow *window);
uint32_t cui_window_event_get_codepoint(CuiWindow *window);

//
// widget
//

void cui_widget_init(CuiWidget *widget, uint32_t type);
void cui_widget_add_flags(CuiWidget *widget, uint32_t flags);
void cui_widget_remove_flags(CuiWidget *widget, uint32_t flags);
void cui_widget_set_window(CuiWidget *widget, CuiWindow *window);
void cui_widget_set_textinput_buffer(CuiWidget *widget, void *buffer, int64_t size);
void cui_widget_set_textinput_value(CuiWidget *widget, CuiString value);
CuiString cui_widget_get_textinput_value(CuiWidget *widget);
CuiWidget * cui_widget_get_first_child(CuiWidget *widget);
void cui_widget_append_child(CuiWidget *widget, CuiWidget *child);
void cui_widget_insert_before(CuiWidget *widget, CuiWidget *anchor_child, CuiWidget *new_child);
void cui_widget_replace_child(CuiWidget *widget, CuiWidget *old_child, CuiWidget *new_child);
void cui_widget_remove_child(CuiWidget *widget, CuiWidget *old_child);
void cui_widget_set_main_axis(CuiWidget *widget, CuiAxis axis);
void cui_widget_set_x_axis_gravity(CuiWidget *widget, CuiGravity gravity);
void cui_widget_set_y_axis_gravity(CuiWidget *widget, CuiGravity gravity);
void cui_widget_set_value(CuiWidget *widget, uint32_t value);
void cui_widget_set_label(CuiWidget *widget, CuiString label);
void cui_widget_set_icon(CuiWidget *widget, CuiIconType icon_type);
void cui_widget_set_inline_padding(CuiWidget *widget, float padding);
void cui_widget_set_preferred_size(CuiWidget *widget, float width, float height);
void cui_widget_set_padding(CuiWidget *widget, float padding_top, float padding_right, float padding_bottom, float padding_left);
void cui_widget_set_border_width(CuiWidget *widget, float border_top, float border_right, float border_bottom, float border_left);
void cui_widget_set_border_radius(CuiWidget *widget, float topleft, float topright, float bottomright, float bottomleft);
void cui_widget_set_box_shadow(CuiWidget *widget, float x_offset, float y_offset, float blur_radius);
void cui_widget_set_ui_scale(CuiWidget *widget, float ui_scale);
void cui_widget_set_font(CuiWidget *widget, CuiFontId font_id);
void cui_widget_set_color_theme(CuiWidget *widget, const CuiColorTheme *color_theme);
void cui_widget_relayout_parent(CuiWidget *widget);
CuiPoint cui_widget_get_preferred_size(CuiWidget *widget);
void cui_widget_layout(CuiWidget *widget, CuiRect rect);
void cui_widget_draw(CuiWidget *widget, CuiGraphicsContext *ctx, const CuiColorTheme *color_theme);
bool cui_widget_handle_event(CuiWidget *widget, CuiEventType event_type);

//
// drawing
//

CuiRect cui_draw_set_clip_rect(CuiGraphicsContext *ctx, CuiRect clip_rect);
int32_t cui_draw_get_max_texture_width(CuiGraphicsContext *ctx);
int32_t cui_draw_get_max_texture_height(CuiGraphicsContext *ctx);
void cui_draw_allocate_texture(CuiGraphicsContext *ctx, int32_t texture_id, CuiBitmap bitmap);
void cui_draw_update_texture(CuiGraphicsContext *ctx, int32_t texture_id, CuiRect update_rect);
void cui_draw_deallocate_texture(CuiGraphicsContext *ctx, int32_t texture_id);
void cui_draw_fill_textured_rect(CuiGraphicsContext *ctx, CuiRect rect, CuiRect uv, int32_t texture_id, CuiColor color);
void cui_draw_fill_rect(CuiGraphicsContext *ctx, CuiRect rect, CuiColor color);
void cui_draw_fill_rounded_rect_1(CuiGraphicsContext *ctx, CuiRect rect, float radius, CuiColor color);
void cui_draw_fill_rounded_rect_4(CuiGraphicsContext *ctx, CuiRect rect, float top_left_radius, float top_right_radius,
                                  float bottom_right_radius, float bottom_left_radius, CuiColor color);
void cui_draw_fill_rounded_rect_8(CuiGraphicsContext *ctx, CuiRect rect, float top_left_radius_x, float top_left_radius_y,
                                  float top_right_radius_x, float top_right_radius_y, float bottom_right_radius_x,
                                  float bottom_right_radius_y, float bottom_left_radius_x, float bottom_left_radius_y, CuiColor color);
void cui_draw_stroke_rounded_rect_4(CuiGraphicsContext *ctx, CuiRect rect, float top_left_radius, float top_right_radius,
                                    float bottom_right_radius, float bottom_left_radius, int32_t stroke_width, CuiColor color);
void cui_draw_fill_inverted_rounded_rect_1(CuiGraphicsContext *ctx, CuiRect rect, float radius, CuiColor color);
void cui_draw_fill_shadow(CuiGraphicsContext *ctx, int32_t x, int32_t y, int32_t max, int32_t blur_radius, CuiDirection direction, CuiColor color);
void cui_draw_fill_shadow_corner(CuiGraphicsContext *ctx, int32_t x, int32_t y, int32_t radius, int32_t blur_radius, CuiDirection direction_x, CuiDirection direction_y, CuiColor color);
void cui_draw_fill_codepoint(CuiGraphicsContext *ctx, CuiFontId font_id, float x, float y, uint32_t codepoint, CuiColor color);
void cui_draw_fill_string(CuiGraphicsContext *ctx, CuiFontId font_id, float x, float y, CuiString str, CuiColor color);
void cui_draw_fill_shape(CuiGraphicsContext *ctx, float x, float y, CuiShapeType shape_type, float scale, CuiColor color);
void cui_draw_fill_icon(CuiGraphicsContext *ctx, float x, float y, float scale, CuiIconType icon_type, CuiColor color);

//
// event loop
//

CuiString *cui_get_files_to_open(void);
CuiString *cui_get_command_line_arguments(void);
CuiString cui_get_executable_directory(void);
CuiString cui_get_bundle_directory(void);

bool cui_init(int argument_count, char **arguments);
int cui_main_loop(void);
void cui_step(void);
void cui_signal_main_thread(void);
void cui_set_signal_callback(void (*signal_callback)(void));

#endif

#ifdef __cplusplus
}
#endif

typedef struct CuiArrayHeader
{
    int32_t count;
    int32_t allocated;
    CuiArena *arena;
} CuiArrayHeader;

static inline void *
_cui_array_grow(void *array, int32_t new_allocated, int32_t item_size, CuiArena *arena)
{
    CuiArrayHeader *new_array_header;

    if (array)
    {
        CuiArrayHeader *old_array_header = _cui_array_header(array);

        CuiAssert(old_array_header->count);

        int32_t old_allocated = old_array_header->allocated;
        int32_t old_size = sizeof(CuiArrayHeader) + (old_allocated * item_size);

        CuiAssert(old_allocated < new_allocated);

        int32_t new_size = sizeof(CuiArrayHeader) + (new_allocated * item_size);

        new_array_header = (CuiArrayHeader *) cui_realloc(old_array_header->arena, old_array_header, old_size, new_size, CuiDefaultAllocationParams());

        new_array_header->allocated = new_allocated;
    }
    else
    {
        CuiAssert(arena);

        if (new_allocated <= 0)
        {
            new_allocated = 16;
        }

        int32_t size = sizeof(CuiArrayHeader) + (new_allocated * item_size);

        new_array_header = (CuiArrayHeader *) cui_alloc(arena, size, CuiDefaultAllocationParams());

        new_array_header->count = 0;
        new_array_header->allocated = new_allocated;
        new_array_header->arena = arena;
    }

    return new_array_header + 1;
}

#if CUI_PLATFORM_WINDOWS

#ifndef UNICODE
#  define UNICODE
#endif
#define _UNICODE
#define NOMINMAX

#include <windows.h>

#endif

#endif /* _CUI_H */
