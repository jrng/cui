static CuiKernel
_cui_get_1d_blur_kernel(CuiArena *arena, int32_t blur_radius)
{
    CuiAssert(blur_radius >= 0);

    int32_t kernel_size = 2 * blur_radius + 1;

    CuiKernel result;

    result.weights = cui_alloc_array(arena, float, kernel_size, CuiDefaultAllocationParams());

    float sigma = 0.5f * (float) blur_radius;
    float sigma2 = 2.0f * sigma * sigma;
    float inv_sqrt_sigma = 1.0f / sqrt(PI2_F32 * sigma * sigma);

    float sum = 0.0f;

    for (int32_t x = -blur_radius; x <= blur_radius; x += 1)
    {
        float value = exp(- (float) (x * x) / sigma2) * inv_sqrt_sigma;
        result.weights[x + blur_radius] = value;
        sum += value;
    }

    result.factor = 1.0f / sum;

    return result;
}

static inline void
_cui_push_textured_rect(CuiCommandBuffer *command_buffer, CuiRect rect, CuiRect uv, CuiColor color, int32_t texture_id, uint32_t clip_rect_offset)
{
    CuiAssert((texture_id >= 0) && (texture_id < CUI_MAX_TEXTURE_COUNT));
    CuiAssert(command_buffer->index_buffer_count < command_buffer->max_index_buffer_count);

    CuiAssert((rect.min.x >= INT16_MIN) && (rect.min.x <= INT16_MAX));
    CuiAssert((rect.min.y >= INT16_MIN) && (rect.min.y <= INT16_MAX));
    CuiAssert((rect.max.x >= INT16_MIN) && (rect.max.x <= INT16_MAX));
    CuiAssert((rect.max.y >= INT16_MIN) && (rect.max.y <= INT16_MAX));
    CuiAssert((uv.min.x >= 0) && (uv.min.x <= INT16_MAX));
    CuiAssert((uv.min.y >= 0) && (uv.min.y <= INT16_MAX));
    CuiAssert((uv.max.x >= 0) && (uv.max.x <= INT16_MAX));
    CuiAssert((uv.max.y >= 0) && (uv.max.y <= INT16_MAX));

    uint32_t offset = command_buffer->push_buffer_size;

    color.r *= color.a;
    color.g *= color.a;
    color.b *= color.a;

    CuiTexturedRect *textured_rect = (CuiTexturedRect *) _cui_command_buffer_push_primitive(command_buffer, sizeof(CuiTexturedRect));

    textured_rect->x0 = (int16_t) rect.min.x;
    textured_rect->y0 = (int16_t) rect.min.y;
    textured_rect->u0 = (int16_t)   uv.min.x;
    textured_rect->v0 = (int16_t)   uv.min.y;
    textured_rect->x1 = (int16_t) rect.max.x;
    textured_rect->y1 = (int16_t) rect.max.y;
    textured_rect->u1 = (int16_t)   uv.max.x;
    textured_rect->v1 = (int16_t)   uv.max.y;
    textured_rect->color = color;
    textured_rect->texture_id = texture_id;
    textured_rect->clip_rect = clip_rect_offset;

    command_buffer->index_buffer[command_buffer->index_buffer_count++] = offset;
}

static inline void
_cui_draw_fill_rounded_corner(CuiGraphicsContext *ctx, int32_t x_min, int32_t y_min, float radius_x, float radius_y,
                              int32_t offset_x, int32_t offset_y, bool flip_x, bool flip_y, CuiColor color)
{
    CuiAssert((int32_t) ceilf(radius_x) == offset_x);
    CuiAssert((int32_t) ceilf(radius_y) == offset_y);

    CuiRect rect = cui_make_rect(x_min, y_min, x_min + offset_x, y_min + offset_y);

    if (cui_rect_overlap(ctx->clip_rect, rect))
    {
        CuiRect uv;

        if (!_cui_glyph_cache_find(ctx->glyph_cache, 0, CUI_SHAPE_ROUNDED_CORNER, 0.0f, radius_x, radius_y, &uv))
        {
            CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(ctx->temporary_memory);

            int32_t width  = offset_x;
            int32_t height = offset_y;

            uv = _cui_glyph_cache_allocate_texture(ctx->glyph_cache, width, height, ctx->command_buffer);

            CuiBitmap bitmap;
            bitmap.width  = cui_rect_get_width(uv);
            bitmap.height = cui_rect_get_height(uv);
            bitmap.stride = ctx->glyph_cache->texture.stride;
            bitmap.pixels = (uint8_t *) ctx->glyph_cache->texture.pixels + (uv.min.y * bitmap.stride) + (uv.min.x * 4);

            CuiPathCommand *path = 0;
            cui_array_init(path, 16, ctx->temporary_memory);

            float c = 0.552f;

            cui_path_move_to(&path, radius_x, 0.0f);
            cui_path_line_to(&path, 0.0f, 0.0f);
            cui_path_line_to(&path, 0.0f, radius_y);
            cui_path_cubic_curve_to(&path, radius_x * c, radius_y, radius_x, radius_y * c, radius_x, 0.0f);

            CuiEdge *edge_list = 0;
            cui_array_init(edge_list, 16, ctx->temporary_memory);

            _cui_path_to_edge_list(path, &edge_list);
            _cui_edge_list_fill(ctx->temporary_memory, &bitmap, edge_list, cui_make_color(1.0f, 1.0f, 1.0f, 1.0f));

            _cui_glyph_cache_put(ctx->glyph_cache, 0, CUI_SHAPE_ROUNDED_CORNER, 0.0f, radius_x, radius_y, uv);

            cui_end_temporary_memory(temp_memory);
        }

        if (flip_x)
        {
            int32_t temp = uv.min.x;
            uv.min.x = uv.max.x;
            uv.max.x = temp;
        }

        if (flip_y)
        {
            int32_t temp = uv.min.y;
            uv.min.y = uv.max.y;
            uv.max.y = temp;
        }

        _cui_push_textured_rect(ctx->command_buffer, rect, uv, color, ctx->glyph_cache->texture_id, ctx->clip_rect_offset);
    }
}

CuiRect
cui_draw_set_clip_rect(CuiGraphicsContext *ctx, CuiRect clip_rect)
{
    CuiRect prev_rect = ctx->clip_rect;
    ctx->clip_rect = clip_rect;
    ctx->clip_rect_offset = _cui_command_buffer_push_clip_rect(ctx->command_buffer, ctx->clip_rect);
    return prev_rect;
}

int32_t
cui_draw_get_max_texture_width(CuiGraphicsContext *ctx)
{
    return ctx->command_buffer->max_texture_width;
}

int32_t
cui_draw_get_max_texture_height(CuiGraphicsContext *ctx)
{
    return ctx->command_buffer->max_texture_height;
}

void
cui_draw_allocate_texture(CuiGraphicsContext *ctx, int32_t texture_id, CuiBitmap bitmap)
{
    CuiTextureOperation *texture_op = _cui_command_buffer_add_texture_operation(ctx->command_buffer);

    texture_op->type = CUI_TEXTURE_OPERATION_ALLOCATE;
    texture_op->texture_id = texture_id;
    texture_op->payload.bitmap = bitmap;
}

