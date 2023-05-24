#include <cui.h>

static const CuiColorTheme color_tool_color_theme = {
    /* window_titlebar_background           */ CuiHexColorLiteral(0xFF2F333D),
    /* window_titlebar_border               */ CuiHexColorLiteral(0xFF181A1F),
    /* window_titlebar_text                 */ CuiHexColorLiteral(0xFFD7DAE0),
    /* window_titlebar_icon                 */ CuiHexColorLiteral(0xFFD7DAE0),
    /* window_drop_shadow                   */ CuiHexColorLiteral(0x6F000000),
    /* window_outline                       */ CuiHexColorLiteral(0xAF050607),
    /* default_bg                           */ CuiHexColorLiteral(0xFF282C34),
    /* default_fg                           */ CuiHexColorLiteral(0xFFD7DAE0),
    /* default_border                       */ CuiHexColorLiteral(0xFF585C64),
    /* default_button_normal_background     */ CuiHexColorLiteral(0xFF2F333D),
    /* default_button_normal_box_shadow     */ CuiHexColorLiteral(0x3F000000),
    /* default_button_normal_border         */ CuiHexColorLiteral(0xFF1E1E1E),
    /* default_button_normal_text           */ CuiHexColorLiteral(0xFFD7DAE0),
    /* default_button_normal_icon           */ CuiHexColorLiteral(0xFFB7BAC0),
    /* default_textinput_normal_background  */ CuiHexColorLiteral(0xFF20232A),
    /* default_textinput_normal_box_shadow  */ CuiHexColorLiteral(0x3F000000),
    // /* default_textinput_normal_border      */ CuiHexColorLiteral(0xFF3A3D45),
    /* default_textinput_normal_border      */ { 0.094f, 0.102f, 0.122f, 1.0f },
    /* default_textinput_normal_text        */ CuiHexColorLiteral(0xFFD7DAE0),
    /* default_textinput_normal_placeholder */ CuiHexColorLiteral(0xFF585C64),
    /* default_textinput_normal_icon        */ CuiHexColorLiteral(0xFFB7BAC0),
};

#define TEXTINPUT_NUMBER_BUFFER_SIZE 64
#define TEXTINPUT_BUFFER_SIZE CuiKiB(1)

static const uint32_t WIDGET_TYPE_COLOR_PREVIEW = CUI_WIDGET_TYPE_CUSTOM + 0;
static const uint32_t WIDGET_TYPE_COLOR_LIST    = CUI_WIDGET_TYPE_CUSTOM + 1;

typedef struct ColorPreview
{
    CuiWidget base;

    CuiRect rect;
    CuiColor fill_color;
    CuiColor border_color;
} ColorPreview;

typedef struct ColorList
{
    CuiWidget base;

    CuiColor *colors;
} ColorList;

typedef struct ColorTool
{
    CuiArena temporary_memory;
    CuiArena widget_arena;
    CuiArena output_arena;

    CuiColor rgb_color;
    CuiColor hsv_color;
    CuiColor hsl_color;

    CuiWindow *window;

    CuiWidget *root_widget;

    CuiWidget *color_list_row;
    ColorList *color_list;
    CuiWidget *color_add_button;
    ColorPreview *color_preview;

    CuiWidget *input_a;

    CuiWidget *input_r;
    CuiWidget *input_g;
    CuiWidget *input_b;

    CuiWidget *input_h1;
    CuiWidget *input_s1;
    CuiWidget *input_v1;

    CuiWidget *input_h2;
    CuiWidget *input_s2;
    CuiWidget *input_l2;

    CuiWidget *copy_css_hex_small;
    CuiWidget *copy_css_hex_big;
    CuiWidget *copy_css_rgb;

    CuiWidget *copy_c_hex;
    CuiWidget *copy_c_float;
    CuiWidget *copy_c_float_braces;

    CuiWidget *copy_shader_vec3;
    CuiWidget *copy_shader_vec4;
    CuiWidget *copy_shader_float3;
    CuiWidget *copy_shader_float4;
} ColorTool;

static ColorTool app;

static void
color_preview_set_ui_scale(CuiWidget *widget, float ui_scale)
{
    (void) widget;
    (void) ui_scale;
}

static CuiPoint
color_preview_get_preferred_size(CuiWidget *widget)
{
    (void) widget;

    return cui_make_point(0, 0);
}

static void
color_preview_layout(CuiWidget *widget, CuiRect rect)
{
    (void) widget;
    (void) rect;
}

static void
color_preview_draw(CuiWidget *widget, CuiGraphicsContext *ctx, const CuiColorTheme *color_theme)
{
    (void) color_theme;

    ColorPreview *color_preview = CuiContainerOf(widget, ColorPreview, base);

    CuiRect border_rect = widget->rect;
    CuiRect fill_rect = widget->rect;

    int32_t border_width = widget->effective_border_width.min.x;
    int32_t border_radius = widget->effective_border_radius.min.x;
    int32_t fill_radius = border_radius - border_width;

    fill_rect.min.x += border_width;
    fill_rect.min.y += border_width;
    fill_rect.max.x -= border_width;
    fill_rect.max.y -= border_width;

    cui_draw_fill_rounded_rect_1(ctx, border_rect, (float) border_radius, color_preview->border_color);
    cui_draw_fill_rounded_rect_1(ctx, fill_rect, (float) fill_radius, color_preview->fill_color);
}

