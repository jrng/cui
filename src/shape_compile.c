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
    {  24,  24,           "LOAD_24", "M 11.921875,4.5058594 A 0.50005,0.50005 0 0 0 11.646484,4.6464844 L 8.6464844,7.6464844 A 0.5,0.5 0 0 0 8.6464844,8.3535156 0.5,0.5 0 0 0 9.3535156,8.3535156 L 11.5,6.2070312 V 14 H 12.5 V 6.2070312 L 14.646484,8.3535156 A 0.5,0.5 0 0 0 15.353516,8.3535156 0.5,0.5 0 0 0 15.353516,7.6464844 L 12.353516,4.6464844 A 0.50005,0.50005 0 0 0 11.921875,4.5058594 Z M 4,12 C 3.446,12 3,12.446 3,13 V 16 C 3,16.554 3.446,17 4,17 H 20 C 20.554,17 21,16.554 21,16 V 13 C 21,12.446 20.554,12 20,12 H 14 V 13 H 20 V 16 H 4 V 13 H 10 V 12 Z M 16.5,14 A 0.5,0.5 0 0 0 16,14.5 0.5,0.5 0 0 0 16.5,15 0.5,0.5 0 0 0 17,14.5 0.5,0.5 0 0 0 16.5,14 Z M 18.5,14 A 0.5,0.5 0 0 0 18,14.5 0.5,0.5 0 0 0 18.5,15 0.5,0.5 0 0 0 19,14.5 0.5,0.5 0 0 0 18.5,14 Z" },
    {  24,  24,           "TAPE_24", "M 6 8 A 4 4 0 0 0 2 12 A 4 4 0 0 0 6 16 L 18 16 A 4 4 0 0 0 22 12 A 4 4 0 0 0 18 8 A 4 4 0 0 0 14 12 A 4 4 0 0 0 15.357422 15 L 8.6425781 15 A 4 4 0 0 0 10 12 A 4 4 0 0 0 6 8 z M 6 9 A 3 3 0 0 1 9 12 A 3 3 0 0 1 6 15 A 3 3 0 0 1 3 12 A 3 3 0 0 1 6 9 z M 18 9 A 3 3 0 0 1 21 12 A 3 3 0 0 1 18 15 A 3 3 0 0 1 15 12 A 3 3 0 0 1 18 9 z " },
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

int main()
{
    FILE *file = fopen("src/cui_shapes.c", "wb");

    if (file)
    {
        fprintf(file, "// DO NOT EDIT. This file is generated by shape_compile.\n\n");

        fprintf(file, "void\n"
                      "cui_draw_fill_shape(CuiArena *temporary_memory, CuiGraphicsContext *ctx, float x, float y,\n"
                      "                    CuiShapeType shape_type, float scale, CuiColor color)\n"
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
                      "    if (!cui_glyph_cache_find(ctx->cache, 0, shape_type, scale, offset_x, offset_y, &uv))\n"
                      "    {\n"
                      "        CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(temporary_memory);\n"
                      "\n"
                      "        switch (shape_type)\n"
                      "        {\n");

        for (int i = 0; i < ArrayCount(shapes); i += 1)
        {
            Shape shape = shapes[i];

            fprintf(file, "            case CUI_SHAPE_%s:\n"
                          "            {\n"
                          "                int32_t width  = (int32_t) ceilf(scale * %d.0f);\n"
                          "                int32_t height = (int32_t) ceilf(scale * %d.0f);\n"
                          "                uv = cui_glyph_cache_allocate_texture(ctx->cache, width, height);\n"
                          "\n"
                          "                CuiBitmap bitmap;\n"
                          "                bitmap.width = uv.max.x - uv.min.x;\n"
                          "                bitmap.height = uv.max.y - uv.min.y;\n"
                          "                bitmap.stride = ctx->cache->texture.stride;\n"
                          "                bitmap.pixels = (uint8_t *) ctx->cache->texture.pixels + (uv.min.y * bitmap.stride) + (uv.min.x * 4);\n"
                          "\n"
                          "                CuiPathCommand *path = 0;\n"
                          "                cui_array_init(path, 16, temporary_memory);\n\n",
                          shape.name, shape.width, shape.height);

            char type = 0;
            float cx = 0.0f, cy = 0.0f;
            const char *at = shape.path;

            float h = (float) shape.height;

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

                        y = h - y;

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

                        y = h - y;

                        cx = x;
                        cy = y;

                        fprintf(file, "                cui_path_line_to(&path, scale * %ff, scale * %ff);\n", x, y);
                    } break;

                    case 'V':
                    {
                        skip_whitespace(at);
                        float y = parse_float(&at);

                        y = h - y;

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

                        y = h - y;
                        sweep_flag = 1 - sweep_flag;

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

                        cy1 = h - cy1;
                        cy2 = h - cy2;
                        y = h - y;

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
                          "                cui_array_init(edge_list, 16, temporary_memory);\n"
                          "\n"
                          "                cui_path_to_edge_list(path, &edge_list);\n"
                          "                cui_edge_list_fill(&bitmap, edge_list, cui_make_color(1.0f, 1.0f, 1.0f, 1.0f));\n"
                          "            } break;\n\n");
        }

        fprintf(file, "        }\n"
                      "\n"
                      "        cui_glyph_cache_put(ctx->cache, 0, shape_type, scale, offset_x, offset_y, uv);\n"
                      "\n"
                      "        cui_end_temporary_memory(temp_memory);\n"
                      "    }\n"
                      "\n"
                      "    CuiRect rect;\n"
                      "    rect.min.x = (int32_t) bitmap_x;\n"
                      "    rect.min.y = (int32_t) bitmap_y;\n"
                      "    rect.max.x = rect.min.x + (uv.max.x - uv.min.x);\n"
                      "    rect.max.y = rect.min.y + (uv.max.y - uv.min.y);\n"
                      "\n"
                      "    // TODO: if overlap\n"
                      "\n"
                      "    cui_draw_fill_textured_rect(ctx, rect, uv.min, color);\n"
                      "}\n");

        fclose(file);
    }

    return 0;
}