void
cui_draw_update_texture(CuiGraphicsContext *ctx, int32_t texture_id, CuiRect update_rect)
{
    CuiTextureOperation *texture_op = 0;

    for (int32_t index = ctx->command_buffer->texture_operation_count - 1; index > 0; index -= 1)
    {
        CuiTextureOperation *op = ctx->command_buffer->texture_operations + index;

        if ((op->type == CUI_TEXTURE_OPERATION_UPDATE) && (op->texture_id == texture_id))
        {
            texture_op = op;
            break;
        }
    }

    if (texture_op)
    {
        texture_op->payload.rect = cui_rect_get_union(texture_op->payload.rect, update_rect);
    }
    else
    {
        texture_op = _cui_command_buffer_add_texture_operation(ctx->command_buffer);

        texture_op->type = CUI_TEXTURE_OPERATION_UPDATE;
        texture_op->texture_id = texture_id;
        texture_op->payload.rect = update_rect;
    }
}

void
cui_draw_deallocate_texture(CuiGraphicsContext *ctx, int32_t texture_id)
{
    CuiTextureOperation *texture_op = _cui_command_buffer_add_texture_operation(ctx->command_buffer);

    texture_op->type = CUI_TEXTURE_OPERATION_DEALLOCATE;
    texture_op->texture_id = texture_id;
}

void
cui_draw_fill_textured_rect(CuiGraphicsContext *ctx, CuiRect rect, CuiRect uv, int32_t texture_id, CuiColor color)
{
    if (cui_rect_overlap(ctx->clip_rect, rect))
    {
        _cui_push_textured_rect(ctx->command_buffer, rect, uv, color, texture_id, ctx->clip_rect_offset);
    }
}

void
cui_draw_fill_rect(CuiGraphicsContext *ctx, CuiRect rect, CuiColor color)
{
    if (cui_rect_overlap(ctx->clip_rect, rect))
    {
        _cui_push_textured_rect(ctx->command_buffer, rect, cui_make_rect(0, 0, 0, 0),
                                color, ctx->glyph_cache->texture_id, ctx->clip_rect_offset);
    }
}

void
cui_draw_fill_rounded_rect_1(CuiGraphicsContext *ctx, CuiRect rect, float radius, CuiColor color)
{
    if (cui_rect_overlap(ctx->clip_rect, rect))
    {
        int32_t offset = (int32_t) ceilf(radius);

        CuiRect uv;

        if (!_cui_glyph_cache_find(ctx->glyph_cache, 0, CUI_SHAPE_ROUNDED_CORNER, 0.0f, radius, radius, &uv))
        {
            CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(ctx->temporary_memory);

            int32_t width  = offset;
            int32_t height = offset;

            uv = _cui_glyph_cache_allocate_texture(ctx->glyph_cache, width, height, ctx->command_buffer);

            CuiBitmap bitmap;
            bitmap.width  = cui_rect_get_width(uv);
            bitmap.height = cui_rect_get_height(uv);
            bitmap.stride = ctx->glyph_cache->texture.stride;
            bitmap.pixels = (uint8_t *) ctx->glyph_cache->texture.pixels + (uv.min.y * bitmap.stride) + (uv.min.x * 4);

            CuiPathCommand *path = 0;
            cui_array_init(path, 16, ctx->temporary_memory);

            float c = 0.552f;

            cui_path_move_to(&path, radius, 0.0f);
            cui_path_line_to(&path, 0.0f, 0.0f);
            cui_path_line_to(&path, 0.0f, radius);
            cui_path_cubic_curve_to(&path, radius * c, radius, radius, radius * c, radius, 0.0f);

            CuiEdge *edge_list = 0;
            cui_array_init(edge_list, 16, ctx->temporary_memory);

            _cui_path_to_edge_list(path, &edge_list);
            _cui_edge_list_fill(ctx->temporary_memory, &bitmap, edge_list, cui_make_color(1.0f, 1.0f, 1.0f, 1.0f));

            _cui_glyph_cache_put(ctx->glyph_cache, 0, CUI_SHAPE_ROUNDED_CORNER, 0.0f, radius, radius, uv);

            cui_end_temporary_memory(temp_memory);
        }

        _cui_push_textured_rect(ctx->command_buffer, cui_make_rect(rect.max.x - offset, rect.max.y - offset, rect.max.x, rect.max.y),
                                uv, color, ctx->glyph_cache->texture_id, ctx->clip_rect_offset);

        int32_t temp = uv.min.x;
        uv.min.x = uv.max.x;
        uv.max.x = temp;

        _cui_push_textured_rect(ctx->command_buffer, cui_make_rect(rect.min.x, rect.max.y - offset, rect.min.x + offset, rect.max.y),
                                uv, color, ctx->glyph_cache->texture_id, ctx->clip_rect_offset);

        temp = uv.min.y;
        uv.min.y = uv.max.y;
        uv.max.y = temp;

        _cui_push_textured_rect(ctx->command_buffer, cui_make_rect(rect.min.x, rect.min.y, rect.min.x + offset, rect.min.y + offset),
                                uv, color, ctx->glyph_cache->texture_id, ctx->clip_rect_offset);

        temp = uv.min.x;
        uv.min.x = uv.max.x;
        uv.max.x = temp;

        _cui_push_textured_rect(ctx->command_buffer, cui_make_rect(rect.max.x - offset, rect.min.y, rect.max.x, rect.min.y + offset),
                                uv, color, ctx->glyph_cache->texture_id, ctx->clip_rect_offset);

        _cui_push_textured_rect(ctx->command_buffer, cui_make_rect(rect.min.x + offset, rect.min.y, rect.max.x - offset, rect.min.y + offset),
                                cui_make_rect(0, 0, 0, 0), color, ctx->glyph_cache->texture_id, ctx->clip_rect_offset);
        _cui_push_textured_rect(ctx->command_buffer, cui_make_rect(rect.min.x, rect.min.y + offset, rect.max.x, rect.max.y - offset),
                                cui_make_rect(0, 0, 0, 0), color, ctx->glyph_cache->texture_id, ctx->clip_rect_offset);
        _cui_push_textured_rect(ctx->command_buffer, cui_make_rect(rect.min.x + offset, rect.max.y - offset, rect.max.x - offset, rect.max.y),
                                cui_make_rect(0, 0, 0, 0), color, ctx->glyph_cache->texture_id, ctx->clip_rect_offset);
    }
}

