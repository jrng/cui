#ifndef _CUI_H_
#define _CUI_H_

#define CUI_PLATFORM_ANDROID 0
#define CUI_PLATFORM_WINDOWS 0
#define CUI_PLATFORM_LINUX 0
#define CUI_PLATFORM_MACOS 0

#if __ANDROID__
#undef CUI_PLATFORM_ANDROID
#define CUI_PLATFORM_ANDROID 1
#elif _WIN32
#undef CUI_PLATFORM_WINDOWS
#define CUI_PLATFORM_WINDOWS 1
#elif __linux__
#undef CUI_PLATFORM_LINUX
#define CUI_PLATFORM_LINUX 1
#elif __APPLE__ && __MACH__
#undef PLATFORM_MACOS
#define PLATFORM_MACOS 1
#endif

#define CUI_ARCH_ARM64 0
#define CUI_ARCH_X86_64 0

#if CUI_PLATFORM_ANDROID && __aarch64__
#undef CUI_ARCH_ARM64
#define CUI_ARCH_ARM64 1
#elif CUI_PLATFORM_WINDOWS && _M_AMD64
#undef CUI_ARCH_X86_64
#define CUI_ARCH_X86_64 1
#elif CUI_PLATFORM_LINUX && __x86_64__
#undef CUI_ARCH_X86_64
#define CUI_ARCH_X86_64 1
#elif CUI_PLATFORM_MACOS && __x86_64__
#undef CUI_ARCH_X86_64
#define CUI_ARCH_X86_64 1
#endif

#define CUI_IS_LITTLE_ENDIAN 1

#define CuiNArgs(...) __CuiNArgs(_CuiNArgs(__VA_ARGS__, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0))
#define __CuiNArgs(x) x
#define _CuiNArgs(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, n, ...) n

#define CuiArrayCount(array) (sizeof(array)/sizeof((array)[0]))
#define CuiStringLiteral(str) cui_make_string((char *) (str), sizeof(str) - 1)
#define CuiCString(str) cui_make_string((char *) (str), cui_string_length(str))
#define CuiClearToZero(val) cui_clear_to_zero(&(val), sizeof(val))

#define CuiKiB(value) (      (value) * (int64_t) 1024)
#define CuiMiB(value) (CuiKiB(value) * (int64_t) 1024)
#define CuiGiB(value) (CuiMiB(value) * (int64_t) 1024)

#define CuiAlign(value, alignment) (((value) + (alignment) - (int64_t) 1) & ~((alignment) - (int64_t) 1))

#define CuiForWidget(iter, sentinel)                        \
    for (CuiWidget *iter = (CuiWidget *) (sentinel)->next;  \
         &iter->list != (sentinel);                         \
         iter = (CuiWidget *) iter->list.next)

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

#ifdef NDEBUG
#define CuiAssert(expression)
#else
#define CuiAssert(expression) if (!(expression)) { _cui_assert(#expression, __FILE__, __LINE__); }
#endif

#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

static inline int32_t cui_min_int32(int32_t a, int32_t b) { return (a < b) ? a : b; }
static inline int32_t cui_max_int32(int32_t a, int32_t b) { return (a > b) ? a : b; }

static inline float cui_min_float(float a, float b) { return (a < b) ? a : b; }
static inline float cui_max_float(float a, float b) { return (a > b) ? a : b; }

static inline float
cui_abs_float(float a)
{
    union { float f; uint32_t u; } number = { a };

    number.u &= 0x7FFFFFFF;

    return number.f;
}

static inline uint16_t
cui_read_u16_be(uint8_t *data, int64_t offset)
{
    uint8_t *buffer = data + offset;
    return (buffer[0] << 8) | buffer[1];
}

static inline uint32_t
cui_read_u32_be(uint8_t *data, int64_t offset)
{
    uint8_t *buffer = data + offset;
    return (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3];
}

static inline void
cui_clear_to_zero(void *ptr, uint64_t size)
{
    uint8_t *dst = (uint8_t *) ptr;
    while (size--) *dst++ = 0;
}

static inline void
_cui_assert(const char *expression, const char *file, int line_number)
{
#if CUI_PLATFORM_WINDOWS
    __debugbreak();
#else
    printf("assertion failed: '%s', %s:%d\n", expression, file, line_number);
    __builtin_abort();
#endif
}

typedef enum CuiDirection
{
    CUI_DIRECTION_NORTH = 0,
    CUI_DIRECTION_EAST  = 1,
    CUI_DIRECTION_SOUTH = 2,
    CUI_DIRECTION_WEST  = 3,
} CuiDirection;

