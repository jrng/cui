static inline bool
_cui_is_printable_codepoint(uint32_t codepoint)
{
    return ((codepoint >= 32) && (codepoint != 127));
}

#define _cui_textinput_to_string(text_input) cui_make_string((text_input).data, (text_input).count)

static void
_cui_textinput_remove(CuiTextInput *input, int64_t start, int64_t end)
{
    CuiString str = _cui_textinput_to_string(*input);

    int64_t byte_start = cui_utf8_get_character_byte_offset(str, start);
    int64_t byte_end   = cui_utf8_get_character_byte_offset(str, end);

    uint8_t *src = input->data + byte_end;
    uint8_t *dst = input->data + byte_start;
    int64_t count = input->count - byte_end;

    while (count-- > 0)
    {
        *dst++ = *src++;
    }

    input->count -= (byte_end - byte_start);
}

static void
_cui_textinput_delete_range(CuiTextInput *input)
{
    int64_t a = cui_min_int64(input->cursor_start, input->cursor_end);
    int64_t b = cui_max_int64(input->cursor_start, input->cursor_end);
    _cui_textinput_remove(input, a, b);
    input->cursor_end   = a;
    input->cursor_start = a;
}

static void
_cui_textinput_insert(CuiTextInput *input, uint32_t codepoint)
{
    if (input->cursor_start != input->cursor_end)
    {
        _cui_textinput_delete_range(input);
    }

    uint8_t buf[4];
    int64_t byte_count = cui_utf8_encode(CuiStringLiteral(buf), 0, codepoint);

    if (byte_count)
    {
        CuiString str   = _cui_textinput_to_string(*input);
        int64_t at_byte = cui_utf8_get_character_byte_offset(str, input->cursor_end);
        int64_t count   = input->count - at_byte;

        uint8_t *src = input->data + at_byte;
        uint8_t *dst = src + byte_count;

        cui_copy_memory(dst, src, count);
        cui_copy_memory(input->data + at_byte, buf, byte_count);

        input->count += byte_count;
        input->cursor_end  += 1;
        input->cursor_start = input->cursor_end;
    }
}

static void
_cui_textinput_backspace(CuiTextInput *input)
{
    if (input->cursor_start == input->cursor_end)
    {
        if (input->cursor_end > 0)
        {
            input->cursor_end -= 1;
            _cui_textinput_delete_range(input);
        }
    }
    else
    {
        _cui_textinput_delete_range(input);
    }
}

static void
_cui_textinput_move_left(CuiTextInput *input, bool shift_is_down)
{
    if (shift_is_down)
    {
        input->cursor_end = cui_max_int64(input->cursor_end - 1, (int64_t) 0);
    }
    else
    {
        int64_t at = cui_min_int64(input->cursor_start, input->cursor_end);
        if (input->cursor_start == input->cursor_end)
        {
            input->cursor_end = cui_max_int64(at - 1, (int64_t) 0);
        }
        else
        {
            input->cursor_end = at;
        }
        input->cursor_start = input->cursor_end;
    }
}

static void
_cui_textinput_move_right(CuiTextInput *input, bool shift_is_down)
{
    if (shift_is_down)
    {
        CuiString str = _cui_textinput_to_string(*input);
        int64_t max_count = cui_utf8_get_character_count(str);
        input->cursor_end = cui_min_int64(input->cursor_end + 1, max_count);
    }
    else
    {
        int64_t at = cui_max_int64(input->cursor_start, input->cursor_end);
        if (input->cursor_start == input->cursor_end)
        {
            int64_t max_count = cui_utf8_get_character_count(_cui_textinput_to_string(*input));
            input->cursor_end = cui_min_int64(at + 1, max_count);
        }
        else
        {
            input->cursor_end = at;
        }
        input->cursor_start = input->cursor_end;
    }
}

void
cui_widget_box_init(CuiWidget *widget)
{
    widget->type = CUI_WIDGET_TYPE_BOX;
    widget->flags = 0;
    widget->state = 0;
    widget->parent = 0;
    widget->window = 0;
    widget->color_theme = 0;
    widget->ui_scale = 0.0f;
    widget->label = cui_make_string(0, 0);

    CuiDListInit(&widget->list);
    CuiDListInit(&widget->children);
}

void
cui_widget_gravity_box_init(CuiWidget *widget, CuiDirection direction)
{
    widget->type = CUI_WIDGET_TYPE_GRAVITY_BOX;
    widget->flags = 0;
    widget->state = 0;
    widget->parent = 0;
    widget->window = 0;
    widget->color_theme = 0;
    widget->ui_scale = 0.0f;
    widget->label = cui_make_string(0, 0);

    widget->padding_top    = 4.0f;
    widget->padding_right  = 8.0f;
    widget->padding_bottom = 4.0f;
    widget->padding_left   = 8.0f;
    widget->padding_inline = 8.0f;

    CuiDListInit(&widget->list);
    CuiDListInit(&widget->children);

    widget->direction = direction;
}

void
cui_widget_tabs_init(CuiWidget *widget)
{
    widget->type = CUI_WIDGET_TYPE_TABS;
    widget->flags = 0;
    widget->state = 0;
    widget->parent = 0;
    widget->window = 0;
    widget->color_theme = 0;
    widget->ui_scale = 0.0f;
    widget->active_index = 0;
    widget->label = cui_make_string(0, 0);

    widget->padding_top    = 4.0f;
    widget->padding_right  = 8.0f;
    widget->padding_bottom = 4.0f;
    widget->padding_left   = 12.0f;

    CuiDListInit(&widget->list);
    CuiDListInit(&widget->children);
}

