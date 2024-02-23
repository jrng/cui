#include <cui.h>

#include "private_cui.h"

#include "cui_font.c"

static inline bool
_cui_atomic_compare_and_swap(volatile uint32_t *dst, uint32_t old_value, uint32_t new_value)
{
#if CUI_PLATFORM_WINDOWS
    uint32_t result = _InterlockedCompareExchange((volatile long *) dst, new_value, old_value);
    return (result == old_value);
#else
    return __sync_bool_compare_and_swap(dst, old_value, new_value);
#endif
}

static inline uint32_t
_cui_atomic_read(volatile uint32_t *src)
{
#if CUI_PLATFORM_WINDOWS
    // TODO: add a write barrier here
    return *src;
#else
    return __sync_fetch_and_add(src, 0);
#endif
}

static inline void
_cui_atomic_increment(volatile uint32_t *dst)
{
#if CUI_PLATFORM_WINDOWS
    _InterlockedIncrement((volatile long *) dst);
#else
    __sync_add_and_fetch(dst, 1);
#endif
}

static void
_cui_font_file_manager_scan_fonts(CuiArena *temporary_memory, CuiFontFileManager *font_file_manager)
{
    CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(temporary_memory);

    CuiString *scan_paths = 0;
    cui_array_init(scan_paths, 16, temporary_memory);

    cui_platform_get_font_directories(temporary_memory, temporary_memory, &scan_paths);

    for (int32_t path_index = 0; path_index < cui_array_count(scan_paths); path_index += 1)
    {
        CuiString path = scan_paths[path_index];

        CuiFileInfo *file_list = 0;
        cui_array_init(file_list, 2, temporary_memory);

        cui_platform_get_files(temporary_memory, temporary_memory, path, &file_list);

        for (int32_t i = 0; i < cui_array_count(file_list); i += 1)
        {
            CuiFileInfo *file_info = file_list + i;

            if (file_info->attr.flags & CUI_FILE_ATTRIBUTE_IS_DIRECTORY)
            {
                if (!cui_string_equals(file_info->name, CuiStringLiteral(".")) &&
                    !cui_string_equals(file_info->name, CuiStringLiteral("..")))
                {
                    CuiString file_path = cui_path_concat(temporary_memory, path, file_info->name);
                    *cui_array_append(scan_paths) = file_path;
                }
            }
            else
            {
                if (cui_string_ends_with(file_info->name, CuiStringLiteral(".ttf")))
                {
                    CuiFontRef *font_ref = cui_array_append(font_file_manager->font_refs);

                    CuiString name = file_info->name;
                    name.count -= CuiStringLiteral(".ttf").count;

                    font_ref->name = cui_copy_string(&font_file_manager->arena, name);
                    font_ref->path = cui_path_concat(&font_file_manager->arena, path, file_info->name);
                }
            }
        }
    }

    cui_end_temporary_memory(temp_memory);

#if 0
    for (int32_t i = 0; i < cui_array_count(font_file_manager->font_refs); i += 1)
    {
        CuiFontRef *ref = font_file_manager->font_refs + i;

#if CUI_PLATFORM_ANDROID
        android_print("'%.*s' -> '%.*s'\n", (int32_t) ref->name.count, ref->name.data, (int32_t) ref->path.count, ref->path.data);
#else
        printf("'%.*s' -> '%.*s'\n", (int32_t) ref->name.count, ref->name.data, (int32_t) ref->path.count, ref->path.data);
#endif
    }
#endif
}

static inline CuiFontFile *
_cui_font_file_manager_get_font_file_from_id(CuiFontFileManager *font_file_manager, CuiFontFileId font_file_id)
{
    CuiAssert(font_file_id.value > 0);

    int32_t index = (int32_t) font_file_id.value - 1;

    CuiAssert(index < cui_array_count(font_file_manager->font_files));

    return font_file_manager->font_files + index;
}

static inline CuiFont *
_cui_font_manager_get_font_from_id(CuiFontManager *font_manager, CuiFontId font_id)
{
    CuiFont *result = 0;

    if (font_id.value > 0)
    {
        int32_t index = (int32_t) font_id.value - 1;

        CuiAssert(index < cui_array_count(font_manager->sized_fonts));

        result = &font_manager->sized_fonts[index].font;
    }

    return result;
}

