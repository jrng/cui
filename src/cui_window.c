static inline void
_cui_window_set_ui_scale(CuiWindow *window, float ui_scale)
{
    if (window->base.ui_scale != ui_scale)
    {
        window->base.ui_scale = ui_scale;

        for (int32_t i = 0; i < cui_array_count(window->base.font_manager.sized_fonts); i += 1)
        {
            CuiSizedFont *sized_font = window->base.font_manager.sized_fonts + i;
            _cui_sized_font_update(sized_font, window->base.font_manager.font_file_manager, window->base.ui_scale);
        }

        if (window->base.platform_root_widget)
        {
            cui_widget_set_ui_scale(window->base.platform_root_widget, window->base.ui_scale);
        }
    }
}

void
cui_window_pack(CuiWindow *window)
{
    if (window->base.user_root_widget)
    {
        CuiPoint size = cui_widget_get_preferred_size(window->base.user_root_widget);
        cui_window_resize(window, size.x, size.y);
    }
}

bool
cui_window_is_maximized(CuiWindow *window)
{
    return (window->base.state & CUI_WINDOW_STATE_MAXIMIZED) ? true : false;
}

bool
cui_window_is_fullscreen(CuiWindow *window)
{
    return (window->base.state & CUI_WINDOW_STATE_FULLSCREEN) ? true : false;
}

bool
cui_window_is_tiled(CuiWindow *window)
{
    return (window->base.state & (CUI_WINDOW_STATE_TILED_LEFT | CUI_WINDOW_STATE_TILED_RIGHT |
                                  CUI_WINDOW_STATE_TILED_TOP | CUI_WINDOW_STATE_TILED_BOTTOM)) ? true : false;
}

bool
cui_window_is_precise_scrolling(CuiWindow *window)
{
    return window->base.event.wheel.is_precise_scrolling;
}

float
cui_window_get_wheel_dx(CuiWindow *window)
{
    return window->base.event.wheel.dx;
}

float
cui_window_get_wheel_dy(CuiWindow *window)
{
    return window->base.event.wheel.dy;
}

int32_t
cui_window_get_pointer_index(CuiWindow *window)
{
    return window->base.event.pointer.index;
}

int32_t
cui_window_get_pointer_x(CuiWindow *window)
{
    return window->base.event.pointer.x;
}

int32_t
cui_window_get_pointer_y(CuiWindow *window)
{
    return window->base.event.pointer.y;
}

void
cui_window_set_pointer_capture(CuiWindow *window, CuiWidget *widget, int32_t pointer_index)
{
    CuiPointerCapture *capture = cui_array_append(window->base.pointer_captures);

    capture->widget = widget;
    capture->pointer_index = pointer_index;
}

bool
cui_event_is_inside_widget(CuiWindow *window, CuiWidget *widget)
{
    return cui_rect_has_point_inside(widget->rect, window->base.event.mouse);
}

bool
cui_event_pointer_is_inside_widget(CuiWindow *window, CuiWidget *widget)
{
    return cui_rect_has_point_inside(widget->rect, cui_make_point(window->base.event.pointer.x, window->base.event.pointer.y));
}

float
cui_window_get_ui_scale(CuiWindow *window)
{
    return window->base.ui_scale;
}

void
cui_window_set_hovered(CuiWindow *window, CuiWidget *widget)
{
    CuiWindowBase *window_base = &window->base;

    CuiWidget *old_widget = window_base->hovered_widget;

    if (widget != old_widget)
    {
        window_base->hovered_widget = widget;

        for (CuiWidget *w = old_widget; w && !cui_widget_contains(w, widget); w = w->parent)
        {
            cui_widget_handle_event(w, CUI_EVENT_TYPE_MOUSE_LEAVE);
        }
    }
}

void
cui_window_set_pressed(CuiWindow *window, CuiWidget *widget)
{
    CuiWindowBase *window_base = &window->base;
    window_base->pressed_widget = widget;
}

void
cui_window_set_focused(CuiWindow *window, CuiWidget *widget)
{
    CuiWindowBase *window_base = &window->base;

    CuiWidget *old_widget = window_base->focused_widget;

    if (widget != old_widget)
    {
        window_base->focused_widget = widget;

        for (CuiWidget *w = old_widget; w && !cui_widget_contains(w, widget); w = w->parent)
        {
            cui_widget_handle_event(w, CUI_EVENT_TYPE_UNFOCUS);
        }
    }
}

void
cui_window_request_redraw(CuiWindow *window)
{
    CuiWindowBase *window_base = &window->base;
    window_base->needs_redraw = true;
}

void
cui_window_set_color_theme(CuiWindow *window, const CuiColorTheme *color_theme)
{
    CuiWindowBase *window_base = &window->base;
    window_base->color_theme = color_theme;
}

