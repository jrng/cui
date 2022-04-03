static inline void
cui_path_move_to(CuiPathCommand **path, float x, float y)
{
    CuiPathCommand *command = cui_array_append(*path);

    command->type = CUI_PATH_COMMAND_MOVE_TO;
    command->x = x;
    command->y = y;
}

static inline void
cui_path_line_to(CuiPathCommand **path, float x, float y)
{
    CuiPathCommand *command = cui_array_append(*path);

    command->type = CUI_PATH_COMMAND_LINE_TO;
    command->x = x;
    command->y = y;
}

static inline void
cui_path_quadratic_curve_to(CuiPathCommand **path, float cx, float cy, float x, float y)
{
    CuiPathCommand *command = cui_array_append(*path);

    command->type = CUI_PATH_COMMAND_QUADRATIC_CURVE_TO;
    command->x = x;
    command->y = y;
    command->cx1 = cx;
    command->cy1 = cy;
}

static inline void
cui_path_cubic_curve_to(CuiPathCommand **path, float cx1, float cy1, float cx2, float cy2, float x, float y)
{
    CuiPathCommand *command = cui_array_append(*path);

    command->type = CUI_PATH_COMMAND_CUBIC_CURVE_TO;
    command->x = x;
    command->y = y;
    command->cx1 = cx1;
    command->cy1 = cy1;
    command->cx2 = cx2;
    command->cy2 = cy2;
}

static inline void
cui_path_arc_to(CuiPathCommand **path, float rx, float ry, float x_rotation, bool large_arc_flag, bool sweep_flag, float x, float y)
{
    CuiPathCommand *command = cui_array_append(*path);

    command->type = CUI_PATH_COMMAND_ARC_TO;
    command->x = x;
    command->y = y;
    command->cx1 = rx;
    command->cy1 = ry;
    command->cx2 = x_rotation;
    command->cy2 = 0.0f;
    command->large_arc_flag = large_arc_flag;
    command->sweep_flag = sweep_flag;
}

static inline void
cui_path_close(CuiPathCommand **path)
{
    CuiPathCommand *command = cui_array_append(*path);
    command->type = CUI_PATH_COMMAND_CLOSE_PATH;
}

static inline void
_cui_close(CuiPathCommand **outline, float last_cx, float last_cy, float start_x, float start_y,
           float start_cx, float start_cy, bool was_off_point, bool start_with_off_point)
{
    if (was_off_point)
    {
        if (start_with_off_point)
        {
            float mx = 0.5f * (last_cx + start_cx);
            float my = 0.5f * (last_cy + start_cy);

            cui_path_quadratic_curve_to(outline, last_cx, last_cy, mx, my);
            cui_path_quadratic_curve_to(outline, start_cx, start_cy, start_x, start_y);
        }
        else
        {
            cui_path_quadratic_curve_to(outline, last_cx, last_cy, start_x, start_y);
        }
    }
    else
    {
        if (start_with_off_point)
        {
            cui_path_quadratic_curve_to(outline, start_cx, start_cy, start_x, start_y);
        }
        else
        {
            cui_path_line_to(outline, start_x, start_y);
        }
    }
}

static void
_cui_approximate_quadratic_curve(CuiEdge **edge_list, float x0, float y0, float x1, float y1, float x2, float y2, uint32_t n)
{
    float mx = (x0 + 2.0f * x1 + x2) * 0.25f;
    float my = (y0 + 2.0f * y1 + y2) * 0.25f;

    float dx = 0.5f * (x0 + x2) - mx;
    float dy = 0.5f * (y0 + y2) - my;

    if ((n > 0) && (((dx * dx) + (dy * dy)) > (0.125f * 0.125f)))
    {
        _cui_approximate_quadratic_curve(edge_list, x0, y0, (x0 + x1) * 0.5f, (y0 + y1) * 0.5f, mx, my, n - 1);
        _cui_approximate_quadratic_curve(edge_list, mx, my, (x1 + x2) * 0.5f, (y1 + y2) * 0.5f, x2, y2, n - 1);
    }
    else
    {
        if (y2 > y0)
        {
            CuiEdge *edge = cui_array_append(*edge_list);
            edge->positive = true;
            edge->x0 = x0;
            edge->y0 = y0;
            edge->x1 = x2;
            edge->y1 = y2;
        }
        else if (y0 > y2)
        {
            CuiEdge *edge = cui_array_append(*edge_list);
            edge->positive = false;
            edge->x0 = x2;
            edge->y0 = y2;
            edge->x1 = x0;
            edge->y1 = y0;
        }
    }
}