void
cui_draw_fill_rounded_rect_4(CuiGraphicsContext *ctx, CuiRect rect, float top_left_radius, float top_right_radius,
                             float bottom_right_radius, float bottom_left_radius, CuiColor color)
{
    if (!cui_rect_overlap(ctx->clip_rect, rect))
    {
        return;
    }

    int32_t top_left_offset     = (int32_t) ceilf(top_left_radius);
    int32_t top_right_offset    = (int32_t) ceilf(top_right_radius);
    int32_t bottom_right_offset = (int32_t) ceilf(bottom_right_radius);
    int32_t bottom_left_offset  = (int32_t) ceilf(bottom_left_radius);

    int32_t top_offset = cui_max_int32(top_left_offset, top_right_offset);
    int32_t bottom_offset = cui_max_int32(bottom_left_offset, bottom_right_offset);

    CuiRect top_rect = rect;
    CuiRect middle_rect = rect;
    CuiRect bottom_rect = rect;

    middle_rect.min.y += top_offset;
    middle_rect.max.y -= bottom_offset;

    top_rect.min.x += top_left_offset;
    top_rect.max.x -= top_right_offset;
    top_rect.max.y = top_rect.min.y + top_offset;

    bottom_rect.min.x += bottom_left_offset;
    bottom_rect.max.x -= bottom_right_offset;
    bottom_rect.min.y = bottom_rect.max.y - bottom_offset;

    if (top_offset)
    {
        if (top_left_offset)
        {
            if (top_left_offset < top_right_offset)
            {
                cui_draw_fill_rect(ctx, cui_make_rect(rect.min.x, rect.min.y + top_left_offset, rect.min.x + top_left_offset, rect.min.y + top_offset), color);
            }

            _cui_draw_fill_rounded_corner(ctx, rect.min.x, rect.min.y, top_left_radius, top_left_radius,
                                          top_left_offset, top_left_offset, true, true, color);
        }

        if (top_right_offset)
        {
            if (top_right_offset < top_left_offset)
            {
                cui_draw_fill_rect(ctx, cui_make_rect(rect.max.x - top_right_offset, rect.min.y + top_right_offset, rect.max.x, rect.min.y + top_offset), color);
            }

            _cui_draw_fill_rounded_corner(ctx, rect.max.x - top_right_offset, rect.min.y, top_right_radius, top_right_radius,
                                          top_right_offset, top_right_offset, false, true, color);
        }

        cui_draw_fill_rect(ctx, top_rect, color);
    }

    cui_draw_fill_rect(ctx, middle_rect, color);

    if (bottom_offset)
    {
        if (bottom_left_offset)
        {
            if (bottom_left_offset < bottom_right_offset)
            {
                cui_draw_fill_rect(ctx, cui_make_rect(rect.min.x, rect.max.y - bottom_offset, rect.min.x + bottom_left_offset, rect.max.y - bottom_left_offset), color);
            }

            _cui_draw_fill_rounded_corner(ctx, rect.min.x, rect.max.y - bottom_left_offset, bottom_left_radius, bottom_left_radius,
                                          bottom_left_offset, bottom_left_offset, true, false, color);
        }

        if (bottom_right_offset)
        {
            if (bottom_right_offset < bottom_left_offset)
            {
                cui_draw_fill_rect(ctx, cui_make_rect(rect.max.x - bottom_right_offset, rect.max.y - bottom_offset, rect.max.x, rect.max.y - bottom_right_offset), color);
            }

            _cui_draw_fill_rounded_corner(ctx, rect.max.x - bottom_right_offset, rect.max.y - bottom_right_offset,
                                          bottom_right_radius, bottom_right_radius, bottom_right_offset, bottom_right_offset,
                                          false, false, color);
        }

        cui_draw_fill_rect(ctx, bottom_rect, color);
    }
}

void
cui_draw_fill_rounded_rect_8(CuiGraphicsContext *ctx, CuiRect rect, float top_left_radius_x, float top_left_radius_y,
                             float top_right_radius_x, float top_right_radius_y, float bottom_right_radius_x,
                             float bottom_right_radius_y, float bottom_left_radius_x, float bottom_left_radius_y, CuiColor color)
{
    if (!cui_rect_overlap(ctx->clip_rect, rect))
    {
        return;
    }

    int32_t top_left_offset_x     = (int32_t) ceilf(top_left_radius_x);
    int32_t top_left_offset_y     = (int32_t) ceilf(top_left_radius_y);
    int32_t top_right_offset_x    = (int32_t) ceilf(top_right_radius_x);
    int32_t top_right_offset_y    = (int32_t) ceilf(top_right_radius_y);
    int32_t bottom_right_offset_x = (int32_t) ceilf(bottom_right_radius_x);
    int32_t bottom_right_offset_y = (int32_t) ceilf(bottom_right_radius_y);
    int32_t bottom_left_offset_x  = (int32_t) ceilf(bottom_left_radius_x);
    int32_t bottom_left_offset_y  = (int32_t) ceilf(bottom_left_radius_y);

    if (!top_left_offset_x || !top_left_offset_y)
    {
        top_left_offset_x = 0;
        top_left_offset_y = 0;
    }

    if (!top_right_offset_x || !top_right_offset_y)
    {
        top_right_offset_x = 0;
        top_right_offset_y = 0;
    }

    if (!bottom_right_offset_x || !bottom_right_offset_y)
    {
        bottom_right_offset_x = 0;
        bottom_right_offset_y = 0;
    }

    if (!bottom_left_offset_x || !bottom_left_offset_y)
    {
        bottom_left_offset_x = 0;
        bottom_left_offset_y = 0;
    }

    int32_t top_offset_y = cui_max_int32(top_left_offset_y, top_right_offset_y);
    int32_t bottom_offset_y = cui_max_int32(bottom_left_offset_y, bottom_right_offset_y);

    CuiRect top_rect = rect;
    CuiRect middle_rect = rect;
    CuiRect bottom_rect = rect;

    middle_rect.min.y += top_offset_y;
    middle_rect.max.y -= bottom_offset_y;

    top_rect.min.x += top_left_offset_x;
    top_rect.max.x -= top_right_offset_x;
    top_rect.max.y = top_rect.min.y + top_offset_y;

    bottom_rect.min.x += bottom_left_offset_x;
    bottom_rect.max.x -= bottom_right_offset_x;
    bottom_rect.min.y = bottom_rect.max.y - bottom_offset_y;

    if (top_offset_y)
    {
        if (top_left_offset_x && top_left_offset_y)
        {
            if (top_left_offset_y < top_right_offset_y)
            {
                cui_draw_fill_rect(ctx, cui_make_rect(rect.min.x, rect.min.y + top_left_offset_y,
                                                      rect.min.x + top_left_offset_x, rect.min.y + top_offset_y), color);
            }

            _cui_draw_fill_rounded_corner(ctx, rect.min.x, rect.min.y, top_left_radius_x, top_left_radius_y,
                                          top_left_offset_x, top_left_offset_y, true, true, color);
        }

        if (top_right_offset_x && top_right_offset_y)
        {
            if (top_right_offset_y < top_left_offset_y)
            {
                cui_draw_fill_rect(ctx, cui_make_rect(rect.max.x - top_right_offset_x, rect.min.y + top_right_offset_y,
                                                      rect.max.x, rect.min.y + top_offset_y), color);
            }

            _cui_draw_fill_rounded_corner(ctx, rect.max.x - top_right_offset_x, rect.min.y, top_right_radius_x, top_right_radius_y,
                                          top_right_offset_x, top_right_offset_y, false, true, color);
        }

        cui_draw_fill_rect(ctx, top_rect, color);
    }

    cui_draw_fill_rect(ctx, middle_rect, color);

    if (bottom_offset_y)
    {
        if (bottom_left_offset_x && bottom_left_offset_y)
        {
            if (bottom_left_offset_y < bottom_right_offset_y)
            {
                cui_draw_fill_rect(ctx, cui_make_rect(rect.min.x, rect.max.y - bottom_offset_y,
                                                      rect.min.x + bottom_left_offset_x, rect.max.y - bottom_left_offset_y), color);
            }

            _cui_draw_fill_rounded_corner(ctx, rect.min.x, rect.max.y - bottom_left_offset_y, bottom_left_radius_x, bottom_left_radius_y,
                                          bottom_left_offset_x, bottom_left_offset_y, true, false, color);
        }

        if (bottom_right_offset_x && bottom_right_offset_y)
        {
            if (bottom_right_offset_y < bottom_left_offset_y)
            {
                cui_draw_fill_rect(ctx, cui_make_rect(rect.max.x - bottom_right_offset_x, rect.max.y - bottom_offset_y,
                                                      rect.max.x, rect.max.y - bottom_right_offset_y), color);
            }

            _cui_draw_fill_rounded_corner(ctx, rect.max.x - bottom_right_offset_x, rect.max.y - bottom_right_offset_y,
                                          bottom_right_radius_x, bottom_right_radius_y, bottom_right_offset_x, bottom_right_offset_y,
                                          false, false, color);
        }

        cui_draw_fill_rect(ctx, bottom_rect, color);
    }
}