typedef struct CuiColor
{
    float r;
    float g;
    float b;
    float a;
} CuiColor;

#define CuiHexColor(color) ((CuiColor) { (float) ((color >> 16) & 0xFF) / 255.0f,   \
                                         (float) ((color >>  8) & 0xFF) / 255.0f,   \
                                         (float) ((color >>  0) & 0xFF) / 255.0f,   \
                                         (float) ((color >> 24) & 0xFF) / 255.0f })

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

typedef struct CuiBitmap
{
    int32_t width;
    int32_t height;
    int32_t stride;
    void *pixels;
} CuiBitmap;

typedef struct CuiColorTheme
{
    CuiColor default_bg;
    CuiColor default_fg;
    CuiColor tabs_bg;
    CuiColor tabs_active_tab_bg;
    CuiColor tabs_active_tab_fg;
    CuiColor tabs_inactive_tab_bg;
    CuiColor tabs_inactive_tab_fg;
} CuiColorTheme;

static const CuiColorTheme cui_color_theme_default_dark = {
    /* default_bg           */ CuiHexColor(0xFF282C34),
    /* default_fg           */ CuiHexColor(0xFFD7DAE0),
    /* tabs_bg              */ CuiHexColor(0xFF21252B),
    /* tabs_active_tab_bg   */ CuiHexColor(0xFF282C34),
    /* tabs_active_tab_fg   */ CuiHexColor(0xFFD7DAE0),
    /* tabs_inactive_tab_bg */ CuiHexColor(0xFF21252B),
    /* tabs_inactive_tab_fg */ CuiHexColor(0xFF6A717C),
};

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

static inline bool
cui_rect_overlaps(CuiRect a, CuiRect b)
{
    return ((a.min.x < b.max.x) && (a.max.x > b.min.x) &&
            (a.min.y < b.max.y) && (a.max.y > b.min.y));
}

static inline bool
cui_rect_has_area(CuiRect rect)
{
    return ((rect.min.x < rect.max.x) && (rect.min.y < rect.max.y));
}

typedef enum CuiEventType
{
    CUI_EVENT_TYPE_ENTER   = 0,
    CUI_EVENT_TYPE_LEAVE   = 1,
    CUI_EVENT_TYPE_MOVE    = 2,
    CUI_EVENT_TYPE_PRESS   = 3,
    CUI_EVENT_TYPE_DRAG    = 4,
    CUI_EVENT_TYPE_RELEASE = 5,
    CUI_EVENT_TYPE_WHEEL   = 6,
} CuiEventType;

typedef struct CuiEvent
{
    struct
    {
        int32_t x;
        int32_t y;
    } mouse;
    struct
    {
        int32_t dx;
    } wheel;
} CuiEvent;

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
#define cui_alloc_array(arena, type, count, params) (type *) cui_alloc(arena, count * sizeof(type), params)

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

typedef struct CuiString
{
    int64_t count;
    uint8_t *data;
} CuiString;

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

#define cui_string_concat(arena, ...) cui_string_concat_n(arena, CuiNArgs(__VA_ARGS__), __VA_ARGS__)

static inline CuiString
cui_make_string(void *data, int64_t count)
{
    CuiString result;
    result.count = count;
    result.data = (uint8_t *) data;
    return result;
}

static inline int64_t
cui_string_length(const char *str)
{
    int64_t length = 0;
    while (*str++) length++;
    return length;
}

static inline int64_t
cui_wide_string_length(const uint16_t *str)
{
    int64_t length = 0;
    while (*str++) length++;
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

    for (int64_t index = 0; index < a.count; index++)
    {
        if (a.data[index] != b.data[index]) return false;
    }

    return true;
}

static inline bool
cui_string_ends_with(CuiString str, CuiString end)
{
    if (str.count < end.count) return false;
    return cui_string_equals(cui_make_string(str.data + (str.count - end.count), end.count), end);
}

typedef enum CuiPathCommandType
{
    CUI_PATH_COMMAND_MOVE_TO            = 0,
    CUI_PATH_COMMAND_LINE_TO            = 1,
    CUI_PATH_COMMAND_QUADRATIC_CURVE_TO = 2,
    CUI_PATH_COMMAND_CUBIC_CURVE_TO     = 3,
    CUI_PATH_COMMAND_ARC_TO             = 4,
    CUI_PATH_COMMAND_CLOSE_PATH         = 5,
} CuiPathCommandType;

