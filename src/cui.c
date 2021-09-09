#include <cui.h>

#include "private_cui.h"

static inline bool
cui_atomic_compare_and_swap(volatile uint32_t *dst, uint32_t old_value, uint32_t new_value)
{
#if CUI_PLATFORM_WINDOWS
    uint32_t result = _InterlockedCompareExchange((volatile long *) dst, new_value, old_value);
    return (result == old_value);
#else
    return __sync_bool_compare_and_swap(dst, old_value, new_value);
#endif
}

static inline void
cui_atomic_increment(volatile uint32_t *dst)
{
#if CUI_PLATFORM_WINDOWS
    _InterlockedIncrement((volatile long *) dst);
#else
    __sync_add_and_fetch(dst, 1);
#endif
}

typedef struct CuiUnicodeResult
{
    uint32_t codepoint;
    int32_t  byte_count;
} CuiUnicodeResult;

CuiUnicodeResult
cui_utf8_decode(CuiString str, int64_t index)
{
    CuiUnicodeResult result = { '?', 1 };

    if (index < str.count)
    {
        uint8_t c = str.data[index];

        if ((c & 0x80) == 0x00)
        {
            result.codepoint = (c & 0x7F);
        }
        else if (((c & 0xE0) == 0xC0) && ((index + 1) < str.count) &&
                 ((str.data[index + 1] & 0xC0) == 0x80))
        {
            result.codepoint = ((uint32_t) (c & 0x1F) << 6) |
                                (uint32_t) (str.data[index + 1] & 0x3F);
            result.byte_count = 2;
        }
        else if (((c & 0xF0) == 0xE0) && ((index + 2) < str.count) &&
                 ((str.data[index + 1] & 0xC0) == 0x80) &&
                 ((str.data[index + 2] & 0xC0) == 0x80))
        {
            result.codepoint = ((uint32_t) (c & 0x0F) << 12) |
                               ((uint32_t) (str.data[index + 1] & 0x3F) << 6) |
                                (uint32_t) (str.data[index + 2] & 0x3F);
            result.byte_count = 3;
        }
        else if (((c & 0xF8) == 0xF0) && ((index + 3) < str.count) &&
                 ((str.data[index + 1] & 0xC0) == 0x80) &&
                 ((str.data[index + 2] & 0xC0) == 0x80) &&
                 ((str.data[index + 3] & 0xC0) == 0x80))
        {
            result.codepoint = ((uint32_t) (c & 0x0F) << 18) |
                               ((uint32_t) (str.data[index + 1] & 0x3F) << 12) |
                               ((uint32_t) (str.data[index + 2] & 0x3F) << 6) |
                                (uint32_t) (str.data[index + 3] & 0x3F);
            result.byte_count = 4;
        }
    }

    return result;
}

int64_t
cui_utf8_encode(CuiString str, int64_t index, uint32_t codepoint)
{
    if (codepoint < 0x80)
    {
        if (index < str.count)
        {
            str.data[index] = (uint8_t) codepoint;
        }

        return 1;
    }
    else if (codepoint < 0x800)
    {
        if ((index + 1) < str.count)
        {
            str.data[index + 0] = 0xC0 | (uint8_t) ((codepoint >>  6) & 0x1F);
            str.data[index + 1] = 0x80 | (uint8_t) ( codepoint        & 0x3F);
        }

        return 2;
    }
    else if (codepoint < 0x10000)
    {
        if ((index + 2) < str.count)
        {
            str.data[index + 0] = 0xE0 | (uint8_t) ((codepoint >> 12) & 0x0F);
            str.data[index + 1] = 0x80 | (uint8_t) ((codepoint >>  6) & 0x3F);
            str.data[index + 2] = 0x80 | (uint8_t) ( codepoint        & 0x3F);
        }

        return 3;
    }
    else if (codepoint < 0x110000)
    {
        if ((index + 3) < str.count)
        {
            str.data[index + 0] = 0xF0 | (uint8_t) ((codepoint >> 18) & 0x07);
            str.data[index + 1] = 0x80 | (uint8_t) ((codepoint >> 12) & 0x3F);
            str.data[index + 2] = 0x80 | (uint8_t) ((codepoint >>  6) & 0x3F);
            str.data[index + 3] = 0x80 | (uint8_t) ( codepoint        & 0x3F);
        }

        return 4;
    }

    return 0;
}

CuiUnicodeResult
cui_utf16le_decode(CuiString str, int64_t index)
{
    CuiUnicodeResult result = { '?', 2 };

    if ((index + 1) < str.count)
    {
        uint16_t value = (str.data[index + 1] << 8) | str.data[index + 0];

        if (((value & 0xFB) == 0xD8) && ((index + 3) < str.count))
        {
            // TODO
        }
        else
        {
            result.codepoint = value;
            result.byte_count = 2;
        }
    }

    return result;
}

