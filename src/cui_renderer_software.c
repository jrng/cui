#define CUI_SOFTWARE_RENDERER_TILE_COUNT_X 8
#define CUI_SOFTWARE_RENDERER_TILE_COUNT_Y 4

static CuiRenderer *
_cui_renderer_software_create(void)
{
    uint64_t renderer_size          = CuiAlign(sizeof(CuiRendererSoftware), 16);
    uint64_t texture_operation_size = CuiAlign(_CUI_MAX_TEXTURE_OPERATION_COUNT * sizeof(CuiTextureOperation), 16);
    uint64_t index_buffer_size      = CuiAlign(_CUI_MAX_INDEX_BUFFER_COUNT * sizeof(uint32_t), 16);
    uint64_t push_buffer_size       = CuiAlign(_CUI_MAX_PUSH_BUFFER_SIZE, 16);

    uint64_t allocation_size = renderer_size + texture_operation_size + index_buffer_size + push_buffer_size;

    uint8_t *allocation = (uint8_t *) cui_platform_allocate(allocation_size);

    CuiRendererSoftware *renderer = (CuiRendererSoftware *) allocation;
    allocation += renderer_size;

    CuiClearStruct(*renderer);

    renderer->base.type = CUI_RENDERER_TYPE_SOFTWARE;
    renderer->allocation_size = allocation_size;

    CuiCommandBuffer *command_buffer = &renderer->command_buffer;

    command_buffer->max_texture_width  = 32768;
    command_buffer->max_texture_height = 32768;

    command_buffer->max_texture_operation_count = _CUI_MAX_TEXTURE_OPERATION_COUNT;
    command_buffer->texture_operations = (CuiTextureOperation *) allocation;
    allocation += texture_operation_size;

    command_buffer->max_index_buffer_count = _CUI_MAX_INDEX_BUFFER_COUNT;
    command_buffer->index_buffer = (uint32_t *) allocation;
    allocation += index_buffer_size;

    command_buffer->max_push_buffer_size = _CUI_MAX_PUSH_BUFFER_SIZE;
    command_buffer->push_buffer = (uint8_t *) allocation;

    return &renderer->base;
}

static void
_cui_renderer_software_destroy(CuiRendererSoftware *renderer)
{
    cui_platform_deallocate(renderer, renderer->allocation_size);
}

static CuiCommandBuffer *
_cui_renderer_software_begin_command_buffer(CuiRendererSoftware *renderer)
{
    CuiCommandBuffer *command_buffer = &renderer->command_buffer;

    command_buffer->push_buffer_size = 0;
    command_buffer->index_buffer_count = 0;
    command_buffer->texture_operation_count = 0;

    return command_buffer;
}

static void
_cui_renderer_software_render_tile(CuiRendererSoftware *renderer, CuiBitmap *framebuffer, CuiCommandBuffer *command_buffer,
                                   CuiRect tile_rect, CuiColor clear_color)
{
    CuiBitmap clear_bitmap = *framebuffer;
    clear_bitmap.width  = cui_rect_get_width(tile_rect);
    clear_bitmap.height = cui_rect_get_height(tile_rect);
    clear_bitmap.pixels = (uint8_t *) clear_bitmap.pixels + (tile_rect.min.y * clear_bitmap.stride) + (tile_rect.min.x * 4);

    cui_bitmap_clear(&clear_bitmap, clear_color);

    for (uint32_t i = 0; i < command_buffer->index_buffer_count; i += 1)
    {
        uint32_t rect_offset = command_buffer->index_buffer[i];

        CuiTexturedRect *textured_rect = (CuiTexturedRect *) (command_buffer->push_buffer + rect_offset);
        CuiBitmap *texture = renderer->bitmaps + textured_rect->texture_id;

        CuiRect clip_rect;

        if (textured_rect->clip_rect)
        {
            CuiClipRect *rect = (CuiClipRect *) (command_buffer->push_buffer + textured_rect->clip_rect - 1);
            clip_rect = cui_make_rect(rect->x_min, rect->y_min, rect->x_max, rect->y_max);
            clip_rect = cui_rect_get_intersection(tile_rect, clip_rect);
        }
        else
        {
            clip_rect = tile_rect;
        }

        int32_t x0 = textured_rect->x0;
        int32_t y0 = textured_rect->y0;
        int32_t x1 = textured_rect->x1;
        int32_t y1 = textured_rect->y1;
        float u0 = (float) textured_rect->u0;
        float v0 = (float) textured_rect->v0;
        float u1 = (float) textured_rect->u1;
        float v1 = (float) textured_rect->v1;
        CuiColor color = textured_rect->color;

        int32_t x_min = x0;
        int32_t y_min = y0;
        int32_t x_max = x1;
        int32_t y_max = y1;

        if (x_min < clip_rect.min.x) x_min = clip_rect.min.x;
        if (y_min < clip_rect.min.y) y_min = clip_rect.min.y;
        if (x_max > clip_rect.max.x) x_max = clip_rect.max.x;
        if (y_max > clip_rect.max.y) y_max = clip_rect.max.y;

        uint8_t *row = (uint8_t *) framebuffer->pixels + (framebuffer->stride * y_min) + (4 * x_min);

        for (int32_t y = y_min; y < y_max; y += 1)
        {
            uint32_t *pixel = (uint32_t *) row;

            for (int32_t x = x_min; x < x_max; x += 1)
            {
                float wx = ((float) (x - x0) + 0.5f) / (float) (x1 - x0);
                float wy = ((float) (y - y0) + 0.5f) / (float) (y1 - y0);

                int32_t u = (int32_t) (u0 + (u1 - u0) * wx);
                int32_t v = (int32_t) (v0 + (v1 - v0) * wy);

                // TODO: clamp

                uint32_t *texture_pixel = (uint32_t *) ((uint8_t *) texture->pixels + (texture->stride * v) + (4 * u));

                CuiColor texel = cui_color_unpack_bgra(*texture_pixel);

                texel.r = texel.r * color.r;
                texel.g = texel.g * color.g;
                texel.b = texel.b * color.b;
                texel.a = texel.a * color.a;

                CuiColor result = cui_color_unpack_bgra(*pixel);

                result.r = texel.r + result.r * (1.0f - texel.a);
                result.g = texel.g + result.g * (1.0f - texel.a);
                result.b = texel.b + result.b * (1.0f - texel.a);
                result.a = texel.a + result.a * (1.0f - texel.a);

                *pixel++ = cui_color_pack_bgra(result);
            }

            row += framebuffer->stride;
        }
    }
}