static inline void
_cui_sized_font_update(CuiSizedFont *sized_font, CuiFontFileManager *font_file_manager, float ui_scale)
{
    CuiFont *font = &sized_font->font;

    CuiFontFile *font_file = _cui_font_file_manager_get_font_file_from_id(font_file_manager, font->file_id);

    float font_height = roundf(ui_scale * sized_font->size);

    font->font_scale      = _cui_font_file_get_scale_for_unit_height(font_file, font_height);
    font->line_height     = (int32_t) ceilf(font_height * sized_font->line_height);
    font->baseline_offset = 0.5f * ((float) font->line_height - font_height) +
                            (font_file->ascent * font->font_scale);
    font->cursor_offset   = (int32_t) floorf(0.5f * ((float) font->line_height - font_height));
    font->cursor_height   = font->line_height - 2 * font->cursor_offset;
}

static float
_cui_font_get_codepoint_width(CuiFontManager *font_manager, CuiFontId font_id, uint32_t codepoint)
{
    CuiAssert(font_id.value > 0);

    CuiFont *font = _cui_font_manager_get_font_from_id(font_manager, font_id);

    CuiAssert(font);

    uint32_t glyph_index = 0;
    CuiFont *used_font = font;

    while (used_font)
    {
        CuiFontFile *font_file = _cui_font_file_manager_get_font_file_from_id(font_manager->font_file_manager, used_font->file_id);
        glyph_index = _cui_font_file_get_glyph_index_from_codepoint(font_file, codepoint);

        if (glyph_index) break;

        used_font = _cui_font_manager_get_font_from_id(font_manager, used_font->fallback_id);
    }

    if (!used_font)
    {
        used_font = font;
        glyph_index = 0;
    }

    CuiFontFile *used_font_file = _cui_font_file_manager_get_font_file_from_id(font_manager->font_file_manager, used_font->file_id);

    return used_font->font_scale * (float) _cui_font_file_get_glyph_advance(used_font_file, glyph_index);
}

static float
_cui_font_get_string_width(CuiFontManager *font_manager, CuiFontId font_id, CuiString str)
{
    CuiAssert(font_id.value > 0);

    CuiFont *font = _cui_font_manager_get_font_from_id(font_manager, font_id);
    // CuiFont *prev_used_font = 0;

    CuiAssert(font);

    int64_t index = 0;
    float width = 0.0f;
    uint32_t glyph_index /*, prev_glyph_index */;

    while (index < str.count)
    {
        CuiUnicodeResult utf8 = cui_utf8_decode(str, index);

        glyph_index = 0;
        CuiFont *used_font = font;

        while (used_font)
        {
            CuiFontFile *font_file = _cui_font_file_manager_get_font_file_from_id(font_manager->font_file_manager, used_font->file_id);
            glyph_index = _cui_font_file_get_glyph_index_from_codepoint(font_file, utf8.codepoint);

            if (glyph_index) break;

            used_font = _cui_font_manager_get_font_from_id(font_manager, used_font->fallback_id);
        }

        if (!used_font)
        {
            used_font = font;
            glyph_index = 0;
        }

        CuiFontFile *used_font_file = _cui_font_file_manager_get_font_file_from_id(font_manager->font_file_manager, used_font->file_id);

#if 0
        if ((index > 0) && (used_font == prev_used_font))
        {
            width += used_font->font_scale * get_glyph_kerning(used_font->file, prev_glyph_index, glyph_index);
        }
#endif

        width += used_font->font_scale * (float) _cui_font_file_get_glyph_advance(used_font_file, glyph_index);

        // prev_glyph_index = glyph_index;
        // prev_used_font = used_font;
        index += utf8.byte_count;
    }

    return width;
}