static bool
color_preview_handle_event(CuiWidget *widget, CuiEventType event_type)
{
    (void) widget;
    (void) event_type;

    return false;
}

static void
color_list_set_ui_scale(CuiWidget *widget, float ui_scale)
{
    (void) widget;
    (void) ui_scale;
}

static CuiPoint
color_list_get_preferred_size(CuiWidget *widget)
{
    ColorList *color_list = CuiContainerOf(widget, ColorList, base);
    int32_t color_count = cui_array_count(color_list->colors);

    return cui_make_point(color_count * lroundf(widget->ui_scale * 28.0f), lroundf(widget->ui_scale * 28.0f));
}

static void
color_list_layout(CuiWidget *widget, CuiRect rect)
{
    (void) widget;
    (void) rect;
}

static void
color_list_draw(CuiWidget *widget, CuiGraphicsContext *ctx, const CuiColorTheme *color_theme)
{
    (void) color_theme;

    ColorList *color_list = CuiContainerOf(widget, ColorList, base);

    int32_t color_count = cui_array_count(color_list->colors);

    int32_t x = widget->rect.min.x;
    int32_t y = widget->rect.min.y;

    int32_t x_advance = lroundf(widget->ui_scale * 28.0f);
    int32_t y_advance = x_advance;

    if (color_count == 1)
    {
        CuiRect rect;
        rect.min.x = x;
        rect.min.y = y;
        rect.max.x = x + x_advance;
        rect.max.y = y + y_advance;

        cui_draw_fill_rounded_rect_1(ctx, rect, (float) widget->effective_border_radius.min.x, color_list->colors[0]);
    }
    else
    {
        for (int32_t i = 0; i < color_count; i += 1)
        {
            CuiRect rect;
            rect.min.x = x;
            rect.min.y = y;
            rect.max.x = x + x_advance;
            rect.max.y = y + y_advance;

            if (i == 0)
            {
                cui_draw_fill_rounded_rect_4(ctx, rect, (float) widget->effective_border_radius.min.x, 0.0f,
                                             0.0f, (float) widget->effective_border_radius.min.x,
                                             color_list->colors[i]);
            }
            else if (i == (color_count - 1))
            {
                cui_draw_fill_rounded_rect_4(ctx, rect, 0.0f, (float) widget->effective_border_radius.min.x,
                                             (float) widget->effective_border_radius.min.x, 0.0f,
                                             color_list->colors[i]);
            }
            else
            {
                cui_draw_fill_rect(ctx, rect, color_list->colors[i]);
            }

            x += x_advance;
        }
    }
}

static bool
color_list_handle_event(CuiWidget *widget, CuiEventType event_type)
{
    (void) widget;
    (void) event_type;

    return false;
}

static CuiWidget *
create_widget(CuiArena *arena, uint32_t type)
{
    CuiWidget *widget = cui_alloc_type(arena, CuiWidget, CuiDefaultAllocationParams());
    cui_widget_init(widget, type);
    return widget;
}

static ColorPreview *
create_color_preview(CuiArena *arena)
{
    ColorPreview *color_preview = cui_alloc_type(arena, ColorPreview, CuiDefaultAllocationParams());
    cui_widget_init(&color_preview->base, WIDGET_TYPE_COLOR_PREVIEW);
    CuiWidgetInitCustomFunctions(&color_preview->base, color_preview_);
    cui_widget_set_border_width(&color_preview->base, 1.0f, 1.0f, 1.0f, 1.0f);
    cui_widget_set_border_radius(&color_preview->base, 4.0f, 4.0f, 4.0f, 4.0f);
    return color_preview;
}

static ColorList *
create_color_list(CuiArena *arena)
{
    ColorList *color_list = cui_alloc_type(arena, ColorList, CuiDefaultAllocationParams());
    cui_widget_init(&color_list->base, WIDGET_TYPE_COLOR_LIST);
    CuiWidgetInitCustomFunctions(&color_list->base, color_list_);
    cui_widget_set_border_radius(&color_list->base, 2.0f, 2.0f, 2.0f, 2.0f);
    cui_array_init(color_list->colors, 8, arena);
    return color_list;
}

static inline CuiColor
lighten(CuiColor color, float amount)
{
    CuiColor hsv_color = cui_color_rgb_to_hsv(color);

    hsv_color.b = cui_lerp_float(hsv_color.b, 1.0f, amount); // v
    hsv_color.g = cui_lerp_float(hsv_color.g, 0.0f, amount); // s

    if (hsv_color.r < 120)
    {
        hsv_color.r = cui_lerp_float(hsv_color.r, 60.0f, amount);
    }
    else if (hsv_color.r < 240)
    {
        hsv_color.r = cui_lerp_float(hsv_color.r, 180.0f, amount);
    }
    else
    {
        hsv_color.r = cui_lerp_float(hsv_color.r, 300.0f, amount);
    }

    return cui_color_hsv_to_rgb(hsv_color);
}

