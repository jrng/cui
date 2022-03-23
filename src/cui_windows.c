#include <windowsx.h>

CuiFile *
cui_file_open(CuiArena *temporary_memory, CuiString filename, uint32_t mode)
{
    CuiFile *result = 0;

    DWORD flags = FILE_ATTRIBUTE_NORMAL;
    DWORD share_mode = 0;
    DWORD access_mode = 0;

    if (mode & CUI_FILE_MODE_READ)
    {
        share_mode = FILE_SHARE_READ;
        access_mode = GENERIC_READ;
    }

    if (mode & CUI_FILE_MODE_WRITE)
    {
        flags = FILE_FLAG_WRITE_THROUGH;
        access_mode |= GENERIC_WRITE;
    }

    CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(temporary_memory);

    CuiString utf16_string = cui_utf8_to_utf16le(temporary_memory, filename);
    HANDLE file = CreateFile((LPCWSTR) cui_to_c_string(temporary_memory, utf16_string),
                             access_mode, share_mode, 0, OPEN_EXISTING, flags, 0);

    if (file != INVALID_HANDLE_VALUE)
    {
        result = (CuiFile *) file;
    }

    cui_end_temporary_memory(temp_memory);

    return result;
}

CuiFileAttributes
cui_file_get_attributes(CuiFile *file)
{
    CuiAssert(file);

    CuiFileAttributes result = { 0 };
    BY_HANDLE_FILE_INFORMATION file_info = { 0 };

    if (GetFileInformationByHandle((HANDLE) file, &file_info))
    {
        if (file_info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            result.flags = CUI_FILE_ATTRIBUTE_IS_DIRECTORY;
        }

        result.size = ((uint64_t) file_info.nFileSizeHigh << 32) | (uint64_t) file_info.nFileSizeLow;
    }

    return result;
}

CuiFileAttributes
cui_get_file_attributes(CuiArena *temporary_memory, CuiString filename)
{
    CuiFileAttributes result = { 0 };

    CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(temporary_memory);

    CuiString utf16_string = cui_utf8_to_utf16le(temporary_memory, filename);
    HANDLE file = CreateFile((LPCWSTR) cui_to_c_string(temporary_memory, utf16_string),
                             GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING,
                             FILE_ATTRIBUTE_NORMAL, 0);

    if (file != INVALID_HANDLE_VALUE)
    {
        result = cui_file_get_attributes((CuiFile *) file);
    }

    cui_end_temporary_memory(temp_memory);

    return result;
}

void
cui_file_read(CuiFile *file, void *buffer, uint64_t offset, uint64_t size)
{
    CuiAssert(file);

    DWORD bytes_read = 0;
    LARGE_INTEGER seek_offset;
    seek_offset.QuadPart = offset;
    SetFilePointerEx((HANDLE) file, seek_offset, 0, FILE_BEGIN);
    ReadFile((HANDLE) file, buffer, size, &bytes_read, 0);
}

void
cui_file_close(CuiFile *file)
{
    CuiAssert(file);
    CloseHandle((HANDLE) file);
}

void
cui_get_files(CuiArena *temporary_memory, CuiString directory, CuiFileInfo **file_list, CuiArena *arena)
{
    CuiString search_path = cui_path_concat(temporary_memory, directory, CuiStringLiteral("*"));

    WIN32_FIND_DATA find_data = { 0 };
    CuiString utf16_string = cui_utf8_to_utf16le(temporary_memory, search_path);
    HANDLE search = FindFirstFile((LPCWSTR) cui_to_c_string(temporary_memory, utf16_string), &find_data);

    if (search != INVALID_HANDLE_VALUE)
    {
        for (;;)
        {
            CuiFileInfo *file_info = cui_array_append(*file_list);

            CuiString utf16_string = cui_make_string(find_data.cFileName, 2 * wcslen(find_data.cFileName));
            file_info->name = cui_utf16le_to_utf8(arena, utf16_string);

            if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                file_info->attr.flags = CUI_FILE_ATTRIBUTE_IS_DIRECTORY;
            }
            else
            {
                file_info->attr.flags = 0;
            }

            file_info->attr.size = ((uint64_t) find_data.nFileSizeHigh << 32) | (uint64_t) find_data.nFileSizeLow;

            if (!FindNextFile(search, &find_data))
            {
                FindClose(search);
                break;
            }
        }
    }
}