typedef struct CuiPathCommand
{
    CuiPathCommandType type;

    float x, y;

    float cx1, cy1;
    float cx2, cy2;

    bool large_arc_flag, sweep_flag;
} CuiPathCommand;

typedef struct CuiTransform
{
    float m[6];
} CuiTransform;

typedef struct CuiContourPoint
{
    uint8_t flags;
    float x;
    float y;
} CuiContourPoint;

typedef struct CuiEdge
{
    bool positive;
    float x0, y0;
    float x1, y1;
} CuiEdge;

typedef struct CuiFontFile
{
    CuiString name;
    CuiString contents;

    int16_t ascent;
    int16_t descent;
    int16_t line_gap;

    uint16_t glyph_count;
    uint16_t hmetrics_count;
    uint16_t loca_index_format;

    uint8_t *cmap;
    uint8_t *mapping_table;
    uint8_t *glyf;
    uint8_t *hmtx;
    uint8_t *loca;
} CuiFontFile;

typedef struct CuiFont
{
    float font_scale;
    float line_height;
    float baseline_offset;

    CuiFontFile *file;
    struct CuiFont *fallback;
} CuiFont;

typedef enum CuiShapeType
{
    CUI_SHAPE_LOAD           = 0,
    CUI_SHAPE_TAPE           = 1,
    CUI_SHAPE_CHECKBOX_OUTER = 2,
    CUI_SHAPE_CHECKBOX_INNER = 3,
    CUI_SHAPE_CHECKMARK      = 4,
} CuiShapeType;

typedef enum CuiIconType
{
    CUI_ICON_LOAD = 0,
    CUI_ICON_TAPE = 1,
} CuiIconType;

typedef enum CuiWidgetType
{
    CUI_WIDGET_TYPE_BOX         = 0,
    CUI_WIDGET_TYPE_GRAVITY_BOX = 1,
    CUI_WIDGET_TYPE_TOOLBAR     = 2,
    CUI_WIDGET_TYPE_TABS        = 3,
    CUI_WIDGET_TYPE_ICON_BUTTON = 4,
    CUI_WIDGET_TYPE_CHECKBOX    = 5,

    CUI_WIDGET_TYPE_CUSTOM      = 100,
} CuiWidgetType;

typedef enum CuiWidgetState
{
    CUI_WIDGET_STATE_HOVERED = (1 << 0),
    CUI_WIDGET_STATE_PRESSED = (1 << 1),
    CUI_WIDGET_STATE_FOCUSED = (1 << 2),
} CuiWidgetState;

typedef enum CuiWidgetFlag
{
    CUI_WIDGET_FLAG_FILL_BACKGROUND = (1 << 0),
} CuiWidgetFlag;

typedef struct CuiWidgetList
{
    struct CuiWidgetList *prev;
    struct CuiWidgetList *next;
} CuiWidgetList;

typedef struct CuiWindow CuiWindow;
typedef struct CuiGraphicsContext CuiGraphicsContext;

typedef struct CuiWidget
{
    // NOTE: this needs to be the first member of the struct.
    CuiWidgetList list;
    CuiWidgetList children;

    struct CuiWidget *parent;
    CuiWindow *window;

    CuiColorTheme *color_theme;

    uint32_t type;
    uint32_t flags;
    uint32_t state;

    uint32_t value;
    uint32_t old_value;

    float ui_scale;
    uint32_t active_index;

    CuiRect rect;
    CuiRect action_rect;
    CuiDirection direction;

    CuiString label;
    CuiIconType icon_type;

    int32_t tabs_width;
    int32_t tabs_height;
    int32_t border_width;
    int32_t inline_padding;
    CuiRect padding;

    void (*on_action)(struct CuiWidget *widget);

    CuiPoint (*get_preferred_size)(struct CuiWidget *widget);
    void (*set_ui_scale)(struct CuiWidget *widget, float ui_scale);
    void (*layout)(struct CuiWidget *widget, CuiRect rect);
    void (*draw)(struct CuiWidget *widget, CuiGraphicsContext *ctx, const CuiColorTheme *color_theme, CuiArena *temporary_memory);
    bool (*handle_event)(struct CuiWidget *widget, CuiEventType event_type);
} CuiWidget;

typedef struct CuiArrayHeader
{
    int32_t count;
    int32_t allocated;
    CuiArena *arena;
} CuiArrayHeader;

typedef enum CuiFontType
{
    CUI_FONT_DEFAULT = 0,
} CuiFontType;

#ifdef __cplusplus