static inline CuiColor
darken(CuiColor color, float amount)
{
    CuiColor hsv_color = cui_color_rgb_to_hsv(color);

    hsv_color.b = cui_lerp_float(hsv_color.b, 0.0f, amount); // v
    hsv_color.g = cui_lerp_float(hsv_color.g, 1.0f, amount); // s

    if (hsv_color.r < 60)
    {
        hsv_color.r = cui_lerp_float(hsv_color.r, 0.0f, amount);
    }
    else if (hsv_color.r < 180)
    {
        hsv_color.r = cui_lerp_float(hsv_color.r, 120.0f, amount);
    }
    else if (hsv_color.r < 300)
    {
        hsv_color.r = cui_lerp_float(hsv_color.r, 240.0f, amount);
    }
    else
    {
        hsv_color.r = cui_lerp_float(hsv_color.r, 360.0f, amount);
    }

    return cui_color_hsv_to_rgb(hsv_color);
}

static void
update_output(void)
{
    cui_arena_clear(&app.output_arena);

    app.color_preview->fill_color = app.rgb_color;

    if (app.hsv_color.b > 0.5f)
    {
        app.color_preview->border_color = darken(app.rgb_color, 0.125f);
    }
    else
    {
        app.color_preview->border_color = lighten(app.rgb_color, 0.125f);
    }

    float float_r = app.rgb_color.r;
    float float_g = app.rgb_color.g;
    float float_b = app.rgb_color.b;
    float float_a = app.rgb_color.a;

    uint32_t packed_color = cui_color_pack_bgra(app.rgb_color);

    uint32_t r = (uint32_t) ((app.rgb_color.r * 255.0f) + 0.5f) & 0xff;
    uint32_t g = (uint32_t) ((app.rgb_color.g * 255.0f) + 0.5f) & 0xff;
    uint32_t b = (uint32_t) ((app.rgb_color.b * 255.0f) + 0.5f) & 0xff;
    // uint32_t a = (uint32_t) ((app.rgb_color.a * 255.0f) + 0.5f) & 0xff;

    cui_widget_set_label(app.copy_css_hex_small, cui_sprint(&app.output_arena, CuiStringLiteral("#%06x"), packed_color & 0xffffff));
    cui_widget_set_label(app.copy_css_hex_big, cui_sprint(&app.output_arena, CuiStringLiteral("#%06X"), packed_color & 0xffffff));
    cui_widget_set_label(app.copy_css_rgb, cui_sprint(&app.output_arena, CuiStringLiteral("rgb(%u, %u, %u)"), r, g, b));

    cui_widget_set_label(app.copy_c_hex, cui_sprint(&app.output_arena, CuiStringLiteral("0x%08x"), packed_color));
    cui_widget_set_label(app.copy_c_float, cui_sprint(&app.output_arena, CuiStringLiteral("%.3ff, %.3ff, %.3ff, %.3ff"), float_r, float_g, float_b, float_a));
    cui_widget_set_label(app.copy_c_float_braces, cui_sprint(&app.output_arena, CuiStringLiteral("{ %.3ff, %.3ff, %.3ff, %.3ff }"), float_r, float_g, float_b, float_a));

    cui_widget_set_label(app.copy_shader_vec3, cui_sprint(&app.output_arena, CuiStringLiteral("vec3(%.3ff, %.3ff, %.3ff)"), float_r, float_g, float_b));
    cui_widget_set_label(app.copy_shader_vec4, cui_sprint(&app.output_arena, CuiStringLiteral("vec4(%.3ff, %.3ff, %.3ff, %.3ff)"), float_r, float_g, float_b, float_a));
    cui_widget_set_label(app.copy_shader_float3, cui_sprint(&app.output_arena, CuiStringLiteral("float3(%.3f, %.3f, %.3f)"), float_r, float_g, float_b));
    cui_widget_set_label(app.copy_shader_float4, cui_sprint(&app.output_arena, CuiStringLiteral("float4(%.3f, %.3f, %.3f, %.3f)"), float_r, float_g, float_b, float_a));

    cui_widget_relayout_parent(app.copy_css_rgb);
    cui_window_request_redraw(app.window);
}

static void
update_hsv(void)
{
    CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(&app.temporary_memory);

    uint32_t h = (uint32_t) (app.hsv_color.r + 0.5f) & 0xff;
    uint32_t s = (uint32_t) ((app.hsv_color.g * 100.0f) + 0.5f) & 0xff;
    uint32_t v = (uint32_t) ((app.hsv_color.b * 100.0f) + 0.5f) & 0xff;

    cui_widget_set_textinput_value(app.input_h1, cui_sprint(&app.temporary_memory, CuiStringLiteral("%u"), h));
    cui_widget_set_textinput_value(app.input_s1, cui_sprint(&app.temporary_memory, CuiStringLiteral("%u"), s));
    cui_widget_set_textinput_value(app.input_v1, cui_sprint(&app.temporary_memory, CuiStringLiteral("%u"), v));

    cui_end_temporary_memory(temp_memory);
}