void
cui_get_font_directories(CuiArena *temporary_memory, CuiString **font_dirs, CuiArena *arena)
{
    *cui_array_append(*font_dirs) = CuiStringLiteral("C:\\Windows\\Fonts");
}

static void
_cui_window_resize_backbuffer(CuiWindow *window, int32_t width, int32_t height)
{
    window->backbuffer.width  = width;
    window->backbuffer.height = height;
    window->backbuffer.stride = CuiAlign(window->backbuffer.width * 4, 64);

    int64_t needed_size = window->backbuffer.stride * window->backbuffer.height;

    if (needed_size > window->backbuffer_memory_size)
    {
        if (window->backbuffer.pixels)
        {
            cui_deallocate_platform_memory(window->backbuffer.pixels, window->backbuffer_memory_size);
        }

        window->backbuffer_memory_size = CuiAlign(needed_size, CuiMiB(4));
        window->backbuffer.pixels = cui_allocate_platform_memory(window->backbuffer_memory_size);
    }
}

LRESULT CALLBACK
_cui_window_callback(HWND window_handle, UINT message, WPARAM w_param, LPARAM l_param)
{
    CuiWindow *window = 0;

    if (message == WM_NCCREATE)
    {
        LPCREATESTRUCT create_struct = (LPCREATESTRUCT) l_param;
        window = (CuiWindow *) create_struct->lpCreateParams;
        SetWindowLongPtr(window_handle, GWLP_USERDATA, (LONG_PTR) window);
    }
    else
    {
        window = (CuiWindow *) GetWindowLongPtr(window_handle, GWLP_USERDATA);
    }

    LRESULT result = FALSE;

    switch (message)
    {
        case WM_DPICHANGED:
        {
            float ui_scale = (float) HIWORD(w_param) / 96.0f;
            window->base.ui_scale = ui_scale;

            cui_font_update_with_size_and_line_height(window->base.font, roundf(window->base.ui_scale * 14.0f), 1.0f);
            cui_glyph_cache_reset(&window->base.glyph_cache);

            cui_widget_set_ui_scale(&window->base.root_widget, ui_scale);

            RECT *new_window_rect = (RECT *) l_param;

            SetWindowPos(window->window_handle, 0, new_window_rect->left, new_window_rect->top,
                         new_window_rect->right - new_window_rect->left, new_window_rect->bottom - new_window_rect->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);

            result = TRUE;
        } break;

        case WM_SIZE:
        {
            RECT client_rect;
            GetClientRect(window_handle, &client_rect);

            int32_t width = client_rect.right - client_rect.left;
            int32_t height = client_rect.bottom - client_rect.top;

            if ((window->backbuffer.width != width) || (window->backbuffer.height != height))
            {
                _cui_window_resize_backbuffer(window, width, height);

                CuiRect rect = cui_make_rect(0, 0, window->backbuffer.width, window->backbuffer.height);

                cui_widget_layout(&window->base.root_widget, rect);
                cui_window_request_redraw(window, rect);
            }
        } break;

        case WM_PAINT:
        {
            if (cui_window_needs_redraw(window))
            {
                cui_window_redraw(window, window->redraw_rect);
            }

            PAINTSTRUCT paint;
            HDC device_context = BeginPaint(window_handle, &paint);

            BITMAPINFO bitmap;
            bitmap.bmiHeader.biSize        = sizeof(bitmap.bmiHeader);
            bitmap.bmiHeader.biWidth       = window->backbuffer.stride / sizeof(uint32_t);
            bitmap.bmiHeader.biHeight      = -(LONG) window->backbuffer.height;
            bitmap.bmiHeader.biPlanes      = 1;
            bitmap.bmiHeader.biBitCount    = 32;
            bitmap.bmiHeader.biCompression = BI_RGB;

            StretchDIBits(device_context, 0, 0, window->backbuffer.width, window->backbuffer.height,
                          0, 0, window->backbuffer.width, window->backbuffer.height, window->backbuffer.pixels,
                          &bitmap, DIB_RGB_COLORS, SRCCOPY);

            EndPaint(window_handle, &paint);
        } break;

        case WM_MOUSEMOVE:
        {
            if (!window->is_tracking)
            {
                TRACKMOUSEEVENT tracking = { sizeof(TRACKMOUSEEVENT) };
                tracking.dwFlags = TME_LEAVE;
                tracking.hwndTrack = window->window_handle;

                TrackMouseEvent(&tracking);
                window->is_tracking = true;
            }

            window->base.event.mouse.x = GET_X_LPARAM(l_param);
            window->base.event.mouse.y = GET_Y_LPARAM(l_param);

            cui_window_handle_event(window, CUI_EVENT_TYPE_MOVE);
        } break;

        case WM_MOUSELEAVE:
        {
            window->is_tracking = false;
            cui_window_handle_event(window, CUI_EVENT_TYPE_LEAVE);
        } break;

        case WM_LBUTTONDOWN:
        {
            window->base.event.mouse.x = GET_X_LPARAM(l_param);
            window->base.event.mouse.y = GET_Y_LPARAM(l_param);

            cui_window_handle_event(window, CUI_EVENT_TYPE_PRESS);
        } break;

        case WM_LBUTTONUP:
        {
            window->base.event.mouse.x = GET_X_LPARAM(l_param);
            window->base.event.mouse.y = GET_Y_LPARAM(l_param);

            cui_window_handle_event(window, CUI_EVENT_TYPE_RELEASE);
        } break;

        case WM_MOUSEWHEEL:
        {
            window->base.event.wheel.dx = GET_WHEEL_DELTA_WPARAM(w_param) / WHEEL_DELTA;

            cui_window_handle_event(window, CUI_EVENT_TYPE_WHEEL);
        } break;

        case WM_DESTROY:
        {
            cui_window_destroy(window);
        } break;

        default:
        {
            result = DefWindowProc(window_handle, message, w_param, l_param);
        } break;
    }

    return result;
}