static void
_cui_approximate_cubic_curve(CuiEdge **edge_list, float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, uint32_t n)
{
    float mx = (x0 + 3.0f * x1 + 3.0f * x2 + x3) * 0.125f;
    float my = (y0 + 3.0f * y1 + 3.0f * y2 + y3) * 0.125f;

    float dx = 0.5f * (x0 + x3) - mx;
    float dy = 0.5f * (y0 + y3) - my;

    if ((n > 0) && (((dx * dx) + (dy * dy)) > (0.125f * 0.125f)))
    {
        _cui_approximate_cubic_curve(edge_list, x0, y0, (x0 + x1) * 0.5f, (y0 + y1) * 0.5f,
                                     (x0 + 2.0f * x1 + x2) * 0.25f, (y0 + 2.0f * y1 + y2) * 0.25f, mx, my, n - 1);
        _cui_approximate_cubic_curve(edge_list, mx, my, (x1 + 2.0f * x2 + x3) * 0.25f, (y1 + 2.0f * y2 + y3) * 0.25f,
                                     (x2 + x3) * 0.5f, (y2 + y3) * 0.5f, x3, y3, n - 1);
    }
    else
    {
        if (y3 > y0)
        {
            CuiEdge *edge = cui_array_append(*edge_list);
            edge->positive = true;
            edge->x0 = x0;
            edge->y0 = y0;
            edge->x1 = x3;
            edge->y1 = y3;
        }
        else if (y0 > y3)
        {
            CuiEdge *edge = cui_array_append(*edge_list);
            edge->positive = false;
            edge->x0 = x3;
            edge->y0 = y3;
            edge->x1 = x0;
            edge->y1 = y0;
        }
    }
}

static void
approximate_arc(CuiEdge **edge_list, float cx, float cy, float rx, float ry, float phi_start, float phi_delta, uint32_t n)
{
    if (n > 0)
    {
        float phi_mid = 0.5f * phi_delta;
        approximate_arc(edge_list, cx, cy, rx, ry, phi_start, phi_mid, n - 1);
        approximate_arc(edge_list, cx, cy, rx, ry, phi_start + phi_mid, phi_mid, n - 1);
    }
    else
    {
        float x0 = cx + rx * cos(phi_start);
        float y0 = cy + ry * sin(phi_start);
        float x1 = cx + rx * cos(phi_start + phi_delta);
        float y1 = cy + ry * sin(phi_start + phi_delta);

        if (y1 > y0)
        {
            CuiEdge *edge = cui_array_append(*edge_list);
            edge->positive = true;
            edge->x0 = x0;
            edge->y0 = y0;
            edge->x1 = x1;
            edge->y1 = y1;
        }
        else if (y0 > y1)
        {
            CuiEdge *edge = cui_array_append(*edge_list);
            edge->positive = false;
            edge->x0 = x1;
            edge->y0 = y1;
            edge->x1 = x0;
            edge->y1 = y0;
        }
    }
}

static inline float
angle_between(float x0, float y0, float x1, float y1)
{
    return copysign(acos(x0 * x1 + y0 * y1), (x0 * y1) - (y0 * x1));
}

#define PI_F32  3.141592654f
#define PI2_F32 6.283185307f

static void
_cui_approximate_arc(CuiEdge **edge_list, float x0, float y0, float rx, float ry, float x_rotation,
                     bool large_arc_flag, bool sweep_flag, float x1, float y1, uint32_t n)
{
    float phi_sin = sinf(x_rotation);
    float phi_cos = cosf(x_rotation);

    float rx_squared = rx * rx;
    float ry_squared = ry * ry;

    float x_half = 0.5f * (x0 - x1);
    float y_half = 0.5f * (y0 - y1);

    float x_center = 0.5f * (x0 + x1);
    float y_center = 0.5f * (y0 + y1);

    float x_r = phi_cos * x_half + phi_sin * y_half;
    float y_r = phi_cos * y_half - phi_sin * x_half;

    float factor = sqrtf(((rx_squared * ry_squared) - (rx_squared * y_r * y_r) - (ry_squared * x_r * x_r)) /
                         ((rx_squared * y_r * y_r) + (ry_squared * x_r * x_r)));

    if (large_arc_flag == sweep_flag)
    {
        factor = -factor;
    }

    float cx = factor * ((rx * y_r) / ry);
    float cy = factor * (-(ry * x_r) / rx);

    float center_x = phi_cos * cx - phi_sin * cy + x_center;
    float center_y = phi_cos * cy + phi_sin * cx + y_center;

    float phi_start = angle_between(1.0f, 0.0f, (x0 - center_x) / rx, (y0 - center_y) / ry) - x_rotation;
    float phi_delta = angle_between((x0 - center_x) / rx, (y0 - center_y) / ry, (x1 - center_x) / rx, (y1 - center_y) / ry);

    if (sweep_flag && (phi_delta < 0.0f))
    {
        phi_delta += PI2_F32;
    }
    else if (!sweep_flag && (phi_delta > 0.0f))
    {
        phi_delta -= PI2_F32;
    }

    approximate_arc(edge_list, center_x, center_y, rx, ry, phi_start, phi_delta, n);
}

