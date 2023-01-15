static void
_cui_widget_draw_box_shadow(CuiGraphicsContext *ctx, CuiWidget *widget, const CuiColorTheme *color_theme)
{
    int32_t r00 = widget->effective_border_radius.min.y;
    int32_t r10 = widget->effective_border_radius.max.x;
    int32_t r11 = widget->effective_border_radius.max.y;
    int32_t r01 = widget->effective_border_radius.min.x;

    int32_t blur_radius = widget->effective_blur_radius;
    int32_t x_offset = widget->effective_shadow_offset.x;
    int32_t y_offset = widget->effective_shadow_offset.y;

    CuiAssert(blur_radius > 0);

    CuiRect rect = widget->rect;

    rect.min.x += x_offset;
    rect.max.x += x_offset;
    rect.min.y += y_offset;
    rect.max.y += y_offset;

    int32_t x_min = rect.min.x + blur_radius;
    int32_t y_min = rect.min.y + blur_radius;
    int32_t x_max = rect.max.x - blur_radius;
    int32_t y_max = rect.max.y - blur_radius;

    CuiColor box_shadow_color = cui_color_theme_get_color(color_theme, widget->color_normal_box_shadow);

    cui_draw_fill_shadow(ctx, x_min + r00, rect.min.y, x_max - r10, blur_radius, CUI_DIRECTION_NORTH, box_shadow_color);
    cui_draw_fill_shadow(ctx, rect.max.x, y_min + r10, y_max - r11, blur_radius, CUI_DIRECTION_EAST, box_shadow_color);
    cui_draw_fill_shadow(ctx, x_min + r01, rect.max.y, x_max - r11, blur_radius, CUI_DIRECTION_SOUTH, box_shadow_color);
    cui_draw_fill_shadow(ctx, rect.min.x, y_min + r00, y_max - r01, blur_radius, CUI_DIRECTION_WEST, box_shadow_color);

    cui_draw_fill_shadow_corner(ctx, rect.min.x, rect.min.y, r00, blur_radius, CUI_DIRECTION_WEST, CUI_DIRECTION_NORTH, box_shadow_color);
    cui_draw_fill_shadow_corner(ctx, rect.max.x, rect.min.y, r10, blur_radius, CUI_DIRECTION_EAST, CUI_DIRECTION_NORTH, box_shadow_color);
    cui_draw_fill_shadow_corner(ctx, rect.max.x, rect.max.y, r11, blur_radius, CUI_DIRECTION_EAST, CUI_DIRECTION_SOUTH, box_shadow_color);
    cui_draw_fill_shadow_corner(ctx, rect.min.x, rect.max.y, r01, blur_radius, CUI_DIRECTION_WEST, CUI_DIRECTION_SOUTH, box_shadow_color);
}

static void
_cui_widget_draw_background(CuiGraphicsContext *ctx, CuiWidget *widget, const CuiColorTheme *color_theme)
{
    int32_t b0 = widget->effective_border_width.min.y;
    int32_t b1 = widget->effective_border_width.max.x;
    int32_t b2 = widget->effective_border_width.max.y;
    int32_t b3 = widget->effective_border_width.min.x;

    // TODO: handle each border separately
    int32_t border_width = cui_max_int32(cui_max_int32(b0, b1), cui_max_int32(b2, b3));

    int32_t r00 = widget->effective_border_radius.min.y;
    int32_t r10 = widget->effective_border_radius.max.x;
    int32_t r11 = widget->effective_border_radius.max.y;
    int32_t r01 = widget->effective_border_radius.min.x;

    int32_t has_border = b0 | b1 | b2 | b3;
    int32_t has_radius = r00 | r10 | r11 | r01;

    CuiColorThemeId background_id = widget->color_normal_background;
    CuiColorThemeId border_id = widget->color_normal_border;

    CuiColor background_color = cui_color_theme_get_color(color_theme, background_id);

    if (has_border)
    {
        CuiRect border_rect = widget->rect;
        CuiRect content_rect = widget->rect;

        content_rect.min.y += b0;
        content_rect.max.x -= b1;
        content_rect.max.y -= b2;
        content_rect.min.x += b3;

        CuiColor border_color = cui_color_theme_get_color(color_theme, border_id);

        if (has_radius)
        {
            cui_draw_stroke_rounded_rect_4(ctx, border_rect, (float) r00, (float) r10, (float) r11, (float) r01, border_width, border_color);

            int32_t r00_x = cui_max_int32(r00 - b3, 0);
            int32_t r00_y = cui_max_int32(r00 - b0, 0);
            int32_t r10_x = cui_max_int32(r10 - b1, 0);
            int32_t r10_y = cui_max_int32(r10 - b0, 0);
            int32_t r11_x = cui_max_int32(r11 - b1, 0);
            int32_t r11_y = cui_max_int32(r11 - b2, 0);
            int32_t r01_x = cui_max_int32(r01 - b3, 0);
            int32_t r01_y = cui_max_int32(r01 - b2, 0);

            cui_draw_fill_rounded_rect_8(ctx, content_rect, (float) r00_x, (float) r00_y, (float) r10_x, (float) r10_y,
                                         (float) r11_x, (float) r11_y, (float) r01_x, (float) r01_y, background_color);
        }
        else
        {
            cui_draw_fill_rect(ctx, border_rect, border_color);
            cui_draw_fill_rect(ctx, content_rect, background_color);
        }
    }
    else
    {
        if (has_radius)
        {
            cui_draw_fill_rounded_rect_4(ctx, widget->rect, (float) r00, (float) r10, (float) r11, (float) r01, background_color);
        }
        else
        {
            cui_draw_fill_rect(ctx, widget->rect, background_color);
        }
    }
}

static void
_cui_widget_update_text_offset(CuiWidget *widget)
{
    CuiAssert(widget->type == CUI_WIDGET_TYPE_TEXTINPUT);

    CuiAssert(widget->window);
    CuiWindow *window = widget->window;

    CuiFontId font_id = widget->font_id;

    if (font_id.value == 0)
    {
        font_id = window->font_id;
    }

    int32_t padding_x = widget->effective_padding.min.x + widget->effective_padding.max.x;
    int32_t padding_y = widget->effective_padding.min.y + widget->effective_padding.max.y;

    int32_t border_width_x = widget->effective_border_width.min.x + widget->effective_border_width.max.x;
    int32_t border_width_y = widget->effective_border_width.min.y + widget->effective_border_width.max.y;

    int32_t content_width  = cui_rect_get_width(widget->rect) - (padding_x + border_width_x);
    int32_t content_height = cui_rect_get_height(widget->rect) - (padding_y + border_width_y);

    CuiFont *font = _cui_font_manager_get_font_from_id(&window->base.font_manager, font_id);

    widget->text_offset = cui_make_float_point((float) (widget->effective_padding.min.x + widget->effective_border_width.min.x),
                                                (float) (widget->effective_padding.min.y + widget->effective_border_width.min.y) + font->baseline_offset);

    if (widget->icon_type)
    {
        int32_t text_offset = widget->effective_icon_size.x + widget->effective_inline_padding;
        widget->text_offset.x += (float) text_offset;
        content_width -= text_offset;
    }

    switch (widget->x_gravity)
    {
        case CUI_GRAVITY_START: break;

        case CUI_GRAVITY_CENTER:
        {
            CuiString text = cui_text_input_to_string(widget->text_input);
            float text_width = _cui_font_get_string_width(&window->base.font_manager, font_id, text);
            widget->text_offset.x += 0.5f * ((float) content_width - text_width);
        } break;

        case CUI_GRAVITY_END:
        {
            CuiString text = cui_text_input_to_string(widget->text_input);
            float text_width = _cui_font_get_string_width(&window->base.font_manager, font_id, text);
            widget->text_offset.x += (float) content_width - text_width;
        } break;
    }

    switch (widget->y_gravity)
    {
        case CUI_GRAVITY_START: break;

        case CUI_GRAVITY_CENTER:
        {
            widget->text_offset.y += 0.5f * ((float) (content_height - font->line_height));
        } break;

        case CUI_GRAVITY_END:
        {
            widget->text_offset.y += (float) (content_height - font->line_height);
        } break;
    }
}