static void
update_hsl(void)
{
    CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(&app.temporary_memory);

    uint32_t h = (uint32_t) (app.hsv_color.r + 0.5f) & 0xff;
    uint32_t s = (uint32_t) ((app.hsv_color.g * 100.0f) + 0.5f) & 0xff;
    uint32_t l = (uint32_t) ((app.hsv_color.b * 100.0f) + 0.5f) & 0xff;

    cui_widget_set_textinput_value(app.input_h2, cui_sprint(&app.temporary_memory, CuiStringLiteral("%u"), h));
    cui_widget_set_textinput_value(app.input_s2, cui_sprint(&app.temporary_memory, CuiStringLiteral("%u"), s));
    cui_widget_set_textinput_value(app.input_l2, cui_sprint(&app.temporary_memory, CuiStringLiteral("%u"), l));

    cui_end_temporary_memory(temp_memory);
}

static void
on_rgb_changed(CuiWidget *widget)
{
    (void) widget;

    CuiString r_str = cui_widget_get_textinput_value(app.input_r);
    CuiString g_str = cui_widget_get_textinput_value(app.input_g);
    CuiString b_str = cui_widget_get_textinput_value(app.input_b);

    // TODO: these don't report if they fail
    int32_t r = cui_parse_int32(r_str);
    int32_t g = cui_parse_int32(g_str);
    int32_t b = cui_parse_int32(b_str);

    CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(&app.temporary_memory);

    if (r < 0)
    {
        r = 0;
        cui_widget_set_textinput_value(app.input_r, cui_sprint(&app.temporary_memory, CuiStringLiteral("%u"), r));
    }

    if (g < 0)
    {
        g = 0;
        cui_widget_set_textinput_value(app.input_g, cui_sprint(&app.temporary_memory, CuiStringLiteral("%u"), g));
    }

    if (b < 0)
    {
        b = 0;
        cui_widget_set_textinput_value(app.input_b, cui_sprint(&app.temporary_memory, CuiStringLiteral("%u"), b));
    }

    if (r > 255)
    {
        r = 255;
        cui_widget_set_textinput_value(app.input_r, cui_sprint(&app.temporary_memory, CuiStringLiteral("%u"), r));
    }

    if (g > 255)
    {
        g = 255;
        cui_widget_set_textinput_value(app.input_g, cui_sprint(&app.temporary_memory, CuiStringLiteral("%u"), g));
    }

    if (b > 255)
    {
        b = 255;
        cui_widget_set_textinput_value(app.input_b, cui_sprint(&app.temporary_memory, CuiStringLiteral("%u"), b));
    }

    cui_end_temporary_memory(temp_memory);

    // TODO: handle alpha
    uint32_t hex_color = 0xff000000 | (r << 16) | (g << 8) | b;

    app.rgb_color = CuiHexColor(hex_color);
    app.hsv_color = cui_color_rgb_to_hsv(app.rgb_color);
    app.hsl_color = cui_color_rgb_to_hsl(app.rgb_color);

    update_hsv();
    update_hsl();
    update_output();

    cui_window_request_redraw(app.window);
}

static void
create_rgb_row(CuiWidget *parent_widget, CuiArena *arena)
{
    CuiWidget *rgb_row = create_widget(arena, CUI_WIDGET_TYPE_BOX);

    cui_widget_set_main_axis(rgb_row, CUI_AXIS_X);
    cui_widget_set_x_axis_gravity(rgb_row, CUI_GRAVITY_START);
    cui_widget_set_inline_padding(rgb_row, 8.0f);
    cui_widget_set_padding(rgb_row, 0.0f, 0.0f, 0.0f, 0.0f);

    uint32_t r = (uint32_t) ((app.rgb_color.r * 255.0f) + 0.5f) & 0xff;
    uint32_t g = (uint32_t) ((app.rgb_color.g * 255.0f) + 0.5f) & 0xff;
    uint32_t b = (uint32_t) ((app.rgb_color.b * 255.0f) + 0.5f) & 0xff;

    CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(&app.temporary_memory);

    {
        app.input_r = create_widget(arena, CUI_WIDGET_TYPE_TEXTINPUT);

        cui_widget_set_icon(app.input_r, CUI_ICON_UPPERCASE_R_12);
        cui_widget_set_preferred_size(app.input_r, 48.0f, 0.0f);
        cui_widget_set_x_axis_gravity(app.input_r, CUI_GRAVITY_END);
        cui_widget_set_textinput_buffer(app.input_r, cui_alloc(arena, TEXTINPUT_NUMBER_BUFFER_SIZE, CuiDefaultAllocationParams()), TEXTINPUT_NUMBER_BUFFER_SIZE);
        cui_widget_set_textinput_value(app.input_r, cui_sprint(&app.temporary_memory, CuiStringLiteral("%u"), r));

        app.input_r->on_changed = on_rgb_changed;

        cui_widget_append_child(rgb_row, app.input_r);
    }

    {
        app.input_g = create_widget(arena, CUI_WIDGET_TYPE_TEXTINPUT);

        cui_widget_set_icon(app.input_g, CUI_ICON_UPPERCASE_G_12);
        cui_widget_set_preferred_size(app.input_g, 48.0f, 0.0f);
        cui_widget_set_x_axis_gravity(app.input_g, CUI_GRAVITY_END);
        cui_widget_set_textinput_buffer(app.input_g, cui_alloc(arena, TEXTINPUT_NUMBER_BUFFER_SIZE, CuiDefaultAllocationParams()), TEXTINPUT_NUMBER_BUFFER_SIZE);
        cui_widget_set_textinput_value(app.input_g, cui_sprint(&app.temporary_memory, CuiStringLiteral("%u"), g));

        app.input_g->on_changed = on_rgb_changed;

        cui_widget_append_child(rgb_row, app.input_g);
    }

    {
        app.input_b = create_widget(arena, CUI_WIDGET_TYPE_TEXTINPUT);

        cui_widget_set_icon(app.input_b, CUI_ICON_UPPERCASE_B_12);
        cui_widget_set_preferred_size(app.input_b, 48.0f, 0.0f);
        cui_widget_set_x_axis_gravity(app.input_b, CUI_GRAVITY_END);
        cui_widget_set_textinput_buffer(app.input_b, cui_alloc(arena, TEXTINPUT_NUMBER_BUFFER_SIZE, CuiDefaultAllocationParams()), TEXTINPUT_NUMBER_BUFFER_SIZE);
        cui_widget_set_textinput_value(app.input_b, cui_sprint(&app.temporary_memory, CuiStringLiteral("%u"), b));

        app.input_b->on_changed = on_rgb_changed;

        cui_widget_append_child(rgb_row, app.input_b);
    }

    cui_end_temporary_memory(temp_memory);

    cui_widget_append_child(parent_widget, rgb_row);
}