void
cui_draw_stroke_rounded_rect_4(CuiGraphicsContext *ctx, CuiRect rect, float top_left_radius, float top_right_radius,
                               float bottom_right_radius, float bottom_left_radius, int32_t stroke_width, CuiColor color)
{
    if (!cui_rect_overlap(ctx->clip_rect, rect))
    {
        return;
    }

    int32_t top_left_offset     = (int32_t) ceilf(top_left_radius);
    int32_t top_right_offset    = (int32_t) ceilf(top_right_radius);
    int32_t bottom_right_offset = (int32_t) ceilf(bottom_right_radius);
    int32_t bottom_left_offset  = (int32_t) ceilf(bottom_left_radius);

    CuiRect top_rect = rect;
    CuiRect left_rect = rect;
    CuiRect right_rect = rect;
    CuiRect bottom_rect = rect;

    top_rect.min.x += cui_max_int32(stroke_width, top_left_offset);
    top_rect.max.x -= cui_max_int32(stroke_width, top_right_offset);
    top_rect.max.y = top_rect.min.y + stroke_width;

    left_rect.min.y += top_left_offset;
    left_rect.max.y -= bottom_left_offset;
    left_rect.max.x = left_rect.min.x + stroke_width;

    right_rect.min.y += top_right_offset;
    right_rect.max.y -= bottom_right_offset;
    right_rect.min.x = right_rect.max.x - stroke_width;

    bottom_rect.min.x += cui_max_int32(stroke_width, bottom_left_offset);
    bottom_rect.max.x -= cui_max_int32(stroke_width, bottom_right_offset);
    bottom_rect.min.y = bottom_rect.max.y - stroke_width;

    // TODO: use stroked corners, not filled

    if (top_left_offset)
    {
        _cui_draw_fill_rounded_corner(ctx, rect.min.x, rect.min.y, top_left_radius, top_left_radius,
                                      top_left_offset, top_left_offset, true, true, color);
    }

    if (top_right_offset)
    {
        _cui_draw_fill_rounded_corner(ctx, rect.max.x - top_right_offset, rect.min.y, top_right_radius, top_right_radius,
                                      top_right_offset, top_right_offset, false, true, color);
    }

    if (bottom_left_offset)
    {
        _cui_draw_fill_rounded_corner(ctx, rect.min.x, rect.max.y - bottom_left_offset, bottom_left_radius, bottom_left_radius,
                                      bottom_left_offset, bottom_left_offset, true, false, color);
    }

    if (bottom_right_offset)
    {
        _cui_draw_fill_rounded_corner(ctx, rect.max.x - bottom_right_offset, rect.max.y - bottom_right_offset,
                                      bottom_right_radius, bottom_right_radius, bottom_right_offset, bottom_right_offset,
                                      false, false, color);
    }

    cui_draw_fill_rect(ctx, top_rect, color);
    cui_draw_fill_rect(ctx, left_rect, color);
    cui_draw_fill_rect(ctx, right_rect, color);
    cui_draw_fill_rect(ctx, bottom_rect, color);
}

void
cui_draw_fill_shadow(CuiGraphicsContext *ctx, int32_t x, int32_t y, int32_t max, int32_t blur_radius,
                     CuiDirection direction, CuiColor color)
{
    CuiRect uv;
    int32_t span = 2 * blur_radius;

    if ((direction == CUI_DIRECTION_EAST) || (direction == CUI_DIRECTION_WEST))
    {
        if (!_cui_glyph_cache_find(ctx->glyph_cache, 0, CUI_SHAPE_SHADOW_HORIZONTAL, (float) blur_radius, 0.0f, 0.0f, &uv))
        {
            CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(ctx->temporary_memory);

            uv = _cui_glyph_cache_allocate_texture(ctx->glyph_cache, span, 1, ctx->command_buffer);

            CuiBitmap bitmap;
            bitmap.width  = cui_rect_get_width(uv);
            bitmap.height = cui_rect_get_height(uv);
            bitmap.stride = ctx->glyph_cache->texture.stride;
            bitmap.pixels = (uint8_t *) ctx->glyph_cache->texture.pixels + (uv.min.y * bitmap.stride) + (uv.min.x * 4);

            CuiKernel blur_kernel = _cui_get_1d_blur_kernel(ctx->temporary_memory, blur_radius);

            uint32_t *pixel = (uint32_t *) bitmap.pixels;
            float sum = (float) blur_kernel.weights[0];

            for (int32_t i = 0; i < span; i += 1)
            {
                float value = sum * blur_kernel.factor;
                uint8_t val = (uint8_t) ((value * 255.0f) + 0.5f);
                *pixel = 0x01010101 * (uint32_t) val;

                pixel += 1;
                sum += (float) blur_kernel.weights[i + 1];
            }

            cui_end_temporary_memory(temp_memory);

            _cui_glyph_cache_put(ctx->glyph_cache, 0, CUI_SHAPE_SHADOW_HORIZONTAL, (float) blur_radius, 0.0f, 0.0f, uv);
        }
    }
    else
    {
        CuiAssert((direction == CUI_DIRECTION_NORTH) || (direction == CUI_DIRECTION_SOUTH));

        if (!_cui_glyph_cache_find(ctx->glyph_cache, 0, CUI_SHAPE_SHADOW_VERTICAL, (float) blur_radius, 0.0f, 0.0f, &uv))
        {
            CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(ctx->temporary_memory);

            uv = _cui_glyph_cache_allocate_texture(ctx->glyph_cache, 1, span, ctx->command_buffer);

            CuiBitmap bitmap;
            bitmap.width  = cui_rect_get_width(uv);
            bitmap.height = cui_rect_get_height(uv);
            bitmap.stride = ctx->glyph_cache->texture.stride;
            bitmap.pixels = (uint8_t *) ctx->glyph_cache->texture.pixels + (uv.min.y * bitmap.stride) + (uv.min.x * 4);

            CuiKernel blur_kernel = _cui_get_1d_blur_kernel(ctx->temporary_memory, blur_radius);

            uint8_t *row = (uint8_t *) bitmap.pixels;
            float sum = (float) blur_kernel.weights[0];

            for (int32_t i = 0; i < span; i += 1)
            {
                float value = sum * blur_kernel.factor;
                uint8_t val = (uint8_t) ((value * 255.0f) + 0.5f);
                *(uint32_t *) row = 0x01010101 * (uint32_t) val;

                row += bitmap.stride;
                sum += (float) blur_kernel.weights[i + 1];
            }

            cui_end_temporary_memory(temp_memory);

            _cui_glyph_cache_put(ctx->glyph_cache, 0, CUI_SHAPE_SHADOW_VERTICAL, (float) blur_radius, 0.0f, 0.0f, uv);
        }
    }

    CuiRect draw_rect;

    switch (direction)
    {
        case CUI_DIRECTION_SOUTH:
            cui_swap_int32(&uv.min.y, &uv.max.y);
            /* FALLTHRU */
        case CUI_DIRECTION_NORTH:
            draw_rect.min.x = x;
            draw_rect.min.y = y - blur_radius;
            draw_rect.max.x = max;
            draw_rect.max.y = y + blur_radius;
            break;

        case CUI_DIRECTION_EAST:
            cui_swap_int32(&uv.min.x, &uv.max.x);
            /* FALLTHRU */
        case CUI_DIRECTION_WEST:
            draw_rect.min.x = x - blur_radius;
            draw_rect.min.y = y;
            draw_rect.max.x = x + blur_radius;
            draw_rect.max.y = max;
            break;
    }

    if (cui_rect_overlap(ctx->clip_rect, draw_rect))
    {
        _cui_push_textured_rect(ctx->command_buffer, draw_rect, uv, color, ctx->glyph_cache->texture_id, ctx->clip_rect_offset);
    }
}