void
cui_widget_init(CuiWidget *widget, uint32_t type)
{
    CuiClearStruct(*widget);

    widget->type = type;
    widget->color_normal_background = CUI_COLOR_DEFAULT_BG;
    widget->color_normal_border     = CUI_COLOR_DEFAULT_BORDER;
    widget->color_normal_text       = CUI_COLOR_DEFAULT_FG;

    CuiDListInit(&widget->list);
    CuiDListInit(&widget->children);

    switch (widget->type)
    {
        case CUI_WIDGET_TYPE_BOX:
        {
        } break;

        case CUI_WIDGET_TYPE_STACK:
        {
        } break;

        case CUI_WIDGET_TYPE_LABEL:
        {
        } break;

        case CUI_WIDGET_TYPE_BUTTON:
        {
            widget->flags = CUI_WIDGET_FLAG_DRAW_BACKGROUND;

            widget->color_normal_background = CUI_COLOR_DEFAULT_BUTTON_NORMAL_BACKGROUND;
            widget->color_normal_box_shadow = CUI_COLOR_DEFAULT_BUTTON_NORMAL_BOX_SHADOW;
            widget->color_normal_border     = CUI_COLOR_DEFAULT_BUTTON_NORMAL_BORDER;
            widget->color_normal_text       = CUI_COLOR_DEFAULT_BUTTON_NORMAL_TEXT;
            widget->color_normal_icon       = CUI_COLOR_DEFAULT_BUTTON_NORMAL_ICON;

            widget->inline_padding = 8.0f;
            widget->padding = cui_make_float_rect(8.0f, 4.0f, 8.0f, 4.0f);
            // widget->border_width = cui_make_float_rect(1.0f, 1.0f, 1.0f, 1.0f);
            widget->border_radius = cui_make_float_rect(2.0f, 2.0f, 2.0f, 2.0f);
            widget->blur_radius = 2.0f;
            widget->shadow_offset = cui_make_float_point(0.0f, 1.0f);
        } break;

        case CUI_WIDGET_TYPE_CHECKBOX:
        {
            widget->inline_padding = 4.0f;
            widget->y_gravity = CUI_GRAVITY_CENTER;
        } break;

        case CUI_WIDGET_TYPE_TEXTINPUT:
        {
            widget->flags = CUI_WIDGET_FLAG_FIXED_WIDTH;

            widget->color_normal_background = CUI_COLOR_DEFAULT_TEXTINPUT_NORMAL_BACKGROUND;
            widget->color_normal_box_shadow = CUI_COLOR_DEFAULT_TEXTINPUT_NORMAL_BOX_SHADOW;
            widget->color_normal_border     = CUI_COLOR_DEFAULT_TEXTINPUT_NORMAL_BORDER;
            widget->color_normal_text       = CUI_COLOR_DEFAULT_TEXTINPUT_NORMAL_TEXT;
            widget->color_normal_icon       = CUI_COLOR_DEFAULT_TEXTINPUT_NORMAL_ICON;

            widget->preferred_size.x = 64.0f;

            widget->inline_padding = 4.0f;

            widget->padding = cui_make_float_rect(6.0f, 4.0f, 6.0f, 4.0f);
            widget->border_width = cui_make_float_rect(1.0f, 1.0f, 1.0f, 1.0f);
            widget->border_radius = cui_make_float_rect(4.0f, 4.0f, 4.0f, 4.0f);
        } break;
    }
}

void
cui_widget_add_flags(CuiWidget *widget, uint32_t flags)
{
    widget->flags |= flags;
}

void
cui_widget_remove_flags(CuiWidget *widget, uint32_t flags)
{
    widget->flags &= ~flags;
}

void
cui_widget_set_window(CuiWidget *widget, CuiWindow *window)
{
    CuiAssert(window);

    widget->window = window;
    cui_widget_set_ui_scale(widget, window->base.ui_scale);

    CuiForEachWidget(child, &widget->children)
    {
        cui_widget_set_window(child, window);
    }
}

void
cui_widget_set_textinput_buffer(CuiWidget *widget, void *buffer, int64_t size)
{
    widget->text_input.cursor_start = 0;
    widget->text_input.cursor_end = 0;
    widget->text_input.count = 0;
    widget->text_input.capacity = size;
    widget->text_input.data = (uint8_t *) buffer;
}

void
cui_widget_set_textinput_value(CuiWidget *widget, CuiString value)
{
    // TODO: clamp the value count to the last possible character
    CuiAssert(value.count <= widget->text_input.capacity);

    widget->text_input.count = value.count;

    uint8_t *src = value.data;
    uint8_t *dst = widget->text_input.data;

    cui_copy_memory(dst, src, value.count);

    widget->text_input.cursor_start = 0;
    widget->text_input.cursor_end = 0;

    if (widget->window)
    {
        _cui_widget_update_text_offset(widget);
    }
}

CuiString
cui_widget_get_textinput_value(CuiWidget *widget)
{
    return cui_text_input_to_string(widget->text_input);
}

CuiWidget *
cui_widget_get_first_child(CuiWidget *widget)
{
    CuiWidget *result = 0;

    if (!CuiDListIsEmpty(&widget->children))
    {
        result = CuiContainerOf(widget->children.next, CuiWidget, list);
    }

    return result;
}

void
cui_widget_append_child(CuiWidget *widget, CuiWidget *child)
{
    child->parent = widget;
    CuiDListInsertBefore(&widget->children, &child->list);

    if (widget->window)
    {
        cui_widget_set_window(child, widget->window);
    }
}

void
cui_widget_insert_before(CuiWidget *widget, CuiWidget *anchor_child, CuiWidget *new_child)
{
#if CUI_DEBUG_BUILD
    CuiAssert(anchor_child->parent == widget);

    CuiWidget *found_widget = 0;

    CuiForEachWidget(child, &widget->children)
    {
        if (child == anchor_child)
        {
            found_widget = child;
            break;
        }
    }

    CuiAssert(found_widget);
#endif

    new_child->parent = widget;
    CuiDListInsertBefore(&anchor_child->list, &new_child->list);

    if (widget->window)
    {
        cui_widget_set_window(new_child, widget->window);
    }
}

void
cui_widget_replace_child(CuiWidget *widget, CuiWidget *old_child, CuiWidget *new_child)
{
#if CUI_DEBUG_BUILD
    CuiAssert(old_child->parent == widget);

    CuiWidget *found_widget = 0;

    CuiForEachWidget(child, &widget->children)
    {
        if (child == old_child)
        {
            found_widget = child;
            break;
        }
    }

    CuiAssert(found_widget);
#endif

    new_child->parent = widget;
    CuiDListInsertBefore(&old_child->list, &new_child->list);
    CuiDListRemove(&old_child->list);

    if (widget->window)
    {
        cui_widget_set_window(new_child, widget->window);
    }
}

void
cui_widget_remove_child(CuiWidget *widget, CuiWidget *old_child)
{
#if CUI_DEBUG_BUILD
    CuiAssert(old_child->parent == widget);

    CuiWidget *found_widget = 0;

    CuiForEachWidget(child, &widget->children)
    {
        if (child == old_child)
        {
            found_widget = child;
            break;
        }
    }

    CuiAssert(found_widget);
#endif

    CuiDListRemove(&old_child->list);
}

void
cui_widget_set_main_axis(CuiWidget *widget, CuiAxis axis)
{
    widget->main_axis = axis;
}

void
cui_widget_set_x_axis_gravity(CuiWidget *widget, CuiGravity gravity)
{
    widget->x_gravity = gravity;
}

void
cui_widget_set_y_axis_gravity(CuiWidget *widget, CuiGravity gravity)
{
    widget->y_gravity = gravity;
}

void
cui_widget_set_value(CuiWidget *widget, uint32_t value)
{
    widget->value = value;
}

void
cui_widget_set_label(CuiWidget *widget, CuiString label)
{
    widget->label = label;
}

void
cui_widget_set_icon(CuiWidget *widget, CuiIconType icon_type)
{
    widget->icon_type = icon_type;
}

void
cui_widget_set_inline_padding(CuiWidget *widget, float padding)
{
    widget->inline_padding = padding;

    widget->effective_inline_padding = lroundf(widget->ui_scale * widget->inline_padding);
}

void
cui_widget_set_preferred_size(CuiWidget *widget, float width, float height)
{
    widget->preferred_size.x = width;
    widget->preferred_size.y = height;

    widget->effective_preferred_size.x = lroundf(widget->ui_scale * widget->preferred_size.x);
    widget->effective_preferred_size.y = lroundf(widget->ui_scale * widget->preferred_size.y);
}

void
cui_widget_set_padding(CuiWidget *widget, float padding_top, float padding_right, float padding_bottom, float padding_left)
{
    widget->padding.min.x = padding_left;
    widget->padding.min.y = padding_top;
    widget->padding.max.x = padding_right;
    widget->padding.max.y = padding_bottom;

    widget->effective_padding.min.x = lroundf(widget->ui_scale * widget->padding.min.x);
    widget->effective_padding.min.y = lroundf(widget->ui_scale * widget->padding.min.y);
    widget->effective_padding.max.x = lroundf(widget->ui_scale * widget->padding.max.x);
    widget->effective_padding.max.y = lroundf(widget->ui_scale * widget->padding.max.y);
}

void
cui_widget_set_border_width(CuiWidget *widget, float border_top, float border_right, float border_bottom, float border_left)
{
    widget->border_width.min.x = border_left;
    widget->border_width.min.y = border_top;
    widget->border_width.max.x = border_right;
    widget->border_width.max.y = border_bottom;

    widget->effective_border_width.min.x = lroundf(widget->ui_scale * widget->border_width.min.x);
    widget->effective_border_width.min.y = lroundf(widget->ui_scale * widget->border_width.min.y);
    widget->effective_border_width.max.x = lroundf(widget->ui_scale * widget->border_width.max.x);
    widget->effective_border_width.max.y = lroundf(widget->ui_scale * widget->border_width.max.y);
}

void
cui_widget_set_border_radius(CuiWidget *widget, float topleft, float topright, float bottomright, float bottomleft)
{
    widget->border_radius.min.x = bottomleft;
    widget->border_radius.min.y = topleft;
    widget->border_radius.max.x = topright;
    widget->border_radius.max.y = bottomright;

    widget->effective_border_radius.min.x = lroundf(widget->ui_scale * widget->border_radius.min.x);
    widget->effective_border_radius.min.y = lroundf(widget->ui_scale * widget->border_radius.min.y);
    widget->effective_border_radius.max.x = lroundf(widget->ui_scale * widget->border_radius.max.x);
    widget->effective_border_radius.max.y = lroundf(widget->ui_scale * widget->border_radius.max.y);
}

void
cui_widget_set_box_shadow(CuiWidget *widget, float x_offset, float y_offset, float blur_radius)
{
    widget->blur_radius = blur_radius;
    widget->shadow_offset.x = x_offset;
    widget->shadow_offset.y = y_offset;

    widget->effective_blur_radius = lroundf(widget->ui_scale * widget->blur_radius);
    widget->effective_shadow_offset.x = lroundf(widget->ui_scale * widget->shadow_offset.x);
    widget->effective_shadow_offset.y = lroundf(widget->ui_scale * widget->shadow_offset.y);
}

