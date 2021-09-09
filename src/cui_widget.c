void
cui_widget_box_init(CuiWidget *widget)
{
    widget->type = CUI_WIDGET_TYPE_BOX;
    widget->flags = 0;
    widget->parent = 0;
    widget->window = 0;
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
    widget->parent = 0;
    widget->window = 0;
    widget->ui_scale = 0.0f;
    widget->label = cui_make_string(0, 0);

    CuiDListInit(&widget->list);
    CuiDListInit(&widget->children);

    widget->direction = direction;
}

void
cui_widget_toolbar_init(CuiWidget *widget)
{
    widget->type = CUI_WIDGET_TYPE_TOOLBAR;
    widget->flags = 0;
    widget->parent = 0;
    widget->window = 0;
    widget->ui_scale = 0.0f;
    widget->label = cui_make_string(0, 0);

    CuiDListInit(&widget->list);
    CuiDListInit(&widget->children);
}

void
cui_widget_tabs_init(CuiWidget *widget)
{
    widget->type = CUI_WIDGET_TYPE_TABS;
    widget->flags = 0;
    widget->parent = 0;
    widget->window = 0;
    widget->ui_scale = 0.0f;
    widget->active_index = 0;
    widget->label = cui_make_string(0, 0);

    CuiDListInit(&widget->list);
    CuiDListInit(&widget->children);
}

void
cui_widget_icon_button_init(CuiWidget *widget, CuiString label, CuiIconType icon_type)
{
    widget->type = CUI_WIDGET_TYPE_ICON_BUTTON;
    widget->flags = 0;
    widget->parent = 0;
    widget->window = 0;
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
    widget->parent = 0;
    widget->window = 0;
    widget->ui_scale = 0.0f;
    widget->label = label;
    widget->value = initial_value;

    CuiDListInit(&widget->list);
    CuiDListInit(&widget->children);

    widget->on_action = 0;
}