static float
_cui_font_get_substring_width(CuiFontManager *font_manager, CuiFontId font_id, CuiString str, int64_t character_index)
{
    CuiAssert(font_id.value > 0);

    CuiFont *font = _cui_font_manager_get_font_from_id(font_manager, font_id);
    // CuiFont *prev_used_font = 0;

    CuiAssert(font);

    int64_t index = 0;
    int64_t count = 0;
    float width = 0.0f;
    uint32_t glyph_index /*, prev_glyph_index */;

    while (index < str.count)
    {
        if (count == character_index)
        {
            break;
        }

        CuiUnicodeResult utf8 = cui_utf8_decode(str, index);

        glyph_index = 0;
        CuiFont *used_font = font;

        while (used_font)
        {
            CuiFontFile *font_file = _cui_font_file_manager_get_font_file_from_id(font_manager->font_file_manager, used_font->file_id);
            glyph_index = _cui_font_file_get_glyph_index_from_codepoint(font_file, utf8.codepoint);

            if (glyph_index) break;

            used_font = _cui_font_manager_get_font_from_id(font_manager, used_font->fallback_id);
        }

        if (!used_font)
        {
            used_font = font;
            glyph_index = 0;
        }

        CuiFontFile *used_font_file = _cui_font_file_manager_get_font_file_from_id(font_manager->font_file_manager, used_font->file_id);

#if 0
        if ((index > 0) && (used_font == prev_used_font))
        {
            width += used_font->font_scale * get_glyph_kerning(used_font->file, prev_glyph_index, glyph_index);
        }
#endif

        width += used_font->font_scale * (float) _cui_font_file_get_glyph_advance(used_font_file, glyph_index);

        // prev_glyph_index = glyph_index;
        // prev_used_font = used_font;
        index += utf8.byte_count;
        count += 1;
    }

    return width;
}

#define _cui_font_manager_find_font(temporary_memory, font_manager, ui_scale, ...) \
    _cui_font_manager_find_font_n(temporary_memory, font_manager, ui_scale, CuiNArgs(__VA_ARGS__), __VA_ARGS__)

static CuiFontId
_cui_font_manager_find_font_valist(CuiArena *temporary_memory, CuiFontManager *font_manager, float ui_scale, const uint32_t n, va_list args)
{
    CuiFontId result  = { .value = 0 };
    CuiFontId current = { .value = 0 };

    float size, line_height;

    int32_t start_index = cui_array_count(font_manager->sized_fonts);

    CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(temporary_memory);

    for (uint32_t i = 0; i < n; i += 1)
    {
        CuiSizedFontSpec sized_font_spec = va_arg(args, CuiSizedFontSpec);
        CuiString font_name = sized_font_spec.name;

        CuiFontFileId font_file_id = { .value = 0 };

        for (int32_t i = 0; i < cui_array_count(font_manager->font_file_manager->font_files); i += 1)
        {
            CuiFontFile *file = font_manager->font_file_manager->font_files + i;

            if (cui_string_equals(file->name, font_name))
            {
                font_file_id.value = (uint16_t) (i + 1);
                break;
            }
        }

        if (!font_file_id.value)
        {
            for (int32_t i = 0; i < cui_array_count(font_manager->font_file_manager->font_refs); i += 1)
            {
                CuiFontRef *font_ref = font_manager->font_file_manager->font_refs + i;

                if (cui_string_equals(font_ref->name, font_name))
                {
                    CuiFontFile *font_file = cui_array_append(font_manager->font_file_manager->font_files);
                    font_file_id.value = cui_array_count(font_manager->font_file_manager->font_files);

                    CuiString font_contents = { 0 };

                    CuiFile *file = cui_platform_file_open(temporary_memory, font_ref->path, CUI_FILE_MODE_READ);

                    if (file)
                    {
                        uint64_t file_size = cui_platform_file_get_size(file);
                        font_contents.data = (uint8_t *) cui_platform_allocate(file_size);

                        if (font_contents.data)
                        {
                            font_contents.count = file_size;
                            cui_platform_file_read(file, font_contents.data, 0, file_size);
                        }

                        cui_platform_file_close(file);
                    }

                    if (!_cui_font_file_init(font_file, font_contents.data, font_contents.count))
                    {
                        _cui_array_header(font_manager->font_file_manager->font_files)->count -= 1;
                        font_file_id.value = 0;
                    }
                    else
                    {
                        font_file->name = font_ref->name;
                    }

                    break;
                }
            }
        }

        if (font_file_id.value)
        {
            CuiFontId font_id;
            CuiSizedFont *sized_font = cui_array_append(font_manager->sized_fonts);
            font_id.value = cui_array_count(font_manager->sized_fonts);

            if (current.value)
            {
                CuiSizedFont *current_sized_font = font_manager->sized_fonts + ((int32_t) current.value - 1);
                current_sized_font->font.fallback_id = font_id;
                current = font_id;
            }
            else
            {
                size = sized_font_spec.size;
                line_height = sized_font_spec.line_height;

                result = font_id;
                current = font_id;
            }

            sized_font->size = size;
            sized_font->line_height = line_height;
            sized_font->font.file_id = font_file_id;
        }
    }

    cui_end_temporary_memory(temp_memory);

    int32_t end_index = cui_array_count(font_manager->sized_fonts);

    for (int32_t index = start_index; index < end_index; index += 1)
    {
        CuiSizedFont *sized_font = font_manager->sized_fonts + index;
        _cui_sized_font_update(sized_font, font_manager->font_file_manager, ui_scale);
    }

    return result;
}