int64_t
cui_utf16be_encode(CuiString str, int64_t index, uint32_t codepoint)
{
    if ((codepoint < 0xD800) || ((codepoint >= 0xE000) && (codepoint < 0x10000)))
    {
        if ((index + 1) < str.count)
        {
            str.data[index + 0] = (uint8_t) ((codepoint >> 8) & 0xFF);
            str.data[index + 1] = (uint8_t) ( codepoint       & 0xFF);
        }

        return 2;
    }

    return 0;
}

int64_t
cui_utf16le_encode(CuiString str, int64_t index, uint32_t codepoint)
{
    if ((codepoint < 0xD800) || ((codepoint >= 0xE000) && (codepoint < 0x10000)))
    {
        if ((index + 1) < str.count)
        {
            str.data[index + 0] = (uint8_t) ( codepoint       & 0xFF);
            str.data[index + 1] = (uint8_t) ((codepoint >> 8) & 0xFF);
        }

        return 2;
    }

    return 0;
}

CuiString
cui_utf8_to_utf16be(CuiArena *arena, CuiString utf8_str)
{
    CuiString result = { 0 };

    if (utf8_str.count)
    {
        result.count = 2 * utf8_str.count;
        result.data  = cui_alloc_array(arena, uint8_t, result.count, CuiDefaultAllocationParams());

        int64_t input_index = 0;
        int64_t output_index = 0;

        while (input_index < utf8_str.count)
        {
            CuiUnicodeResult utf8 = cui_utf8_decode(utf8_str, input_index);
            output_index += cui_utf16be_encode(result, output_index, utf8.codepoint);
            input_index += utf8.byte_count;
        }

        result.count = output_index;
    }

    return result;
}

CuiString
cui_utf8_to_utf16le(CuiArena *arena, CuiString utf8_str)
{
    CuiString result = { 0 };

    if (utf8_str.count)
    {
        result.count = 2 * utf8_str.count;
        result.data  = cui_alloc_array(arena, uint8_t, result.count, CuiDefaultAllocationParams());

        int64_t input_index = 0;
        int64_t output_index = 0;

        while (input_index < utf8_str.count)
        {
            CuiUnicodeResult utf8 = cui_utf8_decode(utf8_str, input_index);
            output_index += cui_utf16le_encode(result, output_index, utf8.codepoint);
            input_index += utf8.byte_count;
        }

        result.count = output_index;
    }

    return result;
}

CuiString
cui_utf16le_to_utf8(CuiArena *arena, CuiString utf16_str)
{
    CuiString result = { 0 };

    if (utf16_str.count)
    {
        result.count = utf16_str.count / 2;
        result.data  = cui_alloc_array(arena, uint8_t, result.count, CuiDefaultAllocationParams());

        int64_t input_index = 0;
        int64_t output_index = 0;

        while (input_index < utf16_str.count)
        {
            CuiUnicodeResult utf16 = cui_utf16le_decode(utf16_str, input_index);
            output_index += cui_utf8_encode(result, output_index, utf16.codepoint);
            input_index += utf16.byte_count;
        }

        result.count = output_index;
    }

    return result;
}

CuiString
cui_copy_string(CuiArena *arena, CuiString str)
{
    CuiString result = { 0 };

    if (str.count)
    {
        result.count = str.count;
        result.data  = cui_alloc_array(arena, uint8_t, str.count, CuiDefaultAllocationParams());

        uint8_t *src = str.data;
        uint8_t *dst = result.data;

        while (str.count--)
        {
            *dst++ = *src++;
        }
    }

    return result;
}

CuiString
cui_string_concat_n(CuiArena *arena, int32_t n, ...)
{
    CuiAssert(n <= 10);

    CuiString strings[10];
    CuiString result = { 0 };

    va_list args;
    va_start(args, n);

    for (int32_t i = 0; i < n; i += 1)
    {
        strings[i] = va_arg(args, CuiString);
        result.count += strings[i].count;
    }

    va_end(args);

    result.data = cui_alloc_array(arena, uint8_t, result.count, CuiDefaultAllocationParams());

    CuiAssert(result.data);

    uint8_t *at = result.data;

    for (int32_t i = 0; i < n; i += 1)
    {
        CuiString str = strings[i];

        for (int64_t j = 0; j < str.count; j += 1)
        {
            *at++ = str.data[j];
        }
    }

    return result;
}