bool
cui_widget_contains(CuiWidget *widget, CuiWidget *child)
{
    for (; child; child = child->parent)
    {
        if (child == widget)
        {
            return true;
        }
    }

    return false;
}

void
cui_widget_set_ui_scale(CuiWidget *widget, float ui_scale)
{
    if (widget->ui_scale == ui_scale)
    {
        return;
    }

    widget->ui_scale = ui_scale;

    widget->effective_padding.min.x = lroundf(widget->ui_scale * widget->padding.min.x);
    widget->effective_padding.min.y = lroundf(widget->ui_scale * widget->padding.min.y);
    widget->effective_padding.max.x = lroundf(widget->ui_scale * widget->padding.max.x);
    widget->effective_padding.max.y = lroundf(widget->ui_scale * widget->padding.max.y);

    widget->effective_border_width.min.x = lroundf(widget->ui_scale * widget->border_width.min.x);
    widget->effective_border_width.min.y = lroundf(widget->ui_scale * widget->border_width.min.y);
    widget->effective_border_width.max.x = lroundf(widget->ui_scale * widget->border_width.max.x);
    widget->effective_border_width.max.y = lroundf(widget->ui_scale * widget->border_width.max.y);

    widget->effective_border_radius.min.x = lroundf(widget->ui_scale * widget->border_radius.min.x);
    widget->effective_border_radius.min.y = lroundf(widget->ui_scale * widget->border_radius.min.y);
    widget->effective_border_radius.max.x = lroundf(widget->ui_scale * widget->border_radius.max.x);
    widget->effective_border_radius.max.y = lroundf(widget->ui_scale * widget->border_radius.max.y);

    widget->effective_preferred_size.x = lroundf(widget->ui_scale * widget->preferred_size.x);
    widget->effective_preferred_size.y = lroundf(widget->ui_scale * widget->preferred_size.y);

    widget->effective_shadow_offset.x = lroundf(widget->ui_scale * widget->shadow_offset.x);
    widget->effective_shadow_offset.y = lroundf(widget->ui_scale * widget->shadow_offset.y);

    widget->effective_inline_padding = lroundf(widget->ui_scale * widget->inline_padding);

    widget->effective_blur_radius = lroundf(widget->ui_scale * widget->blur_radius);

    if (widget->type >= CUI_WIDGET_TYPE_CUSTOM)
    {
        widget->set_ui_scale(widget, ui_scale);
        return;
    }

    switch (widget->type)
    {
        case CUI_WIDGET_TYPE_BOX:
        {
            CuiForEachWidget(child, &widget->children)
            {
                cui_widget_set_ui_scale(child, ui_scale);
            }
        } break;

        case CUI_WIDGET_TYPE_STACK:
        {
            CuiForEachWidget(child, &widget->children)
            {
                cui_widget_set_ui_scale(child, ui_scale);
            }
        } break;

        case CUI_WIDGET_TYPE_LABEL:
        {
            int32_t icon_size = (widget->icon_type >> CUI_ICON_SIZE_SHIFT) & 0xff;

            widget->effective_icon_size.x = (int32_t) ceilf(widget->ui_scale * (float) icon_size);
            widget->effective_icon_size.y = widget->effective_icon_size.x;
        } break;

        case CUI_WIDGET_TYPE_BUTTON:
        {
            int32_t icon_size = (widget->icon_type >> CUI_ICON_SIZE_SHIFT) & 0xff;

            widget->effective_icon_size.x = (int32_t) ceilf(widget->ui_scale * (float) icon_size);
            widget->effective_icon_size.y = widget->effective_icon_size.x;
        } break;

        case CUI_WIDGET_TYPE_CHECKBOX:
        {
            // width & height of the checkmark icon
            widget->effective_icon_size.x = (int32_t) ceilf(widget->ui_scale * 16.0f);
            widget->effective_icon_size.y = widget->effective_icon_size.x;
        } break;

        case CUI_WIDGET_TYPE_TEXTINPUT:
        {
            int32_t icon_size = (widget->icon_type >> CUI_ICON_SIZE_SHIFT) & 0xff;

            widget->effective_icon_size.x = (int32_t) ceilf(widget->ui_scale * (float) icon_size);
            widget->effective_icon_size.y = widget->effective_icon_size.x;
        } break;
    }
}

void
cui_widget_set_font(CuiWidget *widget, CuiFontId font_id)
{
    widget->font_id = font_id;
}

void
cui_widget_set_color_theme(CuiWidget *widget, const CuiColorTheme *color_theme)
{
    widget->color_theme = color_theme;
}

void
cui_widget_relayout_parent(CuiWidget *widget)
{
    CuiWidget *parent = widget->parent;
    CuiAssert(parent);

    cui_widget_layout(parent, parent->rect);
}

CuiPoint
cui_widget_get_preferred_size(CuiWidget *widget)
{
    CuiPoint result = cui_make_point(0, 0);

    if (widget->type >= CUI_WIDGET_TYPE_CUSTOM)
    {
        return widget->get_preferred_size(widget);
    }

    CuiAssert(widget->window);
    CuiWindow *window = widget->window;

    CuiFontId font_id = widget->font_id;

    if (font_id.value == 0)
    {
        font_id = window->font_id;
    }

    switch (widget->type)
    {
        case CUI_WIDGET_TYPE_BOX:
        {
            int32_t width = 0;
            int32_t height = 0;

            switch (widget->main_axis)
            {
                case CUI_AXIS_X:
                {
                    if (!CuiDListIsEmpty(&widget->children))
                    {
                        width -= widget->effective_inline_padding;

                        CuiForEachWidget(child, &widget->children)
                        {
                            CuiPoint size = cui_widget_get_preferred_size(child);

                            width += size.x + widget->effective_inline_padding;
                            height = cui_max_int32(height, size.y);
                        }
                    }
                } break;

                case CUI_AXIS_Y:
                {
                    if (!CuiDListIsEmpty(&widget->children))
                    {
                        height -= widget->effective_inline_padding;

                        CuiForEachWidget(child, &widget->children)
                        {
                            CuiPoint size = cui_widget_get_preferred_size(child);

                            width = cui_max_int32(width, size.x);
                            height += size.y + widget->effective_inline_padding;
                        }
                    }
                } break;
            }

            if (widget->flags & CUI_WIDGET_FLAG_FIXED_WIDTH)
            {
                width = widget->effective_preferred_size.x;
            }

            if (widget->flags & CUI_WIDGET_FLAG_FIXED_HEIGHT)
            {
                height = widget->effective_preferred_size.y;
            }

            int32_t padding_x = widget->effective_padding.min.x + widget->effective_padding.max.x;
            int32_t padding_y = widget->effective_padding.min.y + widget->effective_padding.max.y;

            int32_t border_width_x = widget->effective_border_width.min.x + widget->effective_border_width.max.x;
            int32_t border_width_y = widget->effective_border_width.min.y + widget->effective_border_width.max.y;

            width  += padding_x + border_width_x;
            height += padding_y + border_width_y;

            result = cui_make_point(width, height);
        } break;

        case CUI_WIDGET_TYPE_STACK:
        {
            int32_t width  = 0;
            int32_t height = 0;

            CuiForEachWidget(child, &widget->children)
            {
                CuiPoint size = cui_widget_get_preferred_size(child);

                width = cui_max_int32(width, size.x);
                height = cui_max_int32(height, size.y);
            }

            result = cui_make_point(width, height);
        } break;

        case CUI_WIDGET_TYPE_LABEL:
        case CUI_WIDGET_TYPE_BUTTON:
        {
            CuiFont *font = _cui_font_manager_get_font_from_id(&window->base.font_manager, font_id);

            int32_t label_width  = 0;
            int32_t label_height = 0;

            if (widget->label.count)
            {
                label_width  = (int32_t) ceilf(_cui_font_get_string_width(&window->base.font_manager, font_id, widget->label));
                label_height = font->line_height;
            }

            int32_t icon_width  = widget->effective_icon_size.x;
            int32_t icon_height = widget->effective_icon_size.y;

            int32_t width = icon_width + label_width;
            int32_t height = cui_max_int32(icon_height, label_height);

            if (widget->icon_type && widget->label.count)
            {
                width += widget->effective_inline_padding;
            }

            if (widget->flags & CUI_WIDGET_FLAG_FIXED_WIDTH)
            {
                width = widget->effective_preferred_size.x;
            }

            if (widget->flags & CUI_WIDGET_FLAG_FIXED_HEIGHT)
            {
                height = widget->effective_preferred_size.y;
            }

            int32_t padding_x = widget->effective_padding.min.x + widget->effective_padding.max.x;
            int32_t padding_y = widget->effective_padding.min.y + widget->effective_padding.max.y;

            int32_t border_width_x = widget->effective_border_width.min.x + widget->effective_border_width.max.x;
            int32_t border_width_y = widget->effective_border_width.min.y + widget->effective_border_width.max.y;

            width  += padding_x + border_width_x;
            height += padding_y + border_width_y;

            result = cui_make_point(width, height);
        } break;

        case CUI_WIDGET_TYPE_CHECKBOX:
        {
            CuiFont *font = _cui_font_manager_get_font_from_id(&window->base.font_manager, font_id);

            int32_t label_width  = (int32_t) ceilf(_cui_font_get_string_width(&window->base.font_manager, font_id, widget->label));
            int32_t label_height = font->line_height;

            int32_t checkmark_width  = widget->effective_icon_size.x;
            int32_t checkmark_height = widget->effective_icon_size.y;

            int32_t width = checkmark_width + widget->effective_inline_padding + label_width;
            int32_t height = cui_max_int32(checkmark_height, label_height);

            width  += widget->effective_padding.min.x + widget->effective_padding.max.x;
            height += widget->effective_padding.min.y + widget->effective_padding.max.y;

            result = cui_make_point(width, height);
        } break;

        case CUI_WIDGET_TYPE_TEXTINPUT:
        {
            CuiFont *font = _cui_font_manager_get_font_from_id(&window->base.font_manager, font_id);

            int32_t label_width  = (int32_t) ceilf(_cui_font_get_string_width(&window->base.font_manager, font_id, widget->label));
            int32_t label_height = font->line_height;

            int32_t icon_width  = widget->effective_icon_size.x;
            int32_t icon_height = widget->effective_icon_size.y;

            int32_t width = icon_width + label_width;
            int32_t height = cui_max_int32(icon_height, label_height);

            if (widget->icon_type && widget->label.count)
            {
                width += widget->effective_inline_padding;
            }

            if (widget->flags & CUI_WIDGET_FLAG_FIXED_WIDTH)
            {
                width = widget->effective_preferred_size.x;
            }

            if (widget->flags & CUI_WIDGET_FLAG_FIXED_HEIGHT)
            {
                height = widget->effective_preferred_size.y;
            }

            int32_t padding_x = widget->effective_padding.min.x + widget->effective_padding.max.x;
            int32_t padding_y = widget->effective_padding.min.y + widget->effective_padding.max.y;

            int32_t border_width_x = widget->effective_border_width.min.x + widget->effective_border_width.max.x;
            int32_t border_width_y = widget->effective_border_width.min.y + widget->effective_border_width.max.y;

            width  += padding_x + border_width_x;
            height += padding_y + border_width_y;

            result = cui_make_point(width, height);
        } break;
    }

    return result;
}