static inline CuiFontId
_cui_font_manager_find_font_n(CuiArena *temporary_memory, CuiFontManager *font_manager, float ui_scale, const uint32_t n, ...)
{
    va_list args;
    va_start(args, n);

    CuiFontId result = _cui_font_manager_find_font_valist(temporary_memory, font_manager, ui_scale, n, args);

    va_end(args);

    return result;
}

#ifndef CUI_NO_BACKEND

static const uint32_t _CUI_MAX_PUSH_BUFFER_SIZE        = CuiMiB(1);
static const uint32_t _CUI_MAX_INDEX_BUFFER_COUNT      = 16 * 1024;
static const uint32_t _CUI_MAX_TEXTURE_OPERATION_COUNT = 32;

static bool
_cui_do_next_worker_thread_queue_entry(CuiWorkerThreadQueue *queue)
{
    bool should_sleep = true;

    uint32_t read_index = queue->read_index;
    uint32_t next_read_index = (read_index + 1) % CuiArrayCount(queue->entries);

    if (read_index != queue->write_index)
    {
        if (_cui_atomic_compare_and_swap(&queue->read_index, read_index, next_read_index))
        {
            CuiWorkerThreadQueueEntry entry = queue->entries[read_index];
            CuiWorkerThreadTaskGroup *task_group = entry.task_group;

            task_group->task_func(entry.data);

            _cui_atomic_increment(&task_group->completion_count);
        }

        should_sleep = false;
    }

    return should_sleep;
}

static CuiWorkerThreadTaskGroup
_cui_begin_worker_thread_task_group(void (*task_func)(void *))
{
    CuiWorkerThreadTaskGroup result = { 0 };

    result.task_func = task_func;

    return result;
}

static void
_cui_add_worker_thread_queue_entry(CuiWorkerThreadQueue *queue, CuiWorkerThreadTaskGroup *task_group, void *data)
{
    uint32_t write_index = queue->write_index;
    uint32_t next_write_index = (write_index + 1) % CuiArrayCount(queue->entries);

    CuiAssert(next_write_index != queue->read_index);

    CuiWorkerThreadQueueEntry *entry = queue->entries + write_index;
    entry->task_group = task_group;
    entry->data = data;

    _cui_atomic_increment(&task_group->completion_goal);
    queue->write_index = next_write_index;

#if CUI_PLATFORM_WINDOWS
    ReleaseSemaphore(queue->semaphore, 1, 0);
#else
    pthread_cond_signal(&queue->semaphore_cond);
#endif
}

static void
_cui_complete_worker_thread_task_group(CuiWorkerThreadQueue *queue, CuiWorkerThreadTaskGroup *task_group)
{
    while (task_group->completion_count != task_group->completion_goal)
    {
        _cui_do_next_worker_thread_queue_entry(queue);
    }
}