void *
cui_allocate_platform_memory(uint64_t size)
{
    return VirtualAlloc(0, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

void
cui_deallocate_platform_memory(void *ptr, uint64_t size)
{
    (void) size;
    VirtualFree(ptr, 0, MEM_RELEASE);
}

static DWORD
_cui_thread_proc(void *data)
{
    CuiWorkQueue *work_queue = &_cui_context.common.work_queue;

    for (;;)
    {
        if (_cui_work_queue_do_next_entry(work_queue))
        {
            WaitForSingleObject(work_queue->semaphore, INFINITE);
        }
    }

    return 0;
}

typedef BOOL WINAPI set_process_dpi_aware(void);
typedef BOOL WINAPI set_process_dpi_awareness_context(DPI_AWARENESS_CONTEXT);

bool
cui_init(int arg_count, char **args)
{
    if (!_cui_common_init(arg_count, args))
    {
        return false;
    }

    SYSTEM_INFO system_info;
    GetSystemInfo(&system_info);

    int32_t core_count = system_info.dwNumberOfProcessors;

    int32_t thread_count = cui_max_int32(1, cui_min_int32(core_count - 1, 31));

    _cui_context.common.work_queue.semaphore = CreateSemaphore(0, 0, CuiArrayCount(_cui_context.common.work_queue.entries), 0);

    for (int32_t i = 0; i < thread_count; i++)
    {
        CreateThread(0, 0, _cui_thread_proc, 0, 0, 0);
    }

    HMODULE user_32 = LoadLibrary(L"User32.dll");

    CuiAssert(user_32);

    BOOL (*SetProcessDpiAwarenessContext)(DPI_AWARENESS_CONTEXT) =
        (BOOL (*)(DPI_AWARENESS_CONTEXT)) GetProcAddress(user_32, "SetProcessDpiAwarenessContext");

    if (SetProcessDpiAwarenessContext)
    {
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    }
    else
    {
        BOOL (*SetProcessDPIAware)() = (BOOL (*)()) GetProcAddress(user_32, "SetProcessDPIAware");

        if (SetProcessDPIAware)
        {
            SetProcessDPIAware();
        }
    }

    _cui_context.GetDpiForSystem = (UINT (*)()) GetProcAddress(user_32, "GetDpiForSystem");
    _cui_context.GetDpiForWindow = (UINT (*)(HWND)) GetProcAddress(user_32, "GetDpiForWindow");

    _cui_context.cursor_arrow           = LoadCursor(0, IDC_ARROW);
    _cui_context.cursor_text            = LoadCursor(0, IDC_IBEAM);
    _cui_context.cursor_hand            = LoadCursor(0, IDC_HAND);
    _cui_context.cursor_move_all        = LoadCursor(0, IDC_SIZEALL);
    _cui_context.cursor_move_left_right = LoadCursor(0, IDC_SIZEWE);
    _cui_context.cursor_move_top_down   = LoadCursor(0, IDC_SIZENS);

    _cui_context.window_class_name = L"my_very_special_window_class";

    WNDCLASS window_class = { sizeof(WNDCLASS) };
    window_class.style = CS_VREDRAW | CS_HREDRAW;
    window_class.lpfnWndProc = _cui_window_callback;
    window_class.hInstance = GetModuleHandle(0);
    window_class.lpszClassName = _cui_context.window_class_name;
    window_class.hCursor = _cui_context.cursor_arrow;

    if (!RegisterClass(&window_class))
    {
        return false;
    }

    return true;
}

CuiWindow *
cui_window_create()
{
    CuiWindow *window = _cui_add_window();

    window->base.ui_scale = 1.0f;

    if (_cui_context.GetDpiForSystem)
    {
        window->base.ui_scale = (float) _cui_context.GetDpiForSystem() / 96.0f;
    }
    else
    {
        HDC device_context = GetDC(0);
        window->base.ui_scale = (float) GetDeviceCaps(device_context, LOGPIXELSX) / 96.0f;
        ReleaseDC(0, device_context);
    }

    int32_t width = lroundf(window->base.ui_scale * CUI_DEFAULT_WINDOW_WIDTH);
    int32_t height = lroundf(window->base.ui_scale * CUI_DEFAULT_WINDOW_HEIGHT);

    window->backbuffer_memory_size = 0;
    window->backbuffer.pixels = 0;

    _cui_window_resize_backbuffer(window, width, height);

    window->window_handle = CreateWindowEx(0, _cui_context.window_class_name, L"", WS_OVERLAPPEDWINDOW,
                                           CW_USEDEFAULT, CW_USEDEFAULT, width, height, 0, 0, GetModuleHandle(0), window);

    if (!window->window_handle)
    {
        cui_window_destroy(window);
        return 0;
    }

    if (_cui_context.GetDpiForWindow)
    {
        window->base.ui_scale = (float) _cui_context.GetDpiForWindow(window->window_handle) / 96.0f;
    }

    cui_font_update_with_size_and_line_height(window->base.font, roundf(window->base.ui_scale * 14.0f), 1.0f);

    return window;
}

void
cui_window_set_title(CuiWindow *window, const char *title)
{
    CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(&window->base.temporary_memory);

    CuiString utf16_str = cui_utf8_to_utf16le(&window->base.temporary_memory, CuiCString(title));

    SetWindowText(window->window_handle, (LPCWSTR) cui_to_c_string(&window->base.temporary_memory, utf16_str));

    cui_end_temporary_memory(temp_memory);
}

void
cui_window_resize(CuiWindow *window, int32_t width, int32_t height)
{
    RECT window_rect, client_rect;
    GetWindowRect(window->window_handle, &window_rect);
    GetClientRect(window->window_handle, &client_rect);

    int32_t offset_x = (window_rect.right - window_rect.left) -
                       (client_rect.right - client_rect.left);
    int32_t offset_y = (window_rect.bottom - window_rect.top) -
                       (client_rect.bottom - client_rect.top);

    SetWindowPos(window->window_handle, 0, 0, 0, width + offset_x, height + offset_y,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOOWNERZORDER);
}

void
cui_window_show(CuiWindow *window)
{
    CuiRect rect = cui_make_rect(0, 0, window->backbuffer.width, window->backbuffer.height);

    cui_widget_set_window(&window->base.root_widget, window);
    cui_widget_set_ui_scale(&window->base.root_widget, window->base.ui_scale);
    cui_widget_layout(&window->base.root_widget, rect);
    cui_window_redraw_all(window);

    ShowWindow(window->window_handle, SW_SHOW);
}

void
cui_window_close(CuiWindow *window)
{
    DestroyWindow(window->window_handle);
}

void
cui_window_destroy(CuiWindow *window)
{
    if (window->backbuffer.pixels)
    {
        cui_deallocate_platform_memory(window->backbuffer.pixels, window->backbuffer_memory_size);
    }

    _cui_remove_window(window);
}

bool
cui_platform_dialog_file_open(CuiWindow *window, CuiString **filenames, CuiString title, CuiString *filters, bool can_select_multiple)
{
    bool result = false;

    CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(&window->base.temporary_memory);

    int32_t max_filename_length = 32 * 1024;
    uint16_t *filename_buffer = cui_alloc_array(&window->base.temporary_memory, uint16_t, max_filename_length, cui_make_allocation_params(true, 8));

    filename_buffer[0] = 0;

    uint16_t *filter_string = 0;

    if ((cui_array_count(filters) > 0) && ((cui_array_count(filters) % 2) == 0))
    {
        CuiString *utf16_filters = 0;
        cui_array_init(utf16_filters, cui_array_count(filters), &window->base.temporary_memory);

        int64_t total_count = 0;

        for (int32_t i = 0; i < cui_array_count(filters); i += 1)
        {
            CuiString *utf16_string = cui_array_append(utf16_filters);
            *utf16_string = cui_utf8_to_utf16le(&window->base.temporary_memory, filters[i]);

            CuiAssert(!(utf16_string->count % 2));

            total_count += (utf16_string->count / 2) + 1;
        }

        total_count += 1;

        filter_string = cui_alloc_array(&window->base.temporary_memory, uint16_t, total_count, CuiDefaultAllocationParams());

        uint8_t *at = (uint8_t *) filter_string;

        for (int32_t i = 0; i < cui_array_count(utf16_filters); i += 1)
        {
            CuiString utf16_string = utf16_filters[i];

            for (int64_t j = 0; j < utf16_string.count; j += 1)
            {
                *at++ = utf16_string.data[j];
            }

            *at++ = 0;
            *at++ = 0;
        }

        *at++ = 0;
        *at++ = 0;
    }

    OPENFILENAME open_file = { sizeof(OPENFILENAME) };
    open_file.hwndOwner         = window->window_handle;
    open_file.lpstrFilter       = filter_string;
    open_file.lpstrFile         = (LPWSTR) filename_buffer;
    open_file.nMaxFile          = max_filename_length;
    open_file.lpstrTitle        = (LPWSTR) cui_to_c_string(&window->base.temporary_memory, cui_utf8_to_utf16le(&window->base.temporary_memory, title));
    open_file.Flags             = OFN_FILEMUSTEXIST | OFN_EXPLORER;

    if (can_select_multiple)
    {
        open_file.Flags |= OFN_ALLOWMULTISELECT;
    }

    if (GetOpenFileName(&open_file))
    {
        int64_t filename_length = cui_wide_string_length(filename_buffer);

        CuiString utf16_string = cui_make_string(filename_buffer, 2 * filename_length);
        CuiString filename = cui_utf16le_to_utf8(&window->base.temporary_memory, utf16_string);

        filename_buffer += (filename_length + 1);

        if (*filename_buffer)
        {
            CuiString path = filename;

            while (*filename_buffer)
            {
                filename_length = cui_wide_string_length(filename_buffer);

                utf16_string = cui_make_string(filename_buffer, 2 * filename_length);
                filename = cui_utf16le_to_utf8(&window->base.temporary_memory, utf16_string);

                filename_buffer += (filename_length + 1);

                *cui_array_append(*filenames) = cui_path_concat(_cui_array_header(*filenames)->arena, path, filename);
            }
        }
        else
        {
            *cui_array_append(*filenames) = cui_copy_string(_cui_array_header(*filenames)->arena, filename);
        }

        result = true;
    }

    cui_end_temporary_memory(temp_memory);

    return result;
}

void
cui_step()
{
    MSG message;

    while (PeekMessage(&message, 0, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&message);
        DispatchMessage(&message);
    }
}

void
cui_window_request_redraw(CuiWindow *window, CuiRect rect)
{
    if (window->needs_redraw)
    {
        // TODO: very simple redraw
        window->redraw_rect = cui_rect_get_union(window->redraw_rect, rect);
    }
    else
    {
        window->needs_redraw = true;
        window->redraw_rect = rect;
    }

    // NOTE: This invalidates the whole client rect to compensate for a weird dwm behavior
    // in windows where the clip rect is not correct. This leads to visual glitches and things not rendering.
    //
    // https://social.msdn.microsoft.com/Forums/en-US/7de13a8d-0bcb-4e4b-a975-e2935c226e24/dwm-invalidaterect
    // https://stackoverflow.com/questions/17277622/dwm-in-win7-8-gdi

    InvalidateRect(window->window_handle, 0, FALSE);
}

bool
cui_window_needs_redraw(CuiWindow *window)
{
    return window->needs_redraw;
}

void
cui_window_redraw(CuiWindow *window, CuiRect rect)
{
    CuiCommandBuffer command_buffer;

    command_buffer.push_buffer_size = 0;
    command_buffer.max_push_buffer_size = window->base.max_push_buffer_size;
    command_buffer.push_buffer = window->base.push_buffer;

    command_buffer.index_buffer_count = 0;
    command_buffer.max_index_buffer_count = window->base.max_index_buffer_count;
    command_buffer.index_buffer = window->base.index_buffer;

    CuiRect window_rect = cui_make_rect(0, 0, window->backbuffer.width, window->backbuffer.height);
    CuiRect redraw_rect = cui_rect_get_intersection(window_rect, rect);

    CuiGraphicsContext ctx;
    ctx.redraw_rect = redraw_rect;
    ctx.command_buffer = &command_buffer;
    ctx.cache = &window->base.glyph_cache;

    const CuiColorTheme *color_theme = &cui_color_theme_default_dark;

    if (window->base.color_theme)
    {
        color_theme = window->base.color_theme;
    }

    cui_draw_set_clip_rect(&ctx, redraw_rect);

    CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(&window->base.temporary_memory);
    cui_widget_draw(&window->base.root_widget, &ctx, color_theme, &window->base.temporary_memory);
    cui_end_temporary_memory(temp_memory);

    cui_render(&window->backbuffer, &command_buffer, redraw_rect, &window->base.glyph_cache.texture);

    window->needs_redraw = false;
}

void
cui_window_redraw_all(CuiWindow *window)
{
    CuiRect rect = cui_make_rect(0, 0, window->backbuffer.width, window->backbuffer.height);
    cui_window_redraw(window, rect);
}
