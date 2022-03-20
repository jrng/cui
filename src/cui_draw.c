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

#if 0
void
cui_draw_fill_rect(CuiGraphicsContext *ctx, CuiRect rect, CuiColor color)
{
    uint8_t r = (uint8_t) ((color.r * 255.0f) + 0.5f);
    uint8_t g = (uint8_t) ((color.g * 255.0f) + 0.5f);
    uint8_t b = (uint8_t) ((color.b * 255.0f) + 0.5f);
    uint8_t a = (uint8_t) ((color.a * 255.0f) + 0.5f);

    uint32_t final_color = (a << 24) | (r << 16) | (g << 8) | b;

    CuiBitmap bitmap = ctx->command_buffer->canvas;

    rect = cui_rect_get_intersection(rect, ctx->clip_rect);

    int32_t x_min = rect.min.x;
    int32_t y_min = rect.min.y;
    int32_t x_max = rect.max.x;
    int32_t y_max = rect.max.y;

    uint8_t *row = (uint8_t *) bitmap.pixels + (y_min * bitmap.stride) + (x_min * 4);

    for (int32_t y = y_min; y < y_max; y += 1)
    {
        uint32_t *pixel = (uint32_t *) row;

        for (int32_t x = x_min; x < x_max; x += 1)
        {
            *pixel++ = final_color;
        }

        row += bitmap.stride;
    }
}

void
cui_draw_fill_textured_rect(CuiGraphicsContext *ctx, CuiRect rect, CuiPoint uv, CuiColor color)
{
    CuiBitmap bitmap = ctx->command_buffer->canvas;
    CuiBitmap texture = ctx->cache->texture;

    int32_t x_min = rect.min.x;
    int32_t y_min = rect.min.y;
    int32_t x_max = rect.max.x;
    int32_t y_max = rect.max.y;

    CuiRect clip_rect = ctx->clip_rect;

    if (x_min < clip_rect.min.x)
    {
        uv.x += (clip_rect.min.x - x_min);
        x_min = clip_rect.min.x;
    }

    if (y_min < clip_rect.min.y)
    {
        uv.y += (clip_rect.min.y - y_min);
        y_min = clip_rect.min.y;
    }

    if (x_max > clip_rect.max.x)
    {
        x_max = clip_rect.max.x;
    }

    if (y_max > clip_rect.max.y)
    {
        y_max = clip_rect.max.y;
    }

    uint8_t *src_row = (uint8_t *) texture.pixels + (uv.y * texture.stride) + (uv.x * 4);
    uint8_t *dst_row = (uint8_t *) bitmap.pixels + (y_min * bitmap.stride) + (x_min * 4);

    for (int32_t y = y_min; y < y_max; y += 1)
    {
        uint32_t *src_pixel = (uint32_t *) src_row;
        uint32_t *dst_pixel = (uint32_t *) dst_row;

        for (int32_t x = x_min; x < x_max; x += 1)
        {
            float texture_a = (float) ((*src_pixel >> 24) & 0xFF) / 255.0f;
            float texture_r = (float) ((*src_pixel >> 16) & 0xFF) / 255.0f;
            float texture_g = (float) ((*src_pixel >>  8) & 0xFF) / 255.0f;
            float texture_b = (float) ((*src_pixel >>  0) & 0xFF) / 255.0f;

            src_pixel += 1;

            float src_a = texture_a * color.a;
            float src_r = texture_r * color.r;
            float src_g = texture_g * color.g;
            float src_b = texture_b * color.b;

            float dst_a = (float) ((*dst_pixel >> 24) & 0xFF) / 255.0f;
            float dst_r = (float) ((*dst_pixel >> 16) & 0xFF) / 255.0f;
            float dst_g = (float) ((*dst_pixel >>  8) & 0xFF) / 255.0f;
            float dst_b = (float) ((*dst_pixel >>  0) & 0xFF) / 255.0f;

            float final_a = src_a + dst_a * (1.0f - src_a);
            float final_r = src_r + dst_r * (1.0f - src_a);
            float final_g = src_g + dst_g * (1.0f - src_a);
            float final_b = src_b + dst_b * (1.0f - src_a);

            uint8_t a = (uint8_t) ((final_a * 255.0f) + 0.5f);
            uint8_t r = (uint8_t) ((final_r * 255.0f) + 0.5f);
            uint8_t g = (uint8_t) ((final_g * 255.0f) + 0.5f);
            uint8_t b = (uint8_t) ((final_b * 255.0f) + 0.5f);

            *dst_pixel++ = (a << 24) | (r << 16) | (g << 8) | b;
        }

        src_row += texture.stride;
        dst_row += bitmap.stride;
    }
}
#endif