typedef struct CuiRenderWork
{
    CuiRendererSoftware *renderer;
    CuiBitmap *framebuffer;
    CuiCommandBuffer *command_buffer;
    CuiRect tile_rect;
    CuiColor clear_color;
} CuiRenderWork;

static void
_cui_renderer_software_do_render_work(void *data)
{
    CuiRenderWork *job = (CuiRenderWork *) data;
    _cui_renderer_software_render_tile(job->renderer, job->framebuffer, job->command_buffer, job->tile_rect, job->clear_color);
}

static void
_cui_renderer_software_render(CuiRendererSoftware *renderer, CuiFramebuffer *framebuffer, CuiCommandBuffer *command_buffer, CuiColor clear_color)
{
    CuiAssert(&renderer->command_buffer == command_buffer);

    for (uint32_t index = 0; index < command_buffer->texture_operation_count; index += 1)
    {
        CuiTextureOperation *texture_op = command_buffer->texture_operations + index;

        int32_t texture_id = texture_op->texture_id;
        CuiAssert(texture_id >= 0);
        CuiAssert(texture_id < CUI_MAX_TEXTURE_COUNT);

        switch (texture_op->type)
        {
            case CUI_TEXTURE_OPERATION_ALLOCATE:
            {
                renderer->bitmaps[texture_id] = texture_op->payload.bitmap;
            } break;

            case CUI_TEXTURE_OPERATION_DEALLOCATE:
            {
                CuiClearStruct(renderer->bitmaps[texture_id]);
            } break;

            case CUI_TEXTURE_OPERATION_UPDATE:
            {
            } break;
        }
    }

    CuiBitmap *render_target = &framebuffer->bitmap;

    CuiRenderWork render_jobs[CUI_SOFTWARE_RENDERER_TILE_COUNT_X *
                              CUI_SOFTWARE_RENDERER_TILE_COUNT_Y];

    uint32_t tile_width  = (render_target->width  + (CUI_SOFTWARE_RENDERER_TILE_COUNT_X - 1)) /
                           CUI_SOFTWARE_RENDERER_TILE_COUNT_X;
    uint32_t tile_height = (render_target->height + (CUI_SOFTWARE_RENDERER_TILE_COUNT_Y - 1)) /
                           CUI_SOFTWARE_RENDERER_TILE_COUNT_Y;

    tile_width = CuiAlign(tile_width, 16); // align to cache-line

    CuiRect framebuffer_rect = cui_make_rect(0, 0, render_target->width, render_target->height);

    CuiWorkerThreadQueue *queue = &_cui_context.common.worker_thread_queue;
    CuiWorkerThreadTaskGroup render_group = _cui_begin_worker_thread_task_group(_cui_renderer_software_do_render_work);

    uint32_t job_index = 0;

    for (uint32_t tile_y = 0; tile_y < CUI_SOFTWARE_RENDERER_TILE_COUNT_Y; tile_y += 1)
    {
        for (uint32_t tile_x = 0; tile_x < CUI_SOFTWARE_RENDERER_TILE_COUNT_X; tile_x += 1)
        {
            CuiRenderWork *job = render_jobs + job_index++;

            CuiRect tile_rect;
            tile_rect.min.x = tile_x * tile_width;
            tile_rect.min.y = tile_y * tile_height;
            tile_rect.max.x = tile_rect.min.x + tile_width;
            tile_rect.max.y = tile_rect.min.y + tile_height;

            job->renderer = renderer;
            job->framebuffer = render_target;
            job->command_buffer = command_buffer;
            job->tile_rect = cui_rect_get_intersection(tile_rect, framebuffer_rect);
            job->clear_color = clear_color;

            _cui_add_worker_thread_queue_entry(queue, &render_group, job);
        }
    }

    _cui_complete_worker_thread_task_group(queue, &render_group);

#if 0
    for (int32_t x = 0; x < render_target->width; x += 1)
    {
        *(uint32_t *) ((uint8_t *) render_target->pixels + (0 * render_target->stride) + (x * 4)) = 0xFFFF0000;
        *(uint32_t *) ((uint8_t *) render_target->pixels + ((render_target->height - 1) * render_target->stride) + (x * 4)) = 0xFFFF0000;
    }

    for (int32_t y = 0; y < render_target->height; y += 1)
    {
        *(uint32_t *) ((uint8_t *) render_target->pixels + (y * render_target->stride) + (0 * 4)) = 0xFFFF0000;
        *(uint32_t *) ((uint8_t *) render_target->pixels + (y * render_target->stride) + ((render_target->width - 1) * 4)) = 0xFFFF0000;
    }
#endif
}
