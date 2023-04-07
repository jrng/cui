#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#define ArrayCount(array) (sizeof(array)/sizeof((array)[0]))

#define is_digit(character) (((character) >= '0') && ((character) <= '9'))
#define is_alpha(character) ((((character) >= 'a') && ((character) <= 'z')) || (((character) >= 'A') && ((character) <= 'Z')))
#define skip_whitespace(str) do { while(*(str) == ' ') (str) += 1; } while(0)
#define skip_whitespace_and_comma(str) do { while((*(str) == ' ') || (*(str) == ',')) (str) += 1; } while(0)

typedef struct Shape
{
    int32_t width;
    int32_t height;
    const char *name;
    const char *path;
} Shape;

static const Shape shapes[] = {
    {  16,  16, "CHECKBOX_OUTER_16", "M 3,1 H 13 C 14.108,1 15,1.892 15,3 V 13 C 15,14.108 14.108,15 13,15 H 3 C 1.892,15 1,14.108 1,13 V 3 C 1,1.892 1.892,1 3,1 Z" },
    {  16,  16, "CHECKBOX_INNER_16", "M 3,2 H 13 C 13.554,2 14,2.446 14,3 V 13 C 14,13.554 13.554,14 13,14 H 3 C 2.446,14 2,13.554 2,13 V 3 C 2,2.446 2.446,2 3,2 Z" },
    {  16,  16,      "CHECKMARK_16", "M 11.910156,4.0039062 A 1,1 0 0 0 11.232422,4.359375 L 6.9335937,9.5195313 4.7070312,7.2929687 A 1,1 0 0 0 3.2929688,7.2929687 1,1 0 0 0 3.2929688,8.7070313 L 6.2929687,11.707031 A 1.0001,1.0001 0 0 0 7.7675781,11.640625 L 12.767578,5.640625 A 1,1 0 0 0 12.640625,4.2324219 1,1 0 0 0 11.910156,4.0039062 Z" },
    {  12,  12,       "ANGLE_UP_12", "M 5.292951,2.2927763 0.29282508,7.2927763 A 1.0000252,1 0 0 0 0.29282508,8.7068388 1.0000252,1 0 0 0 1.7069237,8.7068388 L 6.0000002,4.41387 10.293077,8.7068388 A 1.0000252,1 0 0 0 11.707175,8.7068388 1.0000252,1 0 0 0 11.707175,7.2927763 L 6.7070492,2.2927763 A 1.0001252,1.0001 0 0 0 5.292951,2.2927763 Z" },
    {  12,  12,    "ANGLE_RIGHT_12", "M 9.7068804,6.70688 4.7068804,11.70688 A 1,1 0 0 1 3.2928179,11.70688 1,1 0 0 1 3.2928179,10.292817 L 7.5857867,5.9998487 3.2928179,1.7068797 A 1,1 0 0 1 3.2928179,0.2928177 1,1 0 0 1 4.7068804,0.2928177 L 9.7068804,5.2928175 A 1.0001,1.0001 0 0 1 9.7068804,6.70688 Z" },
    {  12,  12,     "ANGLE_DOWN_12", "M 5.2928177,9.7068804 0.2928177,4.7068804 A 1,1 0 0 1 0.2928177,3.2928179 1,1 0 0 1 1.7068807,3.2928179 L 5.999849,7.5857867 10.292818,3.2928179 A 1,1 0 0 1 11.70688,3.2928179 1,1 0 0 1 11.70688,4.7068804 L 6.7068802,9.7068804 A 1.0001,1.0001 0 0 1 5.2928177,9.7068804 Z" },
    {  12,  12,     "ANGLE_LEFT_12", "M 2.2927763,6.70688 7.2927763,11.70688 A 1,1 0 0 0 8.7068388,11.70688 1,1 0 0 0 8.7068388,10.292817 L 4.41387,5.9998487 8.7068388,1.7068797 A 1,1 0 0 0 8.7068388,0.2928177 1,1 0 0 0 7.2927763,0.2928177 L 2.2927763,5.2928175 A 1.0001,1.0001 0 0 0 2.2927763,6.70688 Z" },
    {  12,  12,           "INFO_12", "M 6,0.5 C 5.1715729,0.5 4.5,1.1715729 4.5,2 4.5,2.8284271 5.1715729,3.5 6,3.5 6.8284271,3.5 7.5,2.8284271 7.5,2 7.5,1.1715729 6.8284271,0.5 6,0.5 Z M 3.5,5 V 6 C 4.1903559,6 4.75,6.1269295 4.75,6.5 V 10.5 C 4.75,10.860277 4.1903559,11 3.5,11 V 12 H 4.75 7.25 8.5 V 11 C 7.8096441,11 7.25,10.859325 7.25,10.5 V 5 Z" },
    {  12,  12,         "EXPAND_12", "M 6 0 A 1 1 0 0 0 5 1 A 1 1 0 0 0 6 2 L 10 2 L 10 6 A 1 1 0 0 0 11 7 A 1 1 0 0 0 12 6 L 12 1 A 1 1 0 0 0 11 0 L 6 0 z M 1 5 A 1 1 0 0 0 0 6 L 0 11 A 1 1 0 0 0 1 12 L 6 12 A 1 1 0 0 0 7 11 A 1 1 0 0 0 6 10 L 2 10 L 2 6 A 1 1 0 0 0 1 5 Z" },
    {  12,  12,         "SEARCH_12", "M 5 0.25 C 2.3855361 0.25 0.25 2.3855361 0.25 5 C 0.25 7.6144639 2.3855361 9.75 5 9.75 C 6.0373687 9.75 6.9981656 9.4126813 7.78125 8.84375 L 10.46875 11.53125 A 0.75 0.75 0 0 0 11.53125 11.53125 A 0.75 0.75 0 0 0 11.53125 10.46875 L 8.84375 7.78125 C 9.4126813 6.9981656 9.75 6.0373687 9.75 5 C 9.75 2.3855361 7.6144639 0.25 5 0.25 z M 5 1.75 C 6.8038053 1.75 8.25 3.1961947 8.25 5 C 8.25 6.8038053 6.8038053 8.25 5 8.25 C 3.1961947 8.25 1.75 6.8038053 1.75 5 C 1.75 3.1961947 3.1961947 1.75 5 1.75 Z" },
    {  12,  12,    "UPPERCASE_A_12", "M 6 1.25 A 0.750075 0.750075 0 0 0 5.296875 1.7363281 L 2.296875 9.7363281 A 0.75 0.75 0 0 0 2.7363281 10.703125 A 0.75 0.75 0 0 0 3.703125 10.263672 L 4.2714844 8.75 L 7.7285156 8.75 L 8.296875 10.263672 A 0.75 0.75 0 0 0 9.2636719 10.703125 A 0.75 0.75 0 0 0 9.703125 9.7363281 L 6.703125 1.7363281 A 0.750075 0.750075 0 0 0 6 1.25 z M 6 4.1386719 L 7.1660156 7.25 L 4.8339844 7.25 L 6 4.1386719 Z" },
    {  12,  12,    "UPPERCASE_B_12", "M 3.0058594 1.2207031 A 0.75 0.75 0 0 0 2.25 1.9648438 A 0.75 0.75 0 0 0 2.2519531 1.9824219 A 0.75 0.75 0 0 0 2.25 2 L 2.25 6 L 2.25 10 A 0.75 0.75 0 0 0 2.2519531 10.017578 A 0.75 0.75 0 0 0 2.25 10.035156 A 0.75 0.75 0 0 0 3.0058594 10.779297 L 7 10.75 C 8.5098953 10.75 9.75 9.5098949 9.75 8 C 9.75 7.2150738 9.4148126 6.502784 8.8808594 6 C 9.4148126 5.497216 9.75 4.7849264 9.75 4 C 9.75 2.4901049 8.5098951 1.25 7 1.25 L 3.0058594 1.2207031 z M 3.75 2.7265625 L 6.9941406 2.75 A 0.750075 0.750075 0 0 0 7 2.75 C 7.6992373 2.75 8.25 3.3007627 8.25 4 C 8.25 4.6992373 7.6992373 5.25 7 5.25 L 3.75 5.25 L 3.75 2.7265625 z M 3.75 6.75 L 7 6.75 C 7.6992377 6.75 8.25 7.3007626 8.25 8 C 8.25 8.6992377 7.6992377 9.25 7 9.25 A 0.750075 0.750075 0 0 0 6.9941406 9.25 L 3.75 9.2734375 L 3.75 6.75 Z" },
    {  12,  12,    "UPPERCASE_G_12", "" },
    {  12,  12,    "UPPERCASE_H_12", "M 3 1.25 A 0.75 0.75 0 0 0 2.25 2 L 2.25 10 A 0.75 0.75 0 0 0 3 10.75 A 0.75 0.75 0 0 0 3.75 10 L 3.75 6.75 L 8.25 6.75 L 8.25 10 A 0.75 0.75 0 0 0 9 10.75 A 0.75 0.75 0 0 0 9.75 10 L 9.75 2 A 0.75 0.75 0 0 0 9 1.25 A 0.75 0.75 0 0 0 8.25 2 L 8.25 5.25 L 3.75 5.25 L 3.75 2 A 0.75 0.75 0 0 0 3 1.25 Z" },
    {  12,  12,    "UPPERCASE_L_12", "M 4,1.25 A 0.75,0.75 0 0 0 3.25,2 V 10 A 0.750075,0.750075 0 0 0 4,10.75 H 8 A 0.75,0.75 0 0 0 8.75,10 0.75,0.75 0 0 0 8,9.25 H 4.75 V 2 A 0.75,0.75 0 0 0 4,1.25 Z" },
    {  12,  12,    "UPPERCASE_R_12", "M 3 1.25 A 0.75 0.75 0 0 0 2.25 2 L 2.25 6 L 2.25 10 A 0.75 0.75 0 0 0 3 10.75 A 0.75 0.75 0 0 0 3.75 10 L 3.75 6.75 L 6.5371094 6.75 L 8.3300781 10.335938 A 0.75 0.75 0 0 0 9.3359375 10.669922 A 0.75 0.75 0 0 0 9.6699219 9.6640625 L 8.0976562 6.5195312 C 9.0668388 6.0926812 9.75 5.1203872 9.75 4 C 9.75 2.4901034 8.5098966 1.25 7 1.25 L 3 1.25 z M 3.75 2.75 L 7 2.75 C 7.699238 2.75 8.25 3.300762 8.25 4 C 8.25 4.699238 7.699238 5.25 7 5.25 L 3.75 5.25 L 3.75 2.75 Z" },
    {  12,  12,    "UPPERCASE_S_12", "" },
    {  12,  12,    "UPPERCASE_V_12", "M 2.7363281,1.296875 A 0.75,0.75 0 0 0 2.296875,2.2636719 L 5.296875,10.263672 A 0.750075,0.750075 0 0 0 6.703125,10.263672 L 9.703125,2.2636719 A 0.75,0.75 0 0 0 9.2636719,1.296875 0.75,0.75 0 0 0 8.296875,1.7363281 L 6,7.8613281 3.703125,1.7363281 A 0.75,0.75 0 0 0 2.7363281,1.296875 Z" },
    {  12,  12,           "PLUS_12", "M 6 1 A 1 1 0 0 0 5 2 L 5 5 L 2 5 A 1 1 0 0 0 1 6 A 1 1 0 0 0 2 7 L 5 7 L 5 10 A 1 1 0 0 0 6 11 A 1 1 0 0 0 7 10 L 7 7 L 10 7 A 1 1 0 0 0 11 6 A 1 1 0 0 0 10 5 L 7 5 L 7 2 A 1 1 0 0 0 6 1 Z" },
};