void
cui_draw_fill_shape(CuiArena *temporary_memory, CuiGraphicsContext *ctx, float x, float y,
                    CuiShapeType shape_type, float scale, CuiColor color)
{
    float draw_x = x;
    float draw_y = y;

    float bitmap_x = floorf(draw_x);
    float bitmap_y = floorf(draw_y);

    float offset_x = draw_x - bitmap_x;
    float offset_y = bitmap_y - draw_y;

    offset_x = 0.125f * roundf(offset_x * 8.0f);
    offset_y = 0.125f * roundf(offset_y * 8.0f);

    CuiRect uv;

    if (!cui_glyph_cache_find(ctx->cache, 0, shape_type, scale, offset_x, offset_y, &uv))
    {
        CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(temporary_memory);

        switch (shape_type)
        {
            case CUI_SHAPE_LOAD:
            {
                int32_t width  = (int32_t) ceilf(scale * 24.0f);
                int32_t height = (int32_t) ceilf(scale * 24.0f);

                uv = cui_glyph_cache_allocate_texture(ctx->cache, width, height);

                CuiBitmap bitmap;
                bitmap.width = uv.max.x - uv.min.x;
                bitmap.height = uv.max.y - uv.min.y;
                bitmap.stride = ctx->cache->texture.stride;
                bitmap.pixels = (uint8_t *) ctx->cache->texture.pixels + (uv.min.y * bitmap.stride) + (uv.min.x * 4);

                CuiPathCommand *path = 0;
                cui_array_init(path, 16, temporary_memory);

                cui_path_move_to(&path, scale * 11.9552021f, scale * 18.9997329f);
                cui_path_cubic_curve_to(&path, scale * 11.8378963f, scale * 18.9896183f, scale * 11.7279233f, scale * 18.9384365f, scale * 11.6446485f, scale * 18.8551979f);
                cui_path_line_to(&path, scale * 8.6485157f, scale * 15.8492975f);
                cui_path_cubic_curve_to(&path, scale * 8.4533014f, scale * 15.6540422f, scale * 8.4533014f, scale * 15.3375110f, scale * 8.6485157f, scale * 15.1422567f);
                cui_path_cubic_curve_to(&path, scale * 8.8437719f, scale * 14.9470405f, scale * 9.1603021f, scale * 14.9470405f, scale * 9.3555564f, scale * 15.1422567f);
                cui_path_line_to(&path, scale * 11.4962110f, scale * 17.2868175f);
                cui_path_line_to(&path, scale * 11.4962110f, scale * 10.4941415f);
                cui_path_line_to(&path, scale * 11.4962110f, scale * 9.9999942f);
                cui_path_line_to(&path, scale * 12.5020847f, scale * 9.9999942f);
                cui_path_line_to(&path, scale * 12.5020847f, scale * 10.4941415f);
                cui_path_line_to(&path, scale * 12.5020847f, scale * 17.2868175f);
                cui_path_line_to(&path, scale * 14.6486005f, scale * 15.1422567f);
                cui_path_cubic_curve_to(&path, scale * 14.8431673f, scale * 14.9509143f, scale * 15.1552143f, scale * 14.9509143f, scale * 15.3497810f, scale * 15.1422567f);
                cui_path_cubic_curve_to(&path, scale * 15.5449953f, scale * 15.3375110f, scale * 15.5449953f, scale * 15.6540422f, scale * 15.3497810f, scale * 15.8492975f);
                cui_path_line_to(&path, scale * 12.3536462f, scale * 18.8551959f);
                cui_path_cubic_curve_to(&path, scale * 12.2488269f, scale * 18.9600639f, scale * 12.1028757f, scale * 19.0130081f, scale * 11.9552021f, scale * 18.9997329f);
                cui_path_close(&path);
                cui_path_move_to(&path, scale * 4.0000138f, scale * 11.9980697f);
                cui_path_cubic_curve_to(&path, scale * 3.4460062f, scale * 11.9980697f, scale * 3.0f, scale * 11.5520629f, scale * 3.0f, scale * 10.9980545f);
                cui_path_line_to(&path, scale * 3.0f, scale * 7.9980139f);
                cui_path_cubic_curve_to(&path, scale * 3.0f, scale * 7.4440069f, scale * 3.4460062f, scale * 6.9980001f, scale * 4.0000138f, scale * 6.9980001f);
                cui_path_line_to(&path, scale * 20.0002365f, scale * 6.9980001f);
                cui_path_cubic_curve_to(&path, scale * 20.5542430f, scale * 6.9980001f, scale * 21.0002498f, scale * 7.4440069f, scale * 21.0002498f, scale * 7.9980139f);
                cui_path_line_to(&path, scale * 21.0002498f, scale * 10.9980545f);
                cui_path_cubic_curve_to(&path, scale * 21.0002498f, scale * 11.5520629f, scale * 20.5542430f, scale * 11.9980697f, scale * 20.0002365f, scale * 11.9980697f);
                cui_path_line_to(&path, scale * 14.0001525f, scale * 11.9980697f);
                cui_path_line_to(&path, scale * 14.0001525f, scale * 10.9980545f);
                cui_path_line_to(&path, scale * 20.0002365f, scale * 10.9980545f);
                cui_path_line_to(&path, scale * 20.0002365f, scale * 7.9980139f);
                cui_path_line_to(&path, scale * 4.0000138f, scale * 7.9980139f);
                cui_path_line_to(&path, scale * 4.0000138f, scale * 10.9980545f);
                cui_path_line_to(&path, scale * 10.0000963f, scale * 10.9980545f);
                cui_path_line_to(&path, scale * 10.0000963f, scale * 11.9980697f);
                cui_path_close(&path);
                cui_path_move_to(&path, scale * 16.5001869f, scale * 9.9980421f);
                cui_path_cubic_curve_to(&path, scale * 16.2240428f, scale * 9.9980411f, scale * 16.0001811f, scale * 9.7741794f, scale * 16.0001811f, scale * 9.4980344f);
                cui_path_cubic_curve_to(&path, scale * 16.0001811f, scale * 9.2218894f, scale * 16.2240428f, scale * 8.9980278f, scale * 16.5001869f, scale * 8.9980278f);
                cui_path_cubic_curve_to(&path, scale * 16.7763347f, scale * 8.9980278f, scale * 17.0001926f, scale * 9.2218885f, scale * 17.0001926f, scale * 9.4980344f);
                cui_path_cubic_curve_to(&path, scale * 17.0001926f, scale * 9.7741804f, scale * 16.7763347f, scale * 9.9980421f, scale * 16.5001869f, scale * 9.9980421f);
                cui_path_close(&path);
                cui_path_move_to(&path, scale * 18.5002155f, scale * 9.9980421f);
                cui_path_cubic_curve_to(&path, scale * 18.2240695f, scale * 9.9980421f, scale * 18.0002079f, scale * 9.7741804f, scale * 18.0002079f, scale * 9.4980344f);
                cui_path_cubic_curve_to(&path, scale * 18.0002079f, scale * 9.2218885f, scale * 18.2240695f, scale * 8.9980278f, scale * 18.5002155f, scale * 8.9980278f);
                cui_path_cubic_curve_to(&path, scale * 18.7763614f, scale * 8.9980278f, scale * 19.0002212f, scale * 9.2218885f, scale * 19.0002212f, scale * 9.4980344f);
                cui_path_cubic_curve_to(&path, scale * 19.0002212f, scale * 9.7741804f, scale * 18.7763614f, scale * 9.9980421f, scale * 18.5002155f, scale * 9.9980421f);
                cui_path_close(&path);

                CuiEdge *edge_list = 0;
                cui_array_init(edge_list, 16, temporary_memory);

                cui_path_to_edge_list(path, &edge_list);
                cui_edge_list_fill(&bitmap, edge_list, cui_make_color(1.0f, 1.0f, 1.0f, 1.0f));
            } break;

            case CUI_SHAPE_TAPE:
            {
                int32_t width  = (int32_t) ceilf(scale * 24.0f);
                int32_t height = (int32_t) ceilf(scale * 24.0f);

                uv = cui_glyph_cache_allocate_texture(ctx->cache, width, height);

                CuiBitmap bitmap;
                bitmap.width = uv.max.x - uv.min.x;
                bitmap.height = uv.max.y - uv.min.y;
                bitmap.stride = ctx->cache->texture.stride;
                bitmap.pixels = (uint8_t *) ctx->cache->texture.pixels + (uv.min.y * bitmap.stride) + (uv.min.x * 4);

                CuiPathCommand *path = 0;
                cui_array_init(path, 16, temporary_memory);

                cui_path_move_to(&path, scale * 6.0f, scale * 8.0f);
                cui_path_cubic_curve_to(&path, scale * 3.792f, scale * 8.0f, scale * 2.0f, scale * 9.7919998f, scale * 2.0f, scale * 12.0f);
                cui_path_cubic_curve_to(&path, scale * 2.0f, scale * 14.2080001f, scale * 3.792f, scale * 16.0f, scale * 6.0f, scale * 16.0f);
                cui_path_cubic_curve_to(&path, scale * 8.2080001f, scale * 16.0f, scale * 10.0f, scale * 14.2080001f, scale * 10.0f, scale * 12.0f);
                cui_path_cubic_curve_to(&path, scale * 10.0f, scale * 10.8056697f, scale * 9.4753837f, scale * 9.7331314f, scale * 8.6445302f, scale * 9.0f);
                cui_path_line_to(&path, scale * 15.3554687f, scale * 9.0f);
                cui_path_cubic_curve_to(&path, scale * 14.5246152f, scale * 9.7331314f, scale * 14.0f, scale * 10.8056697f, scale * 14.0f, scale * 12.0f);
                cui_path_cubic_curve_to(&path, scale * 14.0f, scale * 14.2080001f, scale * 15.7919998f, scale * 16.0f, scale * 18.0f, scale * 16.0f);
                cui_path_cubic_curve_to(&path, scale * 20.2080001f, scale * 16.0f, scale * 22.0f, scale * 14.2080001f, scale * 22.0f, scale * 12.0f);
                cui_path_cubic_curve_to(&path, scale * 22.0f, scale * 9.7919998f, scale * 20.2080001f, scale * 8.0f, scale * 18.0f, scale * 8.0f);
                cui_path_line_to(&path, scale * 6.0f, scale * 8.0f);
                cui_path_close(&path);
                cui_path_move_to(&path, scale * 6.0f, scale * 9.0f);
                cui_path_cubic_curve_to(&path, scale * 7.6560001f, scale * 9.0f, scale * 9.0f, scale * 10.3439998f, scale * 9.0f, scale * 12.0f);
                cui_path_cubic_curve_to(&path, scale * 9.0f, scale * 13.6560001f, scale * 7.6560001f, scale * 15.0f, scale * 6.0f, scale * 15.0f);
                cui_path_cubic_curve_to(&path, scale * 4.3440003f, scale * 15.0f, scale * 3.0f, scale * 13.6560001f, scale * 3.0f, scale * 12.0f);
                cui_path_cubic_curve_to(&path, scale * 3.0f, scale * 10.3439998f, scale * 4.3440003f, scale * 9.0f, scale * 6.0f, scale * 9.0f);
                cui_path_close(&path);
                cui_path_move_to(&path, scale * 18.0f, scale * 9.0f);
                cui_path_cubic_curve_to(&path, scale * 19.6560001f, scale * 9.0f, scale * 21.0f, scale * 10.3439998f, scale * 21.0f, scale * 12.0f);
                cui_path_cubic_curve_to(&path, scale * 21.0f, scale * 13.6560001f, scale * 19.6560001f, scale * 15.0f, scale * 18.0f, scale * 15.0f);
                cui_path_cubic_curve_to(&path, scale * 16.3439998f, scale * 15.0f, scale * 15.0f, scale * 13.6560001f, scale * 15.0f, scale * 12.0f);
                cui_path_cubic_curve_to(&path, scale * 15.0f, scale * 10.3439998f, scale * 16.3439998f, scale * 9.0f, scale * 18.0f, scale * 9.0f);
                cui_path_close(&path);

                CuiEdge *edge_list = 0;
                cui_array_init(edge_list, 16, temporary_memory);

                cui_path_to_edge_list(path, &edge_list);
                cui_edge_list_fill(&bitmap, edge_list, cui_make_color(1.0f, 1.0f, 1.0f, 1.0f));
            } break;

            case CUI_SHAPE_CHECKBOX_OUTER:
            {
                int32_t width  = (int32_t) ceilf(scale * 16.0f);
                int32_t height = (int32_t) ceilf(scale * 16.0f);

                uv = cui_glyph_cache_allocate_texture(ctx->cache, width, height);

                CuiBitmap bitmap;
                bitmap.width = uv.max.x - uv.min.x;
                bitmap.height = uv.max.y - uv.min.y;
                bitmap.stride = ctx->cache->texture.stride;
                bitmap.pixels = (uint8_t *) ctx->cache->texture.pixels + (uv.min.y * bitmap.stride) + (uv.min.x * 4);

                CuiPathCommand *path = 0;
                cui_array_init(path, 16, temporary_memory);

                cui_path_move_to(&path, scale * 4.0f, scale * 2.0f);
                cui_path_line_to(&path, scale * 12.0f, scale * 2.0f);
                cui_path_cubic_curve_to(&path, scale * 13.1080007f, scale * 2.0f, scale * 14.0f, scale * 2.8919999f, scale * 14.0f, scale * 4.0f);
                cui_path_line_to(&path, scale * 14.0f, scale * 12.0f);
                cui_path_cubic_curve_to(&path, scale * 14.0f, scale * 13.1080007f, scale * 13.1080007f, scale * 14.0f, scale * 12.0f, scale * 14.0f);
                cui_path_line_to(&path, scale * 4.0f, scale * 14.0f);
                cui_path_cubic_curve_to(&path, scale * 2.8919999f, scale * 14.0f, scale * 2.0f, scale * 13.1080007f, scale * 2.0f, scale * 12.0f);
                cui_path_line_to(&path, scale * 2.0f, scale * 4.0f);
                cui_path_cubic_curve_to(&path, scale * 2.0f, scale * 2.8919999f, scale * 2.8919999f, scale * 2.0f, scale * 4.0f, scale * 2.0f);
                cui_path_close(&path);

                CuiEdge *edge_list = 0;
                cui_array_init(edge_list, 16, temporary_memory);

                cui_path_to_edge_list(path, &edge_list);
                cui_edge_list_fill(&bitmap, edge_list, cui_make_color(1.0f, 1.0f, 1.0f, 1.0f));
            } break;

            case CUI_SHAPE_CHECKBOX_INNER:
            {
                int32_t width  = (int32_t) ceilf(scale * 16.0f);
                int32_t height = (int32_t) ceilf(scale * 16.0f);

                uv = cui_glyph_cache_allocate_texture(ctx->cache, width, height);

                CuiBitmap bitmap;
                bitmap.width = uv.max.x - uv.min.x;
                bitmap.height = uv.max.y - uv.min.y;
                bitmap.stride = ctx->cache->texture.stride;
                bitmap.pixels = (uint8_t *) ctx->cache->texture.pixels + (uv.min.y * bitmap.stride) + (uv.min.x * 4);

                CuiPathCommand *path = 0;
                cui_array_init(path, 16, temporary_memory);

                cui_path_move_to(&path, scale * 4.0f, scale * 3.0f);
                cui_path_line_to(&path, scale * 12.0f, scale * 3.0f);
                cui_path_cubic_curve_to(&path, scale * 12.5539999f, scale * 3.0f, scale * 13.0f, scale * 3.4460000f, scale * 13.0f, scale * 4.0f);
                cui_path_line_to(&path, scale * 13.0f, scale * 12.0f);
                cui_path_cubic_curve_to(&path, scale * 13.0f, scale * 12.5539999f, scale * 12.5539999f, scale * 13.0f, scale * 12.0f, scale * 13.0f);
                cui_path_line_to(&path, scale * 4.0f, scale * 13.0f);
                cui_path_cubic_curve_to(&path, scale * 3.4460000f, scale * 13.0f, scale * 3.0f, scale * 12.5539999f, scale * 3.0f, scale * 12.0f);
                cui_path_line_to(&path, scale * 3.0f, scale * 4.0f);
                cui_path_cubic_curve_to(&path, scale * 3.0f, scale * 3.4460000f, scale * 3.4460000f, scale * 3.0f, scale * 4.0f, scale * 3.0f);
                cui_path_close(&path);

                CuiEdge *edge_list = 0;
                cui_array_init(edge_list, 16, temporary_memory);

                cui_path_to_edge_list(path, &edge_list);
                cui_edge_list_fill(&bitmap, edge_list, cui_make_color(1.0f, 1.0f, 1.0f, 1.0f));
            } break;

            case CUI_SHAPE_CHECKMARK:
            {
                int32_t width  = (int32_t) ceilf(scale * 16.0f);
                int32_t height = (int32_t) ceilf(scale * 16.0f);

                uv = cui_glyph_cache_allocate_texture(ctx->cache, width, height);

                CuiBitmap bitmap;
                bitmap.width = uv.max.x - uv.min.x;
                bitmap.height = uv.max.y - uv.min.y;
                bitmap.stride = ctx->cache->texture.stride;
                bitmap.pixels = (uint8_t *) ctx->cache->texture.pixels + (uv.min.y * bitmap.stride) + (uv.min.x * 4);

                CuiPathCommand *path = 0;
                cui_array_init(path, 16, temporary_memory);

                cui_path_move_to(&path, scale * 6.6464843f, scale * 5.6464843f);
                cui_path_move_to(&path, scale * 4.6464843f, scale * 7.6464843f);
                cui_path_arc_to(&path, scale * 0.5f, scale * 0.5f, 0.0f, false, false, scale * 4.6464843f, scale * 8.3535156f);
                cui_path_arc_to(&path, scale * 0.5f, scale * 0.5f, 0.0f, false, false, scale * 5.3535156f, scale * 8.3535156f);
                cui_path_line_to(&path, scale * 7.0f, scale * 6.7070307f);
                cui_path_line_to(&path, scale * 10.6464834f, scale * 10.3535156f);
                cui_path_arc_to(&path, scale * 0.5f, scale * 0.5f, 0.0f, false, false, scale * 11.3535156f, scale * 10.3535156f);
                cui_path_arc_to(&path, scale * 0.5f, scale * 0.5f, 0.0f, false, false, scale * 11.3535156f, scale * 9.6464834f);
                cui_path_line_to(&path, scale * 7.3535156f, scale * 5.6464843f);
                cui_path_arc_to(&path, scale * 0.5000500f, scale * 0.5000500f, 0.0f, false, false, scale * 6.6464843f, scale * 5.6464843f);
                cui_path_close(&path);

                CuiEdge *edge_list = 0;
                cui_array_init(edge_list, 16, temporary_memory);

                cui_path_to_edge_list(path, &edge_list);
                cui_edge_list_fill(&bitmap, edge_list, cui_make_color(1.0f, 1.0f, 1.0f, 1.0f));
            } break;
        }

        cui_glyph_cache_put(ctx->cache, 0, shape_type, scale, offset_x, offset_y, uv);

        cui_end_temporary_memory(temp_memory);
    }

    CuiRect rect;
    rect.min.x = (int32_t) bitmap_x;
    rect.min.y = (int32_t) bitmap_y;
    rect.max.x = rect.min.x + (uv.max.x - uv.min.x);
    rect.max.y = rect.min.y + (uv.max.y - uv.min.y);

    // TODO: if overlap

    cui_draw_fill_textured_rect(ctx, rect, uv.min, color);
}

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