void
cui_widget_custom_init(CuiWidget *widget)
{
    widget->type = CUI_WIDGET_TYPE_CUSTOM;
    widget->flags = 0;
    widget->parent = 0;
    widget->window = 0;
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
        } break;

        case CUI_WIDGET_TYPE_GRAVITY_BOX:
        {
        } break;

        case CUI_WIDGET_TYPE_TOOLBAR:
        {
            int32_t width = 0;
            int32_t height = 0;

            if (!CuiDListIsEmpty(&widget->children))
            {
                width -= widget->inline_padding;

                CuiForWidget(child, &widget->children)
                {
                    CuiPoint size = cui_widget_get_preferred_size(child);

                    width += size.x + widget->inline_padding;
                    height = cui_max_int32(height, size.y);
                }
            }

            width  += widget->padding.min.x + widget->padding.max.x;
            height += widget->padding.min.y + widget->padding.max.y + widget->border_width;

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
            CuiFont *font = &widget->window->base.font;

            float label_width = cui_font_get_string_width(font, widget->label);
            int32_t padding_x = (int32_t) ceilf(0.5f * (label_width - roundf(widget->ui_scale * 32)));

            result.x = cui_max_int32(lroundf(widget->ui_scale * 32.0f) + 2 * padding_x, (int32_t) ceilf(label_width));
            result.y = (int32_t) ceilf(widget->ui_scale * 24.0f) + (int32_t) font->line_height;
        } break;

        case CUI_WIDGET_TYPE_CHECKBOX:
        {
            CuiFont *font = &widget->window->base.font;

            float label_width = cui_font_get_string_width(font, widget->label);

            result.x = (int32_t) ceilf(widget->ui_scale * 16.0f) +
                       widget->inline_padding +
                       (int32_t) ceilf(label_width);
            result.y = cui_max_int32((int32_t) ceilf(widget->ui_scale * 16.0f),
                                     (int32_t) font->line_height);
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
        } break;

        case CUI_WIDGET_TYPE_GRAVITY_BOX:
        {
            CuiForWidget(child, &widget->children)
            {
                cui_widget_set_ui_scale(child, ui_scale);
            }
        } break;

        case CUI_WIDGET_TYPE_TOOLBAR:
        {
            widget->padding.min.x = lroundf(widget->ui_scale * 12.0f);
            widget->padding.min.y = lroundf(widget->ui_scale * 4.0f);
            widget->padding.max.x = lroundf(widget->ui_scale * 12.0f);
            widget->padding.max.y = lroundf(widget->ui_scale * 4.0f);
            widget->inline_padding = lroundf(widget->ui_scale * 12.0f);
            widget->border_width = lroundf(widget->ui_scale * 0.85f);

            CuiForWidget(child, &widget->children)
            {
                cui_widget_set_ui_scale(child, ui_scale);
            }
        } break;

        case CUI_WIDGET_TYPE_TABS:
        {
            CuiFont *font = &widget->window->base.font;

            widget->padding.min.x = lroundf(widget->ui_scale * 12.0f);
            widget->padding.min.y = lroundf(widget->ui_scale * 4.0f);
            widget->padding.max.x = lroundf(widget->ui_scale * 8.0f);
            widget->padding.max.y = lroundf(widget->ui_scale * 4.0f);
            widget->tabs_width = lroundf(widget->ui_scale * 200.0f);
            widget->tabs_height = (int32_t) font->line_height + widget->padding.min.y + widget->padding.max.y;
            widget->border_width = lroundf(widget->ui_scale * 0.85f);

            CuiForWidget(child, &widget->children)
            {
                cui_widget_set_ui_scale(child, ui_scale);
            }
        } break;

        case CUI_WIDGET_TYPE_CHECKBOX:
        {
            widget->inline_padding = lroundf(widget->ui_scale * 4.0f);
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
        case CUI_WIDGET_TYPE_GRAVITY_BOX:
        {
            CuiAssert(!CuiDListIsEmpty(&widget->children));
            CuiAssert(widget->children.next->next == widget->children.prev);

            CuiWidget *first_child  = (CuiWidget *) widget->children.next;
            CuiWidget *second_child = (CuiWidget *) first_child->list.next;

            switch (widget->direction)
            {
                case CUI_DIRECTION_NORTH:
                {
                    CuiPoint size = cui_widget_get_preferred_size(first_child);

                    CuiRect first_rect = rect;
                    CuiRect second_rect = rect;

                    first_rect.max.y = first_rect.min.y + size.y;
                    second_rect.min.y = cui_min_int32(first_rect.max.y, second_rect.max.y);

                    cui_widget_layout(first_child, first_rect);
                    cui_widget_layout(second_child, second_rect);
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
                    CuiAssert(!"Not implemented");
                } break;
            }
        } break;

        case CUI_WIDGET_TYPE_TOOLBAR:
        {
            int32_t x = widget->rect.min.x + widget->padding.min.x;
            int32_t y = widget->rect.min.y + widget->padding.min.y;

            CuiForWidget(child, &widget->children)
            {
                CuiPoint size = cui_widget_get_preferred_size(child);

                cui_widget_layout(child, cui_make_rect(x, y, x + size.x, y + size.y));

                x += size.x + widget->inline_padding;
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
            CuiFont *font = &widget->window->base.font;

            float label_width = cui_font_get_string_width(font, widget->label);
            int32_t padding_x = (int32_t) ceilf(0.5f * (label_width - roundf(widget->ui_scale * 32)));

            int32_t x0 = widget->rect.min.x + padding_x;
            int32_t y0 = widget->rect.min.y;
            int32_t x1 = x0 + lroundf(widget->ui_scale * 32.0f);
            int32_t y1 = y0 + (int32_t) ceilf(widget->ui_scale * 24.0f);

            widget->action_rect = cui_make_rect(x0, y0, x1, y1);
        } break;
    }
}