static inline int
parse_int(const char **str)
{
    bool sign = false;
    int value = 0;

    if (**str == '-')
    {
        sign = true;
        *str += 1;
    }

    while (is_digit(**str))
    {
        value = (10 * value) + (**str - '0');
        *str += 1;
    }

    if (sign) value = -value;

    return value;
}

static inline float
parse_float(const char **str)
{
    bool sign = false;
    float value = 0.0f;

    if (**str == '-')
    {
        sign = true;
        *str += 1;
    }

    while (is_digit(**str))
    {
        value = (10.0f * value) + (float) (**str - '0');
        *str += 1;
    }

    if (**str == '.')
    {
        float factor = 1.0f / 10.0f;
        *str += 1;

        while (is_digit(**str))
        {
            value += factor * (float) (**str - '0');
            factor /= 10.0f;
            *str += 1;
        }
    }

    if (sign) value = -value;

    return value;
}

int main(void)
{
    FILE *file = fopen("src/cui_shapes.c", "w");

    if (file)
    {
        fprintf(file, "// DO NOT EDIT. This file is generated by shape_compile.\n\n");

        fprintf(file, "void\n"
                      "cui_draw_fill_shape(CuiGraphicsContext *ctx, float x, float y, CuiShapeType shape_type, float scale, CuiColor color)\n"
                      "{\n"
                      "    float draw_x = x;\n"
                      "    float draw_y = y;\n"
                      "\n"
                      "    float bitmap_x = floorf(draw_x);\n"
                      "    float bitmap_y = floorf(draw_y);\n"
                      "\n"
                      "    float offset_x = draw_x - bitmap_x;\n"
                      "    float offset_y = bitmap_y - draw_y;\n"
                      "\n"
                      "    offset_x = 0.125f * roundf(offset_x * 8.0f);\n"
                      "    offset_y = 0.125f * roundf(offset_y * 8.0f);\n"
                      "\n"
                      "    CuiRect uv;\n"
                      "\n"
                      "    if (!_cui_glyph_cache_find(ctx->glyph_cache, 0, shape_type, scale, offset_x, offset_y, &uv))\n"
                      "    {\n"
                      "        CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(ctx->temporary_memory);\n"
                      "\n"
                      "        switch (shape_type)\n"
                      "        {\n"
                      "            case CUI_SHAPE_ROUNDED_CORNER:           { CuiAssert(!\"unsupported\"); } break;\n"
                      "            case CUI_SHAPE_INVERTED_ROUNDED_CORNER:  { CuiAssert(!\"unsupported\"); } break;\n"
                      "            case CUI_SHAPE_SHADOW_CORNER:            { CuiAssert(!\"unsupported\"); } break;\n"
                      "            case CUI_SHAPE_SHADOW_HORIZONTAL:        { CuiAssert(!\"unsupported\"); } break;\n"
                      "            case CUI_SHAPE_SHADOW_VERTICAL:          { CuiAssert(!\"unsupported\"); } break;\n"
                      "            case CUI_SHAPE_WINDOWS_MINIMIZE:         { CuiAssert(!\"unsupported\"); } break;\n"
                      "            case CUI_SHAPE_WINDOWS_MAXIMIZE:         { CuiAssert(!\"unsupported\"); } break;\n"
                      "            case CUI_SHAPE_WINDOWS_CLOSE:            { CuiAssert(!\"unsupported\"); } break;\n");

        for (unsigned int i = 0; i < ArrayCount(shapes); i += 1)
        {
            Shape shape = shapes[i];

            fprintf(file, "\n            case CUI_SHAPE_%s:\n"
                          "            {\n"
                          "                int32_t width  = (int32_t) ceilf(scale * %d.0f);\n"
                          "                int32_t height = (int32_t) ceilf(scale * %d.0f);\n"
                          "\n"
                          "                uv = _cui_glyph_cache_allocate_texture(ctx->glyph_cache, width, height, ctx->command_buffer);\n"
                          "\n"
                          "                CuiBitmap bitmap;\n"
                          "                bitmap.width = cui_rect_get_width(uv);\n"
                          "                bitmap.height = cui_rect_get_height(uv);\n"
                          "                bitmap.stride = ctx->glyph_cache->texture.stride;\n"
                          "                bitmap.pixels = (uint8_t *) ctx->glyph_cache->texture.pixels + (uv.min.y * bitmap.stride) + (uv.min.x * 4);\n"
                          "\n"
                          "                CuiPathCommand *path = 0;\n"
                          "                cui_array_init(path, 16, ctx->temporary_memory);\n\n",
                          shape.name, shape.width, shape.height);

            char type = 0;
            float cx = 0.0f, cy = 0.0f;
            const char *at = shape.path;

            while (*at)
            {
                skip_whitespace(at);

                if (!*at) break;

                if (is_alpha(*at))
                {
                    type = *at;
                    at += 1;
                }

                switch (type)
                {
                    case 'M':
                    {
                        skip_whitespace(at);
                        float x = parse_float(&at);
                        skip_whitespace_and_comma(at);
                        float y = parse_float(&at);

                        cx = x;
                        cy = y;

                        fprintf(file, "                cui_path_move_to(&path, scale * %ff, scale * %ff);\n", x, y);

                        type = 'L';
                    } break;

                    case 'L':
                    {
                        skip_whitespace(at);
                        float x = parse_float(&at);
                        skip_whitespace_and_comma(at);
                        float y = parse_float(&at);

                        cx = x;
                        cy = y;

                        fprintf(file, "                cui_path_line_to(&path, scale * %ff, scale * %ff);\n", x, y);
                    } break;

                    case 'V':
                    {
                        skip_whitespace(at);
                        float y = parse_float(&at);

                        cy = y;

                        fprintf(file, "                cui_path_line_to(&path, scale * %ff, scale * %ff);\n", cx, y);
                    } break;

                    case 'H':
                    {
                        skip_whitespace(at);
                        float x = parse_float(&at);

                        cx = x;

                        fprintf(file, "                cui_path_line_to(&path, scale * %ff, scale * %ff);\n", x, cy);
                    } break;

                    case 'A':
                    {
                        skip_whitespace(at);
                        float rx = parse_float(&at);
                        skip_whitespace_and_comma(at);
                        float ry = parse_float(&at);
                        skip_whitespace_and_comma(at);
                        float x_rotate = parse_float(&at);
                        skip_whitespace_and_comma(at);
                        int large_arc_flag = parse_int(&at);
                        skip_whitespace_and_comma(at);
                        int sweep_flag = parse_int(&at);
                        skip_whitespace_and_comma(at);
                        float x = parse_float(&at);
                        skip_whitespace_and_comma(at);
                        float y = parse_float(&at);

                        cx = x;
                        cy = y;

                        fprintf(file, "                cui_path_arc_to(&path, scale * %ff, scale * %ff, %ff, %s, %s, scale * %ff, scale * %ff);\n",
                                rx, ry, x_rotate, large_arc_flag ? "true" : "false", sweep_flag ? "true" : "false", x, y);
                    } break;

                    case 'C':
                    {
                        skip_whitespace(at);
                        float cx1 = parse_float(&at);
                        skip_whitespace_and_comma(at);
                        float cy1 = parse_float(&at);
                        skip_whitespace_and_comma(at);
                        float cx2 = parse_float(&at);
                        skip_whitespace_and_comma(at);
                        float cy2 = parse_float(&at);
                        skip_whitespace_and_comma(at);
                        float x = parse_float(&at);
                        skip_whitespace_and_comma(at);
                        float y = parse_float(&at);

                        cx = x;
                        cy = y;

                        fprintf(file, "                cui_path_cubic_curve_to(&path, scale * %ff, scale * %ff, scale * %ff, scale * %ff, scale * %ff, scale * %ff);\n", cx1, cy1, cx2, cy2, x, y);
                    } break;

                    case 'z':
                    case 'Z':
                    {
                        fprintf(file, "                cui_path_close(&path);\n");
                    } break;

                    default:
                    {
                        printf("unknown type: '%c'\n", type);
                    } break;
                }
            }

            fprintf(file, "\n                CuiEdge *edge_list = 0;\n"
                          "                cui_array_init(edge_list, 16, ctx->temporary_memory);\n"
                          "\n"
                          "                _cui_path_to_edge_list(path, &edge_list);\n"
                          "                _cui_edge_list_fill(ctx->temporary_memory, &bitmap, edge_list, cui_make_color(1.0f, 1.0f, 1.0f, 1.0f));\n"
                          "            } break;\n");
        }

        fprintf(file, "        }\n"
                      "\n"
                      "        _cui_glyph_cache_put(ctx->glyph_cache, 0, shape_type, scale, offset_x, offset_y, uv);\n"
                      "\n"
                      "        cui_end_temporary_memory(temp_memory);\n"
                      "    }\n"
                      "\n"
                      "    CuiRect rect;\n"
                      "    rect.min.x = (int32_t) bitmap_x;\n"
                      "    rect.min.y = (int32_t) bitmap_y;\n"
                      "    rect.max.x = rect.min.x + cui_rect_get_width(uv);\n"
                      "    rect.max.y = rect.min.y + cui_rect_get_height(uv);\n"
                      "\n"
                      "    if (cui_rect_overlap(ctx->clip_rect, rect))\n"
                      "    {\n"
                      "        _cui_push_textured_rect(ctx->command_buffer, rect, uv, color, ctx->glyph_cache->texture_id, ctx->clip_rect_offset);\n"
                      "    }\n"
                      "}\n");

        fclose(file);
    }

    return 0;
}