CuiString
cui_path_concat(CuiArena *arena, CuiString a, CuiString b)
{
#if CUI_PLATFORM_WINDOWS
    if (a.count && (a.data[a.count - 1] == '\\'))
#else
    if (a.count && (a.data[a.count - 1] == '/'))
#endif
    {
        a.count -= 1;
    }

#if CUI_PLATFORM_WINDOWS
    if (b.count && (b.data[0] == '\\'))
#else
    if (b.count && (b.data[0] == '/'))
#endif
    {
        cui_string_advance(&b, 1);
    }

#if CUI_PLATFORM_WINDOWS
    return cui_string_concat(arena, a, CuiStringLiteral("\\"), b);
#else
    return cui_string_concat(arena, a, CuiStringLiteral("/"), b);
#endif
}

char *
cui_to_c_string(CuiArena *arena, CuiString str)
{
    // NOTE: we do two zero bytes at the end
    // to handle wide character encodings.
    char *result = cui_alloc_array(arena, char, str.count + 2, CuiDefaultAllocationParams());

    uint8_t *src = str.data;
    uint8_t *dst = (uint8_t *) result;

    while (str.count--)
    {
        *dst++ = *src++;
    }

    *dst++ = 0;
    *dst = 0;

    return result;
}

void
cui_glyph_cache_reset(CuiGlyphCache *cache)
{
    cache->count = 0;

    for (uint32_t index = 0; index < cache->allocated; index++)
    {
        cache->hashes[index] = 0;
    }

    cache->x = 0;
    cache->y = 0;
    cache->y_max = 0;

    uint8_t *row = (uint8_t *) cache->texture.pixels;

    for (int32_t y = 0; y < cache->texture.height; y += 1)
    {
        uint32_t *pixel = (uint32_t *) row;

        for (int32_t x = 0; x < cache->texture.width; x += 1)
        {
            *pixel++ = 0;
        }

        row += cache->texture.stride;
    }
}

void
cui_glyph_cache_create(CuiGlyphCache *cache)
{
    cache->allocated = 4096;
    cache->hashes    = (uint32_t *) cui_allocate_platform_memory(sizeof(uint32_t) * cache->allocated);
    cache->keys      = (CuiGlyphKey *) cui_allocate_platform_memory(sizeof(CuiGlyphKey) * cache->allocated);
    cache->rects     = (CuiRect *) cui_allocate_platform_memory(sizeof(CuiRect) * cache->allocated);

    cache->texture.width  = 2048;
    cache->texture.height = 2048;
    cache->texture.stride = CuiAlign(cache->texture.width * 4, 64);
    cache->texture.pixels = cui_allocate_platform_memory(cache->texture.stride * cache->texture.height);

    cui_glyph_cache_reset(cache);
}

bool
cui_glyph_cache_find(CuiGlyphCache *cache, uint32_t id, uint32_t codepoint, float scale, float offset_x, float offset_y, CuiRect *rect)
{
    uint32_t mask = cache->allocated - 1;

    uint32_t hash = codepoint | 1;
    uint32_t bucket = hash & mask;

    while (cache->hashes[bucket])
    {
        if (cache->hashes[bucket] == hash)
        {
            if ((cache->keys[bucket].id == id) &&
                (cache->keys[bucket].codepoint == codepoint) &&
                (cache->keys[bucket].scale == scale) &&
                (cache->keys[bucket].offset_x == offset_x) &&
                (cache->keys[bucket].offset_y == offset_y))
            {
                *rect = cache->rects[bucket];
                return true;
            }
        }

        bucket = (bucket + 1) & mask;
    }

    return false;
}

void
cui_glyph_cache_put(CuiGlyphCache *cache, uint32_t id, uint32_t codepoint, float scale, float offset_x, float offset_y, CuiRect rect)
{
    uint32_t mask = cache->allocated - 1;

    uint32_t hash = codepoint | 1;
    uint32_t bucket = hash & mask;

    cache->count += 1;

    for (;;)
    {
        if (!cache->hashes[bucket])
        {
            cache->hashes[bucket] = hash;
            cache->keys[bucket].id = id;
            cache->keys[bucket].codepoint = codepoint;
            cache->keys[bucket].scale = scale;
            cache->keys[bucket].offset_x = offset_x;
            cache->keys[bucket].offset_y = offset_y;
            cache->rects[bucket] = rect;
            return;
        }

        bucket = (bucket + 1) & mask;
    }
}

CuiRect
cui_glyph_cache_allocate_texture(CuiGlyphCache *cache, int32_t width, int32_t height)
{
    CuiRect result = { 0 };

    if ((cache->x + width) > cache->texture.width)
    {
        cache->y = cache->y_max;
        cache->x = 0;
    }

    if ((cache->y + height) <= cache->texture.height)
    {
        result.min.x = cache->x;
        result.min.y = cache->y;
        result.max.x = result.min.x + width;
        result.max.y = result.min.y + height;

        cache->x += width;

        if ((cache->y + height) > cache->y_max)
        {
            cache->y_max = cache->y + height;
        }
    }

    return result;
}

