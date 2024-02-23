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

static CuiFramebuffer *
_cui_window_frame_routine(CuiWindow *window, CuiEvent *events, CuiWindowFrameResult *window_frame_result)
{
    window->base.window_frame_result.window_frame_actions = 0;

    int32_t event_count = cui_array_count(events);

    for (int32_t event_index = 0; event_index < event_count; event_index += 1)
    {
        CuiEvent *event = events + event_index;

        window->base.event = *event;
        cui_window_handle_event(window, event->type);
    }

    _cui_array_header(window->base.events)->count = 0;

    *window_frame_result = window->base.window_frame_result;

    CuiFramebuffer *framebuffer = 0;

    if (window->base.needs_redraw)
    {
        CuiCommandBuffer *command_buffer = _cui_renderer_begin_command_buffer(window->base.renderer);

        if (window->base.platform_root_widget)
        {
            if (!window->base.glyph_cache.allocated)
            {
                _cui_glyph_cache_initialize(&window->base.glyph_cache, command_buffer,
                                            cui_window_allocate_texture_id(window));
            }
            else
            {
                _cui_glyph_cache_maybe_reset(&window->base.glyph_cache, command_buffer);
            }

            CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(&window->base.temporary_memory);

            CuiRect window_rect = cui_make_rect(0, 0, window->base.width, window->base.height);

            CuiGraphicsContext ctx;
            ctx.clip_rect_offset = 0;
            ctx.clip_rect = window_rect;
            ctx.window_rect = window_rect;
            ctx.command_buffer = command_buffer;
            ctx.glyph_cache = &window->base.glyph_cache;
            ctx.temporary_memory = &window->base.temporary_memory;
            ctx.font_manager = &window->base.font_manager;

            const CuiColorTheme *color_theme = &cui_color_theme_default_dark;

            if (window->base.color_theme)
            {
                color_theme = window->base.color_theme;
            }

            cui_widget_draw(window->base.platform_root_widget, &ctx, color_theme);

#if 0
            int32_t texture_width = ctx.glyph_cache->texture.width;
            int32_t texture_height = ctx.glyph_cache->texture.height;
            _cui_push_textured_rect(ctx.command_buffer, cui_make_rect(10, 10, 10 + texture_width, 10 + texture_height),
                                    cui_make_rect(0, 0, 0, 0), cui_make_color(0.0f, 0.0f, 0.0f, 1.0f), ctx.glyph_cache->texture_id, 0);
            _cui_push_textured_rect(ctx.command_buffer, cui_make_rect(10, 10, 10 + texture_width, 10 + texture_height),
                                    cui_make_rect(0, 0, texture_width, texture_height), cui_make_color(1.0f, 1.0f, 1.0f, 1.0f),
                                    ctx.glyph_cache->texture_id, 0);
#endif

#if 0
            _cui_push_textured_rect(ctx.command_buffer, cui_make_rect(0, 0, window->base.width, 2),
                                    cui_make_rect(0, 0, 0, 0), cui_make_color(1.0f, 0.0f, 0.0f, 1.0f), ctx.glyph_cache->texture_id, 0);
            _cui_push_textured_rect(ctx.command_buffer, cui_make_rect(0, window->base.height - 2, window->base.width, window->base.height),
                                    cui_make_rect(0, 0, 0, 0), cui_make_color(1.0f, 0.0f, 0.0f, 1.0f), ctx.glyph_cache->texture_id, 0);
            _cui_push_textured_rect(ctx.command_buffer, cui_make_rect(0, 2, 2, window->base.height - 2),
                                    cui_make_rect(0, 0, 0, 0), cui_make_color(1.0f, 0.0f, 0.0f, 1.0f), ctx.glyph_cache->texture_id, 0);
            _cui_push_textured_rect(ctx.command_buffer, cui_make_rect(window->base.width - 2, 2, window->base.width, window->base.height - 2),
                                    cui_make_rect(0, 0, 0, 0), cui_make_color(1.0f, 0.0f, 0.0f, 1.0f), ctx.glyph_cache->texture_id, 0);
#endif

            cui_end_temporary_memory(temp_memory);
        }

#if CUI_FRAMEBUFFER_SCREENSHOT_ENABLED

        bool bgra = true;
        bool top_to_bottom = true;
        CuiArena screenshot_arena;
        CuiBitmap screenshot_bitmap = { 0 };

        if (window->base.take_screenshot)
        {
            cui_arena_allocate(&screenshot_arena, CuiMiB(32));
        }

#endif

        CuiColor clear_color = CuiHexColor(0xFF000000);

#if CUI_PLATFORM_LINUX && CUI_BACKEND_WAYLAND_ENABLED

        if (_cui_context.backend == CUI_LINUX_BACKEND_WAYLAND)
        {
            clear_color = CuiHexColor(0x00000000);
        }

#endif

        framebuffer = _cui_acquire_framebuffer(window, window->base.width, window->base.height);
        _cui_renderer_render(window->base.renderer, framebuffer, command_buffer, clear_color);

#if CUI_FRAMEBUFFER_SCREENSHOT_ENABLED

        if (window->base.take_screenshot)
        {
            switch (window->base.renderer->type)
            {
                case CUI_RENDERER_TYPE_SOFTWARE:
                {
#if CUI_RENDERER_SOFTWARE_ENABLED
                    screenshot_bitmap = framebuffer->bitmap;
#else
                    CuiAssert(!"CUI_RENDERER_TYPE_SOFTWARE not enabled.");
#endif
                } break;

                case CUI_RENDERER_TYPE_OPENGLES2:
                {
#if CUI_RENDERER_OPENGLES2_ENABLED
                    bgra = false;
                    top_to_bottom = false;

                    screenshot_bitmap.width = framebuffer->width;
                    screenshot_bitmap.height = framebuffer->height;
                    screenshot_bitmap.stride = screenshot_bitmap.width * 4;
                    screenshot_bitmap.pixels = cui_alloc(&screenshot_arena, screenshot_bitmap.stride * screenshot_bitmap.height,
                                                         CuiDefaultAllocationParams());

                    glReadPixels(0, 0, screenshot_bitmap.width, screenshot_bitmap.height, GL_RGBA, GL_UNSIGNED_BYTE,
                                 screenshot_bitmap.pixels);
#else
                    CuiAssert(!"CUI_RENDERER_TYPE_OPENGLES2 not enabled.");
#endif
                } break;

                case CUI_RENDERER_TYPE_METAL:
                {
#if CUI_RENDERER_METAL_ENABLED
                    screenshot_bitmap.width  = framebuffer->width;
                    screenshot_bitmap.height = framebuffer->height;
                    screenshot_bitmap.stride = screenshot_bitmap.width * 4;
                    screenshot_bitmap.pixels = cui_alloc(&screenshot_arena, screenshot_bitmap.stride * screenshot_bitmap.height,
                                                         CuiDefaultAllocationParams());

                    [drawable.texture getBytes: screenshot_bitmap.pixels
                                   bytesPerRow: screenshot_bitmap.stride
                                    fromRegion: MTLRegionMake2D(0, 0, screenshot_bitmap.width, screenshot_bitmap.height)
                                   mipmapLevel: 0];
#else
                    CuiAssert(!"CUI_RENDERER_TYPE_METAL not enabled.");
#endif
                } break;

                case CUI_RENDERER_TYPE_DIRECT3D11:
                {
#if CUI_RENDERER_DIRECT3D11_ENABLED
                    CuiAssert(!"CUI_FRAMEBUFFER_SCREENSHOT is not supported with CUI_RENDERER_DIRECT3D11.");
#else
                    CuiAssert(!"CUI_RENDERER_TYPE_DIRECT3D11 not enabled.");
#endif
                } break;
            }

            CuiString bmp_data = cui_image_encode_bmp(screenshot_bitmap, top_to_bottom, bgra, &screenshot_arena);

            CuiFile *screenshot_file = cui_platform_file_create(&screenshot_arena, CuiStringLiteral("screenshot_cui.bmp"));

            if (screenshot_file)
            {
                cui_platform_file_truncate(screenshot_file, bmp_data.count);
                cui_platform_file_write(screenshot_file, bmp_data.data, 0, bmp_data.count);
                cui_platform_file_close(screenshot_file);
            }

            cui_arena_deallocate(&screenshot_arena);

            window->base.take_screenshot = false;
        }

#endif

        window->base.needs_redraw = false;

    }

    return framebuffer;
}

