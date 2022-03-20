float
cui_window_get_ui_scale(CuiWindow *window)
{
    CuiWindowBase *window_base = (CuiWindowBase *) window;
    return window_base->ui_scale;
}

CuiWidget *
cui_window_get_root_widget(CuiWindow *window)
{
    CuiWindowBase *window_base = (CuiWindowBase *) window;
    return &window_base->root_widget;
}

void
cui_window_set_hovered(CuiWindow *window, CuiWidget *widget)
{
    CuiWindowBase *window_base = (CuiWindowBase *) window;

    CuiWidget *old_widget = window_base->hovered_widget;

    if (widget != old_widget)
    {
        window_base->hovered_widget = widget;

        for (CuiWidget *w = old_widget; w && !cui_widget_contains(w, widget); w = w->parent)
        {
            cui_widget_handle_event(w, CUI_EVENT_TYPE_LEAVE);
        }
    }
}

void
cui_window_set_pressed(CuiWindow *window, CuiWidget *widget)
{
    CuiWindowBase *window_base = (CuiWindowBase *) window;
    window_base->pressed_widget = widget;
}

void
cui_window_set_focused(CuiWindow *window, CuiWidget *widget)
{
    CuiWindowBase *window_base = (CuiWindowBase *) window;

    if (window_base->focused_widget)
    {
        // TODO: do something with the old widget
    }

    window_base->focused_widget = widget;
}

void
cui_window_handle_event(CuiWindow *window, CuiEventType event_type)
{
    CuiWindowBase *window_base = (CuiWindowBase *) window;

    switch (event_type)
    {
        case CUI_EVENT_TYPE_MOVE:
        {
            if (window_base->pressed_widget)
            {
                cui_widget_handle_event(window_base->pressed_widget, CUI_EVENT_TYPE_DRAG);
            }
            else
            {
                if (window_base->hovered_widget)
                {
                    cui_widget_handle_event(&window_base->root_widget, CUI_EVENT_TYPE_MOVE);
                }
                else
                {
                    cui_window_set_hovered(window, &window_base->root_widget);
                    bool handled = cui_widget_handle_event(&window_base->root_widget, CUI_EVENT_TYPE_ENTER);

                    if (!handled)
                    {
                        cui_window_set_hovered(window, 0);
                    }
                }
            }
        } break;

        case CUI_EVENT_TYPE_LEAVE:
        {
            cui_window_set_hovered(window, 0);
        } break;

        case CUI_EVENT_TYPE_PRESS:
        {
            window_base->pressed_widget = &window_base->root_widget;
            cui_widget_handle_event(&window_base->root_widget, CUI_EVENT_TYPE_PRESS);
        } break;

        case CUI_EVENT_TYPE_DRAG:
        {
        } break;

        case CUI_EVENT_TYPE_RELEASE:
        {
            CuiWidget *pressed_widget = window_base->pressed_widget;

            if (pressed_widget)
            {
                window_base->pressed_widget = 0;
                cui_widget_handle_event(pressed_widget, CUI_EVENT_TYPE_RELEASE);
            }
        } break;

        case CUI_EVENT_TYPE_WHEEL:
        {
            if (window_base->pressed_widget)
            {
                cui_widget_handle_event(window_base->pressed_widget, CUI_EVENT_TYPE_WHEEL);
            }
            else
            {
                cui_widget_handle_event(&window_base->root_widget, CUI_EVENT_TYPE_WHEEL);
            }
        } break;

        default:
        {
            CuiAssert(!"Event type not allowed for window");
        } break;
    }
}

void
cui_window_set_user_data(CuiWindow *window, void *user_data)
{
    CuiWindowBase *window_base = (CuiWindowBase *) window;
    window_base->user_data = user_data;
}

void *
cui_window_get_user_data(CuiWindow *window)
{
    CuiWindowBase *window_base = (CuiWindowBase *) window;
    return window_base->user_data;
}

int32_t
cui_window_get_mouse_x(CuiWindow *window)
{
    return window->base.event.mouse.x;
}

int32_t
cui_window_get_mouse_y(CuiWindow *window)
{
    return window->base.event.mouse.y;
}

int32_t
cui_window_get_wheel_dx(CuiWindow *window)
{
    return window->base.event.wheel.dx;
}

CuiFont *
cui_window_get_font(CuiWindow *window, CuiFontType font_type)
{
    return window->base.font;
}