void
cui_widget_layout(CuiWidget *widget, CuiRect rect)
{
    widget->rect = rect;

    if (widget->type >= CUI_WIDGET_TYPE_CUSTOM)
    {
        widget->layout(widget, rect);
        return;
    }

    CuiAssert(widget->window);
    CuiWindow *window = widget->window;

    CuiFontId font_id = widget->font_id;

    if (font_id.value == 0)
    {
        font_id = window->font_id;
    }

    switch (widget->type)
    {
        case CUI_WIDGET_TYPE_BOX:
        {
            switch (widget->main_axis)
            {
                case CUI_AXIS_X:
                {
                    switch (widget->x_gravity)
                    {
                        case CUI_GRAVITY_START:
                        {
                            if (!CuiDListIsEmpty(&widget->children))
                            {
                                CuiRect parent_rect = widget->rect;

                                // TODO: do something about negative sized rects
                                parent_rect.min.x += widget->effective_padding.min.x + widget->effective_border_width.min.x;
                                parent_rect.min.y += widget->effective_padding.min.y + widget->effective_border_width.min.y;
                                parent_rect.max.x -= widget->effective_padding.max.x + widget->effective_border_width.max.x;
                                parent_rect.max.y -= widget->effective_padding.max.y + widget->effective_border_width.max.y;

                                CuiRect child_rect = parent_rect;

                                CuiForEachWidget(child, &widget->children)
                                {
                                    if (child->list.next == &widget->children) // is last child
                                    {
                                        child_rect.min.x = cui_min_int32(child_rect.min.x, parent_rect.max.x);
                                        child_rect.max.x = parent_rect.max.x;
                                    }
                                    else
                                    {
                                        CuiPoint size = cui_widget_get_preferred_size(child);
                                        child_rect.max.x = child_rect.min.x + size.x;
                                    }

                                    cui_widget_layout(child, child_rect);
                                    child_rect.min.x = child_rect.max.x + widget->effective_inline_padding;
                                }
                            }
                        } break;

                        case CUI_GRAVITY_CENTER:
                        {
                            if (!CuiDListIsEmpty(&widget->children))
                            {
                                CuiRect parent_rect = widget->rect;

                                // TODO: do something about negative sized rects
                                parent_rect.min.x += widget->effective_padding.min.x + widget->effective_border_width.min.x;
                                parent_rect.min.y += widget->effective_padding.min.y + widget->effective_border_width.min.y;
                                parent_rect.max.x -= widget->effective_padding.max.x + widget->effective_border_width.max.x;
                                parent_rect.max.y -= widget->effective_padding.max.y + widget->effective_border_width.max.y;

                                CuiWidget *first_child = CuiContainerOf(widget->children.next, CuiWidget, list);
                                CuiWidget *last_child  = CuiContainerOf(widget->children.prev, CuiWidget, list);

                                if (first_child == last_child)
                                {
                                    cui_widget_layout(first_child, parent_rect);
                                }
                                else if (CuiContainerOf(first_child->list.next, CuiWidget, list) == last_child)
                                {
                                    CuiAssert(CuiContainerOf(last_child->list.prev, CuiWidget, list) == first_child);

                                    int32_t width = (cui_rect_get_width(parent_rect) - widget->effective_inline_padding) / 2;

                                    CuiRect child_rect = parent_rect;

                                    child_rect.max.x = child_rect.min.x + width;

                                    cui_widget_layout(first_child, child_rect);

                                    child_rect.min.x = child_rect.max.x + widget->effective_inline_padding;
                                    child_rect.max.x = parent_rect.max.x;

                                    cui_widget_layout(last_child, child_rect);
                                }
                                else
                                {
                                    int32_t children_width = -widget->effective_inline_padding;

                                    for (CuiWidget *child = CuiContainerOf(first_child->list.next, CuiWidget, list);
                                         child != last_child;
                                         child = CuiContainerOf(child->list.next, CuiWidget, list))
                                    {
                                        CuiPoint size = cui_widget_get_preferred_size(child);

                                        children_width += size.x + widget->effective_inline_padding;
                                    }

                                    int32_t x_offset = parent_rect.min.x + (cui_rect_get_width(parent_rect) - children_width) / 2;

                                    CuiRect child_rect = parent_rect;

                                    child_rect.max.x = cui_max_int32(0, x_offset);

                                    cui_widget_layout(first_child, child_rect);

                                    child_rect.min.x = x_offset;

                                    for (CuiWidget *child = CuiContainerOf(first_child->list.next, CuiWidget, list);
                                         child != last_child;
                                         child = CuiContainerOf(child->list.next, CuiWidget, list))
                                    {
                                        CuiPoint size = cui_widget_get_preferred_size(child);
                                        child_rect.max.x = child_rect.min.x + size.x;

                                        cui_widget_layout(child, child_rect);

                                        child_rect.min.x = child_rect.max.x + widget->effective_inline_padding;
                                    }

                                    child_rect.min.x = cui_min_int32(child_rect.min.x, parent_rect.max.x);
                                    child_rect.max.x = parent_rect.max.x;

                                    cui_widget_layout(last_child, child_rect);
                                }
                            }
                        } break;

                        case CUI_GRAVITY_END:
                        {
                            if (!CuiDListIsEmpty(&widget->children))
                            {
                                CuiRect parent_rect = widget->rect;

                                // TODO: do something about negative sized rects
                                parent_rect.min.x += widget->effective_padding.min.x + widget->effective_border_width.min.x;
                                parent_rect.min.y += widget->effective_padding.min.y + widget->effective_border_width.min.y;
                                parent_rect.max.x -= widget->effective_padding.max.x + widget->effective_border_width.max.x;
                                parent_rect.max.y -= widget->effective_padding.max.y + widget->effective_border_width.max.y;

                                CuiRect child_rect = parent_rect;

                                CuiForEachWidget(child, &widget->children)
                                {
                                    if (child->list.next == &widget->children) // is last child
                                    {
                                        child_rect.min.x = parent_rect.min.x;
                                        child_rect.max.x = cui_max_int32(child_rect.max.x, parent_rect.min.x);
                                    }
                                    else
                                    {
                                        CuiPoint size = cui_widget_get_preferred_size(child);
                                        child_rect.min.x = child_rect.max.x - size.x;
                                    }

                                    cui_widget_layout(child, child_rect);
                                    child_rect.max.x = child_rect.min.x - widget->effective_inline_padding;
                                }
                            }
                        } break;
                    }
                } break;

                case CUI_AXIS_Y:
                {
                    switch (widget->y_gravity)
                    {
                        case CUI_GRAVITY_START:
                        {
                            if (!CuiDListIsEmpty(&widget->children))
                            {
                                CuiRect parent_rect = widget->rect;

                                // TODO: do something about negative sized rects
                                parent_rect.min.x += widget->effective_padding.min.x + widget->effective_border_width.min.x;
                                parent_rect.min.y += widget->effective_padding.min.y + widget->effective_border_width.min.y;
                                parent_rect.max.x -= widget->effective_padding.max.x + widget->effective_border_width.max.x;
                                parent_rect.max.y -= widget->effective_padding.max.y + widget->effective_border_width.max.y;

                                CuiRect child_rect = parent_rect;

                                CuiForEachWidget(child, &widget->children)
                                {
                                    if (child->list.next == &widget->children) // is last child
                                    {
                                        child_rect.min.y = cui_min_int32(child_rect.min.y, parent_rect.max.y);
                                        child_rect.max.y = parent_rect.max.y;
                                    }
                                    else
                                    {
                                        CuiPoint size = cui_widget_get_preferred_size(child);
                                        child_rect.max.y = child_rect.min.y + size.y;
                                    }

                                    cui_widget_layout(child, child_rect);
                                    child_rect.min.y = child_rect.max.y + widget->effective_inline_padding;
                                }
                            }
                        } break;

                        case CUI_GRAVITY_CENTER:
                        {
                            if (!CuiDListIsEmpty(&widget->children))
                            {
                                CuiRect parent_rect = widget->rect;

                                // TODO: do something about negative sized rects
                                parent_rect.min.x += widget->effective_padding.min.x + widget->effective_border_width.min.x;
                                parent_rect.min.y += widget->effective_padding.min.y + widget->effective_border_width.min.y;
                                parent_rect.max.x -= widget->effective_padding.max.x + widget->effective_border_width.max.x;
                                parent_rect.max.y -= widget->effective_padding.max.y + widget->effective_border_width.max.y;

                                CuiWidget *first_child = CuiContainerOf(widget->children.next, CuiWidget, list);
                                CuiWidget *last_child  = CuiContainerOf(widget->children.prev, CuiWidget, list);

                                if (first_child == last_child)
                                {
                                    cui_widget_layout(first_child, parent_rect);
                                }
                                else if (CuiContainerOf(first_child->list.next, CuiWidget, list) == last_child)
                                {
                                    CuiAssert(CuiContainerOf(last_child->list.prev, CuiWidget, list) == first_child);

                                    int32_t height = (cui_rect_get_height(parent_rect) - widget->effective_inline_padding) / 2;

                                    CuiRect child_rect = parent_rect;

                                    child_rect.max.y = child_rect.min.y + height;

                                    cui_widget_layout(first_child, child_rect);

                                    child_rect.min.y = child_rect.max.y + widget->effective_inline_padding;
                                    child_rect.max.y = parent_rect.max.y;

                                    cui_widget_layout(last_child, child_rect);
                                }
                                else
                                {
                                    int32_t children_height = -widget->effective_inline_padding;

                                    for (CuiWidget *child = CuiContainerOf(first_child->list.next, CuiWidget, list);
                                         child != last_child;
                                         child = CuiContainerOf(child->list.next, CuiWidget, list))
                                    {
                                        CuiPoint size = cui_widget_get_preferred_size(child);

                                        children_height += size.y + widget->effective_inline_padding;
                                    }

                                    int32_t y_offset = parent_rect.min.y + (cui_rect_get_height(parent_rect) - children_height) / 2;

                                    CuiRect child_rect = parent_rect;

                                    child_rect.max.y = cui_max_int32(0, y_offset);

                                    cui_widget_layout(first_child, child_rect);

                                    child_rect.min.y = y_offset;

                                    for (CuiWidget *child = CuiContainerOf(first_child->list.next, CuiWidget, list);
                                         child != last_child;
                                         child = CuiContainerOf(child->list.next, CuiWidget, list))
                                    {
                                        CuiPoint size = cui_widget_get_preferred_size(child);
                                        child_rect.max.y = child_rect.min.y + size.y;

                                        cui_widget_layout(child, child_rect);

                                        child_rect.min.y = child_rect.max.y + widget->effective_inline_padding;
                                    }

                                    child_rect.min.y = cui_min_int32(child_rect.min.y, parent_rect.max.y);
                                    child_rect.max.y = parent_rect.max.y;

                                    cui_widget_layout(last_child, child_rect);
                                }
                            }
                        } break;

                        case CUI_GRAVITY_END:
                        {
                            if (!CuiDListIsEmpty(&widget->children))
                            {
                                CuiRect parent_rect = widget->rect;

                                // TODO: do something about negative sized rects
                                parent_rect.min.x += widget->effective_padding.min.x + widget->effective_border_width.min.x;
                                parent_rect.min.y += widget->effective_padding.min.y + widget->effective_border_width.min.y;
                                parent_rect.max.x -= widget->effective_padding.max.x + widget->effective_border_width.max.x;
                                parent_rect.max.y -= widget->effective_padding.max.y + widget->effective_border_width.max.y;

                                CuiRect child_rect = parent_rect;

                                CuiForEachWidget(child, &widget->children)
                                {
                                    if (child->list.next == &widget->children) // is last child
                                    {
                                        child_rect.min.y = parent_rect.min.y;
                                        child_rect.max.y = cui_max_int32(child_rect.max.y, parent_rect.min.y);
                                    }
                                    else
                                    {
                                        CuiPoint size = cui_widget_get_preferred_size(child);
                                        child_rect.min.y = child_rect.max.y - size.y;
                                    }

                                    cui_widget_layout(child, child_rect);
                                    child_rect.max.y = child_rect.min.y - widget->effective_inline_padding;
                                }
                            }
                        } break;
                    }
                } break;
            }
        } break;

        case CUI_WIDGET_TYPE_STACK:
        {
            CuiForEachWidget(child, &widget->children)
            {
                cui_widget_layout(child, widget->rect);
            }
        } break;


        case CUI_WIDGET_TYPE_TEXTINPUT:
            _cui_widget_update_text_offset(widget);
            /* FALLTHRU */
        case CUI_WIDGET_TYPE_LABEL:
        case CUI_WIDGET_TYPE_BUTTON:
        case CUI_WIDGET_TYPE_CHECKBOX:
        {
            int32_t padding_x = widget->effective_padding.min.x + widget->effective_padding.max.x;
            int32_t padding_y = widget->effective_padding.min.y + widget->effective_padding.max.y;

            int32_t border_width_x = widget->effective_border_width.min.x + widget->effective_border_width.max.x;
            int32_t border_width_y = widget->effective_border_width.min.y + widget->effective_border_width.max.y;

            int32_t content_width  = cui_rect_get_width(widget->rect) - (padding_x + border_width_x);
            int32_t content_height = cui_rect_get_height(widget->rect) - (padding_y + border_width_y);

            CuiFont *font = _cui_font_manager_get_font_from_id(&window->base.font_manager, font_id);

            widget->label_offset = cui_make_float_point((float) (widget->effective_padding.min.x + widget->effective_border_width.min.x),
                                                        (float) (widget->effective_padding.min.y + widget->effective_border_width.min.y) + font->baseline_offset);

            widget->icon_offset = cui_make_float_point((float) (widget->effective_padding.min.x + widget->effective_border_width.min.x),
                                                       (float) (widget->effective_padding.min.y + widget->effective_border_width.min.y));
            widget->icon_offset.y += 0.5f * (float) (content_height - widget->effective_icon_size.y);

            if (widget->icon_type || (widget->type == CUI_WIDGET_TYPE_CHECKBOX))
            {
                int32_t label_offset = widget->effective_icon_size.x + widget->effective_inline_padding;
                widget->label_offset.x += (float) label_offset;
                content_width -= label_offset;
            }

            switch (widget->x_gravity)
            {
                case CUI_GRAVITY_START: break;

                case CUI_GRAVITY_CENTER:
                {
                    float label_width = _cui_font_get_string_width(&window->base.font_manager, font_id, widget->label);
                    widget->label_offset.x += 0.5f * ((float) content_width - label_width);
                } break;

                case CUI_GRAVITY_END:
                {
                    float label_width = _cui_font_get_string_width(&window->base.font_manager, font_id, widget->label);
                    widget->label_offset.x += (float) content_width - label_width;
                } break;
            }

            switch (widget->y_gravity)
            {
                case CUI_GRAVITY_START: break;

                case CUI_GRAVITY_CENTER:
                {
                    widget->label_offset.y += 0.5f * ((float) (content_height - font->line_height));
                } break;

                case CUI_GRAVITY_END:
                {
                    widget->label_offset.y += (float) (content_height - font->line_height);
                } break;
            }
        } break;
    }
}