void
cui_widget_icon_button_init(CuiWidget *widget, CuiString label, CuiIconType icon_type)
{
    widget->type = CUI_WIDGET_TYPE_ICON_BUTTON;
    widget->flags = 0;
    widget->state = 0;
    widget->parent = 0;
    widget->window = 0;
    widget->color_theme = 0;
    widget->ui_scale = 0.0f;
    widget->label = label;
    widget->icon_type = icon_type;

    CuiDListInit(&widget->list);
    CuiDListInit(&widget->children);

    widget->on_action = 0;
}

void
cui_widget_checkbox_init(CuiWidget *widget, CuiString label, bool initial_value)
{
    widget->type = CUI_WIDGET_TYPE_CHECKBOX;
    widget->flags = 0;
    widget->state = 0;
    widget->parent = 0;
    widget->window = 0;
    widget->color_theme = 0;
    widget->ui_scale = 0.0f;
    widget->label = label;
    widget->value = initial_value;

    CuiDListInit(&widget->list);
    CuiDListInit(&widget->children);

    widget->on_action = 0;
}

void
cui_widget_textinput_init(CuiWidget *widget, void *buffer, int64_t size)
{
    widget->type = CUI_WIDGET_TYPE_TEXTINPUT;
    widget->flags = 0;
    widget->state = 0;
    widget->parent = 0;
    widget->window = 0;
    widget->color_theme = 0;
    widget->ui_scale = 0.0f;
    widget->label = cui_make_string(0, 0);
    widget->icon_type = CUI_ICON_NONE;
    widget->text_input.cursor_start = 0;
    widget->text_input.cursor_end = 0;
    widget->text_input.count = 0;
    widget->text_input.capacity = size;
    widget->text_input.data = (uint8_t *) buffer;

    CuiDListInit(&widget->list);
    CuiDListInit(&widget->children);

    widget->on_action = 0;
}

void
cui_widget_custom_init(CuiWidget *widget)
{
    widget->type = CUI_WIDGET_TYPE_CUSTOM;
    widget->flags = 0;
    widget->state = 0;
    widget->parent = 0;
    widget->window = 0;
    widget->color_theme = 0;
    widget->ui_scale = 0.0f;
    widget->label = cui_make_string(0, 0);

    CuiDListInit(&widget->list);
    CuiDListInit(&widget->children);

    widget->on_action = 0;

    widget->get_preferred_size = 0;
    widget->set_ui_scale = 0;
    widget->layout = 0;
    widget->draw = 0;
    widget->handle_event = 0;
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

    CuiForWidget(child, &widget->children)
    {
        cui_widget_set_window(child, window);
    }
}

void
cui_widget_append_child(CuiWidget *widget, CuiWidget *child)
{
    child->parent = widget;
    CuiDListInsertBefore(&widget->children, &child->list);

    if (widget->window)
    {
        cui_widget_set_window(widget, widget->window);
    }
}

CuiPoint
cui_widget_get_preferred_size(CuiWidget *widget)
{
    CuiPoint result = { 0, 0 };

    if (widget->type >= CUI_WIDGET_TYPE_CUSTOM)
    {
        return widget->get_preferred_size(widget);
    }

    switch (widget->type)
    {
        case CUI_WIDGET_TYPE_BOX:
        {
            if (!CuiDListIsEmpty(&widget->children))
            {
                CuiAssert(widget->children.next == widget->children.prev);
                result = cui_widget_get_preferred_size((CuiWidget *) widget->children.next);
            }
        } break;

        case CUI_WIDGET_TYPE_GRAVITY_BOX:
        {
            int32_t width = 0;
            int32_t height = 0;

            switch (widget->direction)
            {
                case CUI_DIRECTION_NORTH:
                case CUI_DIRECTION_SOUTH:
                {
                    if (!CuiDListIsEmpty(&widget->children))
                    {
                        CuiForWidget(child, &widget->children)
                        {
                            CuiPoint size = cui_widget_get_preferred_size(child);

                            width = cui_max_int32(width, size.x);
                            height += size.y;
                        }
                    }
                } break;

                case CUI_DIRECTION_EAST:
                case CUI_DIRECTION_WEST:
                {
                    if (!CuiDListIsEmpty(&widget->children))
                    {
                        width -= widget->effective_inline_padding;

                        CuiForWidget(child, &widget->children)
                        {
                            CuiPoint size = cui_widget_get_preferred_size(child);

                            width += size.x + widget->effective_inline_padding;
                            height = cui_max_int32(height, size.y);
                        }
                    }
                } break;
            }

            width  += widget->effective_padding.min.x + widget->effective_padding.max.x;
            height += widget->effective_padding.min.y + widget->effective_padding.max.y;

            result = cui_make_point(width, height);
        } break;

        case CUI_WIDGET_TYPE_TABS:
        {
            int32_t width = 0;
            int32_t height = 0;

            CuiForWidget(child, &widget->children)
            {
                CuiPoint size = cui_widget_get_preferred_size(child);

                width = cui_max_int32(width, size.x);
                height = cui_max_int32(height, size.y);
            }

            height += widget->tabs_height + widget->border_width;

            result = cui_make_point(width, height);
        } break;

        case CUI_WIDGET_TYPE_ICON_BUTTON:
        {
            CuiFont *font = widget->window->base.font;

            float label_width = cui_font_get_string_width(font, widget->label);
            int32_t padding_x = (int32_t) ceilf(0.5f * (label_width - roundf(widget->ui_scale * 32)));

            result.x = cui_max_int32(lroundf(widget->ui_scale * 32.0f) + 2 * padding_x, (int32_t) ceilf(label_width));
            result.y = (int32_t) ceilf(widget->ui_scale * 24.0f) + font->line_height;
        } break;

        case CUI_WIDGET_TYPE_CHECKBOX:
        {
            CuiFont *font = widget->window->base.font;

            float label_width = cui_font_get_string_width(font, widget->label);

            int32_t checkbox_icon_size = (int32_t) ceilf(widget->ui_scale * 16.0f);

            result.x = checkbox_icon_size + widget->effective_inline_padding + (int32_t) ceilf(label_width);
            result.y = cui_max_int32(checkbox_icon_size, font->line_height);
        } break;

        case CUI_WIDGET_TYPE_TEXTINPUT:
        {
            // TODO: this only is correct for 16x16 icons
            int32_t icon_width = (int32_t) ceilf(widget->ui_scale * 16.0f);
            int32_t icon_height = icon_width;

            int32_t preferred_text_width = lroundf(widget->ui_scale * 100.0f);

            result.x = widget->effective_padding.min.x + widget->effective_padding.max.x +
                       icon_width + preferred_text_width;
            result.y = 2 * widget->effective_inline_padding + icon_height;
        } break;
    }

    return result;
}