void
cui_window_handle_event(CuiWindow *window, CuiEventType event_type)
{
    CuiWindowBase *window_base = &window->base;

    switch (event_type)
    {
        case CUI_EVENT_TYPE_MOUSE_LEAVE:
        {
            cui_window_set_hovered(window, 0);
        } break;

        case CUI_EVENT_TYPE_MOUSE_MOVE:
        {
            if (window_base->pressed_widget)
            {
                cui_widget_handle_event(window_base->pressed_widget, CUI_EVENT_TYPE_MOUSE_DRAG);
            }
            else if (window_base->platform_root_widget)
            {
                if (window_base->hovered_widget)
                {
                    cui_widget_handle_event(window_base->platform_root_widget, CUI_EVENT_TYPE_MOUSE_MOVE);
                }
                else
                {
                    cui_window_set_hovered(window, window_base->platform_root_widget);

                    if (!cui_widget_handle_event(window_base->platform_root_widget, CUI_EVENT_TYPE_MOUSE_ENTER))
                    {
                        cui_window_set_hovered(window, 0);
                    }
                }
            }
        } break;

        case CUI_EVENT_TYPE_MOUSE_WHEEL:
        {
            if (window_base->pressed_widget)
            {
                cui_widget_handle_event(window_base->pressed_widget, CUI_EVENT_TYPE_MOUSE_WHEEL);
            }
            else if (window_base->platform_root_widget)
            {
                cui_widget_handle_event(window_base->platform_root_widget, CUI_EVENT_TYPE_MOUSE_WHEEL);
            }
        } break;

        case CUI_EVENT_TYPE_LEFT_DOWN:
        {
            if (window_base->platform_root_widget)
            {
                cui_widget_handle_event(window_base->platform_root_widget, CUI_EVENT_TYPE_LEFT_DOWN);
            }
        } break;

        case CUI_EVENT_TYPE_LEFT_UP:
        {
            CuiWidget *pressed_widget = window_base->pressed_widget;

            if (pressed_widget)
            {
                cui_window_set_pressed(window, 0);
                cui_widget_handle_event(pressed_widget, CUI_EVENT_TYPE_LEFT_UP);
            }
        } break;

        case CUI_EVENT_TYPE_DOUBLE_CLICK:
        {
            if (window_base->platform_root_widget)
            {
                cui_widget_handle_event(window_base->platform_root_widget, CUI_EVENT_TYPE_DOUBLE_CLICK);
            }
        } break;

        case CUI_EVENT_TYPE_KEY_DOWN:
        {
            if (window_base->focused_widget)
            {
                cui_widget_handle_event(window_base->focused_widget, CUI_EVENT_TYPE_KEY_DOWN);
            }
        } break;

        case CUI_EVENT_TYPE_POINTER_DOWN:
        {
            if (window_base->platform_root_widget)
            {
                cui_widget_handle_event(window_base->platform_root_widget, CUI_EVENT_TYPE_POINTER_DOWN);
            }
        } break;

        case CUI_EVENT_TYPE_POINTER_UP:
        {
            CuiWidget *target = window_base->platform_root_widget;

            for (int32_t i = 0; i < cui_array_count(window_base->pointer_captures); i += 1)
            {
                CuiPointerCapture *capture = window_base->pointer_captures + i;

                if (capture->pointer_index == window_base->event.pointer.index)
                {
                    target = capture->widget;

                    int32_t index = --_cui_array_header(window_base->pointer_captures)->count;
                    window_base->pointer_captures[i] = window_base->pointer_captures[index];

                    break;
                }
            }

            if (target)
            {
                cui_widget_handle_event(target, CUI_EVENT_TYPE_POINTER_UP);
            }
        } break;

        case CUI_EVENT_TYPE_POINTER_MOVE:
        {
            CuiWidget *target = window_base->platform_root_widget;

            for (int32_t i = 0; i < cui_array_count(window_base->pointer_captures); i += 1)
            {
                CuiPointerCapture *capture = window_base->pointer_captures + i;

                if (capture->pointer_index == window_base->event.pointer.index)
                {
                    target = capture->widget;
                    break;
                }
            }

            if (target)
            {
                cui_widget_handle_event(target, CUI_EVENT_TYPE_POINTER_MOVE);
            }
        } break;

        default:
        {
            CuiAssert(!"Event type not allowed for windows");
        } break;
    }
}

int32_t
cui_window_allocate_texture_id(CuiWindow *window)
{
    CuiAssert((8 * sizeof(window->base.allocated_texture_ids)) >= CUI_MAX_TEXTURE_COUNT);

    int32_t texture_id = -1;

    uint32_t index = 0;
    uint32_t mask = ~window->base.allocated_texture_ids;

    if (cui_count_trailing_zeros_u32(&index, mask) && (index < CUI_MAX_TEXTURE_COUNT))
    {
        window->base.allocated_texture_ids |= ((uint32_t) 1 << index);
        texture_id = (int32_t) index;
    }

    return texture_id;
}

void
cui_window_deallocate_texture_id(CuiWindow *window, int32_t texture_id)
{
    CuiAssert(texture_id >= 0);
    CuiAssert(texture_id < CUI_MAX_TEXTURE_COUNT);

    uint32_t bit_mask = (1 << texture_id);

    CuiAssert(window->base.allocated_texture_ids & bit_mask);

    window->base.allocated_texture_ids &= ~bit_mask;
}

CuiFontId
cui_window_find_font_n(CuiWindow *window, const uint32_t n, ...)
{
    va_list args;
    va_start(args, n);

    CuiFontId result = _cui_font_manager_find_font_valist(&window->base.temporary_memory, &window->base.font_manager, window->base.ui_scale, n, args);

    va_end(args);

    return result;
}

int32_t
cui_window_get_font_line_height(CuiWindow *window, CuiFontId font_id)
{
    CuiFont *font = _cui_font_manager_get_font_from_id(&window->base.font_manager, font_id);
    return font->line_height;
}

float
cui_window_get_font_baseline_offset(CuiWindow *window, CuiFontId font_id)
{
    CuiFont *font = _cui_font_manager_get_font_from_id(&window->base.font_manager, font_id);
    return font->baseline_offset;
}

float
cui_window_get_string_width(CuiWindow *window, CuiFontId font_id, CuiString str)
{
    return _cui_font_get_string_width(&window->base.font_manager, font_id, str);
}