void
cui_widget_draw(CuiWidget *widget, CuiGraphicsContext *ctx, const CuiColorTheme *color_theme)
{
    if (widget->color_theme)
    {
        color_theme = widget->color_theme;
    }

    if (widget->type >= CUI_WIDGET_TYPE_CUSTOM)
    {
        widget->draw(widget, ctx, color_theme);
        return;
    }

    CuiAssert(widget->window);
    CuiWindow *window = widget->window;

    CuiFontId font_id = widget->font_id;

    if (font_id.value == 0)
    {
        font_id = window->font_id;
    }

    switch (widget->type)
    {
        case CUI_WIDGET_TYPE_BOX:
        {
            if (widget->flags & CUI_WIDGET_FLAG_DRAW_BACKGROUND)
            {
                if (widget->effective_blur_radius > 0)
                {
                    _cui_widget_draw_box_shadow(ctx, widget, color_theme);
                }

                _cui_widget_draw_background(ctx, widget, color_theme);
            }

#if 0
            if (widget->state & CUI_WIDGET_STATE_HOVERED)
            {
                cui_draw_fill_rect(ctx, widget->rect, cui_make_color(1.0f, 0.0f, 0.0f, 0.1f));
            }
#endif

            if (widget->flags & CUI_WIDGET_FLAG_CLIP_CONTENT)
            {
                CuiRect clip_rect = widget->rect;
                clip_rect.min.x += widget->effective_border_width.min.x;
                clip_rect.min.y += widget->effective_border_width.min.y;
                clip_rect.max.x -= widget->effective_border_width.max.x;
                clip_rect.max.y -= widget->effective_border_width.max.y;

                clip_rect = cui_rect_get_intersection(ctx->clip_rect, clip_rect);
                CuiRect prev_clip = cui_draw_set_clip_rect(ctx, clip_rect);

                CuiForEachWidget(child, &widget->children)
                {
                    cui_widget_draw(child, ctx, color_theme);
                }

                cui_draw_set_clip_rect(ctx, prev_clip);
            }
            else
            {
                CuiForEachWidget(child, &widget->children)
                {
                    cui_widget_draw(child, ctx, color_theme);
                }
            }
        } break;

        case CUI_WIDGET_TYPE_STACK:
        {
            CuiForEachWidget(child, &widget->children)
            {
                cui_widget_draw(child, ctx, color_theme);
            }
        } break;

        case CUI_WIDGET_TYPE_LABEL:
        case CUI_WIDGET_TYPE_BUTTON:
        {
            if (widget->flags & CUI_WIDGET_FLAG_DRAW_BACKGROUND)
            {
                if (widget->effective_blur_radius > 0)
                {
                    _cui_widget_draw_box_shadow(ctx, widget, color_theme);
                }

                _cui_widget_draw_background(ctx, widget, color_theme);
            }

#if 0
            if (widget->state & CUI_WIDGET_STATE_HOVERED)
            {
                cui_draw_fill_rect(ctx, widget->rect, cui_make_color(0.0f, 0.0f, 1.0f, 0.1f));
            }
#endif

            cui_draw_fill_icon(ctx, (float) widget->rect.min.x + widget->icon_offset.x, (float) widget->rect.min.y + widget->icon_offset.y, widget->ui_scale, widget->icon_type, cui_color_theme_get_color(color_theme, widget->color_normal_icon));
            cui_draw_fill_string(ctx, font_id, (float) widget->rect.min.x + widget->label_offset.x,
                                 (float) widget->rect.min.y + widget->label_offset.y, widget->label, cui_color_theme_get_color(color_theme, widget->color_normal_text));
        } break;

        case CUI_WIDGET_TYPE_CHECKBOX:
        {
#if 0
            if (widget->state & CUI_WIDGET_STATE_HOVERED)
            {
                cui_draw_fill_rect(ctx, widget->rect, cui_make_color(0.0f, 1.0f, 0.0f, 0.1f));
            }
#endif

            float x = (float) widget->rect.min.x + widget->icon_offset.x;
            float y = (float) widget->rect.min.y + widget->icon_offset.y;

            if (widget->value)
            {
                cui_draw_fill_shape(ctx, x, y, CUI_SHAPE_CHECKBOX_OUTER_16, widget->ui_scale, cui_make_color(0.337f, 0.541f, 0.949f, 1.0f));
                cui_draw_fill_shape(ctx, x, y, CUI_SHAPE_CHECKMARK_16, widget->ui_scale, cui_make_color(1.0f, 1.0f, 1.0f, 1.0f));
            }
            else
            {
                cui_draw_fill_shape(ctx, x, y, CUI_SHAPE_CHECKBOX_OUTER_16, widget->ui_scale, cui_make_color(0.094f, 0.102f, 0.122f, 1.0f));
                cui_draw_fill_shape(ctx, x, y, CUI_SHAPE_CHECKBOX_INNER_16, widget->ui_scale, cui_make_color(0.184f, 0.200f, 0.239f, 1.0f));
            }

            cui_draw_fill_string(ctx, font_id, (float) widget->rect.min.x + widget->label_offset.x,
                                 (float) widget->rect.min.y + widget->label_offset.y, widget->label, cui_color_theme_get_color(color_theme, widget->color_normal_text));
        } break;

        case CUI_WIDGET_TYPE_TEXTINPUT:
        {
            if (widget->effective_blur_radius > 0)
            {
                _cui_widget_draw_box_shadow(ctx, widget, color_theme);
            }

            _cui_widget_draw_background(ctx, widget, color_theme);

            cui_draw_fill_icon(ctx, (float) widget->rect.min.x + widget->icon_offset.x, (float) widget->rect.min.y + widget->icon_offset.y,
                               widget->ui_scale, widget->icon_type, cui_color_theme_get_color(color_theme, widget->color_normal_icon));

            CuiRect clip_rect = widget->rect;
            clip_rect.min.x += widget->effective_border_width.min.x;
            clip_rect.min.y += widget->effective_border_width.min.y;
            clip_rect.max.x -= widget->effective_border_width.max.x;
            clip_rect.max.y -= widget->effective_border_width.max.y;

            clip_rect = cui_rect_get_intersection(ctx->clip_rect, clip_rect);
            CuiRect prev_clip = cui_draw_set_clip_rect(ctx, clip_rect);

            CuiString str = cui_text_input_to_string(widget->text_input);

            if ((str.count == 0) && !(widget->state & CUI_WIDGET_STATE_FOCUSED))
            {
                cui_draw_fill_string(ctx, font_id, (float) widget->rect.min.x + widget->label_offset.x,
                                     (float) widget->rect.min.y + widget->label_offset.y, widget->label, cui_color_theme_get_color(color_theme, widget->color_normal_text));
            }
            else
            {
                CuiFont *font = _cui_font_manager_get_font_from_id(&window->base.font_manager, font_id);

                float cursor_end = _cui_font_get_substring_width(&window->base.font_manager, font_id, str, widget->text_input.cursor_end);

                if ((widget->state & CUI_WIDGET_STATE_FOCUSED) && (widget->text_input.cursor_start != widget->text_input.cursor_end))
                {
                    float cursor_start = _cui_font_get_substring_width(&window->base.font_manager, font_id, str, widget->text_input.cursor_start);

                    int32_t a = widget->rect.min.x + lroundf(widget->text_offset.x + cursor_start);
                    int32_t b = widget->rect.min.x + lroundf(widget->text_offset.x + cursor_end);

                    int32_t cursor_x0 = cui_min_int32(a, b);
                    int32_t cursor_x1 = cui_max_int32(a, b);

                    int32_t cursor_y0 = widget->rect.min.y + lroundf(widget->text_offset.y - font->baseline_offset);
                    int32_t cursor_y1 = cursor_y0 + font->line_height;

                    cui_draw_fill_rect(ctx, cui_make_rect(cursor_x0, cursor_y0, cursor_x1, cursor_y1), cui_make_color(0.337f, 0.541f, 0.949f, 0.75f));
                }

                cui_draw_fill_string(ctx, font_id, (float) widget->rect.min.x + widget->text_offset.x,
                                     (float) widget->rect.min.y + widget->text_offset.y, str, cui_color_theme_get_color(color_theme, widget->color_normal_text));

                if ((widget->state & CUI_WIDGET_STATE_FOCUSED) && (widget->text_input.cursor_start == widget->text_input.cursor_end))
                {
                    float cursor_width = roundf(widget->ui_scale * 1.0f); // TODO: add member to widget
                    int32_t cursor_x0 = widget->rect.min.x + lroundf(widget->text_offset.x + cursor_end - 0.5f * cursor_width);
                    int32_t cursor_x1 = cursor_x0 + (int32_t) cursor_width;

                    int32_t cursor_y0 = widget->rect.min.y + lroundf(widget->text_offset.y - font->baseline_offset);
                    int32_t cursor_y1 = cursor_y0 + font->line_height;

                    cui_draw_fill_rect(ctx, cui_make_rect(cursor_x0, cursor_y0, cursor_x1, cursor_y1), cui_make_color(1.0f, 1.0f, 1.0f, 1.0f));
                }
            }

            cui_draw_set_clip_rect(ctx, prev_clip);
        } break;
    }
}