#define _cui_array_header(array) ((CuiArrayHeader *) (array) - 1)
#define _cui_array_ensure_space(array)                      \
    (((cui_array_count(array) + 1) > cui_array_allocated(array)) ? (array) = (decltype(array)) _cui_array_grow(array, 2 * cui_array_allocated(array), sizeof(*(array)), 0) : 0)

#define cui_array_init(array, allocated, arena) ((array) = (decltype(array)) _cui_array_grow(array, allocated, sizeof(*(array)), arena))
#define cui_array_count(array) ((array) ? _cui_array_header(array)->count : 0)
#define cui_array_allocated(array) ((array) ? _cui_array_header(array)->allocated : 0)
#define cui_array_append(array) ((array) ? (_cui_array_ensure_space(array), (array) + _cui_array_header(array)->count++) : 0)

#else

#define _cui_array_header(array) ((CuiArrayHeader *) (array) - 1)
#define _cui_array_ensure_space(array)                      \
    (((cui_array_count(array) + 1) > cui_array_allocated(array)) ? (array) = _cui_array_grow(array, 2 * cui_array_allocated(array), sizeof(*(array)), 0) : 0)

#define cui_array_init(array, allocated, arena) ((array) = _cui_array_grow(array, allocated, sizeof(*(array)), arena))
#define cui_array_count(array) ((array) ? _cui_array_header(array)->count : 0)
#define cui_array_allocated(array) ((array) ? _cui_array_header(array)->allocated : 0)
#define cui_array_append(array) ((array) ? (_cui_array_ensure_space(array), (array) + _cui_array_header(array)->count++) : 0)

#endif