void
cui_path_to_edge_list(CuiPathCommand *path, CuiEdge **edge_list)
{
    CuiAssert(!cui_array_count(path) || (path[0].type == CUI_PATH_COMMAND_MOVE_TO));

    if (!cui_array_count(path))
    {
        return;
    }

    float start_x, x;
    float start_y, y;

    for (int32_t index = 0; index < cui_array_count(path); index += 1)
    {
        CuiPathCommand *command = path + index;

        switch (command->type)
        {
            case CUI_PATH_COMMAND_MOVE_TO:
            {
                start_x = command->x;
                start_y = command->y;
                x = start_x;
                y = start_y;
            } break;

            case CUI_PATH_COMMAND_LINE_TO:
            {
                if (command->y > y)
                {
                    CuiEdge *edge = cui_array_append(*edge_list);
                    edge->positive = true;
                    edge->x0 = x;
                    edge->y0 = y;
                    edge->x1 = command->x;
                    edge->y1 = command->y;
                }
                else if (y > command->y)
                {
                    CuiEdge *edge = cui_array_append(*edge_list);
                    edge->positive = false;
                    edge->x0 = command->x;
                    edge->y0 = command->y;
                    edge->x1 = x;
                    edge->y1 = y;
                }
            } break;

            case CUI_PATH_COMMAND_QUADRATIC_CURVE_TO:
            {
                _cui_approximate_quadratic_curve(edge_list, x, y, command->cx1, command->cy1,
                                                 command->x, command->y, 3);
            } break;

            case CUI_PATH_COMMAND_CUBIC_CURVE_TO:
            {
                _cui_approximate_cubic_curve(edge_list, x, y, command->cx1, command->cy1,
                                             command->cx2, command->cy2, command->x, command->y, 4);
            } break;

            case CUI_PATH_COMMAND_ARC_TO:
            {
                _cui_approximate_arc(edge_list, x, y, command->cx1, command->cy1, command->cx2,
                                     command->large_arc_flag, command->sweep_flag, command->x, command->y, 4);
            } break;

            case CUI_PATH_COMMAND_CLOSE_PATH:
            {
                if (start_y > y)
                {
                    CuiEdge *edge = cui_array_append(*edge_list);
                    edge->positive = true;
                    edge->x0 = x;
                    edge->y0 = y;
                    edge->x1 = start_x;
                    edge->y1 = start_y;
                }
                else if (y > start_y)
                {
                    CuiEdge *edge = cui_array_append(*edge_list);
                    edge->positive = false;
                    edge->x0 = start_x;
                    edge->y0 = start_y;
                    edge->x1 = x;
                    edge->y1 = y;
                }
            } break;
        }

        x = command->x;
        y = command->y;
    }
}