void
cui_window_close(CuiWindow *window)
{
    window->base.window_frame_result.window_frame_actions |= CUI_WINDOW_FRAME_ACTION_CLOSE;
}

void
cui_window_set_fullscreen(CuiWindow *window, bool fullscreen)
{
    window->base.window_frame_result.window_frame_actions |= CUI_WINDOW_FRAME_ACTION_SET_FULLSCREEN;
    window->base.window_frame_result.should_be_fullscreen = fullscreen;
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

CuiPoint
cui_window_get_mouse_position(CuiWindow *window)
{
    return window->base.event.mouse;
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

CuiPoint
cui_window_get_pointer_position(CuiWindow *window)
{
    return window->base.event.pointer.position;
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
    return cui_rect_has_point_inside(widget->rect, window->base.event.pointer.position);
}

bool
cui_window_event_is_alt_down(CuiWindow *window)
{
    return window->base.event.key.alt_is_down;
}

bool
cui_window_event_is_ctrl_down(CuiWindow *window)
{
    return window->base.event.key.ctrl_is_down;
}

bool
cui_window_event_is_shift_down(CuiWindow *window)
{
    return window->base.event.key.shift_is_down;
}

bool
cui_window_event_is_command_down(CuiWindow *window)
{
    return window->base.event.key.command_is_down;
}

uint32_t
cui_window_event_get_codepoint(CuiWindow *window)
{
    return window->base.event.key.codepoint;
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

bool
cui_window_handle_event(CuiWindow *window, CuiEventType event_type)
{
    bool result = false;

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
                result = cui_widget_handle_event(window_base->platform_root_widget, CUI_EVENT_TYPE_LEFT_DOWN);
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
                result = cui_widget_handle_event(window_base->platform_root_widget, CUI_EVENT_TYPE_DOUBLE_CLICK);
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

    return result;
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

void
cui_window_update_font(CuiWindow *window, CuiFontId font_id, float size, float line_height)
{
    while (font_id.value > 0)
    {
        int32_t index = (int32_t) font_id.value - 1;

        CuiAssert(index < cui_array_count(window->base.font_manager.sized_fonts));

        CuiSizedFont *sized_font = window->base.font_manager.sized_fonts + index;

        sized_font->size = size;
        sized_font->line_height = line_height;

        _cui_sized_font_update(sized_font, window->base.font_manager.font_file_manager, window->base.ui_scale);

        font_id = sized_font->font.fallback_id;
    }
}

int32_t
cui_window_get_font_line_height(CuiWindow *window, CuiFontId font_id)
{
    CuiFont *font = _cui_font_manager_get_font_from_id(&window->base.font_manager, font_id);
    return font->line_height;
}

int32_t
cui_window_get_font_cursor_offset(CuiWindow *window, CuiFontId font_id)
{
    CuiFont *font = _cui_font_manager_get_font_from_id(&window->base.font_manager, font_id);
    return font->cursor_offset;
}

int32_t
cui_window_get_font_cursor_height(CuiWindow *window, CuiFontId font_id)
{
    CuiFont *font = _cui_font_manager_get_font_from_id(&window->base.font_manager, font_id);
    return font->cursor_height;
}

float
cui_window_get_font_baseline_offset(CuiWindow *window, CuiFontId font_id)
{
    CuiFont *font = _cui_font_manager_get_font_from_id(&window->base.font_manager, font_id);
    return font->baseline_offset;
}

float
cui_window_get_codepoint_width(CuiWindow *window, CuiFontId font_id, uint32_t codepoint)
{
    return _cui_font_get_codepoint_width(&window->base.font_manager, font_id, codepoint);
}

float
cui_window_get_string_width(CuiWindow *window, CuiFontId font_id, CuiString str)
{
    return _cui_font_get_string_width(&window->base.font_manager, font_id, str);
}

float
cui_window_get_string_width_until_character(CuiWindow *window, CuiFontId font_id, CuiString str, int64_t character_index)
{
    return _cui_font_get_substring_width(&window->base.font_manager, font_id, str, character_index);
}