static void
create_hsv_row(CuiWidget *parent_widget, CuiArena *arena)
{
    CuiWidget *hsv_row = create_widget(arena, CUI_WIDGET_TYPE_BOX);

    cui_widget_set_main_axis(hsv_row, CUI_AXIS_X);
    cui_widget_set_x_axis_gravity(hsv_row, CUI_GRAVITY_START);
    cui_widget_set_inline_padding(hsv_row, 8.0f);
    cui_widget_set_padding(hsv_row, 12.0f, 0.0f, 0.0f, 0.0f);

    uint32_t h = (uint32_t) (app.hsv_color.r + 0.5f) & 0xff;
    uint32_t s = (uint32_t) ((app.hsv_color.g * 100.0f) + 0.5f) & 0xff;
    uint32_t v = (uint32_t) ((app.hsv_color.b * 100.0f) + 0.5f) & 0xff;

    CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(&app.temporary_memory);

    {
        app.input_h1 = create_widget(arena, CUI_WIDGET_TYPE_TEXTINPUT);

        cui_widget_set_icon(app.input_h1, CUI_ICON_UPPERCASE_H_12);
        cui_widget_set_preferred_size(app.input_h1, 48.0f, 0.0f);
        cui_widget_set_x_axis_gravity(app.input_h1, CUI_GRAVITY_END);
        cui_widget_set_textinput_buffer(app.input_h1, cui_alloc(arena, TEXTINPUT_NUMBER_BUFFER_SIZE, CuiDefaultAllocationParams()), TEXTINPUT_NUMBER_BUFFER_SIZE);
        cui_widget_set_textinput_value(app.input_h1, cui_sprint(&app.temporary_memory, CuiStringLiteral("%u"), h));

        cui_widget_append_child(hsv_row, app.input_h1);
    }

    {
        app.input_s1 = create_widget(arena, CUI_WIDGET_TYPE_TEXTINPUT);

        cui_widget_set_icon(app.input_s1, CUI_ICON_UPPERCASE_S_12);
        cui_widget_set_preferred_size(app.input_s1, 48.0f, 0.0f);
        cui_widget_set_x_axis_gravity(app.input_s1, CUI_GRAVITY_END);
        cui_widget_set_textinput_buffer(app.input_s1, cui_alloc(arena, TEXTINPUT_NUMBER_BUFFER_SIZE, CuiDefaultAllocationParams()), TEXTINPUT_NUMBER_BUFFER_SIZE);
        cui_widget_set_textinput_value(app.input_s1, cui_sprint(&app.temporary_memory, CuiStringLiteral("%u"), s));

        cui_widget_append_child(hsv_row, app.input_s1);
    }

    {
        app.input_v1 = create_widget(arena, CUI_WIDGET_TYPE_TEXTINPUT);

        cui_widget_set_icon(app.input_v1, CUI_ICON_UPPERCASE_V_12);
        cui_widget_set_preferred_size(app.input_v1, 48.0f, 0.0f);
        cui_widget_set_x_axis_gravity(app.input_v1, CUI_GRAVITY_END);
        cui_widget_set_textinput_buffer(app.input_v1, cui_alloc(arena, TEXTINPUT_NUMBER_BUFFER_SIZE, CuiDefaultAllocationParams()), TEXTINPUT_NUMBER_BUFFER_SIZE);
        cui_widget_set_textinput_value(app.input_v1, cui_sprint(&app.temporary_memory, CuiStringLiteral("%u"), v));

        cui_widget_append_child(hsv_row, app.input_v1);
    }

    cui_end_temporary_memory(temp_memory);

    cui_widget_append_child(parent_widget, hsv_row);
}

