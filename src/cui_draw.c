static inline void *
cui_command_buffer_push_primitive(CuiCommandBuffer *command_buffer, uint32_t size)
{
    CuiAssert((command_buffer->push_buffer_size + size) <= command_buffer->max_push_buffer_size);

    void *result = command_buffer->push_buffer + command_buffer->push_buffer_size;
    command_buffer->push_buffer_size += size;

    return result;
}

static inline uint32_t
cui_command_buffer_push_clip_rect(CuiCommandBuffer *command_buffer, CuiRect rect)
{
    uint32_t offset = command_buffer->push_buffer_size + 1;

    CuiRect *clip_rect = (CuiRect *) cui_command_buffer_push_primitive(command_buffer, sizeof(CuiRect));
    *clip_rect = rect;

    return offset;
}

CuiRect
cui_draw_set_clip_rect(CuiGraphicsContext *ctx, CuiRect clip_rect)
{
    CuiRect prev_rect = ctx->clip_rect;
    ctx->clip_rect = cui_rect_get_intersection(clip_rect, ctx->redraw_rect);
    ctx->clip_rect_offset = cui_command_buffer_push_clip_rect(ctx->command_buffer, ctx->clip_rect);
    return prev_rect;
}

void
cui_draw_fill_rect(CuiGraphicsContext *ctx, CuiRect rect, CuiColor color)
{
    CuiCommandBuffer *command_buffer = ctx->command_buffer;

    CuiAssert(command_buffer->index_buffer_count < command_buffer->max_index_buffer_count);

    uint32_t offset = command_buffer->push_buffer_size;

    color.r *= color.a;
    color.g *= color.a;
    color.b *= color.a;

    CuiSolidRect *solid_rect = (CuiSolidRect *) cui_command_buffer_push_primitive(command_buffer, sizeof(CuiSolidRect));

    solid_rect->rect = rect;
    solid_rect->color = color;
    solid_rect->clip_rect = ctx->clip_rect_offset;

    command_buffer->index_buffer[command_buffer->index_buffer_count++] = offset;
}

void
cui_draw_fill_textured_rect(CuiGraphicsContext *ctx, CuiRect rect, CuiPoint uv, CuiColor color)
{
    CuiCommandBuffer *command_buffer = ctx->command_buffer;

    CuiAssert(command_buffer->index_buffer_count < command_buffer->max_index_buffer_count);

    uint32_t offset = command_buffer->push_buffer_size;

    color.r *= color.a;
    color.g *= color.a;
    color.b *= color.a;

    CuiTexturedRect *textured_rect = (CuiTexturedRect *) cui_command_buffer_push_primitive(command_buffer, sizeof(CuiTexturedRect));

    textured_rect->rect = rect;
    textured_rect->uv = uv;
    textured_rect->color = color;
    textured_rect->clip_rect = ctx->clip_rect_offset;

    command_buffer->index_buffer[command_buffer->index_buffer_count++] = 0x80000000 | offset;
}

#include "cui_shapes.c"

void
cui_draw_fill_string(CuiArena *temporary_memory, CuiGraphicsContext *ctx, CuiFont *font,
                     float x, float y, CuiString str, CuiColor color)
{
    int64_t index = 0;
    uint32_t glyph_index, prev_glyph_index;

    while (index < str.count)
    {
        // TODO: grapheme cluster
        CuiUnicodeResult utf8 = cui_utf8_decode(str, index);
        glyph_index = cui_font_file_get_glyph_index_from_codepoint(font->file, utf8.codepoint);

        if (index > 0)
        {
            // x += font->font_scale * cui_font_file_get_glyph_kerning(font->file, prev_glyph_index, glyph_index);
        }

        CuiRect bounding_box = cui_font_file_get_glyph_bounding_box(font->file, glyph_index);

        if ((bounding_box.min.x != bounding_box.max.x) && (bounding_box.min.y != bounding_box.max.y))
        {
            CuiFloatRect bound;
            bound.min.x = (float) bounding_box.min.x * font->font_scale;
            bound.min.y = (float) bounding_box.min.y * font->font_scale;
            bound.max.x = (float) bounding_box.max.x * font->font_scale;
            bound.max.y = (float) bounding_box.max.y * font->font_scale;

            float draw_x = x + bound.min.x;
            float draw_y = y - bound.min.y;

            float bitmap_x = floorf(draw_x);
            float bitmap_y =  ceilf(draw_y);

            float offset_x = draw_x - bitmap_x;
            float offset_y = bitmap_y - draw_y;

            offset_x = 0.125f * roundf(offset_x * 8.0f);
            offset_y = 0.125f * roundf(offset_y * 8.0f);

            CuiRect uv;

            uint32_t font_id = 1;

            if (!cui_glyph_cache_find(ctx->cache, font_id, utf8.codepoint, font->font_scale, offset_x, offset_y, &uv))
            {
                CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(temporary_memory);

                int32_t width  = (int32_t) ceilf(x + bound.max.x) - (int32_t) bitmap_x;
                int32_t height = (int32_t) bitmap_y - (int32_t) floorf(y - bound.max.y);

                uv = cui_glyph_cache_allocate_texture(ctx->cache, width, height);

                CuiBitmap bitmap;
                bitmap.width = uv.max.x - uv.min.x;
                bitmap.height = uv.max.y - uv.min.y;
                bitmap.stride = ctx->cache->texture.stride;
                bitmap.pixels = (uint8_t *) ctx->cache->texture.pixels + (uv.min.y * bitmap.stride) + (uv.min.x * 4);

                CuiPathCommand *path = 0;
                cui_array_init(path, 16, temporary_memory);

                CuiTransform transform = { 0 };
                transform.m[0] = font->font_scale;
                transform.m[3] = font->font_scale;
                transform.m[4] = (offset_x - bound.min.x);
                transform.m[5] = (offset_y - bound.min.y);

                cui_font_file_get_glyph_outline(font->file, &path, glyph_index, transform, temporary_memory);

                CuiEdge *edge_list = 0;
                cui_array_init(edge_list, 16, temporary_memory);

                cui_path_to_edge_list(path, &edge_list);
                cui_edge_list_fill(&bitmap, edge_list, cui_make_color(1.0f, 1.0f, 1.0f, 1.0f));

                cui_glyph_cache_put(ctx->cache, font_id, utf8.codepoint, font->font_scale, offset_x, offset_y, uv);

                cui_end_temporary_memory(temp_memory);
            }

            CuiRect rect;
            rect.min.x = (int32_t) bitmap_x;
            rect.max.y = (int32_t) bitmap_y;
            rect.max.x = rect.min.x + (uv.max.x - uv.min.x);
            rect.min.y = rect.max.y - (uv.max.y - uv.min.y);

            // TODO: if overlap

            cui_draw_fill_textured_rect(ctx, rect, uv.min, color);
        }

        x += font->font_scale * cui_font_file_get_glyph_advance(font->file, glyph_index);

        prev_glyph_index = glyph_index;
        index += utf8.byte_count;
    }
}