void
cui_draw_fill_shadow_corner(CuiGraphicsContext *ctx, int32_t x, int32_t y, int32_t radius, int32_t blur_radius,
                            CuiDirection direction_x, CuiDirection direction_y, CuiColor color)
{
    CuiAssert((direction_x == CUI_DIRECTION_EAST) || (direction_x == CUI_DIRECTION_WEST));
    CuiAssert((direction_y == CUI_DIRECTION_NORTH) || (direction_y == CUI_DIRECTION_SOUTH));

    CuiRect uv;
    int32_t distance_from_corner = radius + blur_radius;

    if (!_cui_glyph_cache_find(ctx->glyph_cache, 0, CUI_SHAPE_SHADOW_CORNER, (float) blur_radius, (float) radius, 0.0f, &uv))
    {
        CuiTemporaryMemory temp_memory1 = cui_begin_temporary_memory(ctx->temporary_memory);

        int32_t size = distance_from_corner + blur_radius;

        uv = _cui_glyph_cache_allocate_texture(ctx->glyph_cache, size, size, ctx->command_buffer);

        CuiBitmap bitmap;
        bitmap.width  = cui_rect_get_width(uv);
        bitmap.height = cui_rect_get_height(uv);
        bitmap.stride = ctx->glyph_cache->texture.stride;
        bitmap.pixels = (uint8_t *) ctx->glyph_cache->texture.pixels + (uv.min.y * bitmap.stride) + (uv.min.x * 4);

        CuiBitmap backbuffer1;
        backbuffer1.width  = bitmap.width;
        backbuffer1.height = bitmap.height;
        backbuffer1.stride = backbuffer1.width * 4;
        backbuffer1.pixels = cui_alloc(ctx->temporary_memory, backbuffer1.stride * backbuffer1.height, cui_make_allocation_params(true, 8));

        CuiTemporaryMemory temp_memory2 = cui_begin_temporary_memory(ctx->temporary_memory);

        CuiPathCommand *path = 0;
        cui_array_init(path, 16, ctx->temporary_memory);

        float c = 0.552f;

        cui_path_move_to(&path, (float) distance_from_corner, (float) blur_radius);
        cui_path_line_to(&path, (float) distance_from_corner, 0.0f);
        cui_path_line_to(&path, 0.0f, 0.0f);
        cui_path_line_to(&path, 0.0f, (float) distance_from_corner);
        cui_path_line_to(&path, (float) blur_radius, (float) distance_from_corner);
        cui_path_cubic_curve_to(&path, (float) blur_radius + ((float) radius * c), (float) distance_from_corner,
                                (float) distance_from_corner, (float) blur_radius + ((float) radius * c), (float) distance_from_corner, (float) blur_radius);

        CuiEdge *edge_list = 0;
        cui_array_init(edge_list, 16, ctx->temporary_memory);

        _cui_path_to_edge_list(path, &edge_list);
        _cui_edge_list_fill(ctx->temporary_memory, &backbuffer1, edge_list, cui_make_color(1.0f, 1.0f, 1.0f, 1.0f));

        cui_end_temporary_memory(temp_memory2);

        CuiBitmap backbuffer2;
        backbuffer2.width  = bitmap.width;
        backbuffer2.height = bitmap.height;
        backbuffer2.stride = backbuffer2.width * 4;
        backbuffer2.pixels = cui_alloc(ctx->temporary_memory, backbuffer2.stride * backbuffer2.height, CuiDefaultAllocationParams());

        CuiKernel blur_kernel = _cui_get_1d_blur_kernel(ctx->temporary_memory, blur_radius);

        uint8_t *row = (uint8_t *) backbuffer2.pixels;
        for (int32_t y = 0; y < backbuffer2.height; y += 1)
        {
            uint32_t *pixel = (uint32_t *) row;
            for (int32_t x = 0; x < backbuffer2.width; x += 1)
            {
                CuiColor color = cui_make_color(0.0f, 0.0f, 0.0f, 0.0f);

                for (int32_t offset = -blur_radius; offset <= blur_radius; offset += 1)
                {
                    float weight = blur_kernel.weights[offset + blur_radius];
                    int32_t x_coord = cui_max_int32(0, cui_min_int32(x + offset, backbuffer1.width - 1));
                    uint32_t packed_pixel = *(uint32_t *) ((uint8_t *) backbuffer1.pixels + (y * backbuffer1.stride) + (x_coord * 4));
                    CuiColor pixel = cui_color_unpack_bgra(packed_pixel);

                    color.r += weight * pixel.r;
                    color.g += weight * pixel.g;
                    color.b += weight * pixel.b;
                    color.a += weight * pixel.a;
                }

                color.r *= blur_kernel.factor;
                color.g *= blur_kernel.factor;
                color.b *= blur_kernel.factor;
                color.a *= blur_kernel.factor;

                *pixel = cui_color_pack_bgra(color);
                pixel += 1;
            }
            row += backbuffer2.stride;
        }

        row = (uint8_t *) bitmap.pixels;
        for (int32_t y = 0; y < bitmap.height; y += 1)
        {
            uint32_t *pixel = (uint32_t *) row;
            for (int32_t x = 0; x < bitmap.width; x += 1)
            {
                CuiColor color = cui_make_color(0.0f, 0.0f, 0.0f, 0.0f);

                for (int32_t offset = -blur_radius; offset <= blur_radius; offset += 1)
                {
                    float weight = blur_kernel.weights[offset + blur_radius];
                    int32_t y_coord = cui_max_int32(0, cui_min_int32(y + offset, backbuffer2.height - 1));
                    uint32_t packed_pixel = *(uint32_t *) ((uint8_t *) backbuffer2.pixels + (y_coord * backbuffer2.stride) + (x * 4));
                    CuiColor pixel = cui_color_unpack_bgra(packed_pixel);

                    color.r += weight * pixel.r;
                    color.g += weight * pixel.g;
                    color.b += weight * pixel.b;
                    color.a += weight * pixel.a;
                }

                color.r *= blur_kernel.factor;
                color.g *= blur_kernel.factor;
                color.b *= blur_kernel.factor;
                color.a *= blur_kernel.factor;

                *pixel = cui_color_pack_bgra(color);
                pixel += 1;
            }
            row += bitmap.stride;
        }

        cui_end_temporary_memory(temp_memory1);

        _cui_glyph_cache_put(ctx->glyph_cache, 0, CUI_SHAPE_SHADOW_CORNER, (float) blur_radius, (float) radius, 0.0f, uv);
    }

    CuiRect draw_rect;

    if (direction_x == CUI_DIRECTION_EAST)
    {
        draw_rect.min.x = x - distance_from_corner;
        draw_rect.max.x = x + blur_radius;
    }
    else
    {
        CuiAssert(direction_x == CUI_DIRECTION_WEST);

        cui_swap_int32(&uv.min.x, &uv.max.x);

        draw_rect.min.x = x - blur_radius;
        draw_rect.max.x = x + distance_from_corner;
    }

    if (direction_y == CUI_DIRECTION_SOUTH)
    {
        draw_rect.min.y = y - distance_from_corner;
        draw_rect.max.y = y + blur_radius;
    }
    else
    {
        CuiAssert(direction_y == CUI_DIRECTION_NORTH);

        cui_swap_int32(&uv.min.y, &uv.max.y);

        draw_rect.min.y = y - blur_radius;
        draw_rect.max.y = y + distance_from_corner;
    }

    if (cui_rect_overlap(ctx->clip_rect, draw_rect))
    {
        _cui_push_textured_rect(ctx->command_buffer, draw_rect, uv, color, ctx->glyph_cache->texture_id, ctx->clip_rect_offset);
    }
}