static void
create_hsl_row(CuiWidget *parent_widget, CuiArena *arena)
{
    CuiWidget *hsl_row = create_widget(arena, CUI_WIDGET_TYPE_BOX);

    cui_widget_set_main_axis(hsl_row, CUI_AXIS_X);
    cui_widget_set_x_axis_gravity(hsl_row, CUI_GRAVITY_START);
    cui_widget_set_inline_padding(hsl_row, 8.0f);
    cui_widget_set_padding(hsl_row, 12.0f, 0.0f, 0.0f, 0.0f);

    uint32_t h = (uint32_t) (app.hsl_color.r + 0.5f) & 0xff;
    uint32_t s = (uint32_t) ((app.hsl_color.g * 100.0f) + 0.5f) & 0xff;
    uint32_t l = (uint32_t) ((app.hsl_color.b * 100.0f) + 0.5f) & 0xff;

    CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(&app.temporary_memory);

    {
        app.input_h2 = create_widget(arena, CUI_WIDGET_TYPE_TEXTINPUT);

        cui_widget_set_icon(app.input_h2, CUI_ICON_UPPERCASE_H_12);
        cui_widget_set_preferred_size(app.input_h2, 48.0f, 0.0f);
        cui_widget_set_x_axis_gravity(app.input_h2, CUI_GRAVITY_END);
        cui_widget_set_textinput_buffer(app.input_h2, cui_alloc(arena, TEXTINPUT_NUMBER_BUFFER_SIZE, CuiDefaultAllocationParams()), TEXTINPUT_NUMBER_BUFFER_SIZE);
        cui_widget_set_textinput_value(app.input_h2, cui_sprint(&app.temporary_memory, CuiStringLiteral("%u"), h));

        cui_widget_append_child(hsl_row, app.input_h2);
    }

    {
        app.input_s2 = create_widget(arena, CUI_WIDGET_TYPE_TEXTINPUT);

        cui_widget_set_icon(app.input_s2, CUI_ICON_UPPERCASE_S_12);
        cui_widget_set_preferred_size(app.input_s2, 48.0f, 0.0f);
        cui_widget_set_x_axis_gravity(app.input_s2, CUI_GRAVITY_END);
        cui_widget_set_textinput_buffer(app.input_s2, cui_alloc(arena, TEXTINPUT_NUMBER_BUFFER_SIZE, CuiDefaultAllocationParams()), TEXTINPUT_NUMBER_BUFFER_SIZE);
        cui_widget_set_textinput_value(app.input_s2, cui_sprint(&app.temporary_memory, CuiStringLiteral("%u"), s));

        cui_widget_append_child(hsl_row, app.input_s2);
    }

    {
        app.input_l2 = create_widget(arena, CUI_WIDGET_TYPE_TEXTINPUT);

        cui_widget_set_icon(app.input_l2, CUI_ICON_UPPERCASE_L_12);
        cui_widget_set_preferred_size(app.input_l2, 48.0f, 0.0f);
        cui_widget_set_x_axis_gravity(app.input_l2, CUI_GRAVITY_END);
        cui_widget_set_textinput_buffer(app.input_l2, cui_alloc(arena, TEXTINPUT_NUMBER_BUFFER_SIZE, CuiDefaultAllocationParams()), TEXTINPUT_NUMBER_BUFFER_SIZE);
        cui_widget_set_textinput_value(app.input_l2, cui_sprint(&app.temporary_memory, CuiStringLiteral("%u"), l));

        cui_widget_append_child(hsl_row, app.input_l2);
    }

    cui_end_temporary_memory(temp_memory);

    cui_widget_append_child(parent_widget, hsl_row);
}

static void
on_copy_color(CuiWidget *widget)
{
    CuiString content = widget->label;

    if (content.count)
    {
        CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(&app.temporary_memory);
        cui_platform_set_clipboard_text(&app.temporary_memory, content);
        cui_end_temporary_memory(temp_memory);
    }
}

static void
on_add_color(CuiWidget *widget)
{
    (void) widget;

    bool not_found = true;
    CuiColor color_to_add = app.rgb_color;

    if (app.color_list)
    {
        int32_t color_count = cui_array_count(app.color_list->colors);

        for (int32_t i = 0; i < color_count; i += 1)
        {
            CuiColor color = app.color_list->colors[i];

            if ((color.r == color_to_add.r) &&
                (color.g == color_to_add.g) &&
                (color.b == color_to_add.b) &&
                (color.a == color_to_add.a))
            {
                not_found = false;
                break;
            }
        }
    }
    else
    {
        app.color_list = create_color_list(&app.widget_arena);

        cui_widget_insert_before(app.color_list_row, app.color_add_button, &app.color_list->base);
    }

    if (not_found)
    {
        CuiColor *color = cui_array_append(app.color_list->colors);
        *color = color_to_add;

        cui_widget_relayout_parent(&app.color_list->base);
        cui_window_request_redraw(app.window);
    }
}