void
cui_edge_list_fill(CuiBitmap *buffer, CuiEdge *edge_list, CuiColor color)
{
    color.r *= color.a;
    color.g *= color.a;
    color.b *= color.a;

    float y_max = (float) buffer->height;
    float y_min = y_max - 1.0f;

    uint8_t *row = (uint8_t *) buffer->pixels;
    for (int32_t y = 0; y < buffer->height; y++)
    {
        uint32_t *pixel = (uint32_t *) row;
        for (int32_t x = 0; x < buffer->width; x++)
        {
            float x_min = (float) x;
            float x_max = x_min + 1.0f;

            float coverage = 0.0f;
            for (int32_t i = 0; i < cui_array_count(edge_list); i++)
            {
                CuiEdge edge = edge_list[i];

                if ((edge.y1 >= y_min) && (edge.y0 < y_max))
                {
                    float dx = (edge.x1 - edge.x0) / (edge.y1 - edge.y0);

                    if (edge.y0 < y_min)
                    {
                        edge.x0 += (y_min - edge.y0) * dx;
                        edge.y0 = y_min;
                    }

                    if (edge.y1 >= y_max)
                    {
                        edge.x1 += (y_max - edge.y1) * dx;
                        edge.y1 = y_max;
                    }

                    if ((edge.x0 >= x_min) || (edge.x1 >= x_min))
                    {
                        float area = 0.0f;
                        if ((edge.x0 >= x_max) && (edge.x1 >= x_max))
                        {
                            area = edge.y1 - edge.y0;
                        }
                        else
                        {
                            float dy = 0.0f;
                            if (dx != 0.0f)
                            {
                                dy = 1.0f / dx;
                            }

                            if (edge.x0 < x_min)
                            {
                                edge.y0 += (x_min - edge.x0) * dy;
                                edge.x0 = x_min;
                            }
                            else if (edge.x0 >= x_max)
                            {
                                float lower_bound = (edge.y0 > y_min) ? edge.y0 : y_min;
                                edge.y0 -= (edge.x0 - x_max) * dy;
                                edge.x0 = x_max;

                                area = edge.y0 - lower_bound;
                            }

                            if (edge.x1 < x_min)
                            {
                                edge.y1 += (x_min - edge.x1) * dy;
                                edge.x1 = x_min;
                            }
                            else if (edge.x1 >= x_max)
                            {
                                float upper_bound = (edge.y1 < y_max) ? edge.y1 : y_max;
                                edge.y1 -= (edge.x1 - x_max) * dy;
                                edge.x1 = x_max;

                                area = upper_bound - edge.y1;
                            }

                            float height = edge.y1 - edge.y0;
                            area += 0.5f * height * (edge.x0 + edge.x1 - 2 * x_min);
                        }

                        if (edge.positive)
                        {
                            coverage -= area;
                        }
                        else
                        {
                            coverage += area;
                        }
                    }
                }
            }

            if (coverage < 0.0f) coverage = -coverage;
            if (coverage > 1.0f) coverage = 1.0f;

            float src_a = color.a * coverage;
            float src_r = color.r * coverage;
            float src_g = color.g * coverage;
            float src_b = color.b * coverage;

            float dst_a = (float) ((*pixel >> 24) & 0xFF) / 255.0f;
            float dst_r = (float) ((*pixel >> 16) & 0xFF) / 255.0f;
            float dst_g = (float) ((*pixel >>  8) & 0xFF) / 255.0f;
            float dst_b = (float) ((*pixel >>  0) & 0xFF) / 255.0f;

            float final_a = src_a + dst_a * (1.0f - src_a);
            float final_r = src_r + dst_r * (1.0f - src_a);
            float final_g = src_g + dst_g * (1.0f - src_a);
            float final_b = src_b + dst_b * (1.0f - src_a);

            uint8_t a = (uint8_t) (final_a * 255.0f);
            uint8_t r = (uint8_t) (final_r * 255.0f);
            uint8_t g = (uint8_t) (final_g * 255.0f);
            uint8_t b = (uint8_t) (final_b * 255.0f);

            *pixel++ = (a << 24) | (r << 16) | (g << 8) | b;
        }

        y_min -= 1.0f;
        y_max -= 1.0f;
        row += buffer->stride;
    }
}

uint32_t
cui_font_file_get_glyph_index_from_codepoint(CuiFontFile *font_file, uint32_t codepoint)
{
    uint16_t format = cui_read_u16_be(font_file->mapping_table, 0);

#if 0
    printf("format = %u\n", format);
#endif

    switch (format)
    {
        case 0:
        {
            if (codepoint < 256)
            {
                return font_file->mapping_table[6 + codepoint];
            }
        } break;

        case 4:
        {
            uint16_t segment_count_2 = cui_read_u16_be(font_file->mapping_table, 6);

            uint8_t   *end_codepoints = font_file->mapping_table + 14;
            uint8_t *start_codepoints = end_codepoints + segment_count_2 + 2;
            uint8_t *id_deltas = start_codepoints + segment_count_2;
            uint8_t *id_range_offsets = id_deltas + segment_count_2;

            uint16_t segment_offset = 0;

            for (; segment_offset < segment_count_2; segment_offset += 2)
            {
                uint16_t end_codepoint = cui_read_u16_be(end_codepoints, segment_offset);

                if (codepoint <= end_codepoint)
                {
                    break;
                }
            }

            uint16_t start_codepoint = cui_read_u16_be(start_codepoints, segment_offset);

            if (codepoint >= start_codepoint)
            {
                uint16_t delta = cui_read_u16_be(id_deltas, segment_offset);
                uint16_t offset = cui_read_u16_be(id_range_offsets, segment_offset);

                if (offset)
                {
                    uint32_t glyph_index = cui_read_u16_be(id_range_offsets, segment_offset + offset + 2 * (codepoint - start_codepoint));

                    if (glyph_index)
                    {
                        return (glyph_index + delta) & 0xFFFF;
                    }
                }
                else
                {
                    return (codepoint + delta) & 0xFFFF;
                }
            }
        } break;

        case 12:
        {
            uint32_t a = 0, b = cui_read_u32_be(font_file->mapping_table, 12);

            while (a < b)
            {
                uint32_t mid = (a + b) / 2;
                uint32_t offset = 16 + 12 * mid;

                uint32_t start_codepoint = cui_read_u32_be(font_file->mapping_table, offset);
                uint32_t   end_codepoint = cui_read_u32_be(font_file->mapping_table, offset + 4);

                if (codepoint < start_codepoint)
                {
                    b = mid;
                }
                else if (codepoint > end_codepoint)
                {
                    a = mid + 1;
                }
                else
                {
                    uint32_t start_glyph_index = cui_read_u32_be(font_file->mapping_table, offset + 8);
                    return start_glyph_index + (codepoint - start_codepoint);
                }
            }
        } break;
    }

    return 0;
}