void
cui_draw_fill_string(CuiGraphicsContext *ctx, CuiFontId font_id, float x, float y, CuiString str, CuiColor color)
{
    CuiAssert(font_id.value > 0);

    CuiFont *font = _cui_font_manager_get_font_from_id(ctx->font_manager, font_id);
    // CuiFont *prev_used_font = 0;

    CuiAssert(font);

    int64_t index = 0;
    uint32_t glyph_index /*, prev_glyph_index */;

    while (index < str.count)
    {
        CuiUnicodeResult utf8 = cui_utf8_decode(str, index);

        glyph_index = 0;
        CuiFont *used_font = font;

        while (used_font)
        {
            CuiFontFile *font_file = _cui_font_file_manager_get_font_file_from_id(ctx->font_manager->font_file_manager, used_font->file_id);
            glyph_index = _cui_font_file_get_glyph_index_from_codepoint(font_file, utf8.codepoint);

            if (glyph_index) break;

            used_font = _cui_font_manager_get_font_from_id(ctx->font_manager, used_font->fallback_id);
        }

        if (!used_font)
        {
            used_font = font;
            glyph_index = 0;
        }

        CuiFontFile *used_font_file = _cui_font_file_manager_get_font_file_from_id(ctx->font_manager->font_file_manager, used_font->file_id);

#if 0
        if ((index > 0) && (used_font == prev_used_font))
        {
            x += used_font->font_scale * _cui_font_file_get_glyph_kerning(used_font_file, prev_glyph_index, glyph_index);
        }
#endif

        CuiRect bounding_box;
        CuiColor glyph_color = color;

        CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(ctx->temporary_memory);

        CuiColoredGlyphLayer *layers = 0;
        cui_array_init(layers, 16, ctx->temporary_memory);

        if (_cui_font_file_get_glyph_colored_layers(used_font_file, &layers, glyph_index))
        {
            glyph_color.r = 1.0f;
            glyph_color.g = 1.0f;
            glyph_color.b = 1.0f;

            bounding_box = cui_make_rect(INT32_MAX, INT32_MAX, INT32_MIN, INT32_MIN);

            for (int32_t layer_index = 0; layer_index < cui_array_count(layers); layer_index += 1)
            {
                CuiColoredGlyphLayer *layer = layers + layer_index;
                bounding_box = cui_rect_get_union(bounding_box, layer->bounding_box);
            }
        }
        else
        {
            bounding_box = _cui_font_file_get_glyph_bounding_box(used_font_file, glyph_index);

            CuiColoredGlyphLayer *layer = cui_array_append(layers);

            layer->glyph_index = glyph_index;
            layer->color = cui_make_color(1.0f, 1.0f, 1.0f, 1.0f);
            layer->bounding_box = bounding_box;
        }

        if (cui_rect_has_area(bounding_box))
        {
            CuiFloatRect bound;
            bound.min.x = used_font->font_scale * (float) bounding_box.min.x;
            bound.min.y = used_font->font_scale * (float) bounding_box.min.y;
            bound.max.x = used_font->font_scale * (float) bounding_box.max.x;
            bound.max.y = used_font->font_scale * (float) bounding_box.max.y;

            float draw_x = x + bound.min.x;
            float draw_y = y - bound.max.y;

            float bitmap_x = floorf(draw_x);
            float bitmap_y = floorf(draw_y);

            float offset_x = draw_x - bitmap_x;
            float offset_y = draw_y - bitmap_y;

            // NOTE: round to nearest 8th substep to improve caching.
            // That way more offsets are mapped to the same cached shape.
            offset_x = 0.125f * roundf(offset_x * 8.0f);
            offset_y = 0.125f * roundf(offset_y * 8.0f);

            CuiRect uv;

            if (!_cui_glyph_cache_find(ctx->glyph_cache, used_font->file_id.value, utf8.codepoint, used_font->font_scale, offset_x, offset_y, &uv))
            {
                int32_t width  = (int32_t) ceilf(x + bound.max.x) - (int32_t) bitmap_x;
                int32_t height = (int32_t) ceilf(y - bound.min.y) - (int32_t) bitmap_y;

                uv = _cui_glyph_cache_allocate_texture(ctx->glyph_cache, width, height, ctx->command_buffer);

                CuiBitmap bitmap;
                bitmap.width  = cui_rect_get_width(uv);
                bitmap.height = cui_rect_get_height(uv);
                bitmap.stride = ctx->glyph_cache->texture.stride;
                bitmap.pixels = (uint8_t *) ctx->glyph_cache->texture.pixels + (uv.min.y * bitmap.stride) + (uv.min.x * 4);

                for (int32_t layer_index = 0; layer_index < cui_array_count(layers); layer_index += 1)
                {
                    CuiColoredGlyphLayer *layer = layers + layer_index;

                    CuiTemporaryMemory draw_temp_memory = cui_begin_temporary_memory(ctx->temporary_memory);

                    CuiPathCommand *outline = 0;
                    cui_array_init(outline, 16, ctx->temporary_memory);

                    CuiTransform transform = { 0 };
                    transform.m[0] = used_font->font_scale;
                    transform.m[3] = -used_font->font_scale;
                    transform.m[4] = offset_x - bound.min.x;
                    transform.m[5] = offset_y + bound.max.y;

                    _cui_font_file_get_glyph_outline(used_font_file, &outline, layer->glyph_index, transform, ctx->temporary_memory);

                    CuiEdge *edge_list = 0;
                    cui_array_init(edge_list, 16, ctx->temporary_memory);

                    _cui_path_to_edge_list(outline, &edge_list);
                    _cui_edge_list_fill(ctx->temporary_memory, &bitmap, edge_list, layer->color);

                    cui_end_temporary_memory(draw_temp_memory);
                }

                _cui_glyph_cache_put(ctx->glyph_cache, used_font->file_id.value, utf8.codepoint, used_font->font_scale, offset_x, offset_y, uv);
            }

            CuiRect draw_rect;
            draw_rect.min = cui_make_point((int32_t) bitmap_x, (int32_t) bitmap_y);
            draw_rect.max = cui_point_add(draw_rect.min, cui_point_sub(uv.max, uv.min));

            if (cui_rect_overlap(ctx->clip_rect, draw_rect))
            {
                _cui_push_textured_rect(ctx->command_buffer, draw_rect, uv, glyph_color, ctx->glyph_cache->texture_id, ctx->clip_rect_offset);
            }
        }

        cui_end_temporary_memory(temp_memory);

        x += used_font->font_scale * (float) _cui_font_file_get_glyph_advance(used_font_file, glyph_index);

        // prev_glyph_index = glyph_index;
        // prev_used_font = used_font;
        index += utf8.byte_count;
    }
}