static void
create_color_copy_row(CuiWidget *parent_widget, CuiArena *arena)
{
    CuiWidget *color_copy_row = create_widget(arena, CUI_WIDGET_TYPE_BOX);

    cui_widget_set_main_axis(color_copy_row, CUI_AXIS_Y);
    cui_widget_set_y_axis_gravity(color_copy_row, CUI_GRAVITY_START);
    cui_widget_set_inline_padding(color_copy_row, 8.0f);
    cui_widget_set_padding(color_copy_row, 0.0f, 12.0f, 12.0f, 12.0f);

    CuiWidget *css_hex_row = create_widget(arena, CUI_WIDGET_TYPE_BOX);

    cui_widget_set_main_axis(css_hex_row, CUI_AXIS_X);
    cui_widget_set_x_axis_gravity(css_hex_row, CUI_GRAVITY_CENTER);
    cui_widget_set_inline_padding(css_hex_row, 8.0f);

    app.copy_css_hex_small = create_widget(arena, CUI_WIDGET_TYPE_BUTTON);
    cui_widget_set_x_axis_gravity(app.copy_css_hex_small, CUI_GRAVITY_CENTER);
    app.copy_css_hex_small->on_action = on_copy_color;
    cui_widget_append_child(css_hex_row, app.copy_css_hex_small);

    app.copy_css_hex_big = create_widget(arena, CUI_WIDGET_TYPE_BUTTON);
    cui_widget_set_x_axis_gravity(app.copy_css_hex_big, CUI_GRAVITY_CENTER);
    app.copy_css_hex_big->on_action = on_copy_color;
    cui_widget_append_child(css_hex_row, app.copy_css_hex_big);

    cui_widget_append_child(color_copy_row, css_hex_row);

    app.copy_css_rgb = create_widget(arena, CUI_WIDGET_TYPE_BUTTON);
    cui_widget_set_x_axis_gravity(app.copy_css_rgb, CUI_GRAVITY_CENTER);
    app.copy_css_rgb->on_action = on_copy_color;
    cui_widget_append_child(color_copy_row, app.copy_css_rgb);

    app.copy_c_hex = create_widget(arena, CUI_WIDGET_TYPE_BUTTON);
    cui_widget_set_x_axis_gravity(app.copy_c_hex, CUI_GRAVITY_CENTER);
    app.copy_c_hex->on_action = on_copy_color;
    cui_widget_append_child(color_copy_row, app.copy_c_hex);

    app.copy_c_float = create_widget(arena, CUI_WIDGET_TYPE_BUTTON);
    cui_widget_set_x_axis_gravity(app.copy_c_float, CUI_GRAVITY_CENTER);
    app.copy_c_float->on_action = on_copy_color;
    cui_widget_append_child(color_copy_row, app.copy_c_float);

    app.copy_c_float_braces = create_widget(arena, CUI_WIDGET_TYPE_BUTTON);
    cui_widget_set_x_axis_gravity(app.copy_c_float_braces, CUI_GRAVITY_CENTER);
    app.copy_c_float_braces->on_action = on_copy_color;
    cui_widget_append_child(color_copy_row, app.copy_c_float_braces);

    app.copy_shader_vec3 = create_widget(arena, CUI_WIDGET_TYPE_BUTTON);
    cui_widget_set_x_axis_gravity(app.copy_shader_vec3, CUI_GRAVITY_CENTER);
    app.copy_shader_vec3->on_action = on_copy_color;
    cui_widget_append_child(color_copy_row, app.copy_shader_vec3);

    app.copy_shader_vec4 = create_widget(arena, CUI_WIDGET_TYPE_BUTTON);
    cui_widget_set_x_axis_gravity(app.copy_shader_vec4, CUI_GRAVITY_CENTER);
    app.copy_shader_vec4->on_action = on_copy_color;
    cui_widget_append_child(color_copy_row, app.copy_shader_vec4);

    app.copy_shader_float3 = create_widget(arena, CUI_WIDGET_TYPE_BUTTON);
    cui_widget_set_x_axis_gravity(app.copy_shader_float3, CUI_GRAVITY_CENTER);
    app.copy_shader_float3->on_action = on_copy_color;
    cui_widget_append_child(color_copy_row, app.copy_shader_float3);

    app.copy_shader_float4 = create_widget(arena, CUI_WIDGET_TYPE_BUTTON);
    cui_widget_set_x_axis_gravity(app.copy_shader_float4, CUI_GRAVITY_CENTER);
    app.copy_shader_float4->on_action = on_copy_color;
    cui_widget_append_child(color_copy_row, app.copy_shader_float4);

    cui_widget_append_child(parent_widget, color_copy_row);
}