static CuiTextureOperation *
_cui_command_buffer_add_texture_operation(CuiCommandBuffer *command_buffer)
{
    CuiAssert(command_buffer->texture_operation_count < command_buffer->max_texture_operation_count);

    CuiTextureOperation *texture_op = command_buffer->texture_operations +
                                      command_buffer->texture_operation_count;
    command_buffer->texture_operation_count += 1;

    return texture_op;
}

static inline void *
_cui_command_buffer_push_primitive(CuiCommandBuffer *command_buffer, uint32_t size)
{
    CuiAssert((command_buffer->push_buffer_size + size) <= command_buffer->max_push_buffer_size);

    void *result = command_buffer->push_buffer + command_buffer->push_buffer_size;
    command_buffer->push_buffer_size += size;

    return result;
}

static inline uint32_t
_cui_command_buffer_push_clip_rect(CuiCommandBuffer *command_buffer, CuiRect rect)
{
    uint32_t offset = command_buffer->push_buffer_size + 1;

    CuiAssert((rect.min.x >= INT16_MIN) && (rect.min.x <= INT16_MAX));
    CuiAssert((rect.min.y >= INT16_MIN) && (rect.min.y <= INT16_MAX));
    CuiAssert((rect.max.x >= INT16_MIN) && (rect.max.x <= INT16_MAX));
    CuiAssert((rect.max.y >= INT16_MIN) && (rect.max.y <= INT16_MAX));

    CuiClipRect *clip_rect = (CuiClipRect *) _cui_command_buffer_push_primitive(command_buffer, sizeof(CuiClipRect));

    clip_rect->x_min = (int16_t) rect.min.x;
    clip_rect->y_min = (int16_t) rect.min.y;
    clip_rect->x_max = (int16_t) rect.max.x;
    clip_rect->y_max = (int16_t) rect.max.y;

    return offset;
}

static void
_cui_glyph_cache_reset(CuiGlyphCache *cache, CuiCommandBuffer *command_buffer)
{
    cache->count = 0;
    cache->insertion_failure_count = 0;

    for (uint32_t index = 0; index < cache->allocated; index++)
    {
        cache->hashes[index] = 0;
    }

    cache->x = 1;
    cache->y = 0;
    cache->y_max = 1;

    cui_bitmap_clear(&cache->texture, cui_make_color(0.0f, 0.0f, 0.0f, 0.0f));

    *(uint32_t *) cache->texture.pixels = 0xFFFFFFFF;

    // TODO: remove all texture updates from the command buffer

    CuiTextureOperation *texture_op = _cui_command_buffer_add_texture_operation(command_buffer);

    texture_op->type = CUI_TEXTURE_OPERATION_UPDATE;
    texture_op->texture_id = cache->texture_id;
    texture_op->payload.rect = cui_make_rect(0, 0, 1, 1);
}

static void
_cui_glyph_cache_maybe_reset(CuiGlyphCache *cache, CuiCommandBuffer *command_buffer)
{
    float hash_map_fill_rate = (float) cache->count / (float) cache->allocated;
    float texture_fill_rate = (float) cache->y / (float) cache->texture.height;

    if ((cache->insertion_failure_count > 0) ||
        (hash_map_fill_rate > 0.75f) || (texture_fill_rate > 0.8f))
    {
        _cui_glyph_cache_reset(cache, command_buffer);
    }
}

static void
_cui_glyph_cache_initialize(CuiGlyphCache *cache, CuiCommandBuffer *command_buffer, int32_t texture_id)
{
    cache->texture_id = texture_id;

    cache->allocated = 4096;

    cache->texture.width  = cui_min_int32(2048, command_buffer->max_texture_width);
    cache->texture.height = cui_min_int32(2048, command_buffer->max_texture_height);
    cache->texture.stride = CuiAlign(cache->texture.width * 4, 16);

    uint64_t hashes_size  = CuiAlign(cache->allocated * sizeof(uint32_t), 16);
    uint64_t keys_size    = CuiAlign(cache->allocated * sizeof(CuiGlyphKey), 16);
    uint64_t rects_size   = CuiAlign(cache->allocated * sizeof(CuiRect), 16);
    uint64_t texture_size = cache->texture.stride * cache->texture.height;

    cache->allocation_size = hashes_size + keys_size + rects_size + texture_size;

    uint8_t *allocation = (uint8_t *) cui_platform_allocate(cache->allocation_size);

    cache->hashes = (uint32_t *) allocation;
    allocation += hashes_size;

    cache->keys = (CuiGlyphKey *) allocation;
    allocation += keys_size;

    cache->rects = (CuiRect *) allocation;
    allocation += rects_size;

    cache->texture.pixels = allocation;

    CuiTextureOperation *texture_op = _cui_command_buffer_add_texture_operation(command_buffer);

    texture_op->type = CUI_TEXTURE_OPERATION_ALLOCATE;
    texture_op->texture_id = cache->texture_id;
    texture_op->payload.bitmap = cache->texture;

    _cui_glyph_cache_reset(cache, command_buffer);
}