float
cui_font_file_get_scale_for_unit_height(CuiFontFile *font_file, float unit_height)
{
    return unit_height / (float) (font_file->ascent - font_file->descent);
}

uint16_t
cui_font_file_get_glyph_advance(CuiFontFile *font_file, uint32_t glyph_index)
{
    if (glyph_index >= font_file->hmetrics_count)
    {
        glyph_index = font_file->hmetrics_count - 1;
    }

    return cui_read_u16_be(font_file->hmtx, 4 * glyph_index);
}

CuiRect
cui_font_file_get_glyph_bounding_box(CuiFontFile *font_file, uint32_t glyph_index)
{
    CuiRect result = { { 0, 0 }, { 0, 0 } };

    if (glyph_index < font_file->glyph_count)
    {
        uint8_t *offset1, *offset2;

        if (font_file->loca_index_format == 0)
        {
            offset1 = font_file->glyf + 2 * (uint32_t) cui_read_u16_be(font_file->loca, 2 * (glyph_index + 0));
            offset2 = font_file->glyf + 2 * (uint32_t) cui_read_u16_be(font_file->loca, 2 * (glyph_index + 1));
        }
        else
        {
            offset1 = font_file->glyf + cui_read_u32_be(font_file->loca, 4 * (glyph_index + 0));
            offset2 = font_file->glyf + cui_read_u32_be(font_file->loca, 4 * (glyph_index + 1));
        }

        if (offset1 != offset2)
        {
            result.min.x = (int16_t) cui_read_u16_be(offset1, 2);
            result.min.y = (int16_t) cui_read_u16_be(offset1, 4);
            result.max.x = (int16_t) cui_read_u16_be(offset1, 6);
            result.max.y = (int16_t) cui_read_u16_be(offset1, 8);
        }
    }

    return result;
}