#ifdef __cplusplus
extern "C" {
#endif

CuiString cui_copy_string(CuiArena *arena, CuiString str);
char * cui_to_c_string(CuiArena *arena, CuiString str);

CuiString cui_utf8_to_utf16le(CuiArena *arena, CuiString utf8_str);

void *cui_allocate_platform_memory(uint64_t size);
void cui_deallocate_platform_memory(void *ptr, uint64_t size);

typedef struct CuiFile CuiFile;

typedef enum CuiFileMode
{
    CUI_FILE_MODE_READ  = (1 << 0),
    CUI_FILE_MODE_WRITE = (1 << 1),
} CuiFileMode;

CuiFile *cui_file_open(CuiArena *temporary_memory, CuiString filename, uint32_t mode);
CuiFileAttributes cui_file_get_attributes(CuiFile *file);
CuiFileAttributes cui_get_file_attributes(CuiArena *temporary_memory, CuiString filename);
void cui_file_read(CuiFile *file, void *buffer, uint64_t offset, uint64_t size);
void cui_file_close(CuiFile *file);

static inline uint64_t
cui_file_get_size(CuiFile *file)
{
    return cui_file_get_attributes(file).size;
}

void cui_get_files(CuiArena *temporary_memory, CuiString directory, CuiFileInfo **file_list, CuiArena *arena);
void cui_get_font_directories(CuiArena *temporary_memory, CuiString **font_dirs, CuiArena *arena);

CuiRect cui_draw_set_clip_rect(CuiGraphicsContext *ctx, CuiRect clip_rect);
void cui_draw_fill_rect(CuiGraphicsContext *ctx, CuiRect rect, CuiColor color);
void cui_draw_fill_shape(CuiArena *temporary_memory, CuiGraphicsContext *ctx, float x, float y, CuiShapeType shape_type, float scale, CuiColor color);
void cui_draw_fill_string(CuiArena *temporary_memory, CuiGraphicsContext *ctx, CuiFont *font, float x, float y, CuiString str, CuiColor color);

bool cui_init(int arg_count, char **args);
int cui_main_loop();
void cui_step();

void cui_allocate_arena(CuiArena *arena, uint64_t capacity);
void *cui_alloc(CuiArena *arena, uint64_t size, CuiAllocationParams params);
void *cui_realloc(CuiArena *arena, void *old_pointer, uint64_t old_size, uint64_t size, CuiAllocationParams params);

CuiTemporaryMemory cui_begin_temporary_memory(CuiArena *arena);
void cui_end_temporary_memory(CuiTemporaryMemory temp_memory);

bool cui_font_file_init(CuiFontFile *font_file, void *data, int64_t count);
uint32_t cui_font_file_get_glyph_index_from_codepoint(CuiFontFile *font_file, uint32_t codepoint);
uint16_t cui_font_file_get_glyph_advance(CuiFontFile *font_file, uint32_t glyph_index);
CuiRect cui_font_file_get_glyph_bounding_box(CuiFontFile *font_file, uint32_t glyph_index);
void cui_font_file_get_glyph_outline(CuiFontFile *font_file, CuiPathCommand **path, uint32_t glyph_index, CuiTransform transform, CuiArena *arena);

void cui_font_update_with_size_and_line_height(CuiFont *font, float font_size, float line_height);
float cui_font_get_string_width(CuiFont *font, CuiString str);

CuiWindow *cui_window_create();
void cui_window_set_title(CuiWindow *window, const char *title);
void cui_window_resize(CuiWindow *window, int32_t width, int32_t height);
float cui_window_get_ui_scale(CuiWindow *window);
void cui_window_show(CuiWindow *window);
void cui_window_close(CuiWindow *window);
void cui_window_destroy(CuiWindow *window);
void cui_window_request_redraw(CuiWindow *window, CuiRect rect);
bool cui_window_needs_redraw(CuiWindow *window);
void cui_window_redraw(CuiWindow *window, CuiRect rect);
void cui_window_redraw_all(CuiWindow *window);
float cui_window_get_ui_scale(CuiWindow *window);
CuiWidget *cui_window_get_root_widget(CuiWindow *window);
void cui_window_handle_event(CuiWindow *window, CuiEventType event_type);
void cui_window_set_user_data(CuiWindow *window, void *user_data);
void *cui_window_get_user_data(CuiWindow *window);
void cui_window_set_hovered(CuiWindow *window, CuiWidget *widget);
void cui_window_set_pressed(CuiWindow *window, CuiWidget *widget);
void cui_window_set_focused(CuiWindow *window, CuiWidget *widget);
int32_t cui_window_get_mouse_x(CuiWindow *window);
int32_t cui_window_get_mouse_y(CuiWindow *window);
int32_t cui_window_get_wheel_dx(CuiWindow *window);
CuiFont *cui_window_get_font(CuiWindow *window, CuiFontType font_type);

bool cui_platform_dialog_file_open(CuiWindow *window, CuiString **filenames, CuiString title, CuiString *filters, bool can_select_multiple);

void cui_widget_box_init(CuiWidget *widget);
void cui_widget_gravity_box_init(CuiWidget *widget, CuiDirection direction);
void cui_widget_toolbar_init(CuiWidget *widget);
void cui_widget_tabs_init(CuiWidget *widget);
void cui_widget_icon_button_init(CuiWidget *widget, CuiString label, CuiIconType icon_type);
void cui_widget_checkbox_init(CuiWidget *widget, CuiString label, bool initial_value);
void cui_widget_custom_init(CuiWidget *widget);

void cui_widget_add_flags(CuiWidget *widget, uint32_t flags);
void cui_widget_remove_flags(CuiWidget *widget, uint32_t flags);

void cui_widget_set_window(CuiWidget *widget, CuiWindow *window);
void cui_widget_append_child(CuiWidget *widget, CuiWidget *child);
CuiPoint cui_widget_get_preferred_size(CuiWidget *widget);
void cui_widget_set_ui_scale(CuiWidget *widget, float ui_scale);
void cui_widget_layout(CuiWidget *widget, CuiRect rect);
void cui_widget_draw(CuiWidget *widget, CuiGraphicsContext *ctx, const CuiColorTheme *color_theme, CuiArena *temporary_memory);
bool cui_widget_handle_event(CuiWidget *widget, CuiEventType event_type);
bool cui_widget_contains(CuiWidget *widget, CuiWidget *child);
void cui_widget_set_label(CuiWidget *widget, CuiString label);
void cui_widget_set_icon(CuiWidget *widget, CuiIconType icon_type);
void cui_widget_update_children(CuiWidget *widget);

bool cui_event_is_inside_rect(CuiWindow *window, CuiRect rect);
bool cui_event_is_inside_widget(CuiWindow *window, CuiWidget *widget);

CuiString cui_sprint(CuiArena *arena, CuiString format, ...);

#ifdef __cplusplus
}
#endif

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
        CuiAssert(new_allocated > 0);

        int32_t size = sizeof(CuiArrayHeader) + (new_allocated * item_size);

        new_array_header = (CuiArrayHeader *) cui_alloc(arena, size, CuiDefaultAllocationParams());

        new_array_header->count = 0;
        new_array_header->allocated = new_allocated;
        new_array_header->arena = arena;
    }

    return new_array_header + 1;
}

#if CUI_PLATFORM_WINDOWS

#define UNICODE
#define _UNICODE

#include <windows.h>

#endif

#endif /* _CUI_H_ */