static bool
_cui_glyph_cache_find(CuiGlyphCache *cache, uint32_t id, uint32_t codepoint, float scale, float offset_x, float offset_y, CuiRect *rect)
{
    CuiAssert((cache->allocated > 0) && !((cache->allocated - 1) & cache->allocated));

    uint32_t mask = cache->allocated - 1;

    uint32_t hash = codepoint + 1;
    uint32_t bucket = hash & mask;

    uint32_t probe_increment = 1 + (hash % mask);

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

        bucket = (bucket + probe_increment) & mask;
        probe_increment += 1;
    }

    return false;
}

static void
_cui_glyph_cache_put(CuiGlyphCache *cache, uint32_t id, uint32_t codepoint, float scale, float offset_x, float offset_y, CuiRect rect)
{
    CuiAssert((cache->allocated > 0) && !((cache->allocated - 1) & cache->allocated));

    uint32_t mask = cache->allocated - 1;

    uint32_t hash = codepoint + 1;
    uint32_t bucket = hash & mask;

    uint32_t probe_increment = 1 + (hash % mask);

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

        bucket = (bucket + probe_increment) & mask;
        probe_increment += 1;
    }
}

static CuiRect
_cui_glyph_cache_allocate_texture(CuiGlyphCache *cache, int32_t width, int32_t height, CuiCommandBuffer *command_buffer)
{
    CuiRect result = cui_make_rect(0, 0, 0, 0);

    if ((cache->x + width) > cache->texture.width)
    {
        cache->y = cache->y_max;
        cache->x = 0;
    }

    if ((cache->y + height) <= cache->texture.height)
    {
        result.min = cui_make_point(cache->x, cache->y);
        result.max = cui_make_point(cache->x + width, cache->y + height);

        cache->x += width;

        if ((cache->y + height) > cache->y_max)
        {
            cache->y_max = cache->y + height;
        }

        CuiTextureOperation *texture_op = 0;

        for (int32_t index = command_buffer->texture_operation_count - 1; index > 0; index -= 1)
        {
            CuiTextureOperation *op = command_buffer->texture_operations + index;

            if ((op->type == CUI_TEXTURE_OPERATION_UPDATE) && (op->texture_id == cache->texture_id))
            {
                texture_op = op;
                break;
            }
        }

        if (texture_op)
        {
            texture_op->payload.rect = cui_rect_get_union(texture_op->payload.rect, result);
        }
        else
        {
            texture_op = _cui_command_buffer_add_texture_operation(command_buffer);

            texture_op->type = CUI_TEXTURE_OPERATION_UPDATE;
            texture_op->texture_id = cache->texture_id;
            texture_op->payload.rect = result;
        }
    }
    else
    {
        cache->insertion_failure_count += 1;
    }

    return result;
}