void
cui_font_file_get_glyph_outline(CuiFontFile *font_file, CuiPathCommand **outline, uint32_t glyph_index, CuiTransform transform, CuiArena *arena)
{
    if (glyph_index < font_file->glyph_count)
    {
        uint8_t *offset1, *offset2;

        if (font_file->loca_index_format == 0)
        {
            offset1 = font_file->glyf + 2 * (uint32_t) cui_read_u16_be(font_file->loca, 2 * (glyph_index + 0));
            offset2 = font_file->glyf + 2 * (uint32_t) cui_read_u16_be(font_file->loca, 2 * (glyph_index + 1));
        }
        else
        {
            offset1 = font_file->glyf + cui_read_u32_be(font_file->loca, 4 * (glyph_index + 0));
            offset2 = font_file->glyf + cui_read_u32_be(font_file->loca, 4 * (glyph_index + 1));
        }

        if (offset1 != offset2)
        {
            int16_t contour_count = (int16_t) cui_read_u16_be(offset1, 0);

            if (contour_count > 0)
            {
                uint16_t instruction_length = cui_read_u16_be(offset1, 10 + 2 * contour_count);
                uint16_t point_count = cui_read_u16_be(offset1, 10 + 2 * (contour_count - 1)) + 1;

                uint8_t *at = offset1 + 12 + (2 * contour_count) + instruction_length;

                CuiContourPoint *points = 0;
                cui_array_init(points, point_count, arena);

                int32_t x = 0, y = 0;
                uint8_t flags, repeat_count = 0;

                for (uint16_t i = 0; i < point_count; i++)
                {
                    if (repeat_count == 0)
                    {
                        flags = *at++;

                        if (flags & (1 << 3))
                        {
                            repeat_count = *at++;
                        }
                    }
                    else
                    {
                        repeat_count--;
                    }

                    points[i].flags = flags;
                }

                for (uint16_t i = 0; i < point_count; i++)
                {
                    flags = points[i].flags;

                    if (flags & (1 << 1))
                    {
                        int16_t dx = *at++;
                        x += (flags & (1 << 4)) ? dx : -dx;
                    }
                    else
                    {
                        if (!(flags & (1 << 4)))
                        {
                            // TODO: use cui_read_u16_be
                            x += at[0] * 256 + at[1];
                            at += 2;
                        }
                    }

                    points[i].x = (float) (int16_t) x;
                }

                for (uint16_t i = 0; i < point_count; i++)
                {
                    flags = points[i].flags;

                    if (flags & (1 << 2))
                    {
                        int16_t dy = *at++;
                        y += (flags & (1 << 5)) ? dy : -dy;
                    }
                    else
                    {
                        if (!(flags & (1 << 5)))
                        {
                            // TODO: use cui_read_u16_be
                            y += at[0] * 256 + at[1];
                            at += 2;
                        }
                    }

                    float px = points[i].x;
                    float py = (float) (int16_t) y;

                    points[i].x = transform.m[0] * px + transform.m[2] * py + transform.m[4];
                    points[i].y = transform.m[1] * px + transform.m[3] * py + transform.m[5];
                }

                uint16_t next_move = 0;
                uint16_t contour_index = 0;

                float last_cx, last_cy;
                float start_x, start_y;
                float start_cx, start_cy;

                bool was_off_point = false;
                bool start_with_off_point = false;

                for (uint16_t i = 0; i < point_count; i++)
                {
                    uint8_t flags = points[i].flags;
                    float x       = points[i].x;
                    float y       = points[i].y;

                    if (i == next_move)
                    {
                        if (i > 0)
                        {
                            _cui_close(outline, last_cx, last_cy, start_x, start_y, start_cx, start_cy,
                                       was_off_point, start_with_off_point);
                        }

                        start_with_off_point = !(flags & (1 << 0));

                        if (start_with_off_point)
                        {
                            start_cx = x;
                            start_cy = y;

                            if (points[i+1].flags & (1 << 0))
                            {
                                i++;
                                start_x = points[i].x;
                                start_y = points[i].y;
                            }
                            else
                            {
                                start_x = 0.5f * (x + points[i+1].x);
                                start_y = 0.5f * (y + points[i+1].y);
                            }
                        }
                        else
                        {
                            start_x = x;
                            start_y = y;
                        }

                        cui_path_move_to(outline, start_x, start_y);
                        was_off_point = false;

                        next_move = 1 + cui_read_u16_be(offset1, 10 + contour_index * 2);
                        contour_index++;
                    }
                    else
                    {
                        if (flags & (1 << 0))
                        {
                            if (was_off_point)
                            {
                                cui_path_quadratic_curve_to(outline, last_cx, last_cy, x, y);
                            }
                            else
                            {
                                cui_path_line_to(outline, x, y);
                            }

                            was_off_point = false;
                        }
                        else
                        {
                            if (was_off_point)
                            {
                                float mx = 0.5f * (last_cx + x);
                                float my = 0.5f * (last_cy + y);

                                cui_path_quadratic_curve_to(outline, last_cx, last_cy, mx, my);
                            }

                            last_cx = x;
                            last_cy = y;

                            was_off_point = true;
                        }
                    }
                }

                _cui_close(outline, last_cx, last_cy, start_x, start_y, start_cx, start_cy,
                           was_off_point, start_with_off_point);
            }
            else if (contour_count < 0)
            {
                uint16_t flags = (1 << 5);
                uint8_t *component = offset1 + 10;

                CuiTransform trans;
                float a, b, c, d, e, f;

                while (flags & (1 << 5))
                {
                    flags = cui_read_u16_be(component, 0);
                    uint16_t glyph_index = cui_read_u16_be(component, 2);

                    if (flags & (1 << 1))
                    {
                        if (flags & (1 << 0))
                        {
                            e = (float) (int16_t) cui_read_u16_be(component, 4);
                            f = (float) (int16_t) cui_read_u16_be(component, 6);
                            component += 8;
                        }
                        else
                        {
                            e = (float) (int8_t) *(component + 4);
                            f = (float) (int8_t) *(component + 5);
                            component += 6;
                        }
                    }
                    else
                    {
                        CuiAssert(!"unsupported matching points");
                    }

                    if (flags & (1 << 3))
                    {
                        a = d = (float) (int16_t) cui_read_u16_be(component, 0) / 16384.0f;
                        b = c = 0.0f;
                        component += 2;
                    }
                    else if (flags & (1 << 6))
                    {
                        a = (float) (int16_t) cui_read_u16_be(component, 0) / 16384.0f;
                        d = (float) (int16_t) cui_read_u16_be(component, 2) / 16384.0f;
                        b = c = 0.0f;
                        component += 4;
                    }
                    else if (flags & (1 << 7))
                    {
                        a = (float) (int16_t) cui_read_u16_be(component, 0) / 16384.0f;
                        b = (float) (int16_t) cui_read_u16_be(component, 2) / 16384.0f;
                        c = (float) (int16_t) cui_read_u16_be(component, 4) / 16384.0f;
                        d = (float) (int16_t) cui_read_u16_be(component, 6) / 16384.0f;
                        component += 8;
                    }
                    else
                    {
                        a = d = 1.0f;
                        b = c = 0.0f;
                    }

                    float abs_a = cui_abs_float(a);
                    float abs_b = cui_abs_float(b);
                    float abs_c = cui_abs_float(c);
                    float abs_d = cui_abs_float(d);

                    float m = cui_max_float(abs_a, abs_b);
                    float n = cui_max_float(abs_c, abs_d);

                    if (cui_abs_float(abs_a - abs_c) <= (33.0f / 65536.0f))
                    {
                        m *= 2.0f;
                    }

                    if (cui_abs_float(abs_b - abs_d) <= (33.0f / 65536.0f))
                    {
                        m *= 2.0f;
                    }

                    e *= m;
                    f *= n;

                    trans.m[0] = transform.m[0] * a + transform.m[2] * b;
                    trans.m[1] = transform.m[1] * a + transform.m[3] * b;
                    trans.m[2] = transform.m[0] * c + transform.m[2] * d;
                    trans.m[3] = transform.m[1] * c + transform.m[3] * d;
                    trans.m[4] = transform.m[0] * e + transform.m[2] * f + transform.m[4];
                    trans.m[5] = transform.m[1] * e + transform.m[3] * f + transform.m[5];

                    cui_font_file_get_glyph_outline(font_file, outline, glyph_index, trans, arena);
                }
            }
        }
    }
}