static CuiContext _cui_context;

static inline bool
_cui_common_init(int arg_count, char **args)
{
    cui_allocate_arena(&_cui_context.common.arena, CuiMiB(4));

    _cui_context.common.window_count = 0;

    return true;
}

static CuiWindow *
_cui_add_window()
{
    CuiWindow *window = 0;

    if (_cui_context.common.window_count < CuiArrayCount(_cui_context.common.windows))
    {
        window = (CuiWindow *) cui_allocate_platform_memory(sizeof(CuiWindow));
        _cui_context.common.windows[_cui_context.common.window_count] = window;
        _cui_context.common.window_count += 1;

        window->base.max_push_buffer_size = CuiMiB(256);
        window->base.max_index_buffer_count = 8 * 1024 * 1024;

        window->base.push_buffer = (uint8_t *) cui_allocate_platform_memory(window->base.max_push_buffer_size);
        window->base.index_buffer = (uint32_t *) cui_allocate_platform_memory(window->base.max_index_buffer_count * sizeof(uint32_t));

        cui_allocate_arena(&window->base.temporary_memory, CuiMiB(2));
        cui_glyph_cache_create(&window->base.glyph_cache);

        window->base.font.font_file = &_cui_context.common.font_file;

        cui_widget_box_init(&window->base.root_widget);
    }

    return window;
}

static inline void
_cui_remove_window(CuiWindow *window)
{
    uint32_t window_index = 0;

    for (; window_index < _cui_context.common.window_count; window_index += 1)
    {
        if (_cui_context.common.windows[window_index] == window)
        {
            break;
        }
    }

    CuiAssert(window_index < _cui_context.common.window_count);

    cui_deallocate_platform_memory(window->base.temporary_memory.base, window->base.temporary_memory.capacity);
    // TODO: destroy glyph cache

    _cui_context.common.window_count -= 1;
    _cui_context.common.windows[window_index] = _cui_context.common.windows[_cui_context.common.window_count];

    if (_cui_context.common.window_count == 0)
    {
        _cui_context.common.running = false;
    }
}

static void
_cui_work_queue_add(CuiWorkQueue *queue, CuiCompletionState *completion_state, CuiWorkFunction *work, void *data)
{
    uint32_t write_index = queue->write_index;
    uint32_t next_write_index = (write_index + 1) % CuiArrayCount(queue->entries);

    CuiAssert(next_write_index != queue->read_index);

    CuiWorkQueueEntry *entry = queue->entries + write_index;
    entry->work = work;
    entry->data = data;
    entry->completion_state = completion_state;

    cui_atomic_increment(&completion_state->completion_goal);
    queue->write_index = next_write_index;

#if CUI_PLATFORM_WINDOWS
    ReleaseSemaphore(queue->semaphore, 1, 0);
#else
    pthread_cond_signal(&queue->semaphore_cond);
#endif
}

static bool
_cui_work_queue_do_next_entry(CuiWorkQueue *queue)
{
    bool should_sleep = true;

    uint32_t read_index = queue->read_index;
    uint32_t next_read_index = (read_index + 1) % CuiArrayCount(queue->entries);

    if (read_index != queue->write_index)
    {
        if (cui_atomic_compare_and_swap(&queue->read_index, read_index, next_read_index))
        {
            CuiWorkQueueEntry entry = queue->entries[read_index];
            entry.work(entry.data);
            cui_atomic_increment(&entry.completion_state->completion_count);
        }

        should_sleep = false;
    }

    return should_sleep;
}

static void
_cui_work_queue_complete_all_work(CuiWorkQueue *queue, CuiCompletionState *completion_state)
{
    while (completion_state->completion_count != completion_state->completion_goal)
    {
        _cui_work_queue_do_next_entry(queue);
    }
}

int
cui_main_loop()
{
    _cui_context.common.running = true;

    while (_cui_context.common.running)
    {
        cui_step();
    }

    return 0;
}

void
cui_set_font(void *data, int64_t size)
{
    cui_font_file_init(&_cui_context.common.font_file, data, size);
}

#include "cui_renderer.c"

#if CUI_PLATFORM_WINDOWS
#include "cui_windows.c"
#elif CUI_PLATFORM_LINUX
#include "cui_linux.c"
#endif

#include "cui_font.c"
#include "cui_draw.c"
#include "cui_arena.c"
#include "cui_widget.c"
#include "cui_window.c"
#include "cui_string_builder.c"