static bool
_cui_do_next_background_thread_queue_entry(CuiBackgroundThreadQueue *queue)
{
    bool should_sleep = true;

    uint32_t read_index = queue->read_index;
    uint32_t next_read_index = (read_index + 1) % CuiArrayCount(queue->entries);

    if (read_index != queue->write_index)
    {
        if (_cui_atomic_compare_and_swap(&queue->read_index, read_index, next_read_index))
        {
            CuiBackgroundThreadQueueEntry entry = queue->entries[read_index];
            CuiBackgroundTask *task = entry.task;

            uint32_t current_state;

            do {
                current_state = *(volatile uint32_t *) &task->state;
            } while ((current_state == CUI_BACKGROUND_TASK_STATE_PENDING) &&
                   !_cui_atomic_compare_and_swap(&task->state, current_state, CUI_BACKGROUND_TASK_STATE_RUNNING));

            if (current_state == CUI_BACKGROUND_TASK_STATE_PENDING)
            {
                entry.task_func(task, entry.data);
            }

            do {
                current_state = *(volatile uint32_t *) &task->state;
            } while ((current_state != CUI_BACKGROUND_TASK_STATE_FINISHED) &&
                   !_cui_atomic_compare_and_swap(&task->state, current_state, CUI_BACKGROUND_TASK_STATE_FINISHED));

            cui_signal_main_thread();
        }

        should_sleep = false;
    }

    return should_sleep;
}

static bool
_cui_add_background_thread_queue_entry(CuiBackgroundThreadQueue *queue, CuiBackgroundTask *task, void (*task_func)(CuiBackgroundTask *, void *), void *data)
{
    CuiAssert(task);

    task->state = CUI_BACKGROUND_TASK_STATE_PENDING;

    uint32_t write_index = queue->write_index;
    uint32_t next_write_index = (write_index + 1) % CuiArrayCount(queue->entries);

    if (next_write_index != queue->read_index)
    {
        CuiBackgroundThreadQueueEntry *entry = queue->entries + write_index;
        entry->task = task;
        entry->task_func = task_func;
        entry->data = data;

        queue->write_index = next_write_index;

#if CUI_PLATFORM_WINDOWS
        ReleaseSemaphore(queue->semaphore, 1, 0);
#else
        pthread_cond_signal(&queue->semaphore_cond);
#endif

        return true;
    }
    else
    {
        return false;
    }
}

static CuiContext _cui_context;

static inline bool
_cui_common_init(int argument_count, char **arguments)
{
    (void) argument_count;
    (void) arguments;

    _cui_context.common.window_count = 0;
    _cui_context.common.main_loop_is_running = false;

    cui_arena_allocate(&_cui_context.common.arena, CuiMiB(4));
    cui_arena_allocate(&_cui_context.common.temporary_memory, CuiMiB(4));
    cui_arena_allocate(&_cui_context.common.command_line_arguments_arena, CuiKiB(32));
    cui_arena_allocate(&_cui_context.common.font_file_manager.arena, CuiMiB(8));

    _cui_context.common.scale_factor = cui_platform_get_environment_variable_int32(&_cui_context.common.temporary_memory, CuiStringLiteral("CUI_SCALE_FACTOR"));

    if ((_cui_context.common.scale_factor < 0) || (_cui_context.common.scale_factor > 4))
    {
        _cui_context.common.scale_factor = 0;
    }

    _cui_context.common.command_line_arguments = 0;
    _cui_context.common.font_file_manager.font_files = 0;
    _cui_context.common.font_file_manager.font_refs  = 0;

    cui_array_init(_cui_context.common.command_line_arguments, 8, &_cui_context.common.command_line_arguments_arena);
    cui_array_init(_cui_context.common.font_file_manager.font_files, 8, &_cui_context.common.font_file_manager.arena);
    cui_array_init(_cui_context.common.font_file_manager.font_refs, 32, &_cui_context.common.font_file_manager.arena);

    _cui_font_file_manager_scan_fonts(&_cui_context.common.temporary_memory, &_cui_context.common.font_file_manager);

    // TODO: reset _cui_context.common.temporary_memory or use cui_begin_temporary_memory

    return true;
}