bool
cui_font_file_init(CuiFontFile *font_file, void *data, int64_t count)
{
    CuiClearToZero(*font_file);

    font_file->contents.count = count;
    font_file->contents.data  = (uint8_t *) data;

    uint8_t *head = 0;
    uint8_t *hhea = 0;
    uint8_t *maxp = 0;

    int64_t table_offset = 12;
    uint16_t table_count = cui_read_u16_be(font_file->contents.data, 4);

#if 0
    printf("cmap = 0x%08X\n", 'cmap');
    printf("glyf = 0x%08X\n", 'glyf');
    printf("head = 0x%08X\n", 'head');
    printf("hhea = 0x%08X\n", 'hhea');
    printf("hmtx = 0x%08X\n", 'hmtx');
    printf("loca = 0x%08X\n", 'loca');
    printf("maxp = 0x%08X\n", 'maxp');
#endif

    for (uint16_t table_index = 0; table_index < table_count; table_index += 1)
    {
        uint32_t tag    = cui_read_u32_be(font_file->contents.data, table_offset +  0);
        uint32_t offset = cui_read_u32_be(font_file->contents.data, table_offset +  8);

        switch (tag)
        {
            case 0x636D6170: // cmap
            {
                font_file->cmap = font_file->contents.data + offset;
            } break;

            case 0x676C7966: // glyf
            {
                font_file->glyf = font_file->contents.data + offset;
            } break;

            case 0x68656164: // head
            {
                head = font_file->contents.data + offset;
            } break;

            case 0x68686561: // hhea
            {
                hhea = font_file->contents.data + offset;
            } break;

            case 0x686D7478: // hmtx
            {
                font_file->hmtx = font_file->contents.data + offset;
            } break;

            case 0x6C6F6361: // loca
            {
                font_file->loca = font_file->contents.data + offset;
            } break;

            case 0x6D617870: // maxp
            {
                maxp = font_file->contents.data + offset;
            } break;
        }

        table_offset += 16;
    }

    if (!font_file->cmap || !font_file->glyf || !head || !hhea || !font_file->hmtx || !font_file->loca || !maxp)
    {
        return false;
    }

    font_file->loca_index_format = cui_read_u16_be(head, 50);

    if (font_file->loca_index_format > 1)
    {
        return false;
    }

    font_file->ascent   = (int16_t) cui_read_u16_be(hhea, 4);
    font_file->descent  = (int16_t) cui_read_u16_be(hhea, 6);
    font_file->line_gap = (int16_t) cui_read_u16_be(hhea, 8);
    font_file->hmetrics_count = cui_read_u16_be(hhea, 34);

    font_file->glyph_count = cui_read_u16_be(maxp, 4);

    table_offset = 4;
    table_count = cui_read_u16_be(font_file->cmap, 2);

#if 0
    printf("cmap sub table count = %u\n", table_count);
#endif

    for (uint16_t table_index = 0; table_index < table_count; table_index += 1)
    {
        uint16_t platform_id = cui_read_u16_be(font_file->cmap, table_offset);
        uint16_t platform_specific_id = cui_read_u16_be(font_file->cmap, table_offset + 2);

#if 0
        switch (platform_id)
        {
            case 0:
            {
                printf("  Unicode - ");

                switch (platform_specific_id)
                {
                    case 0:
                    {
                        printf("Version 1.0 semantics\n");
                    } break;

                    case 1:
                    {
                        printf("Version 1.1 semantics\n");
                    } break;

                    case 2:
                    {
                        printf("ISO 10646 1993 semantics\n");
                    } break;

                    case 3:
                    {
                        printf("Unicode 2.0 or later semantics (BMP only)\n");
                    } break;

                    case 4:
                    {
                        printf("Unicode 2.0 or later semantics (non-BMP characters allowed)\n");
                    } break;

                    case 5:
                    {
                        printf("Unicode Variation Sequences\n");
                    } break;

                    case 6:
                    {
                        printf("Last Resort\n");
                    } break;

                    default:
                    {
                        printf("<unknown>\n");
                    } break;
                }
            } break;

            case 1:
            {
                printf("  Macintosh\n");
            } break;

            case 2:
            {
                printf("  (reserved)\n");
            } break;

            case 3:
            {
                printf("  Microsoft - ");

                switch (platform_specific_id)
                {
                    case 0:
                    {
                        printf("Symbol\n");
                    } break;

                    case 1:
                    {
                        printf("Unicode BMP-only (UCS-2)\n");
                    } break;

                    case 2:
                    {
                        printf("Shift-JIS\n");
                    } break;

                    case 3:
                    {
                        printf("PRC\n");
                    } break;

                    case 4:
                    {
                        printf("BigFive\n");
                    } break;

                    case 5:
                    {
                        printf("Johab\n");
                    } break;

                    case 10:
                    {
                        printf("Unicode UCS-4\n");
                    } break;
                }
            } break;

            default:
            {
                printf("  unknown\n");
            } break;
        }
#endif

        switch (platform_id)
        {
            case 0: // Unicode
            {
                font_file->mapping_table = font_file->cmap + cui_read_u32_be(font_file->cmap, table_offset + 4);
            } break;

            case 3: // Microsoft
            {
                if ((platform_specific_id == 1) || (platform_specific_id == 10))
                {
                    font_file->mapping_table = font_file->cmap + cui_read_u32_be(font_file->cmap, table_offset + 4);
                }
            } break;
        }

        table_offset += 8;
    }

    if (!font_file->mapping_table)
    {
        return false;
    }

    return true;
}