bool
cui_widget_handle_event(CuiWidget *widget, CuiEventType event_type)
{
    bool result = false;

    if (widget->type >= CUI_WIDGET_TYPE_CUSTOM)
    {
        return widget->handle_event(widget, event_type);
    }

    CuiAssert(widget->window);
    CuiWindow *window = widget->window;

    switch (widget->type)
    {
        case CUI_WIDGET_TYPE_BOX:
        {
            switch (event_type)
            {
                case CUI_EVENT_TYPE_MOUSE_ENTER:
                case CUI_EVENT_TYPE_MOUSE_MOVE:
                {
                    widget->state |= CUI_WIDGET_STATE_HOVERED;
                    cui_window_request_redraw(window);

                    CuiForEachWidget(child, &widget->children)
                    {
                        if (cui_event_is_inside_widget(window, child))
                        {
                            cui_window_set_hovered(window, child);

                            if (cui_widget_handle_event(child, CUI_EVENT_TYPE_MOUSE_ENTER))
                            {
                                result = true;
                                break;
                            }
                        }
                    }

                    if (!result && (widget->flags & CUI_WIDGET_FLAG_DRAW_BACKGROUND))
                    {
                        cui_window_set_hovered(window, widget);
                        result = true;
                    }
                } break;

                case CUI_EVENT_TYPE_MOUSE_LEAVE:
                {
                    widget->state &= ~CUI_WIDGET_STATE_HOVERED;
                    cui_window_request_redraw(window);
                } break;

                case CUI_EVENT_TYPE_MOUSE_DRAG:
                {
                } break;

                case CUI_EVENT_TYPE_MOUSE_WHEEL:
                {
                    CuiForEachWidget(child, &widget->children)
                    {
                        if (cui_event_is_inside_widget(window, child))
                        {
                            cui_window_set_hovered(window, child);

                            if (cui_widget_handle_event(child, CUI_EVENT_TYPE_MOUSE_WHEEL))
                            {
                                result = true;
                                break;
                            }
                        }
                    }
                } break;

                case CUI_EVENT_TYPE_LEFT_DOWN:
                case CUI_EVENT_TYPE_DOUBLE_CLICK:
                {
                    CuiForEachWidget(child, &widget->children)
                    {
                        if (cui_event_is_inside_widget(window, child))
                        {
                            if (cui_widget_handle_event(child, event_type))
                            {
                                CuiAssert(cui_widget_contains(child, window->base.pressed_widget));
                                result = true;
                                break;
                            }
                        }
                    }

                    // TODO: use a separate flag for consuming mouse input
                    // if (!result && (widget->flags & CUI_WIDGET_FLAG_DRAW_BACKGROUND))
                    if (false)
                    {
                        cui_window_set_pressed(window, widget);
                        result = true;
                    }
                } break;

                case CUI_EVENT_TYPE_LEFT_UP:
                {
                } break;

                case CUI_EVENT_TYPE_RIGHT_DOWN:
                {
                } break;

                case CUI_EVENT_TYPE_RIGHT_UP:
                {
                } break;

                case CUI_EVENT_TYPE_KEY_DOWN:
                {
                } break;

                case CUI_EVENT_TYPE_KEY_UP:
                {
                } break;

                case CUI_EVENT_TYPE_FOCUS:
                {
                } break;

                case CUI_EVENT_TYPE_UNFOCUS:
                {
                } break;

                case CUI_EVENT_TYPE_POINTER_DOWN:
                {
                    CuiForEachWidget(child, &widget->children)
                    {
                        if (cui_event_pointer_is_inside_widget(window, child))
                        {
                            if (cui_widget_handle_event(child, CUI_EVENT_TYPE_POINTER_DOWN))
                            {
                                result = true;
                                break;
                            }
                        }
                    }
                } break;

                case CUI_EVENT_TYPE_POINTER_UP:
                {
                    CuiForEachWidgetReversed(child, &widget->children)
                    {
                        if (cui_event_pointer_is_inside_widget(window, child))
                        {
                            if (cui_widget_handle_event(child, CUI_EVENT_TYPE_POINTER_UP))
                            {
                                result = true;
                                break;
                            }
                        }
                    }
                } break;

                case CUI_EVENT_TYPE_POINTER_MOVE:
                {
                    CuiForEachWidgetReversed(child, &widget->children)
                    {
                        if (cui_event_pointer_is_inside_widget(window, child))
                        {
                            if (cui_widget_handle_event(child, CUI_EVENT_TYPE_POINTER_MOVE))
                            {
                                result = true;
                                break;
                            }
                        }
                    }
                } break;
            }
        } break;

        case CUI_WIDGET_TYPE_STACK:
        {
            switch (event_type)
            {
                case CUI_EVENT_TYPE_MOUSE_ENTER:
                case CUI_EVENT_TYPE_MOUSE_MOVE:
                {
                    CuiForEachWidgetReversed(child, &widget->children)
                    {
                        cui_window_set_hovered(window, child);

                        if (cui_widget_handle_event(child, CUI_EVENT_TYPE_MOUSE_ENTER))
                        {
                            result = true;
                            break;
                        }
                    }
                } break;

                case CUI_EVENT_TYPE_MOUSE_LEAVE:
                {
                } break;

                case CUI_EVENT_TYPE_MOUSE_DRAG:
                {
                } break;

                case CUI_EVENT_TYPE_MOUSE_WHEEL:
                {
                    CuiForEachWidgetReversed(child, &widget->children)
                    {
                        if (cui_widget_handle_event(child, CUI_EVENT_TYPE_MOUSE_WHEEL))
                        {
                            result = true;
                            break;
                        }
                    }
                } break;

                case CUI_EVENT_TYPE_LEFT_DOWN:
                case CUI_EVENT_TYPE_DOUBLE_CLICK:
                {
                    CuiForEachWidgetReversed(child, &widget->children)
                    {
                        if (cui_event_is_inside_widget(window, child))
                        {
                            if (cui_widget_handle_event(child, event_type))
                            {
                                CuiAssert(cui_widget_contains(child, window->base.pressed_widget));
                                result = true;
                                break;
                            }
                        }
                    }
                } break;

                case CUI_EVENT_TYPE_LEFT_UP:
                {
                } break;

                case CUI_EVENT_TYPE_RIGHT_DOWN:
                {
                } break;

                case CUI_EVENT_TYPE_RIGHT_UP:
                {
                } break;

                case CUI_EVENT_TYPE_KEY_DOWN:
                {
                } break;

                case CUI_EVENT_TYPE_KEY_UP:
                {
                } break;

                case CUI_EVENT_TYPE_FOCUS:
                {
                } break;

                case CUI_EVENT_TYPE_UNFOCUS:
                {
                } break;

                case CUI_EVENT_TYPE_POINTER_DOWN:
                {
                    CuiForEachWidgetReversed(child, &widget->children)
                    {
                        if (cui_event_pointer_is_inside_widget(window, child))
                        {
                            if (cui_widget_handle_event(child, CUI_EVENT_TYPE_POINTER_DOWN))
                            {
                                result = true;
                                break;
                            }
                        }
                    }
                } break;

                case CUI_EVENT_TYPE_POINTER_UP:
                {
                    CuiForEachWidgetReversed(child, &widget->children)
                    {
                        if (cui_event_pointer_is_inside_widget(window, child))
                        {
                            if (cui_widget_handle_event(child, CUI_EVENT_TYPE_POINTER_UP))
                            {
                                result = true;
                                break;
                            }
                        }
                    }
                } break;

                case CUI_EVENT_TYPE_POINTER_MOVE:
                {
                    CuiForEachWidgetReversed(child, &widget->children)
                    {
                        if (cui_event_pointer_is_inside_widget(window, child))
                        {
                            if (cui_widget_handle_event(child, CUI_EVENT_TYPE_POINTER_MOVE))
                            {
                                result = true;
                                break;
                            }
                        }
                    }
                } break;
            }
        } break;

        case CUI_WIDGET_TYPE_LABEL:
        {
            switch (event_type)
            {
                case CUI_EVENT_TYPE_MOUSE_ENTER:
                {
                    widget->state |= CUI_WIDGET_STATE_HOVERED;
                    cui_window_request_redraw(window);
                    result = true;
                } break;

                case CUI_EVENT_TYPE_MOUSE_LEAVE:
                {
                    widget->state &= ~CUI_WIDGET_STATE_HOVERED;
                    cui_window_request_redraw(window);
                    result = true;
                } break;

                case CUI_EVENT_TYPE_MOUSE_MOVE:
                {
                } break;

                case CUI_EVENT_TYPE_MOUSE_DRAG:
                {
                } break;

                case CUI_EVENT_TYPE_MOUSE_WHEEL:
                {
                } break;

                case CUI_EVENT_TYPE_LEFT_DOWN:
                {
                } break;

                case CUI_EVENT_TYPE_LEFT_UP:
                {
                } break;

                case CUI_EVENT_TYPE_DOUBLE_CLICK:
                {
                } break;

                case CUI_EVENT_TYPE_RIGHT_DOWN:
                {
                } break;

                case CUI_EVENT_TYPE_RIGHT_UP:
                {
                } break;

                case CUI_EVENT_TYPE_KEY_DOWN:
                {
                } break;

                case CUI_EVENT_TYPE_KEY_UP:
                {
                } break;

                case CUI_EVENT_TYPE_FOCUS:
                {
                } break;

                case CUI_EVENT_TYPE_UNFOCUS:
                {
                } break;

                case CUI_EVENT_TYPE_POINTER_DOWN:
                {
                } break;

                case CUI_EVENT_TYPE_POINTER_UP:
                {
                } break;

                case CUI_EVENT_TYPE_POINTER_MOVE:
                {
                } break;
            }
        } break;

        case CUI_WIDGET_TYPE_BUTTON:
        {
            switch (event_type)
            {
                case CUI_EVENT_TYPE_MOUSE_ENTER:
                {
                    widget->state |= CUI_WIDGET_STATE_HOVERED;
                    cui_window_request_redraw(window);
                    result = true;
                } break;

                case CUI_EVENT_TYPE_MOUSE_LEAVE:
                {
                    widget->state &= ~CUI_WIDGET_STATE_HOVERED;
                    cui_window_request_redraw(window);
                    result = true;
                } break;

                case CUI_EVENT_TYPE_MOUSE_MOVE:
                {
                } break;

                case CUI_EVENT_TYPE_MOUSE_DRAG:
                {
                    if (cui_event_is_inside_widget(window, widget))
                    {
                        if (!(widget->state & CUI_WIDGET_STATE_PRESSED))
                        {
                            widget->state |= CUI_WIDGET_STATE_PRESSED;
                            cui_window_request_redraw(window);
                        }
                    }
                    else
                    {
                        if (widget->state & CUI_WIDGET_STATE_PRESSED)
                        {
                            widget->state &= ~CUI_WIDGET_STATE_PRESSED;
                            cui_window_request_redraw(window);
                        }
                    }
                } break;

                case CUI_EVENT_TYPE_MOUSE_WHEEL:
                {
                } break;

                case CUI_EVENT_TYPE_LEFT_DOWN:
                case CUI_EVENT_TYPE_DOUBLE_CLICK:
                {
                    widget->state |= CUI_WIDGET_STATE_PRESSED;
                    cui_window_request_redraw(window);
                    cui_window_set_pressed(window, widget);
                    result = true;
                } break;

                case CUI_EVENT_TYPE_LEFT_UP:
                {
                    if (widget->state & CUI_WIDGET_STATE_PRESSED)
                    {
                        widget->state &= ~CUI_WIDGET_STATE_PRESSED;
                        cui_window_request_redraw(window);

                        if (widget->on_action)
                        {
                            widget->on_action(widget);
                        }
                    }
                } break;

                case CUI_EVENT_TYPE_RIGHT_DOWN:
                {
                } break;

                case CUI_EVENT_TYPE_RIGHT_UP:
                {
                } break;

                case CUI_EVENT_TYPE_KEY_DOWN:
                {
                } break;

                case CUI_EVENT_TYPE_KEY_UP:
                {
                } break;

                case CUI_EVENT_TYPE_FOCUS:
                {
                } break;

                case CUI_EVENT_TYPE_UNFOCUS:
                {
                } break;

                case CUI_EVENT_TYPE_POINTER_DOWN:
                {
                    widget->state |= CUI_WIDGET_STATE_PRESSED;
                    cui_window_request_redraw(window);
                    cui_window_set_pressed(window, widget);
                    result = true;
                } break;

                case CUI_EVENT_TYPE_POINTER_UP:
                {
                    if (widget->state & CUI_WIDGET_STATE_PRESSED)
                    {
                        widget->state &= ~CUI_WIDGET_STATE_PRESSED;
                        cui_window_request_redraw(window);

                        if (widget->on_action)
                        {
                            widget->on_action(widget);
                        }
                    }
                } break;

                case CUI_EVENT_TYPE_POINTER_MOVE:
                {
                } break;
            }
        } break;

        case CUI_WIDGET_TYPE_CHECKBOX:
        {
            switch (event_type)
            {
                case CUI_EVENT_TYPE_MOUSE_ENTER:
                {
                    widget->state |= CUI_WIDGET_STATE_HOVERED;
                    cui_window_request_redraw(window);
                    result = true;
                } break;

                case CUI_EVENT_TYPE_MOUSE_LEAVE:
                {
                    widget->state &= ~CUI_WIDGET_STATE_HOVERED;
                    cui_window_request_redraw(window);
                    result = true;
                } break;

                case CUI_EVENT_TYPE_MOUSE_MOVE:
                {
                } break;

                case CUI_EVENT_TYPE_MOUSE_DRAG:
                {
                    if (cui_event_is_inside_widget(window, widget))
                    {
                        if (!(widget->state & CUI_WIDGET_STATE_PRESSED))
                        {
                            widget->value = !widget->old_value;
                            widget->state |= CUI_WIDGET_STATE_PRESSED;
                            cui_window_request_redraw(window);
                        }
                    }
                    else
                    {
                        if (widget->state & CUI_WIDGET_STATE_PRESSED)
                        {
                            widget->value = widget->old_value;
                            widget->state &= ~CUI_WIDGET_STATE_PRESSED;
                            cui_window_request_redraw(window);
                        }
                    }
                } break;

                case CUI_EVENT_TYPE_MOUSE_WHEEL:
                {
                } break;

                case CUI_EVENT_TYPE_LEFT_DOWN:
                case CUI_EVENT_TYPE_DOUBLE_CLICK:
                {
                    widget->old_value = widget->value;
                    widget->value = !widget->old_value;
                    widget->state |= CUI_WIDGET_STATE_PRESSED;
                    cui_window_request_redraw(window);
                    cui_window_set_pressed(window, widget);
                    result = true;
                } break;

                case CUI_EVENT_TYPE_LEFT_UP:
                {
                    if (widget->state & CUI_WIDGET_STATE_PRESSED)
                    {
                        widget->value = !widget->old_value;
                        widget->state &= ~CUI_WIDGET_STATE_PRESSED;
                        cui_window_request_redraw(window);

                        if (widget->on_action)
                        {
                            widget->on_action(widget);
                        }
                    }
                } break;

                case CUI_EVENT_TYPE_RIGHT_DOWN:
                {
                } break;

                case CUI_EVENT_TYPE_RIGHT_UP:
                {
                } break;

                case CUI_EVENT_TYPE_KEY_DOWN:
                {
                } break;

                case CUI_EVENT_TYPE_KEY_UP:
                {
                } break;

                case CUI_EVENT_TYPE_FOCUS:
                {
                } break;

                case CUI_EVENT_TYPE_UNFOCUS:
                {
                } break;

                case CUI_EVENT_TYPE_POINTER_DOWN:
                {
                    widget->old_value = widget->value;
                    widget->value = !widget->old_value;
                    widget->state |= CUI_WIDGET_STATE_PRESSED;
                    cui_window_request_redraw(window);
                    result = true;
                } break;

                case CUI_EVENT_TYPE_POINTER_UP:
                {
                    if (widget->state & CUI_WIDGET_STATE_PRESSED)
                    {
                        widget->value = !widget->old_value;
                        widget->state &= ~CUI_WIDGET_STATE_PRESSED;
                        cui_window_request_redraw(window);

                        if (widget->on_action)
                        {
                            widget->on_action(widget);
                        }
                    }
                } break;

                case CUI_EVENT_TYPE_POINTER_MOVE:
                {
                } break;
            }
        } break;

        case CUI_WIDGET_TYPE_TEXTINPUT:
        {
            switch (event_type)
            {
                case CUI_EVENT_TYPE_MOUSE_ENTER:
                {
                    widget->state |= CUI_WIDGET_STATE_HOVERED;
                    cui_window_request_redraw(window);
                    result = true;
                } break;

                case CUI_EVENT_TYPE_MOUSE_LEAVE:
                {
                    widget->state &= ~CUI_WIDGET_STATE_HOVERED;
                    cui_window_request_redraw(window);
                    result = true;
                } break;

                case CUI_EVENT_TYPE_MOUSE_MOVE:
                {
                } break;

                case CUI_EVENT_TYPE_MOUSE_DRAG:
                {
                } break;

                case CUI_EVENT_TYPE_MOUSE_WHEEL:
                {
                } break;

                case CUI_EVENT_TYPE_LEFT_DOWN:
                case CUI_EVENT_TYPE_DOUBLE_CLICK:
                {
                    widget->state |= CUI_WIDGET_STATE_PRESSED | CUI_WIDGET_STATE_FOCUSED;
                    cui_text_input_select_all(&widget->text_input);
                    cui_window_request_redraw(window);
                    cui_window_set_pressed(window, widget);
                    cui_window_set_focused(window, widget);
                    result = true;
                } break;

                case CUI_EVENT_TYPE_LEFT_UP:
                {
                } break;

                case CUI_EVENT_TYPE_RIGHT_DOWN:
                {
                } break;

                case CUI_EVENT_TYPE_RIGHT_UP:
                {
                } break;

                case CUI_EVENT_TYPE_KEY_DOWN:
                {
                    if (window->base.event.key.ctrl_is_down || window->base.event.key.command_is_down)
                    {
                        switch (window->base.event.key.codepoint)
                        {
                            case 'a':
                            {
                                cui_text_input_select_all(&widget->text_input);
                                cui_window_request_redraw(window);
                                result = true;
                            } break;

                            case 'c':
                            {
                                CuiString str = cui_text_input_get_selected_text(&widget->text_input);
                                cui_platform_set_clipboard_text(&window->base.temporary_memory, str);
                                result = true;
                            } break;

                            case 'v':
                            {
                                CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(&window->base.temporary_memory);

                                CuiString str = cui_platform_get_clipboard_text(&window->base.temporary_memory);
                                cui_text_input_insert_string(&widget->text_input, str);
                                _cui_widget_update_text_offset(widget);

                                cui_end_temporary_memory(temp_memory);

                                cui_window_request_redraw(window);
                                result = true;
                            } break;
                        }
                    }
                    else if (cui_unicode_is_printable(window->base.event.key.codepoint))
                    {
                        cui_text_input_insert_codepoint(&widget->text_input, window->base.event.key.codepoint);
                        _cui_widget_update_text_offset(widget);
                        cui_window_request_redraw(window);
                        result = true;
                    }
                    else
                    {
                        switch (window->base.event.key.codepoint)
                        {
                            case CUI_KEY_BACKSPACE:
                            {
                                cui_text_input_backspace(&widget->text_input);
                                _cui_widget_update_text_offset(widget);
                                cui_window_request_redraw(window);
                                result = true;
                            } break;

                            case CUI_KEY_LEFT:
                            {
                                cui_text_input_move_left(&widget->text_input, window->base.event.key.shift_is_down);
                                cui_window_request_redraw(window);
                                result = true;
                            } break;

                            case CUI_KEY_RIGHT:
                            {
                                cui_text_input_move_right(&widget->text_input, window->base.event.key.shift_is_down);
                                cui_window_request_redraw(window);
                                result = true;
                            } break;

                            case CUI_KEY_TAB:
                            {
                            } break;

                            case CUI_KEY_ENTER:
                            {
                                if (widget->on_action)
                                {
                                    widget->on_action(widget);
                                }
                                result = true;
                            } break;
                        }
                    }
                } break;

                case CUI_EVENT_TYPE_KEY_UP:
                {
                } break;

                case CUI_EVENT_TYPE_FOCUS:
                {
                } break;

                case CUI_EVENT_TYPE_UNFOCUS:
                {
                    if (widget->state & CUI_WIDGET_STATE_FOCUSED)
                    {
                        widget->state &= ~CUI_WIDGET_STATE_FOCUSED;
                        cui_window_request_redraw(window);
                    }
                    result = true;
                } break;

                case CUI_EVENT_TYPE_POINTER_DOWN:
                {
                    widget->state |= CUI_WIDGET_STATE_PRESSED | CUI_WIDGET_STATE_FOCUSED;
                    widget->text_input.cursor_start = 0;
                    widget->text_input.cursor_end = cui_utf8_get_character_count(cui_text_input_to_string(widget->text_input));
                    cui_window_request_redraw(window);
                    cui_window_set_focused(window, widget);
                    result = true;
                } break;

                case CUI_EVENT_TYPE_POINTER_UP:
                {
                } break;

                case CUI_EVENT_TYPE_POINTER_MOVE:
                {
                } break;
            }
        } break;
    }

    return result;
}