void
cui_widget_set_ui_scale(CuiWidget *widget, float ui_scale)
{
    if (widget->ui_scale == ui_scale)
    {
         return;
    }

    widget->ui_scale = ui_scale;

    if (widget->type >= CUI_WIDGET_TYPE_CUSTOM)
    {
        widget->set_ui_scale(widget, ui_scale);
        return;
    }

    switch (widget->type)
    {
        case CUI_WIDGET_TYPE_BOX:
        {
            if (!CuiDListIsEmpty(&widget->children))
            {
                CuiAssert(widget->children.next == widget->children.prev);
                cui_widget_set_ui_scale((CuiWidget *) widget->children.next, ui_scale);
            }
        } break;

        case CUI_WIDGET_TYPE_GRAVITY_BOX:
        {
            widget->effective_padding.min.x  = lroundf(widget->ui_scale * widget->padding_left);
            widget->effective_padding.min.y  = lroundf(widget->ui_scale * widget->padding_top);
            widget->effective_padding.max.x  = lroundf(widget->ui_scale * widget->padding_right);
            widget->effective_padding.max.y  = lroundf(widget->ui_scale * widget->padding_bottom);
            widget->effective_inline_padding = lroundf(widget->ui_scale * widget->padding_inline);

            CuiForWidget(child, &widget->children)
            {
                cui_widget_set_ui_scale(child, ui_scale);
            }
        } break;

        case CUI_WIDGET_TYPE_TABS:
        {
            CuiFont *font = widget->window->base.font;

            widget->effective_padding.min.x  = lroundf(widget->ui_scale * widget->padding_left);
            widget->effective_padding.min.y  = lroundf(widget->ui_scale * widget->padding_top);
            widget->effective_padding.max.x  = lroundf(widget->ui_scale * widget->padding_right);
            widget->effective_padding.max.y  = lroundf(widget->ui_scale * widget->padding_bottom);

            widget->tabs_width = lroundf(widget->ui_scale * 200.0f);
            widget->tabs_height = font->line_height + widget->effective_padding.min.y + widget->effective_padding.max.y;
            widget->border_width = lroundf(widget->ui_scale * 0.85f);

            CuiForWidget(child, &widget->children)
            {
                cui_widget_set_ui_scale(child, ui_scale);
            }
        } break;

        case CUI_WIDGET_TYPE_CHECKBOX:
        {
            int32_t checkbox_icon_width = (int32_t) ceilf(widget->ui_scale * 16.0f);

            widget->effective_inline_padding = lroundf(widget->ui_scale * 4.0f);
            widget->effective_padding.min.x = checkbox_icon_width + widget->effective_inline_padding;
        } break;

        case CUI_WIDGET_TYPE_TEXTINPUT:
        {
            int32_t px1 = lroundf(widget->ui_scale * 1.0f);
            int32_t px4 = lroundf(widget->ui_scale * 4.0f);

            int32_t padding = px1 + px4;

            widget->effective_inline_padding = padding;
            widget->effective_padding.min.x = padding;
            widget->effective_padding.max.x = padding;
        } break;
    }
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

    switch (widget->type)
    {
        case CUI_WIDGET_TYPE_BOX:
        {
            if (!CuiDListIsEmpty(&widget->children))
            {
                CuiAssert(widget->children.next == widget->children.prev);
                cui_widget_layout((CuiWidget *) widget->children.next, rect);
            }
        } break;

        case CUI_WIDGET_TYPE_GRAVITY_BOX:
        {
            switch (widget->direction)
            {
                case CUI_DIRECTION_NORTH:
                {
                    if (!CuiDListIsEmpty(&widget->children))
                    {
                        CuiRect parent_rect = widget->rect;

                        parent_rect.min.x += widget->effective_padding.min.x;
                        parent_rect.min.y += widget->effective_padding.min.y;
                        parent_rect.max.x -= widget->effective_padding.max.x;
                        parent_rect.max.y -= widget->effective_padding.max.y;

                        CuiRect child_rect = parent_rect;

                        CuiForWidget(child, &widget->children)
                        {
                            if (child->list.next == &widget->children)
                            {
                                child_rect.min.y = cui_min_int32(child_rect.min.y, parent_rect.max.y);
                                child_rect.max.y = parent_rect.max.y;

                                cui_widget_layout(child, child_rect);
                            }
                            else
                            {
                                CuiPoint size = cui_widget_get_preferred_size(child);

                                child_rect.max.y = child_rect.min.y + size.y;

                                cui_widget_layout(child, child_rect);

                                child_rect.min.y = child_rect.max.y;
                            }
                        }
                    }
                } break;

                case CUI_DIRECTION_EAST:
                {
                    CuiAssert(!"Not implemented");
                } break;

                case CUI_DIRECTION_SOUTH:
                {
                    CuiAssert(!"Not implemented");
                } break;

                case CUI_DIRECTION_WEST:
                {
                    if (!CuiDListIsEmpty(&widget->children))
                    {
                        CuiRect parent_rect = widget->rect;

                        parent_rect.min.x += widget->effective_padding.min.x;
                        parent_rect.min.y += widget->effective_padding.min.y;
                        parent_rect.max.x -= widget->effective_padding.max.x;
                        parent_rect.max.y -= widget->effective_padding.max.y;

                        CuiRect child_rect = parent_rect;

                        CuiForWidget(child, &widget->children)
                        {
                            if (child->list.next == &widget->children)
                            {
                                child_rect.min.x = cui_min_int32(child_rect.min.x, parent_rect.max.x);
                                child_rect.max.x = parent_rect.max.x;

                                cui_widget_layout(child, child_rect);
                            }
                            else
                            {
                                CuiPoint size = cui_widget_get_preferred_size(child);

                                child_rect.max.x = child_rect.min.x + size.x;

                                cui_widget_layout(child, child_rect);

                                child_rect.min.x = child_rect.max.x + widget->effective_inline_padding;
                            }
                        }
                    }
                } break;
            }
        } break;

        case CUI_WIDGET_TYPE_TABS:
        {
            CuiRect child_rect = rect;

            child_rect.min.y = cui_min_int32(child_rect.min.y + widget->tabs_height + widget->border_width, child_rect.max.y);

            CuiForWidget(child, &widget->children)
            {
                cui_widget_layout(child, child_rect);
            }
        } break;

        case CUI_WIDGET_TYPE_ICON_BUTTON:
        {
            CuiFont *font = widget->window->base.font;

            float label_width = cui_font_get_string_width(font, widget->label);
            int32_t padding_x = (int32_t) ceilf(0.5f * (label_width - roundf(widget->ui_scale * 32)));

            int32_t x0 = widget->rect.min.x + padding_x;
            int32_t y0 = widget->rect.min.y;
            int32_t x1 = x0 + lroundf(widget->ui_scale * 32.0f);
            int32_t y1 = y0 + (int32_t) ceilf(widget->ui_scale * 24.0f);

            widget->action_rect = cui_make_rect(x0, y0, x1, y1);
        } break;

        case CUI_WIDGET_TYPE_CHECKBOX:
        {
            CuiFont *font = widget->window->base.font;

            int32_t height = cui_rect_get_height(widget->rect);
            int32_t checkbox_icon_height = (int32_t) ceilf(widget->ui_scale * 16.0f);

            widget->effective_padding.min.y = (height - checkbox_icon_height) / 2;
            widget->effective_padding.max.y = (height - font->line_height) / 2;
        } break;

        case CUI_WIDGET_TYPE_TEXTINPUT:
        {
            CuiFont *font = widget->window->base.font;

            int32_t height = cui_rect_get_height(widget->rect);
            int32_t icon_height = (int32_t) ceilf(widget->ui_scale * 16.0f);

            widget->effective_padding.min.y = (height - icon_height) / 2;
            widget->effective_padding.max.y = (height - font->line_height) / 2;
        } break;
    }
}