void cui_widget_draw(CuiWidget *widget, CuiGraphicsContext *ctx, CuiArena *temporary_memory)
{
    if (widget->type >= CUI_WIDGET_TYPE_CUSTOM)
    {
        widget->draw(widget, ctx, temporary_memory);
        return;
    }

    CuiAssert(widget->window);
    CuiWindow *window = widget->window;

    switch (widget->type)
    {
        case CUI_WIDGET_TYPE_BOX:
        {
            CuiRect prev_clip = cui_draw_set_clip_rect(ctx, widget->rect);

            cui_draw_fill_rect(ctx, widget->rect, cui_make_color(0.1765f, 0.1882f, 0.2157f, 1.0f));

            CuiFont *font = &window->base.font;

            // cui_draw_fill_string(temporary_memory, ctx, font, (float) (widget->rect.min.x + 100),
            //                      (float) (widget->rect.max.y - 100), CuiStringLiteral("Hello world! -> good year."),
            //                      cui_make_color(0.8f, 0.8f, 0.8f, 1.0f));
            cui_draw_fill_string(temporary_memory, ctx, font, (float) (widget->rect.min.x + 100),
                                 (float) (widget->rect.max.y - 100), CuiStringLiteral(">>>><<<<####????"),
                                 cui_make_color(0.8f, 0.8f, 0.8f, 1.0f));
            cui_draw_fill_string(temporary_memory, ctx, font, (float) (widget->rect.max.x - 300),
                                 (float) (widget->rect.max.y - 200), CuiStringLiteral(">>>><<<<####????"),
                                 cui_make_color(0.8f, 0.8f, 0.8f, 1.0f));

            cui_draw_set_clip_rect(ctx, prev_clip);
        } break;

        case CUI_WIDGET_TYPE_GRAVITY_BOX:
        {
            CuiForWidget(child, &widget->children)
            {
                if (cui_rect_overlaps(child->rect, ctx->redraw_rect))
                {
                    cui_widget_draw(child, ctx, temporary_memory);
                }
            }
        } break;

        case CUI_WIDGET_TYPE_TOOLBAR:
        {
            cui_draw_fill_rect(ctx, widget->rect, cui_make_color(0.157f, 0.173f, 0.204f, 1.0f));

            CuiRect border_rect = widget->rect;
            border_rect.min.y = border_rect.max.y - widget->border_width;

            cui_draw_fill_rect(ctx, border_rect, cui_make_color(0.094f, 0.102f, 0.122f, 1.0f));

            CuiForWidget(child, &widget->children)
            {
                if (cui_rect_overlaps(child->rect, ctx->redraw_rect))
                {
                    cui_widget_draw(child, ctx, temporary_memory);
                }
            }
        } break;

        case CUI_WIDGET_TYPE_TABS:
        {
            if (CuiDListIsEmpty(&widget->children))
            {
                cui_draw_fill_rect(ctx, widget->rect, cui_make_color(0.129f, 0.145f, 0.169f, 1.0f));
            }
            else
            {
                CuiRect border_rect = widget->rect;
                CuiRect tab_background_rect = widget->rect;

                tab_background_rect.max.y = cui_min_int32(tab_background_rect.min.y + widget->tabs_height, tab_background_rect.max.y);

                cui_draw_fill_rect(ctx, tab_background_rect, cui_make_color(0.129f, 0.145f, 0.169f, 1.0f));

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

                    CuiFont *font = &window->base.font;

                    if (index == widget->active_index)
                    {
                        active_child = child;

                        cui_draw_fill_rect(ctx, tab_rect, cui_make_color(0.157f, 0.173f, 0.204f, 1.0f));

                        cui_draw_fill_string(temporary_memory, ctx, font, (float) (tab_rect.min.x + widget->padding.min.x),
                                             (float) (tab_rect.min.y + widget->padding.min.y) + font->baseline_offset,
                                             child->label, cui_make_color(0.843f, 0.855f, 0.878f, 1.0f));
                    }
                    else
                    {
                        cui_draw_fill_string(temporary_memory, ctx, font, (float) (tab_rect.min.x + widget->padding.min.x),
                                             (float) (tab_rect.min.y + widget->padding.min.y) + font->baseline_offset,
                                             child->label, cui_make_color(0.416f, 0.443f, 0.486f, 1.0f));
                    }

                    cui_draw_fill_rect(ctx, border_rect, cui_make_color(0.094f, 0.102f, 0.122f, 1.0f));

                    cui_draw_set_clip_rect(ctx, prev_clip);

                    index += 1;
                }

                if (cui_rect_overlaps(active_child->rect, ctx->redraw_rect))
                {
                    cui_widget_draw(active_child, ctx, temporary_memory);
                }
            }
        } break;

        case CUI_WIDGET_TYPE_ICON_BUTTON:
        {
            CuiRect prev_clip = cui_draw_set_clip_rect(ctx, widget->rect);

            if (widget->flags & CUI_WIDGET_FLAG_PRESSED)
            {
                cui_draw_fill_rect(ctx, widget->action_rect, cui_make_color(0.094f, 0.102f, 0.122f, 1.0f));
            }
            else if (widget->flags & CUI_WIDGET_FLAG_HOVERED)
            {
                cui_draw_fill_rect(ctx, widget->action_rect, cui_make_color(0.129f, 0.145f, 0.169f, 1.0f));
            }

            CuiFont *font = &widget->window->base.font;

            float label_width = cui_font_get_string_width(font, widget->label);
            int32_t padding_x = (int32_t) ceilf(0.5f * (label_width - roundf(widget->ui_scale * 32)));
            int32_t icon_offset_x = lroundf(widget->ui_scale * 4.0f);

            float icon_x = (float) (widget->rect.min.x + padding_x + icon_offset_x);

            switch (widget->icon_type)
            {
                case CUI_ICON_LOAD:
                {
                    cui_draw_fill_shape(temporary_memory, ctx, icon_x, (float) widget->rect.min.y,
                                        CUI_SHAPE_LOAD, widget->ui_scale, cui_make_color(0.843f, 0.855f, 0.878f, 1.0f));
                } break;

                case CUI_ICON_TAPE:
                {
                    cui_draw_fill_shape(temporary_memory, ctx, icon_x, (float) widget->rect.min.y,
                                        CUI_SHAPE_TAPE, widget->ui_scale, cui_make_color(0.843f, 0.855f, 0.878f, 1.0f));
                } break;
            }

            cui_draw_fill_string(temporary_memory, ctx, font, (float) widget->rect.min.x,
                                 (float) widget->rect.max.y + font->baseline_offset - font->line_height,
                                 widget->label, cui_make_color(0.843f, 0.855f, 0.878f, 1.0f));

            cui_draw_set_clip_rect(ctx, prev_clip);
        } break;

        case CUI_WIDGET_TYPE_CHECKBOX:
        {
            CuiRect prev_clip = cui_draw_set_clip_rect(ctx, widget->rect);

            CuiFont *font = &widget->window->base.font;

            int32_t px16 = (int32_t) ceilf(widget->ui_scale * 16.0f);
            int32_t offset_x = px16 + widget->inline_padding;

            if (widget->value)
            {
                cui_draw_fill_shape(temporary_memory, ctx, (float) widget->rect.min.x, (float) widget->rect.min.y,
                                    CUI_SHAPE_CHECKBOX_OUTER, widget->ui_scale, cui_make_color(0.337f, 0.541f, 0.949f, 1.0f));
                cui_draw_fill_shape(temporary_memory, ctx, (float) widget->rect.min.x, (float) widget->rect.min.y,
                                    CUI_SHAPE_CHECKMARK, widget->ui_scale, cui_make_color(1.0f, 1.0f, 1.0f, 1.0f));
            }
            else
            {
                cui_draw_fill_shape(temporary_memory, ctx, (float) widget->rect.min.x, (float) widget->rect.min.y,
                                    CUI_SHAPE_CHECKBOX_OUTER, widget->ui_scale, cui_make_color(0.094f, 0.102f, 0.122f, 1.0f));
                cui_draw_fill_shape(temporary_memory, ctx, (float) widget->rect.min.x, (float) widget->rect.min.y,
                                    CUI_SHAPE_CHECKBOX_INNER, widget->ui_scale, cui_make_color(0.184f, 0.200f, 0.239f, 1.0f));
            }

            cui_draw_fill_string(temporary_memory, ctx, font, (float) (widget->rect.min.x + offset_x),
                                 (float) widget->rect.max.y + font->baseline_offset - font->line_height,
                                 widget->label, cui_make_color(0.616f, 0.647f, 0.706f, 1.0f));

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
                case CUI_EVENT_TYPE_ENTER:
                case CUI_EVENT_TYPE_MOVE:
                {
                    result = true;
                } break;
            }
        } break;

        case CUI_WIDGET_TYPE_GRAVITY_BOX:
        case CUI_WIDGET_TYPE_TOOLBAR:
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
                {
                    CuiForWidget(child, &widget->children)
                    {
                        if (cui_event_is_inside_widget(window, child))
                        {
                            if (cui_widget_handle_event(child, CUI_EVENT_TYPE_PRESS))
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
                            if (cui_widget_handle_event(active_child, CUI_EVENT_TYPE_PRESS))
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
                        widget->flags |= CUI_WIDGET_FLAG_HOVERED;
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
                    if (widget->flags & CUI_WIDGET_FLAG_HOVERED)
                    {
                        widget->flags &= ~CUI_WIDGET_FLAG_HOVERED;
                        cui_window_request_redraw(window, widget->rect);
                        result = true;
                    }
                } break;

                case CUI_EVENT_TYPE_PRESS:
                {
                    widget->flags |= CUI_WIDGET_FLAG_PRESSED;
                    cui_window_request_redraw(window, widget->rect);
                    result = true;
                } break;

                case CUI_EVENT_TYPE_DRAG:
                {
                    if (cui_event_is_inside_widget(window, widget))
                    {
                        if (!(widget->flags & CUI_WIDGET_FLAG_PRESSED))
                        {
                            widget->flags |= CUI_WIDGET_FLAG_PRESSED;
                            cui_window_request_redraw(window, widget->rect);
                        }
                    }
                    else
                    {
                        if (widget->flags & CUI_WIDGET_FLAG_PRESSED)
                        {
                            widget->flags &= ~CUI_WIDGET_FLAG_PRESSED;
                            cui_window_request_redraw(window, widget->rect);
                        }
                    }
                } break;

                case CUI_EVENT_TYPE_RELEASE:
                {
                    if (widget->flags & CUI_WIDGET_FLAG_PRESSED)
                    {
                        widget->flags &= ~CUI_WIDGET_FLAG_PRESSED;
                        cui_window_request_redraw(window, widget->rect);

                        if (widget->on_action)
                        {
                            widget->on_action(widget);
                        }
                    }
                    result = true;
                } break;
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
                {
                    widget->old_value = widget->value;
                    widget->value = !widget->old_value;
                    widget->flags |= CUI_WIDGET_FLAG_PRESSED;
                    cui_window_request_redraw(window, widget->rect);
                    result = true;
                } break;

                case CUI_EVENT_TYPE_DRAG:
                {
                    if (cui_event_is_inside_widget(window, widget))
                    {
                        if (!(widget->flags & CUI_WIDGET_FLAG_PRESSED))
                        {
                            widget->value = !widget->old_value;
                            widget->flags |= CUI_WIDGET_FLAG_PRESSED;
                            cui_window_request_redraw(window, widget->rect);
                        }
                    }
                    else
                    {
                        if (widget->flags & CUI_WIDGET_FLAG_PRESSED)
                        {
                            widget->value = widget->old_value;
                            widget->flags &= ~CUI_WIDGET_FLAG_PRESSED;
                            cui_window_request_redraw(window, widget->rect);
                        }
                    }
                    result = true;
                } break;

                case CUI_EVENT_TYPE_RELEASE:
                {
                    if (widget->flags & CUI_WIDGET_FLAG_PRESSED)
                    {
                        widget->value = !widget->old_value;
                        widget->flags &= ~CUI_WIDGET_FLAG_PRESSED;
                        cui_window_request_redraw(window, widget->rect);

                        if (widget->on_action)
                        {
                            widget->on_action(widget);
                        }
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