#include "cui_shapes.c"

void
cui_draw_fill_icon(CuiGraphicsContext *ctx, float x, float y, float scale, CuiIconType icon_type, CuiColor color)
{
    switch (icon_type)
    {
        case CUI_ICON_NONE: break;

        case CUI_ICON_WINDOWS_MINIMIZE:
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

            if (!_cui_glyph_cache_find(ctx->glyph_cache, 0, CUI_SHAPE_WINDOWS_MINIMIZE, scale, offset_x, offset_y, &uv))
            {
                CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(ctx->temporary_memory);

                int32_t width  = (int32_t) ceilf(scale * 16.0f);
                int32_t height = (int32_t) ceilf(scale * 16.0f);

                uv = _cui_glyph_cache_allocate_texture(ctx->glyph_cache, width, height, ctx->command_buffer);

                CuiBitmap bitmap;
                bitmap.width = cui_rect_get_width(uv);
                bitmap.height = cui_rect_get_height(uv);
                bitmap.stride = ctx->glyph_cache->texture.stride;
                bitmap.pixels = (uint8_t *) ctx->glyph_cache->texture.pixels + (uv.min.y * bitmap.stride) + (uv.min.x * 4);

                CuiPathCommand *path = 0;
                cui_array_init(path, 16, ctx->temporary_memory);

                float x0 = roundf(scale * 1.0f);
                float y0 = roundf(scale * 6.0f);
                float x1 = x0 + roundf(scale * 10.0f);
                float line_width = floorf(scale * 1.0f);
                float y1 = y0 + line_width;

                cui_path_move_to(&path, x0, y0);
                cui_path_line_to(&path, x1, y0);
                cui_path_line_to(&path, x1, y1);
                cui_path_line_to(&path, x0, y1);
                cui_path_close(&path);

                CuiEdge *edge_list = 0;
                cui_array_init(edge_list, 16, ctx->temporary_memory);

                _cui_path_to_edge_list(path, &edge_list);
                _cui_edge_list_fill(ctx->temporary_memory, &bitmap, edge_list, cui_make_color(1.0f, 1.0f, 1.0f, 1.0f));

                _cui_glyph_cache_put(ctx->glyph_cache, 0, CUI_SHAPE_WINDOWS_MINIMIZE, scale, offset_x, offset_y, uv);

                cui_end_temporary_memory(temp_memory);
            }

            CuiRect rect;
            rect.min.x = (int32_t) bitmap_x;
            rect.min.y = (int32_t) bitmap_y;
            rect.max.x = rect.min.x + cui_rect_get_width(uv);
            rect.max.y = rect.min.y + cui_rect_get_height(uv);

            if (cui_rect_overlap(ctx->clip_rect, rect))
            {
                _cui_push_textured_rect(ctx->command_buffer, rect, uv, color, ctx->glyph_cache->texture_id, ctx->clip_rect_offset);
            }
        } break;

        case CUI_ICON_WINDOWS_MAXIMIZE:
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

            if (!_cui_glyph_cache_find(ctx->glyph_cache, 0, CUI_SHAPE_WINDOWS_MAXIMIZE, scale, offset_x, offset_y, &uv))
            {
                CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(ctx->temporary_memory);

                int32_t width  = (int32_t) ceilf(scale * 16.0f);
                int32_t height = (int32_t) ceilf(scale * 16.0f);

                uv = _cui_glyph_cache_allocate_texture(ctx->glyph_cache, width, height, ctx->command_buffer);

                CuiBitmap bitmap;
                bitmap.width = cui_rect_get_width(uv);
                bitmap.height = cui_rect_get_height(uv);
                bitmap.stride = ctx->glyph_cache->texture.stride;
                bitmap.pixels = (uint8_t *) ctx->glyph_cache->texture.pixels + (uv.min.y * bitmap.stride) + (uv.min.x * 4);

                CuiPathCommand *path = 0;
                cui_array_init(path, 16, ctx->temporary_memory);

                float x0 = roundf(scale * 1.0f);
                float y0 = x0;
                float x1 = x0 + roundf(scale * 10.0f);
                float y1 = x1;

                cui_path_move_to(&path, x0, y0);
                cui_path_line_to(&path, x1, y0);
                cui_path_line_to(&path, x1, y1);
                cui_path_line_to(&path, x0, y1);
                cui_path_close(&path);

                float line_width = floorf(scale * 1.0f);

                x0 += line_width;
                y0 += line_width;
                x1 -= line_width;
                y1 -= line_width;

                cui_path_move_to(&path, x0, y0);
                cui_path_line_to(&path, x0, y1);
                cui_path_line_to(&path, x1, y1);
                cui_path_line_to(&path, x1, y0);
                cui_path_close(&path);

                CuiEdge *edge_list = 0;
                cui_array_init(edge_list, 16, ctx->temporary_memory);

                _cui_path_to_edge_list(path, &edge_list);
                _cui_edge_list_fill(ctx->temporary_memory, &bitmap, edge_list, cui_make_color(1.0f, 1.0f, 1.0f, 1.0f));

                _cui_glyph_cache_put(ctx->glyph_cache, 0, CUI_SHAPE_WINDOWS_MAXIMIZE, scale, offset_x, offset_y, uv);

                cui_end_temporary_memory(temp_memory);
            }

            CuiRect rect;
            rect.min.x = (int32_t) bitmap_x;
            rect.min.y = (int32_t) bitmap_y;
            rect.max.x = rect.min.x + cui_rect_get_width(uv);
            rect.max.y = rect.min.y + cui_rect_get_height(uv);

            if (cui_rect_overlap(ctx->clip_rect, rect))
            {
                _cui_push_textured_rect(ctx->command_buffer, rect, uv, color, ctx->glyph_cache->texture_id, ctx->clip_rect_offset);
            }
        } break;

        case CUI_ICON_WINDOWS_CLOSE:
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

            if (!_cui_glyph_cache_find(ctx->glyph_cache, 0, CUI_SHAPE_WINDOWS_CLOSE, scale, offset_x, offset_y, &uv))
            {
                CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(ctx->temporary_memory);

                int32_t width  = (int32_t) ceilf(scale * 16.0f);
                int32_t height = (int32_t) ceilf(scale * 16.0f);

                uv = _cui_glyph_cache_allocate_texture(ctx->glyph_cache, width, height, ctx->command_buffer);

                CuiBitmap bitmap;
                bitmap.width = cui_rect_get_width(uv);
                bitmap.height = cui_rect_get_height(uv);
                bitmap.stride = ctx->glyph_cache->texture.stride;
                bitmap.pixels = (uint8_t *) ctx->glyph_cache->texture.pixels + (uv.min.y * bitmap.stride) + (uv.min.x * 4);

                CuiPathCommand *path = 0;
                cui_array_init(path, 16, ctx->temporary_memory);

                float x0 = roundf(scale * 1.0f);
                float y0 = x0;
                float x1 = x0 + roundf(scale * 10.0f);
                float y1 = x1;
                float cx = 0.5f * (x0 + x1);
                float cy = cx;

                float line_width = floorf(scale * 1.0f);
                float offset = sqrtf(0.5f * (line_width * line_width));

                cui_path_move_to(&path, x0, y0);
                cui_path_line_to(&path, x0 + offset, y0);
                cui_path_line_to(&path, cx, cy - offset);
                cui_path_line_to(&path, x1 - offset, y0);
                cui_path_line_to(&path, x1, y0);
                cui_path_line_to(&path, x1, y0 + offset);
                cui_path_line_to(&path, cx + offset, cy);
                cui_path_line_to(&path, x1, y1 - offset);
                cui_path_line_to(&path, x1, y1);
                cui_path_line_to(&path, x1 - offset, y1);
                cui_path_line_to(&path, cx, cy + offset);
                cui_path_line_to(&path, x0 + offset, y1);
                cui_path_line_to(&path, x0, y1);
                cui_path_line_to(&path, x0, y1 - offset);
                cui_path_line_to(&path, cx - offset, cy);
                cui_path_line_to(&path, x0, y0 + offset);
                cui_path_close(&path);

                CuiEdge *edge_list = 0;
                cui_array_init(edge_list, 16, ctx->temporary_memory);

                _cui_path_to_edge_list(path, &edge_list);
                _cui_edge_list_fill(ctx->temporary_memory, &bitmap, edge_list, cui_make_color(1.0f, 1.0f, 1.0f, 1.0f));

                _cui_glyph_cache_put(ctx->glyph_cache, 0, CUI_SHAPE_WINDOWS_CLOSE, scale, offset_x, offset_y, uv);

                cui_end_temporary_memory(temp_memory);
            }

            CuiRect rect;
            rect.min.x = (int32_t) bitmap_x;
            rect.min.y = (int32_t) bitmap_y;
            rect.max.x = rect.min.x + cui_rect_get_width(uv);
            rect.max.y = rect.min.y + cui_rect_get_height(uv);

            if (cui_rect_overlap(ctx->clip_rect, rect))
            {
                _cui_push_textured_rect(ctx->command_buffer, rect, uv, color, ctx->glyph_cache->texture_id, ctx->clip_rect_offset);
            }
        } break;

        case CUI_ICON_ANGLE_UP_12:
        {
            cui_draw_fill_shape(ctx, x, y, CUI_SHAPE_ANGLE_UP_12, scale, color);
        } break;

        case CUI_ICON_ANGLE_RIGHT_12:
        {
            cui_draw_fill_shape(ctx, x, y, CUI_SHAPE_ANGLE_RIGHT_12, scale, color);
        } break;

        case CUI_ICON_ANGLE_DOWN_12:
        {
            cui_draw_fill_shape(ctx, x, y, CUI_SHAPE_ANGLE_DOWN_12, scale, color);
        } break;

        case CUI_ICON_ANGLE_LEFT_12:
        {
            cui_draw_fill_shape(ctx, x, y, CUI_SHAPE_ANGLE_LEFT_12, scale, color);
        } break;

        case CUI_ICON_INFO_12:
        {
            cui_draw_fill_shape(ctx, x, y, CUI_SHAPE_INFO_12, scale, color);
        } break;

        case CUI_ICON_EXPAND_12:
        {
            cui_draw_fill_shape(ctx, x, y, CUI_SHAPE_EXPAND_12, scale, color);
        } break;

        case CUI_ICON_SEARCH_12:
        {
            cui_draw_fill_shape(ctx, x, y, CUI_SHAPE_SEARCH_12, scale, color);
        } break;

        case CUI_ICON_UPPERCASE_A_12:
        {
            cui_draw_fill_shape(ctx, x, y, CUI_SHAPE_UPPERCASE_A_12, scale, color);
        } break;

        case CUI_ICON_UPPERCASE_B_12:
        {
            cui_draw_fill_shape(ctx, x, y, CUI_SHAPE_UPPERCASE_B_12, scale, color);
        } break;

        case CUI_ICON_UPPERCASE_G_12:
        {
            cui_draw_fill_shape(ctx, x, y, CUI_SHAPE_UPPERCASE_G_12, scale, color);
        } break;

        case CUI_ICON_UPPERCASE_H_12:
        {
            cui_draw_fill_shape(ctx, x, y, CUI_SHAPE_UPPERCASE_H_12, scale, color);
        } break;

        case CUI_ICON_UPPERCASE_L_12:
        {
            cui_draw_fill_shape(ctx, x, y, CUI_SHAPE_UPPERCASE_L_12, scale, color);
        } break;

        case CUI_ICON_UPPERCASE_R_12:
        {
            cui_draw_fill_shape(ctx, x, y, CUI_SHAPE_UPPERCASE_R_12, scale, color);
        } break;

        case CUI_ICON_UPPERCASE_S_12:
        {
            cui_draw_fill_shape(ctx, x, y, CUI_SHAPE_UPPERCASE_S_12, scale, color);
        } break;

        case CUI_ICON_UPPERCASE_V_12:
        {
            cui_draw_fill_shape(ctx, x, y, CUI_SHAPE_UPPERCASE_V_12, scale, color);
        } break;

        case CUI_ICON_PLUS_12:
        {
            cui_draw_fill_shape(ctx, x, y, CUI_SHAPE_PLUS_12, scale, color);
        } break;
    }
}