void cui_widget_draw(CuiWidget *widget, CuiGraphicsContext *ctx, const CuiColorTheme *color_theme, CuiArena *temporary_memory)
{
    if (widget->color_theme)
    {
        color_theme = widget->color_theme;
    }

    if (widget->type >= CUI_WIDGET_TYPE_CUSTOM)
    {
        widget->draw(widget, ctx, color_theme, temporary_memory);
        return;
    }

    CuiAssert(widget->window);
    CuiWindow *window = widget->window;

    switch (widget->type)
    {
        case CUI_WIDGET_TYPE_BOX:
        {
            CuiRect prev_clip = cui_draw_set_clip_rect(ctx, widget->rect);

            if (widget->flags & CUI_WIDGET_FLAG_FILL_BACKGROUND)
            {
                cui_draw_fill_rect(ctx, widget->rect, color_theme->default_bg);
            }

            if (!CuiDListIsEmpty(&widget->children))
            {
                CuiAssert(widget->children.next == widget->children.prev);
                cui_widget_draw((CuiWidget *) widget->children.next, ctx, color_theme, temporary_memory);
            }

            cui_draw_set_clip_rect(ctx, prev_clip);
        } break;

        case CUI_WIDGET_TYPE_GRAVITY_BOX:
        {
            CuiForWidget(child, &widget->children)
            {
                if (cui_rect_overlaps(child->rect, ctx->redraw_rect))
                {
                    cui_widget_draw(child, ctx, color_theme, temporary_memory);
                }
            }
        } break;

        case CUI_WIDGET_TYPE_TABS:
        {
            if (CuiDListIsEmpty(&widget->children))
            {
                cui_draw_fill_rect(ctx, widget->rect, color_theme->tabs_bg);
            }
            else
            {
                CuiRect border_rect = widget->rect;
                CuiRect tab_background_rect = widget->rect;

                tab_background_rect.max.y = cui_min_int32(tab_background_rect.min.y + widget->tabs_height, tab_background_rect.max.y);

                cui_draw_fill_rect(ctx, tab_background_rect, color_theme->tabs_bg);

                border_rect.min.y = cui_min_int32(border_rect.min.y + widget->tabs_height, border_rect.max.y);
                border_rect.max.y = cui_min_int32(border_rect.min.y + widget->border_width, border_rect.max.y);

                cui_draw_fill_rect(ctx, border_rect, cui_make_color(0.094f, 0.102f, 0.122f, 1.0f));

                uint32_t index = 0;
                CuiWidget *active_child = 0;

                CuiForWidget(child, &widget->children)
                {
                    CuiRect tab_rect = cui_make_rect(widget->rect.min.x + index * widget->tabs_width,
                                                     widget->rect.min.y, widget->rect.min.x + (index + 1) * widget->tabs_width,
                                                     widget->rect.min.y + widget->tabs_height + widget->border_width);

                    tab_rect = cui_rect_get_intersection(tab_rect, widget->rect);

                    CuiRect border_rect = cui_make_rect(widget->rect.min.x + (index + 1) * widget->tabs_width - widget->border_width,
                                                        widget->rect.min.y, widget->rect.min.x + (index + 1) * widget->tabs_width,
                                                        widget->rect.min.y + widget->tabs_height + widget->border_width);

                    CuiRect prev_clip = cui_draw_set_clip_rect(ctx, tab_rect);

                    CuiFont *font = window->base.font;

                    if (index == widget->active_index)
                    {
                        active_child = child;

                        cui_draw_fill_rect(ctx, tab_rect, color_theme->tabs_active_tab_bg);

                        cui_draw_fill_string(temporary_memory, ctx, font, (float) (tab_rect.min.x + widget->effective_padding.min.x),
                                             (float) (tab_rect.min.y + widget->effective_padding.min.y) + font->baseline_offset,
                                             child->label, color_theme->tabs_active_tab_fg);
                    }
                    else
                    {
                        cui_draw_fill_string(temporary_memory, ctx, font, (float) (tab_rect.min.x + widget->effective_padding.min.x),
                                             (float) (tab_rect.min.y + widget->effective_padding.min.y) + font->baseline_offset,
                                             child->label, color_theme->tabs_inactive_tab_fg);
                    }

                    cui_draw_fill_rect(ctx, border_rect, cui_make_color(0.094f, 0.102f, 0.122f, 1.0f));

                    cui_draw_set_clip_rect(ctx, prev_clip);

                    index += 1;
                }

                if (cui_rect_overlaps(active_child->rect, ctx->redraw_rect))
                {
                    cui_widget_draw(active_child, ctx, color_theme, temporary_memory);
                }
            }
        } break;

        case CUI_WIDGET_TYPE_ICON_BUTTON:
        {
            CuiRect prev_clip = cui_draw_set_clip_rect(ctx, widget->rect);

            if (widget->state & CUI_WIDGET_STATE_PRESSED)
            {
                cui_draw_fill_rounded_rect(temporary_memory, ctx, widget->action_rect, widget->ui_scale * 2.0f, cui_make_color(0.094f, 0.102f, 0.122f, 1.0f));
            }
            else if (widget->state & CUI_WIDGET_STATE_HOVERED)
            {
                cui_draw_fill_rounded_rect(temporary_memory, ctx, widget->action_rect, widget->ui_scale * 2.0f, cui_make_color(0.129f, 0.145f, 0.169f, 1.0f));
            }

            CuiFont *font = widget->window->base.font;

            float label_width = cui_font_get_string_width(font, widget->label);
            int32_t padding_x = (int32_t) ceilf(0.5f * (label_width - roundf(widget->ui_scale * 32)));
            int32_t icon_offset_x = lroundf(widget->ui_scale * 4.0f);

            float icon_x = (float) (widget->rect.min.x + padding_x + icon_offset_x);

            switch (widget->icon_type)
            {
                case CUI_ICON_LOAD:
                {
                    cui_draw_fill_shape(temporary_memory, ctx, icon_x, (float) widget->rect.min.y,
                                        CUI_SHAPE_LOAD_24, widget->ui_scale, cui_make_color(0.843f, 0.855f, 0.878f, 1.0f));
                } break;

                case CUI_ICON_TAPE:
                {
                    cui_draw_fill_shape(temporary_memory, ctx, icon_x, (float) widget->rect.min.y,
                                        CUI_SHAPE_TAPE_24, widget->ui_scale, cui_make_color(0.843f, 0.855f, 0.878f, 1.0f));
                } break;
            }

            cui_draw_fill_string(temporary_memory, ctx, font, (float) widget->rect.min.x,
                                 (float) (widget->rect.max.y - font->line_height) + font->baseline_offset,
                                 widget->label, cui_make_color(0.843f, 0.855f, 0.878f, 1.0f));

            cui_draw_set_clip_rect(ctx, prev_clip);
        } break;

        case CUI_WIDGET_TYPE_CHECKBOX:
        {
            CuiRect prev_clip = cui_draw_set_clip_rect(ctx, widget->rect);

            CuiFont *font = widget->window->base.font;

            if (widget->value)
            {
                cui_draw_fill_shape(temporary_memory, ctx, (float) widget->rect.min.x, (float) (widget->rect.min.y + widget->effective_padding.min.y),
                                    CUI_SHAPE_CHECKBOX_OUTER_16, widget->ui_scale, cui_make_color(0.337f, 0.541f, 0.949f, 1.0f));
                cui_draw_fill_shape(temporary_memory, ctx, (float) widget->rect.min.x, (float) (widget->rect.min.y + widget->effective_padding.min.y),
                                    CUI_SHAPE_CHECKMARK_16, widget->ui_scale, cui_make_color(1.0f, 1.0f, 1.0f, 1.0f));
            }
            else
            {
                cui_draw_fill_shape(temporary_memory, ctx, (float) widget->rect.min.x, (float) (widget->rect.min.y + widget->effective_padding.min.y),
                                    CUI_SHAPE_CHECKBOX_OUTER_16, widget->ui_scale, cui_make_color(0.094f, 0.102f, 0.122f, 1.0f));
                cui_draw_fill_shape(temporary_memory, ctx, (float) widget->rect.min.x, (float) (widget->rect.min.y + widget->effective_padding.min.y),
                                    CUI_SHAPE_CHECKBOX_INNER_16, widget->ui_scale, cui_make_color(0.184f, 0.200f, 0.239f, 1.0f));
            }

            cui_draw_fill_string(temporary_memory, ctx, font, (float) (widget->rect.min.x + widget->effective_padding.min.x),
                                 (float) (widget->rect.min.y + widget->effective_padding.max.y) + font->baseline_offset,
                                 widget->label, cui_make_color(0.616f, 0.647f, 0.706f, 1.0f));

            cui_draw_set_clip_rect(ctx, prev_clip);
        } break;

        case CUI_WIDGET_TYPE_TEXTINPUT:
        {
            CuiRect prev_clip = cui_draw_set_clip_rect(ctx, widget->rect);

            CuiFont *font = widget->window->base.font;

            int32_t px1 = lroundf(widget->ui_scale * 1.0f);
            float radius = roundf(widget->ui_scale * 3.0f);

            CuiRect border_rect = widget->rect;
            CuiRect fill_rect = cui_make_rect(border_rect.min.x + px1, border_rect.min.y + px1,
                                              border_rect.max.x - px1, border_rect.max.y - px1);

            if (widget->state & CUI_WIDGET_STATE_FOCUSED)
            {
                cui_draw_fill_rounded_rect(temporary_memory, ctx, border_rect, radius, cui_make_color(0.337f, 0.541f, 0.949f, 1.0f));
                cui_draw_fill_rounded_rect(temporary_memory, ctx, fill_rect, radius - (float) px1, cui_make_color(0.094f, 0.102f, 0.122f, 1.0f));
            }
            else
            {
                cui_draw_fill_rounded_rect(temporary_memory, ctx, border_rect, radius, cui_make_color(0.184f, 0.200f, 0.239f, 1.0f));
                cui_draw_fill_rounded_rect(temporary_memory, ctx, fill_rect, radius - (float) px1, cui_make_color(0.094f, 0.102f, 0.122f, 1.0f));
            }

            cui_draw_set_clip_rect(ctx, fill_rect);

            CuiString str = _cui_textinput_to_string(widget->text_input);

            int32_t x0 = widget->rect.min.x + widget->effective_padding.min.x;
            int32_t y0 = widget->rect.min.y + widget->effective_padding.max.y;

            float cursor_end = cui_font_get_substring_width(font, str, widget->text_input.cursor_end);

            if (widget->text_input.cursor_start != widget->text_input.cursor_end)
            {
                int32_t cursor_start = lroundf(cui_font_get_substring_width(font, str, widget->text_input.cursor_start));
                int32_t _cursor_end = lroundf(cursor_end);
                int32_t a = cui_min_int32(cursor_start, _cursor_end);
                int32_t b = cui_max_int32(cursor_start, _cursor_end);

                cui_draw_fill_rect(ctx, cui_make_rect(x0 + a, y0, x0 + b, y0 + font->line_height), cui_make_color(0.337f, 0.541f, 0.949f, 0.75f));
            }

            cui_draw_fill_string(temporary_memory, ctx, font, (float) x0, (float) y0 + font->baseline_offset,
                                 str, cui_make_color(0.843f, 0.855f, 0.878f, 1.0f));

            if (widget->state & CUI_WIDGET_STATE_FOCUSED)
            {
                float cursor_width = roundf(widget->ui_scale * 1.0f);
                int32_t cursor_x0 = x0 + lroundf(cursor_end + 0.5f * cursor_width);
                int32_t cursor_x1 = cursor_x0 + (int32_t) cursor_width;

                cui_draw_fill_rect(ctx, cui_make_rect(cursor_x0, y0, cursor_x1, y0 + font->line_height), cui_make_color(1.0f, 1.0f, 1.0f, 1.0f));
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
        case CUI_WIDGET_TYPE_GRAVITY_BOX:
        {
            switch (event_type)
            {
                case CUI_EVENT_TYPE_ENTER:
                case CUI_EVENT_TYPE_MOVE:
                {
                    bool on_hovered_branch = false;

                    CuiForWidget(child, &widget->children)
                    {
                        if (cui_event_is_inside_widget(window, child))
                        {
                            if (cui_widget_contains(child, window->base.hovered_widget))
                            {
                                result = cui_widget_handle_event(child, CUI_EVENT_TYPE_MOVE);
                                on_hovered_branch = true;
                                break;
                            }
                            else
                            {
                                cui_window_set_hovered(window, child);
                                if (cui_widget_handle_event(child, CUI_EVENT_TYPE_ENTER))
                                {
                                    result = true;
                                    break;
                                }
                            }
                        }
                    }

                    if (!on_hovered_branch && !result)
                    {
                        cui_window_set_hovered(window, widget);
                        result = true;
                    }
                } break;

                case CUI_EVENT_TYPE_LEAVE:
                {
                } break;

                case CUI_EVENT_TYPE_PRESS:
                case CUI_EVENT_TYPE_DOUBLE_CLICK:
                {
                    CuiForWidget(child, &widget->children)
                    {
                        if (cui_event_is_inside_widget(window, child))
                        {
                            if (cui_widget_handle_event(child, event_type))
                            {
                                if (window->base.pressed_widget && !cui_widget_contains(child, window->base.pressed_widget))
                                {
                                    cui_window_set_pressed(window, child);
                                }
                                result = true;
                            }
                        }
                    }
                } break;

                case CUI_EVENT_TYPE_WHEEL:
                {
                    CuiForWidget(child, &widget->children)
                    {
                        if (cui_event_is_inside_widget(window, child))
                        {
                            if (cui_widget_handle_event(child, CUI_EVENT_TYPE_WHEEL))
                            {
                                result = true;
                                break;
                            }
                        }
                    }
                } break;

                default: break;
            }
        } break;

        case CUI_WIDGET_TYPE_TABS:
        {
            switch (event_type)
            {
                case CUI_EVENT_TYPE_ENTER:
                case CUI_EVENT_TYPE_MOVE:
                {
                    uint32_t index = 0;
                    CuiWidget *active_child = 0;

                    CuiForWidget(child, &widget->children)
                    {
                        if (index == widget->active_index)
                        {
                            active_child = child;
                            break;
                        }

                        index += 1;
                    }

                    bool on_hovered_branch = false;

                    if (active_child && cui_event_is_inside_widget(window, active_child))
                    {
                        if (cui_widget_contains(active_child, window->base.hovered_widget))
                        {
                            result = cui_widget_handle_event(active_child, CUI_EVENT_TYPE_MOVE);
                            on_hovered_branch = true;
                            break;
                        }
                        else
                        {
                            cui_window_set_hovered(window, active_child);
                            if (cui_widget_handle_event(active_child, CUI_EVENT_TYPE_ENTER))
                            {
                                result = true;
                                break;
                            }
                        }
                    }

                    if (!on_hovered_branch && !result)
                    {
                        cui_window_set_hovered(window, widget);
                        result = true;
                    }
                } break;

                case CUI_EVENT_TYPE_LEAVE:
                {
                } break;

                case CUI_EVENT_TYPE_PRESS:
                case CUI_EVENT_TYPE_DOUBLE_CLICK:
                {
                    CuiRect tab_row_rect = widget->rect;
                    tab_row_rect.max.y = tab_row_rect.min.y + widget->tabs_height;

                    tab_row_rect = cui_rect_get_intersection(tab_row_rect, widget->rect);

                    if (cui_event_is_inside_rect(window, tab_row_rect))
                    {
                        uint32_t index = 0;

                        CuiForWidget(child, &widget->children)
                        {
                            CuiRect tab_rect = cui_make_rect(widget->rect.min.x + index * widget->tabs_width,
                                                             widget->rect.min.y, widget->rect.min.x + (index + 1) * widget->tabs_width,
                                                             widget->rect.min.y + widget->tabs_height + widget->border_width);

                            tab_rect = cui_rect_get_intersection(tab_rect, widget->rect);

                            if (cui_event_is_inside_rect(window, tab_rect))
                            {
                                if (index != widget->active_index)
                                {
                                    widget->active_index = index;
                                    cui_window_request_redraw(window, widget->rect);
                                }

                                break;
                            }

                            index += 1;
                        }
                    }
                    else
                    {
                        uint32_t index = 0;
                        CuiWidget *active_child = 0;

                        CuiForWidget(child, &widget->children)
                        {
                            if (index == widget->active_index)
                            {
                                active_child = child;
                                break;
                            }

                            index += 1;
                        }

                        if (active_child && cui_event_is_inside_widget(window, active_child))
                        {
                            if (cui_widget_handle_event(active_child, event_type))
                            {
                                if (window->base.pressed_widget && !cui_widget_contains(active_child, window->base.pressed_widget))
                                {
                                    cui_window_set_pressed(window, active_child);
                                }
                                result = true;
                            }
                        }
                    }
                } break;

                case CUI_EVENT_TYPE_WHEEL:
                {
                    CuiForWidget(child, &widget->children)
                    {
                        if (cui_event_is_inside_widget(window, child))
                        {
                            if (cui_widget_handle_event(child, CUI_EVENT_TYPE_WHEEL))
                            {
                                result = true;
                                break;
                            }
                        }
                    }
                } break;

                default: break;
            }
        } break;

        case CUI_WIDGET_TYPE_ICON_BUTTON:
        {
            switch (event_type)
            {
                case CUI_EVENT_TYPE_ENTER:
                {
                    // if (cui_event_is_inside_rect(window, widget->action_rect))
                    {
                        widget->state |= CUI_WIDGET_STATE_HOVERED;
                        cui_window_request_redraw(window, widget->rect);
                        result = true;
                    }
                } break;

                case CUI_EVENT_TYPE_MOVE:
                {
                    result = true;
                } break;

                case CUI_EVENT_TYPE_LEAVE:
                {
                    if (widget->state & CUI_WIDGET_STATE_HOVERED)
                    {
                        widget->state &= ~CUI_WIDGET_STATE_HOVERED;
                        cui_window_request_redraw(window, widget->rect);
                        result = true;
                    }
                } break;

                case CUI_EVENT_TYPE_PRESS:
                case CUI_EVENT_TYPE_DOUBLE_CLICK:
                {
                    widget->state |= CUI_WIDGET_STATE_PRESSED;
                    cui_window_request_redraw(window, widget->rect);
                    result = true;
                } break;

                case CUI_EVENT_TYPE_DRAG:
                {
                    if (cui_event_is_inside_widget(window, widget))
                    {
                        if (!(widget->state & CUI_WIDGET_STATE_PRESSED))
                        {
                            widget->state |= CUI_WIDGET_STATE_PRESSED;
                            cui_window_request_redraw(window, widget->rect);
                        }
                    }
                    else
                    {
                        if (widget->state & CUI_WIDGET_STATE_PRESSED)
                        {
                            widget->state &= ~CUI_WIDGET_STATE_PRESSED;
                            cui_window_request_redraw(window, widget->rect);
                        }
                    }
                } break;

                case CUI_EVENT_TYPE_RELEASE:
                {
                    if (widget->state & CUI_WIDGET_STATE_PRESSED)
                    {
                        widget->state &= ~CUI_WIDGET_STATE_PRESSED;
                        cui_window_request_redraw(window, widget->rect);

                        if (widget->on_action)
                        {
                            widget->on_action(widget);
                        }
                    }
                    result = true;
                } break;

                default: break;
            }
        } break;

        case CUI_WIDGET_TYPE_CHECKBOX:
        {
            switch (event_type)
            {
                case CUI_EVENT_TYPE_ENTER:
                {
                    result = true;
                } break;

                case CUI_EVENT_TYPE_MOVE:
                {
                    result = true;
                } break;

                case CUI_EVENT_TYPE_LEAVE:
                {
                    result = true;
                } break;

                case CUI_EVENT_TYPE_PRESS:
                case CUI_EVENT_TYPE_DOUBLE_CLICK:
                {
                    widget->old_value = widget->value;
                    widget->value = !widget->old_value;
                    widget->state |= CUI_WIDGET_STATE_PRESSED;
                    cui_window_request_redraw(window, widget->rect);
                    result = true;
                } break;

                case CUI_EVENT_TYPE_DRAG:
                {
                    if (cui_event_is_inside_widget(window, widget))
                    {
                        if (!(widget->state & CUI_WIDGET_STATE_PRESSED))
                        {
                            widget->value = !widget->old_value;
                            widget->state |= CUI_WIDGET_STATE_PRESSED;
                            cui_window_request_redraw(window, widget->rect);
                        }
                    }
                    else
                    {
                        if (widget->state & CUI_WIDGET_STATE_PRESSED)
                        {
                            widget->value = widget->old_value;
                            widget->state &= ~CUI_WIDGET_STATE_PRESSED;
                            cui_window_request_redraw(window, widget->rect);
                        }
                    }
                    result = true;
                } break;

                case CUI_EVENT_TYPE_RELEASE:
                {
                    if (widget->state & CUI_WIDGET_STATE_PRESSED)
                    {
                        widget->value = !widget->old_value;
                        widget->state &= ~CUI_WIDGET_STATE_PRESSED;
                        cui_window_request_redraw(window, widget->rect);

                        if (widget->on_action)
                        {
                            widget->on_action(widget);
                        }
                    }
                    result = true;
                } break;

                default: break;
            }
        } break;

        case CUI_WIDGET_TYPE_TEXTINPUT:
        {
            switch (event_type)
            {
                case CUI_EVENT_TYPE_ENTER:
                {
                    result = true;
                } break;

                case CUI_EVENT_TYPE_MOVE:
                {
                    result = true;
                } break;

                case CUI_EVENT_TYPE_LEAVE:
                {
                    result = true;
                } break;

                case CUI_EVENT_TYPE_PRESS:
                case CUI_EVENT_TYPE_DOUBLE_CLICK:
                {
                    widget->state |= CUI_WIDGET_STATE_FOCUSED;
                    cui_window_set_focused(window, widget);
                    cui_window_request_redraw(window, widget->rect);
                    result = true;
                } break;

                case CUI_EVENT_TYPE_KEY_PRESS:
                {
                    if (window->base.event.key.ctrl_is_down)
                    {
                    }
                    else if (_cui_is_printable_codepoint(window->base.event.key.codepoint))
                    {
                        _cui_textinput_insert(&widget->text_input, window->base.event.key.codepoint);
                        cui_window_request_redraw(window, widget->rect);
                    }
                    else
                    {
                        switch(window->base.event.key.codepoint)
                        {
                            case CUI_KEY_BACKSPACE:
                            {
                                _cui_textinput_backspace(&widget->text_input);
                                cui_window_request_redraw(window, widget->rect);
                            } break;

                            case CUI_KEY_LEFT:
                            {
                                _cui_textinput_move_left(&widget->text_input, window->base.event.key.shift_is_down);
                                cui_window_request_redraw(window, widget->rect);
                            } break;

                            case CUI_KEY_RIGHT:
                            {
                                _cui_textinput_move_right(&widget->text_input, window->base.event.key.shift_is_down);
                                cui_window_request_redraw(window, widget->rect);
                            } break;

                            case CUI_KEY_TAB:
                            {
                            } break;
                        }
                    }
                } break;

                case CUI_EVENT_TYPE_FOCUS:
                {
                } break;

                case CUI_EVENT_TYPE_UNFOCUS:
                {
                    if (widget->state & CUI_WIDGET_STATE_FOCUSED)
                    {
                        widget->state &= ~CUI_WIDGET_STATE_FOCUSED;
                        cui_window_request_redraw(window, widget->rect);
                    }
                    result = true;
                } break;
            }
        } break;
    }

    return result;
}

bool
cui_event_is_inside_rect(CuiWindow *window, CuiRect rect)
{
    if ((window->base.event.mouse.x >= rect.min.x) && (window->base.event.mouse.y >= rect.min.y) &&
        (window->base.event.mouse.x < rect.max.x) && (window->base.event.mouse.y < rect.max.y))
    {
        return true;
    }

    return false;
}

bool
cui_event_is_inside_widget(CuiWindow *window, CuiWidget *widget)
{
    return cui_event_is_inside_rect(window, widget->rect);
}

bool
cui_widget_contains(CuiWidget *widget, CuiWidget *child_widget)
{
    for (; child_widget; child_widget = child_widget->parent)
    {
        if (child_widget == widget)
        {
            return true;
        }
    }

    return false;
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
cui_widget_update_children(CuiWidget *widget)
{
    if (widget->window)
    {
        float ui_scale = widget->ui_scale;
        CuiRect rect = widget->rect;

        widget->ui_scale = 0.0f;

        cui_widget_set_ui_scale(widget, ui_scale);
        cui_widget_layout(widget, rect);
        cui_window_request_redraw(widget->window, widget->rect);
    }
}