void
cui_font_update_with_size_and_line_height(CuiFont *font, float font_size, float line_height)
{
    font->font_scale = cui_font_file_get_scale_for_unit_height(font->file, font_size);
    font->line_height = (int32_t) ceilf(font_size * line_height);
    font->baseline_offset = 0.5f * ((float) font->line_height - font_size) +
                            (font->file->ascent * font->font_scale);
}

float
cui_font_get_string_width(CuiFont *font, CuiString str)
{
    float width = 0.0f;

    if (str.count > 0)
    {
        int64_t index = 0;

        CuiUnicodeResult utf8 = cui_utf8_decode(str, index);
        uint32_t prev_glyph_index = cui_font_file_get_glyph_index_from_codepoint(font->file, utf8.codepoint);

        width += font->font_scale * cui_font_file_get_glyph_advance(font->file, prev_glyph_index);
        index += utf8.byte_count;

        while (index < str.count)
        {
            utf8 = cui_utf8_decode(str, index);
            uint32_t glyph_index = cui_font_file_get_glyph_index_from_codepoint(font->file, utf8.codepoint);

            width += font->font_scale * (cui_font_file_get_glyph_advance(font->file, glyph_index) +
                                         0 /* cui_font_file_get_glyph_kerning(font->file, prev_glyph_index, glyph_index) */);

            prev_glyph_index = glyph_index;
            index += utf8.byte_count;
        }
    }

    return width;
}

float
cui_font_get_substring_width(CuiFont *font, CuiString str, int64_t character_index)
{
    float width = 0.0f;

    if ((str.count > 0) && (character_index > 0))
    {
        int64_t index = 0;
        int64_t count = 1;

        CuiUnicodeResult utf8 = cui_utf8_decode(str, index);
        uint32_t prev_glyph_index = cui_font_file_get_glyph_index_from_codepoint(font->file, utf8.codepoint);

        width += font->font_scale * cui_font_file_get_glyph_advance(font->file, prev_glyph_index);
        index += utf8.byte_count;

        while (index < str.count)
        {
            if (count == character_index)
            {
                break;
            }

            utf8 = cui_utf8_decode(str, index);
            uint32_t glyph_index = cui_font_file_get_glyph_index_from_codepoint(font->file, utf8.codepoint);

            width += font->font_scale * (cui_font_file_get_glyph_advance(font->file, glyph_index) +
                                         0 /* cui_font_file_get_glyph_kerning(font->file, prev_glyph_index, glyph_index) */);

            prev_glyph_index = glyph_index;
            index += utf8.byte_count;
            count += 1;
        }
    }

    return width;
}