void cui_bitmap_clear(CuiBitmap *bitmap, CuiColor clear_color)
{
    CuiAssert(((intptr_t) bitmap->pixels & 15) == 0);

    uint32_t u32_color = cui_color_pack_bgra(clear_color);

    if (!(bitmap->stride & 15))
    {
#if CUI_ARCH_ARM64
        uint32x4_t wide_color = vmovq_n_u32(u32_color);
#elif CUI_ARCH_X86_64
        __m128 wide_color = _mm_castsi128_ps(_mm_set1_epi32(u32_color));
#else
        uint64_t wide_color = ((uint64_t) u32_color << 32) | (uint64_t) u32_color;
#endif

        uint8_t *row = (uint8_t *) bitmap->pixels;

        for (int32_t y = 0; y < bitmap->height; y += 1)
        {
            uint64_t *pixel = (uint64_t *) row;

            for (int32_t x = 0; x < bitmap->width; x += 4)
            {
#if CUI_ARCH_ARM64
                vst1q_u32((uint32_t *) pixel, wide_color);
#elif CUI_ARCH_X86_64
                _mm_stream_ps((float *) pixel, wide_color);
#else
                *(pixel + 0) = wide_color;
                *(pixel + 1) = wide_color;
#endif

                pixel += 2;
            }

            row += bitmap->stride;
        }
    }
    else
    {
        uint8_t *row = (uint8_t *) bitmap->pixels;

        for (int32_t y = 0; y < bitmap->height; y += 1)
        {
            uint32_t *pixel = (uint32_t *) row;

            for (int32_t x = 0; x < bitmap->width; x += 1)
            {
                *pixel++ = u32_color;
            }

            row += bitmap->stride;
        }
    }
}