static inline CuiWindow *
_cui_add_window(uint32_t creation_flags)
{
    CuiWindow *window = 0;

    if (_cui_context.common.window_count < CuiArrayCount(_cui_context.common.windows))
    {
        window = (CuiWindow *) cui_platform_allocate(sizeof(CuiWindow));

        CuiClearStruct(*window);

        _cui_context.common.windows[_cui_context.common.window_count] = window;
        _cui_context.common.window_count += 1;

        window->base.creation_flags = creation_flags;

        cui_arena_allocate(&window->base.arena, CuiKiB(16));
        cui_arena_allocate(&window->base.temporary_memory, CuiMiB(2));
        cui_arena_allocate(&window->base.font_manager.arena, CuiMiB(1));

        cui_array_init(window->base.pointer_captures, 4, &window->base.arena);
        cui_array_init(window->base.events, 16, &window->base.arena);

        window->base.font_manager.sized_fonts = 0;
        window->base.font_manager.font_file_manager = &_cui_context.common.font_file_manager;

        cui_array_init(window->base.font_manager.sized_fonts, 4, &window->base.font_manager.arena);
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

    cui_platform_deallocate(window->base.arena.base, window->base.arena.capacity);
    cui_platform_deallocate(window->base.temporary_memory.base, window->base.temporary_memory.capacity);
    cui_platform_deallocate(window->base.font_manager.arena.base, window->base.font_manager.arena.capacity);

    _cui_context.common.window_count -= 1;
    _cui_context.common.windows[window_index] = _cui_context.common.windows[_cui_context.common.window_count];

    if (_cui_context.common.window_count == 0)
    {
        _cui_context.common.main_loop_is_running = false;
    }
}

bool
cui_background_task_start(CuiBackgroundTask *task, void (*task_func)(CuiBackgroundTask *, void *), void *data, bool is_interactive)
{
    if (is_interactive)
    {
        return _cui_add_background_thread_queue_entry(&_cui_context.common.interactive_background_thread_queue, task, task_func, data);
    }
    else
    {
        return _cui_add_background_thread_queue_entry(&_cui_context.common.non_interactive_background_thread_queue, task, task_func, data);
    }
}

void cui_background_task_cancel(CuiBackgroundTask *task)
{
    uint32_t current_state;

    do {
        current_state = *(volatile uint32_t *) &task->state;
    } while ((current_state != CUI_BACKGROUND_TASK_STATE_FINISHED) &&
           !_cui_atomic_compare_and_swap(&task->state, current_state, CUI_BACKGROUND_TASK_STATE_CANCELING));
}

bool cui_background_task_has_finished(CuiBackgroundTask *task)
{
    return (_cui_atomic_read(&task->state) == CUI_BACKGROUND_TASK_STATE_FINISHED);
}

CuiString *
cui_get_command_line_arguments(void)
{
    return _cui_context.common.command_line_arguments;
}

CuiString
cui_get_executable_directory(void)
{
    return _cui_context.common.executable_directory;
}

CuiString
cui_get_bundle_directory(void)
{
    return _cui_context.common.bundle_directory;
}

void
cui_set_signal_callback(void (*signal_callback)(void))
{
    _cui_context.common.signal_callback = signal_callback;
}

int
cui_main_loop(void)
{
    _cui_context.common.main_loop_is_running = true;

    while (_cui_context.common.main_loop_is_running)
    {
        cui_step();
    }

    return 0;
}

#if CUI_RENDERER_SOFTWARE_ENABLED
#include "cui_renderer_software.c"
#endif

#if CUI_RENDERER_OPENGLES2_ENABLED
#include "cui_renderer_opengles2.c"
#endif

#if CUI_RENDERER_METAL_ENABLED
#include "cui_renderer_metal.c"
#endif

#if CUI_RENDERER_DIRECT3D11_ENABLED
#include "cui_renderer_direct3d11.c"
#endif

#include "cui_renderer.c"
#include "cui_widget.c"
#include "cui_window.c"
#include "cui_draw.c"

#endif

#include "cui_string.c"
#include "cui_arena.c"
#include "cui_unicode.c"
#include "cui_text_input.c"
#include "cui_string_builder.c"
#include "cui_color.c"
#include "cui_image.c"

#if CUI_PLATFORM_ANDROID
#include "cui_unix.c"
#include "cui_android.c"
#elif CUI_PLATFORM_WINDOWS
#include "cui_windows.c"
#elif CUI_PLATFORM_LINUX
#include "cui_unix.c"
#include "cui_linux.c"
#elif CUI_PLATFORM_MACOS
#include "cui_unix.c"
#include "cui_macos.c"
#else
#error unsupported platform
#endif