static void
create_user_interface(CuiWindow *window, CuiArena *arena)
{
    app.root_widget = create_widget(arena, CUI_WIDGET_TYPE_BOX);

    cui_widget_set_main_axis(app.root_widget, CUI_AXIS_Y);
    cui_widget_set_y_axis_gravity(app.root_widget, CUI_GRAVITY_START);
    cui_widget_add_flags(app.root_widget, CUI_WIDGET_FLAG_DRAW_BACKGROUND);

    app.color_list_row = create_widget(arena, CUI_WIDGET_TYPE_BOX);

    cui_widget_set_main_axis(app.color_list_row, CUI_AXIS_X);
    cui_widget_set_x_axis_gravity(app.color_list_row, CUI_GRAVITY_START);
    cui_widget_add_flags(app.color_list_row, CUI_WIDGET_FLAG_DRAW_BACKGROUND);
    cui_widget_set_inline_padding(app.color_list_row, 8.0f);
    cui_widget_set_padding(app.color_list_row, 8.0f, 12.0f, 8.0f, 12.0f);

    app.color_list_row->color_normal_background = CUI_COLOR_WINDOW_TITLEBAR_BACKGROUND;

    cui_widget_append_child(app.root_widget, app.color_list_row);

    app.color_add_button = create_widget(arena, CUI_WIDGET_TYPE_BUTTON);
    cui_widget_set_x_axis_gravity(app.color_add_button, CUI_GRAVITY_START);
    cui_widget_set_box_shadow(app.color_add_button, 0.0f, 0.0f, 0.0f);
    cui_widget_set_border_width(app.color_add_button, 1.0f, 1.0f, 1.0f, 1.0f);
    cui_widget_set_padding(app.color_add_button, 7.0f, 7.0f, 7.0f, 7.0f);
    cui_widget_set_icon(app.color_add_button, CUI_ICON_PLUS_12);
    app.color_add_button->on_action = on_add_color;
    cui_widget_append_child(app.color_list_row, app.color_add_button);

    CuiWidget *_padding_widget = create_widget(arena, CUI_WIDGET_TYPE_BOX);
    cui_widget_append_child(app.color_list_row, _padding_widget);

    CuiWidget *color_input_row = create_widget(arena, CUI_WIDGET_TYPE_BOX);

    cui_widget_set_main_axis(color_input_row, CUI_AXIS_X);
    cui_widget_set_x_axis_gravity(color_input_row, CUI_GRAVITY_START);
    cui_widget_set_inline_padding(color_input_row, 8.0f);
    cui_widget_set_padding(color_input_row, 12.0f, 12.0f, 12.0f, 12.0f);

    cui_widget_append_child(app.root_widget, color_input_row);

    CuiWidget *color_column = create_widget(arena, CUI_WIDGET_TYPE_BOX);

    cui_widget_set_main_axis(color_column, CUI_AXIS_Y);
    cui_widget_set_y_axis_gravity(color_column, CUI_GRAVITY_END);
    cui_widget_set_inline_padding(color_column, 12.0f);

    cui_widget_append_child(color_input_row, color_column);

    uint32_t a = (uint32_t) ((app.rgb_color.a * 255.0f) + 0.5f) & 0xff;

    {
        app.input_a = create_widget(arena, CUI_WIDGET_TYPE_TEXTINPUT);

        cui_widget_set_icon(app.input_a, CUI_ICON_UPPERCASE_A_12);
        cui_widget_set_preferred_size(app.input_a, 48.0f, 0.0f);
        cui_widget_set_x_axis_gravity(app.input_a, CUI_GRAVITY_END);
        cui_widget_set_textinput_buffer(app.input_a, cui_alloc(arena, TEXTINPUT_NUMBER_BUFFER_SIZE, CuiDefaultAllocationParams()),
                                        TEXTINPUT_NUMBER_BUFFER_SIZE);
        cui_widget_set_textinput_value(app.input_a, cui_sprint(&app.temporary_memory, CuiStringLiteral("%u"), a));

        cui_widget_append_child(color_column, app.input_a);
    }

    app.color_preview = create_color_preview(arena);

    cui_widget_append_child(color_column, &app.color_preview->base);

    CuiWidget *input_column = create_widget(arena, CUI_WIDGET_TYPE_BOX);

    cui_widget_set_main_axis(input_column, CUI_AXIS_Y);
    cui_widget_set_y_axis_gravity(input_column, CUI_GRAVITY_START);

    cui_widget_append_child(color_input_row, input_column);

    create_rgb_row(input_column, arena);
    create_hsv_row(input_column, arena);
    create_hsl_row(input_column, arena);
    create_color_copy_row(app.root_widget, arena);

    cui_window_set_root_widget(window, app.root_widget);
}

CUI_PLATFORM_MAIN
{
    if (!CUI_PLATFORM_INIT)
    {
        return -1;
    }

    cui_arena_allocate(&app.temporary_memory, CuiKiB(4));
    cui_arena_allocate(&app.widget_arena, CuiKiB(32));
    cui_arena_allocate(&app.output_arena, CuiKiB(8));

    app.window = cui_window_create(CUI_WINDOW_CREATION_FLAG_NOT_USER_RESIZABLE);

    if (!app.window)
    {
        return -1;
    }

    cui_window_set_title(app.window, CuiStringLiteral("Color Tool"));
    cui_window_set_color_theme(app.window, &color_tool_color_theme);

    app.rgb_color = CuiHexColor(0xffffd700);
    app.hsv_color = cui_color_rgb_to_hsv(app.rgb_color);
    app.hsl_color = cui_color_rgb_to_hsl(app.rgb_color);

    create_user_interface(app.window, &app.widget_arena);

    update_output();

    cui_window_pack(app.window);
    cui_window_show(app.window);

    return cui_main_loop();
}
