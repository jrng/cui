static void
_cui_render_tile(CuiBitmap *bitmap, CuiCommandBuffer *command_buffer, CuiRect redraw_rect, CuiBitmap *texture, CuiRect tile_bounds)
{
    for (uint32_t i = 0; i < command_buffer->index_buffer_count; i += 1)
    {
        uint32_t index = command_buffer->index_buffer[i];

        if (index & 0x80000000)
        {
            CuiTexturedRect *textured_rect = (CuiTexturedRect *) (command_buffer->push_buffer + (index & 0x7FFFFFFF));

            CuiRect rect = textured_rect->rect;
            CuiColor color = textured_rect->color;
            CuiPoint uv = textured_rect->uv;
            CuiRect clip_rect = redraw_rect;

            if (textured_rect->clip_rect)
            {
                CuiRect *r = (CuiRect *) (command_buffer->push_buffer + (textured_rect->clip_rect - 1));
                clip_rect = cui_rect_get_intersection(tile_bounds, *r);
            }

            int32_t x_min = rect.min.x;
            int32_t y_min = rect.min.y;
            int32_t x_max = rect.max.x;
            int32_t y_max = rect.max.y;

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

            uint8_t *src_row = (uint8_t *) texture->pixels + (uv.y * texture->stride) + (uv.x * 4);
            uint8_t *dst_row = (uint8_t *) bitmap->pixels + (y_min * bitmap->stride) + (x_min * 4);

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

                src_row += texture->stride;
                dst_row += bitmap->stride;
            }
        }
        else
        {
            CuiSolidRect *solid_rect = (CuiSolidRect *) (command_buffer->push_buffer + index);

            CuiRect rect = solid_rect->rect;
            CuiColor color = solid_rect->color;
            CuiRect clip_rect = redraw_rect;

            if (solid_rect->clip_rect)
            {
                CuiRect *r = (CuiRect *) (command_buffer->push_buffer + (solid_rect->clip_rect - 1));
                clip_rect = cui_rect_get_intersection(tile_bounds, *r);
            }

            uint8_t r = (uint8_t) ((color.r * 255.0f) + 0.5f);
            uint8_t g = (uint8_t) ((color.g * 255.0f) + 0.5f);
            uint8_t b = (uint8_t) ((color.b * 255.0f) + 0.5f);
            uint8_t a = (uint8_t) ((color.a * 255.0f) + 0.5f);

            uint32_t final_color = (a << 24) | (r << 16) | (g << 8) | b;

            rect = cui_rect_get_intersection(rect, clip_rect);

            int32_t x_min = rect.min.x;
            int32_t y_min = rect.min.y;
            int32_t x_max = rect.max.x;
            int32_t y_max = rect.max.y;

            uint8_t *row = (uint8_t *) bitmap->pixels + (y_min * bitmap->stride) + (x_min * 4);

            for (int32_t y = y_min; y < y_max; y += 1)
            {
                uint32_t *pixel = (uint32_t *) row;

                for (int32_t x = x_min; x < x_max; x += 1)
                {
                    *pixel++ = final_color;
                }

                row += bitmap->stride;
            }
        }
    }
}

typedef struct CuiRenderWork
{
    CuiBitmap *bitmap;
    CuiBitmap *texture;
    CuiCommandBuffer *command_buffer;
    CuiRect redraw_rect;
    CuiRect tile_bounds;
} CuiRenderWork;

static void
_cui_do_render_work(void *data)
{
    CuiRenderWork *job = (CuiRenderWork *) data;
    _cui_render_tile(job->bitmap, job->command_buffer, job->redraw_rect, job->texture, job->tile_bounds);
}

static void
cui_render(CuiBitmap *bitmap, CuiCommandBuffer *command_buffer, CuiRect redraw_rect, CuiBitmap *texture)
{
    const uint32_t TILE_COUNT_X = 4;
    const uint32_t TILE_COUNT_Y = 4;

    CuiRenderWork render_jobs[32];

    CuiAssert((TILE_COUNT_X * TILE_COUNT_Y) <= CuiArrayCount(render_jobs));

    CuiAssert(redraw_rect.min.x >= 0);
    CuiAssert(redraw_rect.min.y >= 0);

    // The min x position is aligned down to a cache-line size of 64 bytes.
    // So for 4 bytes per pixel, this is 16 pixels aligned.
    int32_t draw_min_x = redraw_rect.min.x & 0xFFFFFFF0;
    int32_t draw_min_y = redraw_rect.min.y;

    int32_t draw_max_x = redraw_rect.max.x;
    int32_t draw_max_y = redraw_rect.max.y;

    int32_t draw_width  = draw_max_x - draw_min_x;
    int32_t draw_height = draw_max_y - draw_min_y;

    int32_t tile_width  = (draw_width  + (TILE_COUNT_X - 1)) / TILE_COUNT_X;
    int32_t tile_height = (draw_height + (TILE_COUNT_Y - 1)) / TILE_COUNT_Y;

    tile_width = CuiAlign(tile_width, 16); // align to cache-line size

    uint32_t job_index = 0;

    CuiCompletionState completion_state = { 0 };

    for (uint32_t tile_y = 0; tile_y < TILE_COUNT_Y; tile_y += 1)
    {
        for (uint32_t tile_x = 0; tile_x < TILE_COUNT_X; tile_x += 1)
        {
            CuiRect tile_bounds;
            tile_bounds.min.x = draw_min_x + (tile_x * tile_width);
            tile_bounds.min.y = draw_min_y + (tile_y * tile_height);
            tile_bounds.max.x = tile_bounds.min.x + tile_width;
            tile_bounds.max.y = tile_bounds.min.y + tile_height;

            CuiRenderWork *job = render_jobs + job_index++;

            job->bitmap = bitmap;
            job->texture = texture;
            job->command_buffer = command_buffer;
            job->redraw_rect = redraw_rect;
            job->tile_bounds = tile_bounds;

            _cui_work_queue_add(&_cui_context.common.work_queue, &completion_state, _cui_do_render_work, job);
        }
    }

    _cui_work_queue_complete_all_work(&_cui_context.common.work_queue, &completion_state);
}
