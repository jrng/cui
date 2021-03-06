#include <dlfcn.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <sys/sysinfo.h>
#include <X11/Xutil.h>

#define _cui_load_gtk3_func(name)                           \
    _cui_context.name =                                     \
        (PFN_##name) dlsym(_cui_context.gtk3_handle, #name);\
    if (!_cui_context.name)                                 \
    {                                                       \
        dlclose(_cui_context.gtk3_handle);                  \
        _cui_context.gtk3_handle = 0;                       \
        return;                                             \
    }

static void
_cui_load_gtk3()
{
    CuiAssert(!_cui_context.gtk3_initialized);

    _cui_context.gtk3_handle = dlopen("libgtk-3.so.0", RTLD_NOW);

    if (_cui_context.gtk3_handle)
    {
        _cui_load_gtk3_func(g_free);
        _cui_load_gtk3_func(g_slist_free);
        _cui_load_gtk3_func(g_main_context_iteration);
        _cui_load_gtk3_func(g_signal_connect_data);

        _cui_load_gtk3_func(gdk_display_get_default);
        _cui_load_gtk3_func(gdk_x11_window_foreign_new_for_display);

        _cui_load_gtk3_func(gtk_window_get_type);
        _cui_load_gtk3_func(gtk_widget_new);
        _cui_load_gtk3_func(gtk_init_check);
        _cui_load_gtk3_func(gtk_widget_destroy);
        _cui_load_gtk3_func(gtk_widget_realize);
        _cui_load_gtk3_func(gtk_widget_set_window);
        _cui_load_gtk3_func(gtk_widget_set_has_window);
        _cui_load_gtk3_func(gtk_dialog_run);
        _cui_load_gtk3_func(gtk_file_chooser_get_filename);
        _cui_load_gtk3_func(gtk_file_chooser_get_filenames);
        _cui_load_gtk3_func(gtk_file_chooser_dialog_new);
        _cui_load_gtk3_func(gtk_file_chooser_set_select_multiple);
        _cui_load_gtk3_func(gtk_file_chooser_set_do_overwrite_confirmation);
        _cui_load_gtk3_func(gtk_file_chooser_set_current_name);

        _cui_context.gtk_init_check(0, 0);

        _cui_context.gdk_display = _cui_context.gdk_display_get_default();
    }

    _cui_context.gtk3_initialized = true;
}

#undef _cui_load_gtk3_func

static void
_cui_on_gtk_window_realize(GtkWidget *widget, void *user_data)
{
    _cui_context.gtk_widget_set_window(widget, (GdkWindow *) user_data);
}

static const int32_t _CUI_DEFAULT_DOUBLE_CLICK_TIME = 400; // ms

static CuiX11DesktopSettings
_cui_parse_desktop_settings(CuiString settings)
{
    CuiX11DesktopSettings result;

    result.ui_scale = 1.0f;
    result.double_click_time = _CUI_DEFAULT_DOUBLE_CLICK_TIME;

    bool is_little_endian = !settings.data[0];

    cui_string_advance(&settings, 12);

    float xft_dpi_scaling;
    float gdk_window_scaling_factor;

    bool has_xft_dpi_scaling = false;
    bool has_gdk_window_scaling_factor = false;

    while (settings.count)
    {
        uint8_t type = settings.data[0];
        uint16_t name_length;

        if (is_little_endian)
        {
            name_length = (settings.data[3] << 8) | settings.data[2];
        }
        else
        {
            name_length = (settings.data[2] << 8) | settings.data[3];
        }

        CuiString name = cui_make_string(settings.data + 4, name_length);

        cui_string_advance(&settings, 8 + CuiAlign(name_length, 4));

        switch (type)
        {
            case 0:
            {
                int32_t value;

                if (is_little_endian)
                {
                    value = (settings.data[3] << 24) | (settings.data[2] << 16) | (settings.data[1] << 8) | settings.data[0];
                }
                else
                {
                    value = (settings.data[0] << 24) | (settings.data[1] << 16) | (settings.data[2] << 8) | settings.data[3];
                }

                cui_string_advance(&settings, 4);

                if (cui_string_equals(name, CuiStringLiteral("Gdk/WindowScalingFactor")))
                {
                    has_gdk_window_scaling_factor = true;
                    gdk_window_scaling_factor = (float) value;
                }
                else if (cui_string_equals(name, CuiStringLiteral("Xft/DPI")))
                {
                    has_xft_dpi_scaling = true;
                    xft_dpi_scaling = (float) (value / 1024) / 96.0f;
                }
                else if (cui_string_equals(name, CuiStringLiteral("Net/DoubleClickTime")))
                {
                    if (value > 0)
                    {
                        result.double_click_time = value;
                    }
                }
            } break;

            case 1:
            {
                uint32_t value_length;

                if (is_little_endian)
                {
                    value_length = (settings.data[1] << 8) | settings.data[0];
                }
                else
                {
                    value_length = (settings.data[0] << 8) | settings.data[1];
                }

                cui_string_advance(&settings, 4 + CuiAlign(value_length, 4));
            } break;

            case 2:
            {
                cui_string_advance(&settings, 8);
            } break;
        }
    }

    if (has_gdk_window_scaling_factor)
    {
        result.ui_scale = gdk_window_scaling_factor;
    }
    else if (has_xft_dpi_scaling)
    {
        result.ui_scale = xft_dpi_scaling;
    }

    return result;
}

static CuiWindow *
_cui_window_get_from_x11_window(Window x11_window)
{
    CuiWindow *window = 0;

    for (uint32_t i = 0; i < _cui_context.common.window_count; i += 1)
    {
        if (_cui_context.common.windows[i]->x11_window == x11_window)
        {
            window = _cui_context.common.windows[i];
            break;
        }
    }

    return window;
}

static Bool
_cui_is_shm_completion_event(Display *x11_display, XEvent *ev, XPointer arg)
{
    return (ev->type == (intptr_t) arg) ? True : False;
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
        int64_t old_backbuffer_memory_size = window->backbuffer_memory_size;
        window->backbuffer_memory_size = CuiAlign(needed_size, CuiMiB(4));

        if (_cui_context.has_shared_memory_extension)
        {
            if (!window->backbuffer_is_ready)
            {
                XEvent ev;
                XIfEvent(_cui_context.x11_display, &ev, _cui_is_shm_completion_event, (XPointer) (intptr_t) _cui_context.frame_completion_event);
                window->backbuffer_is_ready = true;
            }

            if (window->backbuffer.pixels)
            {
                XShmDetach(_cui_context.x11_display, &window->shared_memory_info);
                shmdt(window->shared_memory_info.shmaddr);
                shmctl(window->shared_memory_info.shmid, IPC_RMID, 0);
            }

            window->shared_memory_info.shmid = shmget(IPC_PRIVATE, window->backbuffer_memory_size, IPC_CREAT | 0777);
            window->shared_memory_info.shmaddr = (char *) shmat(window->shared_memory_info.shmid, 0, 0);
            window->shared_memory_info.readOnly = True;

            window->backbuffer.pixels = window->shared_memory_info.shmaddr;

            XShmAttach(_cui_context.x11_display, &window->shared_memory_info);
        }
        else
        {
            if (window->backbuffer.pixels)
            {
                cui_deallocate_platform_memory(window->backbuffer.pixels, old_backbuffer_memory_size);
            }

            window->backbuffer.pixels = cui_allocate_platform_memory(window->backbuffer_memory_size);
        }
    }
}

void *
cui_allocate_platform_memory(uint64_t size)
{
    void *result = mmap(0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return (result == MAP_FAILED) ? 0 : result;
}

void
cui_deallocate_platform_memory(void *ptr, uint64_t size)
{
    munmap(ptr, size);
}

static inline CuiString
_cui_get_data_directory(CuiArena *arena)
{
    CuiString path = {};
    CuiString append = {};

    char *data_dir = getenv("XDG_DATA_HOME");

    if (data_dir)
    {
        path = cui_copy_string(arena, CuiCString(data_dir));
        append = CuiStringLiteral("/");
    }
    else
    {
        data_dir = getenv("HOME");

        if (data_dir)
        {
            path = cui_copy_string(arena, CuiCString(data_dir));
            append = CuiStringLiteral("/.local/share/");
        }
        else
        {
            path = CuiStringLiteral("./");
        }
    }

    return cui_path_concat(arena, path, append);
}

void
cui_get_font_directories(CuiArena *temporary_memory, CuiString **font_dirs, CuiArena *arena)
{
    CuiString data_dir = _cui_get_data_directory(temporary_memory);

    *cui_array_append(*font_dirs) = cui_path_concat(arena, data_dir, CuiStringLiteral("fonts"));
    *cui_array_append(*font_dirs) = CuiStringLiteral("/usr/share/fonts");
}

static void *
_cui_thread_proc(void *data)
{
    CuiWorkQueue *work_queue = &_cui_context.common.work_queue;

    for (;;)
    {
        if (_cui_work_queue_do_next_entry(work_queue))
        {
            pthread_mutex_lock(&work_queue->semaphore_mutex);
            pthread_cond_wait(&work_queue->semaphore_cond, &work_queue->semaphore_mutex);
            pthread_mutex_unlock(&work_queue->semaphore_mutex);
        }
    }

    return 0;
}

bool
cui_init(int arg_count, char **args)
{
    if (!_cui_common_init(arg_count, args))
    {
        return false;
    }

    int32_t core_count = get_nprocs();

    pthread_t threads[15];

    int32_t thread_count = cui_max_int32(1, cui_min_int32(core_count - 1, CuiArrayCount(threads)));

    pthread_cond_init(&_cui_context.common.work_queue.semaphore_cond, 0);
    pthread_mutex_init(&_cui_context.common.work_queue.semaphore_mutex, 0);

    for (int32_t thread_index = 0; thread_index < thread_count; thread_index += 1)
    {
        pthread_create(threads + thread_index, 0, _cui_thread_proc, 0);
    }

    if (!XSupportsLocale() || !XSetLocaleModifiers("@im=none"))
    {
        return false;
    }

    _cui_context.x11_display = XOpenDisplay(0);

    if (!_cui_context.x11_display)
    {
        return false;
    }

    int major, minor;
    int event_base, error_base;

    _cui_context.has_shared_memory_extension = (XShmQueryExtension(_cui_context.x11_display) == True);
    _cui_context.has_xsync_extension = (XSyncQueryExtension(_cui_context.x11_display, &event_base, &error_base) &&
                                        XSyncInitialize(_cui_context.x11_display, &major, &minor));

    if (_cui_context.has_shared_memory_extension)
    {
        _cui_context.frame_completion_event = XShmGetEventBase(_cui_context.x11_display) + ShmCompletion;
    }

    _cui_context.x11_input_method = XOpenIM(_cui_context.x11_display, 0, 0, 0);

    if (!_cui_context.x11_input_method)
    {
        return false;
    }

    _cui_context.default_ui_scale = 1.0f;
    _cui_context.double_click_time = _CUI_DEFAULT_DOUBLE_CLICK_TIME;

    _cui_context.x11_default_screen = DefaultScreen(_cui_context.x11_display);
    _cui_context.x11_root_window = DefaultRootWindow(_cui_context.x11_display);
    _cui_context.x11_default_gc = DefaultGC(_cui_context.x11_display, _cui_context.x11_default_screen);

    char xsettings_screen_name[] = "_XSETTINGS_S\0\0";

    if (_cui_context.x11_default_screen < 10)
    {
        xsettings_screen_name[12] = '0' + _cui_context.x11_default_screen;
    }
    else
    {
        xsettings_screen_name[13] = '0' + (_cui_context.x11_default_screen % 10);
        xsettings_screen_name[12] = '0' + (_cui_context.x11_default_screen / 10);
    }

    _cui_context.atom_manager = XInternAtom(_cui_context.x11_display, "MANAGER", false);
    _cui_context.atom_cardinal = XInternAtom(_cui_context.x11_display, "CARDINAL", false);
    _cui_context.atom_utf8_string = XInternAtom(_cui_context.x11_display, "UTF8_STRING", false);
    _cui_context.atom_wm_protocols = XInternAtom(_cui_context.x11_display, "WM_PROTOCOLS", false);
    _cui_context.atom_wm_delete_window = XInternAtom(_cui_context.x11_display, "WM_DELETE_WINDOW", false);
    _cui_context.atom_wm_sync_request = XInternAtom(_cui_context.x11_display, "_NET_WM_SYNC_REQUEST", false);
    _cui_context.atom_wm_sync_request_counter = XInternAtom(_cui_context.x11_display, "_NET_WM_SYNC_REQUEST_COUNTER", false);
    _cui_context.atom_xsettings_screen = XInternAtom(_cui_context.x11_display, xsettings_screen_name, false);
    _cui_context.atom_xsettings_settings = XInternAtom(_cui_context.x11_display, "_XSETTINGS_SETTINGS", false);

    XGrabServer(_cui_context.x11_display);

    _cui_context.x11_settings_window = XGetSelectionOwner(_cui_context.x11_display, _cui_context.atom_xsettings_screen);

    if (_cui_context.x11_settings_window != None)
    {
        XSelectInput(_cui_context.x11_display, _cui_context.x11_settings_window, StructureNotifyMask | PropertyChangeMask);

        Atom type;
        int format;
        unsigned long length, remaining;
        unsigned char *settings_buffer = 0;

        int ret = XGetWindowProperty(_cui_context.x11_display, _cui_context.x11_settings_window, _cui_context.atom_xsettings_settings,
                                     0, 2048, false, AnyPropertyType, &type, &format, &length, &remaining, &settings_buffer);

        if ((ret == Success) && (length > 0))
        {
            if ((remaining == 0) && (format == 8))
            {
                CuiX11DesktopSettings desktop_settings = _cui_parse_desktop_settings(cui_make_string(settings_buffer, length));

                _cui_context.default_ui_scale = desktop_settings.ui_scale;
                _cui_context.double_click_time = desktop_settings.double_click_time;
            }

            XFree(settings_buffer);
        }
    }

    XUngrabServer(_cui_context.x11_display);

    return true;
}

CuiWindow *
cui_window_create()
{
    CuiAssert(_cui_context.x11_display);

    CuiWindow *window = _cui_add_window();

    window->base.ui_scale = _cui_context.default_ui_scale;

    cui_font_update_with_size_and_line_height(window->base.font, roundf(window->base.ui_scale * 14.0f), 1.0f);

    XSetWindowAttributes window_attr;
    window_attr.event_mask = ButtonPressMask |
                             ButtonReleaseMask |
                             KeyPressMask |
                             KeyReleaseMask |
                             PointerMotionMask |
                             EnterWindowMask |
                             LeaveWindowMask |
                             StructureNotifyMask |
                             FocusChangeMask |
                             ExposureMask;

    int32_t width = lroundf(window->base.ui_scale * CUI_DEFAULT_WINDOW_WIDTH);
    int32_t height = lroundf(window->base.ui_scale * CUI_DEFAULT_WINDOW_HEIGHT);

    window->backbuffer_memory_size = 0;
    window->backbuffer.pixels = 0;
    window->backbuffer_is_ready = true;

    _cui_window_resize_backbuffer(window, width, height);

    window->x11_window = XCreateWindow(_cui_context.x11_display, _cui_context.x11_root_window, 0, 0, width, height,
                                       0, 0, InputOutput, 0, CWEventMask, &window_attr);

    Atom protocols[] = {
        _cui_context.atom_wm_delete_window,
        _cui_context.atom_wm_sync_request,
    };

    XSetWMProtocols(_cui_context.x11_display, window->x11_window, protocols, CuiArrayCount(protocols));

    if (_cui_context.has_xsync_extension)
    {
        XSyncValue counter_value;
        XSyncIntToValue(&counter_value, 0);

        window->frame_sync = XSyncCreateCounter(_cui_context.x11_display, counter_value);

        XChangeProperty(_cui_context.x11_display, window->x11_window, _cui_context.atom_wm_sync_request_counter,
                        _cui_context.atom_cardinal, 32, PropModeReplace, (unsigned char *) &window->frame_sync, 1);
    }

    window->last_left_click_time = INT16_MIN;

    int32_t integer_scale = (int32_t) window->base.ui_scale;

    XSizeHints sh = { 0 };
    sh.flags = PMinSize | PResizeInc | PWinGravity;
    sh.win_gravity = StaticGravity;
    sh.min_width  = integer_scale * 64;
    sh.min_height = integer_scale * 32;
    sh.width_inc  = integer_scale;
    sh.height_inc = integer_scale;

    XSetWMNormalHints(_cui_context.x11_display, window->x11_window, &sh);

    window->x11_input_context = XCreateIC(_cui_context.x11_input_method, XNInputStyle,
                                          XIMPreeditNothing | XIMStatusNothing,
                                          XNClientWindow, window->x11_window, (void *) 0);

    if (!window->x11_input_context)
    {
        cui_window_destroy(window);
        return 0;
    }

    XSetICFocus(window->x11_input_context);

    return window;
}

void
cui_window_set_title(CuiWindow *window, const char *title)
{
    XTextProperty text_prop;
    text_prop.value    = (unsigned char *) title;
    text_prop.encoding = _cui_context.atom_utf8_string;
    text_prop.format   = 8;
    text_prop.nitems   = cui_string_length(title);

    XSetWMName(_cui_context.x11_display, window->x11_window, &text_prop);
}

void
cui_window_resize(CuiWindow *window, int32_t width, int32_t height)
{
    XResizeWindow(_cui_context.x11_display, window->x11_window, width, height);
}

float
cui_window_get_scale(CuiWindow *window)
{
    return window->base.ui_scale;
}

void
cui_window_show(CuiWindow *window)
{
    CuiRect rect = cui_make_rect(0, 0, window->backbuffer.width, window->backbuffer.height);

    cui_widget_set_window(&window->base.root_widget, window);
    cui_widget_set_ui_scale(&window->base.root_widget, window->base.ui_scale);
    cui_widget_layout(&window->base.root_widget, rect);
    cui_window_redraw_all(window);

    XMapWindow(_cui_context.x11_display, window->x11_window);
}

void
cui_window_close(CuiWindow *window)
{
    XDestroyWindow(_cui_context.x11_display, window->x11_window);
}

void
cui_window_destroy(CuiWindow *window)
{
    if (_cui_context.has_shared_memory_extension)
    {
        if (!window->backbuffer_is_ready)
        {
            XEvent ev;
            XIfEvent(_cui_context.x11_display, &ev, _cui_is_shm_completion_event, (XPointer) (intptr_t) _cui_context.frame_completion_event);
            window->backbuffer_is_ready = true;
        }

        if (window->backbuffer.pixels)
        {
            XShmDetach(_cui_context.x11_display, &window->shared_memory_info);
            shmdt(window->shared_memory_info.shmaddr);
            shmctl(window->shared_memory_info.shmid, IPC_RMID, 0);
        }
    }
    else
    {
        if (window->backbuffer.pixels)
        {
            cui_deallocate_platform_memory(window->backbuffer.pixels, window->backbuffer_memory_size);
        }
    }

    _cui_remove_window(window);
}

bool
cui_platform_dialog_file_open(CuiWindow *window, CuiString **filenames, CuiString title, CuiString *filters, bool can_select_multiple)
{
    bool result = false;

    if (!_cui_context.gtk3_initialized)
    {
        _cui_load_gtk3();
    }

    if (!_cui_context.gtk3_handle)
    {
        return false;
    }

    if (!window->gdk_window)
    {
        window->gdk_window = _cui_context.gdk_x11_window_foreign_new_for_display(_cui_context.gdk_display, window->x11_window);
        window->gtk_window = (GtkWindow *) _cui_context.gtk_widget_new(_cui_context.gtk_window_get_type(), (const gchar *) 0);

        _cui_context.g_signal_connect_data(window->gtk_window, "realize", (GCallback) _cui_on_gtk_window_realize,
                                           window->gdk_window, 0, (GConnectFlags) 0);
        _cui_context.gtk_widget_set_has_window((GtkWidget *) window->gtk_window, true);
        _cui_context.gtk_widget_realize((GtkWidget *) window->gtk_window);
    }

    CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(&window->base.temporary_memory);

    char *c_title = cui_to_c_string(&window->base.temporary_memory, title);

    GtkWidget *dialog = _cui_context.gtk_file_chooser_dialog_new(c_title, window->gtk_window, GTK_FILE_CHOOSER_ACTION_OPEN,
                                                                 "Cancel", GTK_RESPONSE_CANCEL,
                                                                 "Open", GTK_RESPONSE_ACCEPT, (const gchar *) 0);

#if 0
    GtkFileFilter *filter = gtk_file_filter_new();

    gtk_file_filter_set_name(filter, "jpeg images (.jpg, .jpeg)");
    gtk_file_filter_add_pattern(filter, "*.[jJ][pP][gG]");
    gtk_file_filter_add_pattern(filter, "*.[jJ][pP][eE][gG]");
    gtk_file_chooser_add_filter(chooser, filter);
#endif

    _cui_context.gtk_file_chooser_set_select_multiple((GtkFileChooser *) dialog, can_select_multiple);

    if (_cui_context.gtk_dialog_run((GtkDialog *) dialog) == GTK_RESPONSE_ACCEPT)
    {
        GSList *names = _cui_context.gtk_file_chooser_get_filenames((GtkFileChooser *) dialog);
        GSList *name = names;

        result = (names != 0);

        while (name)
        {
            const gchar *path = (const gchar *) name->data;

            if (path)
            {
                *cui_array_append(*filenames) = cui_copy_string(_cui_array_header(*filenames)->arena, CuiCString(path));
                _cui_context.g_free(name->data);
            }

            name = name->next;
        }

        _cui_context.g_slist_free(names);
    }

    _cui_context.gtk_widget_destroy(dialog);

    while (_cui_context.g_main_context_iteration(0, false));

    cui_end_temporary_memory(temp_memory);

    return result;
}

void
cui_step()
{
    do {
        XEvent ev;

        XNextEvent(_cui_context.x11_display, &ev);

        Window x11_window = ev.xany.window;

        if (XFilterEvent(&ev, x11_window)) continue;

        if (x11_window == _cui_context.x11_settings_window)
        {
            if ((ev.type == PropertyNotify) &&
                (ev.xproperty.window == _cui_context.x11_settings_window) &&
                (ev.xproperty.state == PropertyNewValue) &&
                (ev.xproperty.atom == _cui_context.atom_xsettings_settings))
            {
                Atom type;
                int format;
                unsigned long length, remaining;
                unsigned char *settings_buffer = 0;

                int ret = XGetWindowProperty(_cui_context.x11_display, _cui_context.x11_settings_window, _cui_context.atom_xsettings_settings,
                                             0, 2048, false, AnyPropertyType, &type, &format, &length, &remaining, &settings_buffer);

                if ((ret == Success) && (length > 0))
                {
                    if ((remaining == 0) && (format == 8))
                    {
                        CuiX11DesktopSettings desktop_settings = _cui_parse_desktop_settings(cui_make_string(settings_buffer, length));

                        if (_cui_context.default_ui_scale != desktop_settings.ui_scale)
                        {
                            _cui_context.default_ui_scale = desktop_settings.ui_scale;
                            _cui_context.double_click_time = desktop_settings.double_click_time;

                            for (uint32_t i = 0; i < _cui_context.common.window_count; i += 1)
                            {
                                CuiWindow *window = _cui_context.common.windows[i];

                                float inv_ui_scale = 1.0f / window->base.ui_scale;
                                window->base.ui_scale = _cui_context.default_ui_scale;

                                int32_t new_width  = lroundf((inv_ui_scale * window->backbuffer.width) * window->base.ui_scale);
                                int32_t new_height = lroundf((inv_ui_scale * window->backbuffer.height) * window->base.ui_scale);

                                cui_font_update_with_size_and_line_height(window->base.font, roundf(window->base.ui_scale * 14.0f), 1.0f);
                                cui_glyph_cache_reset(&window->base.glyph_cache);

                                cui_widget_set_ui_scale(&window->base.root_widget, window->base.ui_scale);

                                int32_t integer_scale = (int32_t) window->base.ui_scale;

                                XSizeHints sh = { 0 };
                                sh.flags = PMinSize | PResizeInc | PWinGravity;
                                sh.win_gravity = StaticGravity;
                                sh.min_width  = integer_scale * 64;
                                sh.min_height = integer_scale * 32;
                                sh.width_inc  = integer_scale;
                                sh.height_inc = integer_scale;

                                XSetWMNormalHints(_cui_context.x11_display, window->x11_window, &sh);

                                cui_window_resize(window, new_width, new_height);
                            }
                        }
                    }

                    XFree(settings_buffer);
                }
            }

            continue;
        }

        CuiWindow *window = _cui_window_get_from_x11_window(x11_window);

        CuiAssert(window);

        if (_cui_context.has_shared_memory_extension && (ev.type == _cui_context.frame_completion_event))
        {
            window->backbuffer_is_ready = true;
            continue;
        }

        switch (ev.type)
        {
            case Expose:
            {
#if 0
                CuiRect client_rect;

                client_rect.min.x = ev.xexpose.x;
                client_rect.min.y = ev.xexpose.y;
                client_rect.max.x = client_rect.min.x + ev.xexpose.width;
                client_rect.max.y = client_rect.min.y + ev.xexpose.height;

                printf("expose (%d) (%d, %d, %d, %d)\n", ev.xexpose.count,
                       client_rect.min.x, client_rect.min.y, client_rect.max.x, client_rect.max.y);
#endif

                if (cui_window_needs_redraw(window))
                {
                    cui_window_redraw(window, window->redraw_rect);
                }

                XImage backbuffer;
                backbuffer.width = window->backbuffer.width;
                backbuffer.height = window->backbuffer.height;
                backbuffer.xoffset = 0;
                backbuffer.format = ZPixmap;
                backbuffer.data = (char *) window->backbuffer.pixels;
                backbuffer.byte_order = LSBFirst;
                backbuffer.bitmap_unit = 32;
                backbuffer.bitmap_bit_order = LSBFirst;
                backbuffer.bitmap_pad = 32;
                backbuffer.depth = 24;
                backbuffer.bytes_per_line = window->backbuffer.stride;
                backbuffer.bits_per_pixel = 32;
                backbuffer.red_mask = 0xFF0000;
                backbuffer.green_mask = 0x00FF00;
                backbuffer.blue_mask = 0x0000FF;

                if (_cui_context.has_shared_memory_extension)
                {
                    if (!window->backbuffer_is_ready)
                    {
                        XEvent ev;
                        XIfEvent(_cui_context.x11_display, &ev, _cui_is_shm_completion_event, (XPointer) (intptr_t) _cui_context.frame_completion_event);
                    }

                    backbuffer.width = CuiAlign(backbuffer.width, 16);
                    backbuffer.obdata = (char *) &window->shared_memory_info;

                    XShmPutImage(_cui_context.x11_display, window->x11_window, _cui_context.x11_default_gc, &backbuffer,
                                 ev.xexpose.x, ev.xexpose.y, ev.xexpose.x, ev.xexpose.y, ev.xexpose.width, ev.xexpose.height, True);

                    window->backbuffer_is_ready = false;
                }
                else
                {
                    XPutImage(_cui_context.x11_display, window->x11_window, _cui_context.x11_default_gc,
                              &backbuffer, ev.xexpose.x, ev.xexpose.y, ev.xexpose.x, ev.xexpose.y,
                              ev.xexpose.width, ev.xexpose.height);
                    XFlush(_cui_context.x11_display);
                }

                if (window->x11_configure_serial)
                {
                    XSyncValue counter_value;

                    XSyncIntsToValue(&counter_value, window->x11_configure_serial & 0xFFFFFFFF, window->x11_configure_serial >> 32);
                    XSyncSetCounter(_cui_context.x11_display, window->frame_sync, counter_value);

                    window->x11_configure_serial = 0;
                }
            } break;

            case ClientMessage:
            {
                if (ev.xclient.message_type == _cui_context.atom_wm_protocols)
                {
                    if ((Atom) ev.xclient.data.l[0] == _cui_context.atom_wm_delete_window)
                    {
                        cui_window_close(window);
                    }
                    else if ((Atom) ev.xclient.data.l[0] == _cui_context.atom_wm_sync_request)
                    {
                        window->x11_sync_request_serial = ((uint64_t) ev.xclient.data.l[3] << 32) | (uint64_t) ev.xclient.data.l[2];
                    }
                }
                else if (ev.xclient.message_type == _cui_context.atom_manager)
                {
                    if ((Atom) ev.xclient.data.l[1] == _cui_context.atom_xsettings_screen)
                    {
                        _cui_context.x11_settings_window = (Window) ev.xclient.data.l[2];
                        XSelectInput(_cui_context.x11_display, _cui_context.x11_settings_window, StructureNotifyMask | PropertyChangeMask);
                    }
                }
            } break;

            case ConfigureNotify:
            {
                window->x11_configure_serial = window->x11_sync_request_serial;
                window->x11_sync_request_serial = 0;

                if ((window->backbuffer.width != ev.xconfigure.width) ||
                    (window->backbuffer.height != ev.xconfigure.height))
                {
                    // printf("resize (%d, %d)\n", ev.xconfigure.width, ev.xconfigure.height);
                    _cui_window_resize_backbuffer(window, ev.xconfigure.width, ev.xconfigure.height);

                    CuiRect rect = cui_make_rect(0, 0, window->backbuffer.width, window->backbuffer.height);

                    cui_widget_layout(&window->base.root_widget, rect);
                    cui_window_request_redraw(window, rect);
                }
            } break;

            case DestroyNotify:
            {
                cui_window_destroy(window);
            } break;

            case EnterNotify:
            case MotionNotify:
            {
                window->base.event.mouse.x = ev.xcrossing.x;
                window->base.event.mouse.y = ev.xcrossing.y;

                cui_window_handle_event(window, CUI_EVENT_TYPE_MOVE);
            } break;

            case LeaveNotify:
            {
                cui_window_handle_event(window, CUI_EVENT_TYPE_LEAVE);
            } break;

            case ButtonPress:
            {
                switch (ev.xbutton.button)
                {
                    case Button1:
                    {
                        if ((ev.xbutton.time - window->last_left_click_time) <= _cui_context.double_click_time)
                        {
                            window->last_left_click_time = INT16_MIN;

                            window->base.event.mouse.x = ev.xbutton.x;
                            window->base.event.mouse.y = ev.xbutton.y;

                            cui_window_handle_event(window, CUI_EVENT_TYPE_DOUBLE_CLICK);
                        }
                        else
                        {
                            window->last_left_click_time = ev.xbutton.time;

                            window->base.event.mouse.x = ev.xbutton.x;
                            window->base.event.mouse.y = ev.xbutton.y;

                            cui_window_handle_event(window, CUI_EVENT_TYPE_PRESS);
                        }
                    } break;

                    case Button2:
                    case Button3:
                    {
                        window->base.event.mouse.x = ev.xbutton.x;
                        window->base.event.mouse.y = ev.xbutton.y;

                        cui_window_handle_event(window, CUI_EVENT_TYPE_PRESS);
                    } break;

                    case Button4:
                    case Button5:
                    {
                        window->base.event.wheel.dx = (ev.xbutton.button == Button4) ? 1 : -1;

                        cui_window_handle_event(window, CUI_EVENT_TYPE_WHEEL);
                    } break;
                }
            } break;

            case ButtonRelease:
            {
                switch (ev.xbutton.button)
                {
                    case Button1:
                    case Button2:
                    case Button3:
                    {
                        window->base.event.mouse.x = ev.xbutton.x;
                        window->base.event.mouse.y = ev.xbutton.y;

                        cui_window_handle_event(window, CUI_EVENT_TYPE_RELEASE);
                    } break;
                }
            } break;

            case KeyPress:
            {
                KeySym key;
                Status status;
                char buffer[32];

                int ret = Xutf8LookupString(window->x11_input_context, &ev.xkey, buffer,
                                            sizeof(buffer), &key, &status);

                CuiAssert(status != XBufferOverflow);

#define _CUI_KEY_PRESS_EVENT(key_id)                                                \
    window->base.event.key.codepoint     = (key_id);                                \
    window->base.event.key.alt_is_down   = 1 & (ev.xkey.state >> Mod1MapIndex);     \
    window->base.event.key.ctrl_is_down  = 1 & (ev.xkey.state >> ControlMapIndex);  \
    window->base.event.key.shift_is_down = 1 & (ev.xkey.state >> ShiftMapIndex);    \
    cui_window_handle_event(window, CUI_EVENT_TYPE_KEY_PRESS);

                if ((status == XLookupKeySym) || (status == XLookupBoth))
                {
                    switch (key)
                    {
                        case XK_BackSpace: { _CUI_KEY_PRESS_EVENT(CUI_KEY_BACKSPACE); } break;
                        case XK_Tab:       { _CUI_KEY_PRESS_EVENT(CUI_KEY_TAB);       } break;
                        case XK_Linefeed:  { _CUI_KEY_PRESS_EVENT(CUI_KEY_LINEFEED);  } break;
                        case XK_Return:    { _CUI_KEY_PRESS_EVENT(CUI_KEY_ENTER);     } break;
                        case XK_Escape:    { _CUI_KEY_PRESS_EVENT(CUI_KEY_ESCAPE);    } break;
                        case XK_Delete:    { _CUI_KEY_PRESS_EVENT(CUI_KEY_DELETE);    } break;
                        case XK_Up:        { _CUI_KEY_PRESS_EVENT(CUI_KEY_UP);        } break;
                        case XK_Down:      { _CUI_KEY_PRESS_EVENT(CUI_KEY_DOWN);      } break;
                        case XK_Left:      { _CUI_KEY_PRESS_EVENT(CUI_KEY_LEFT);      } break;
                        case XK_Right:     { _CUI_KEY_PRESS_EVENT(CUI_KEY_RIGHT);     } break;

                        case XK_F1:  case XK_F2:  case XK_F3:  case XK_F4:
                        case XK_F5:  case XK_F6:  case XK_F7:  case XK_F8:
                        case XK_F9:  case XK_F10: case XK_F11: case XK_F12:
                        {
                            _CUI_KEY_PRESS_EVENT(CUI_KEY_F1 + (key - XK_F1));
                        } break;

                        default:
                        {
                            if ((key >= 32) && (key < 127))
                            {
                                _CUI_KEY_PRESS_EVENT(key);
                            }
                        } break;
                    }
                }

                if ((status == XLookupChars) || (status == XLookupBoth))
                {
                    int64_t i = 0;
                    CuiString str = cui_make_string(buffer, ret);

                    while (i < str.count)
                    {
                        CuiUnicodeResult utf8 = cui_utf8_decode(str, i);

                        if (utf8.codepoint > 127)
                        {
                            _CUI_KEY_PRESS_EVENT(utf8.codepoint);
                        }

                        i += utf8.byte_count;
                    }
                }

#undef _CUI_KEY_PRESS_EVENT

            } break;

            case KeyRelease:
            {
            } break;
        }
    }
    while (XPending(_cui_context.x11_display));

    for (uint32_t window_index = 0;
         window_index < _cui_context.common.window_count; window_index += 1)
    {
        CuiWindow *window = _cui_context.common.windows[window_index];

        if (cui_window_needs_redraw(window))
        {
            CuiRect redraw_rect = window->redraw_rect;

            cui_window_redraw(window, redraw_rect);

            XImage backbuffer;
            backbuffer.width = window->backbuffer.width;
            backbuffer.height = window->backbuffer.height;
            backbuffer.xoffset = 0;
            backbuffer.format = ZPixmap;
            backbuffer.data = (char *) window->backbuffer.pixels;
            backbuffer.byte_order = LSBFirst;
            backbuffer.bitmap_unit = 32;
            backbuffer.bitmap_bit_order = LSBFirst;
            backbuffer.bitmap_pad = 32;
            backbuffer.depth = 24;
            backbuffer.bytes_per_line = window->backbuffer.stride;
            backbuffer.bits_per_pixel = 32;
            backbuffer.red_mask = 0xFF0000;
            backbuffer.green_mask = 0x00FF00;
            backbuffer.blue_mask = 0x0000FF;

            if (_cui_context.has_shared_memory_extension)
            {
                if (!window->backbuffer_is_ready)
                {
                    XEvent ev;
                    XIfEvent(_cui_context.x11_display, &ev, _cui_is_shm_completion_event, (XPointer) (intptr_t) _cui_context.frame_completion_event);
                }

                backbuffer.width = CuiAlign(backbuffer.width, 16);
                backbuffer.obdata = (char *) &window->shared_memory_info;

                XShmPutImage(_cui_context.x11_display, window->x11_window, _cui_context.x11_default_gc, &backbuffer,
                             redraw_rect.min.x, redraw_rect.min.y, redraw_rect.min.x, redraw_rect.min.y,
                             cui_rect_get_width(redraw_rect), cui_rect_get_height(redraw_rect), True);

                window->backbuffer_is_ready = false;
            }
            else
            {
                XPutImage(_cui_context.x11_display, window->x11_window, _cui_context.x11_default_gc,
                          &backbuffer, redraw_rect.min.x, redraw_rect.min.y, redraw_rect.min.x, redraw_rect.min.y,
                          cui_rect_get_width(redraw_rect), cui_rect_get_height(redraw_rect));
                XFlush(_cui_context.x11_display);
            }

            if (window->x11_configure_serial)
            {
                XSyncValue counter_value;

                XSyncIntsToValue(&counter_value, window->x11_configure_serial & 0xFFFFFFFF, window->x11_configure_serial >> 32);
                XSyncSetCounter(_cui_context.x11_display, window->frame_sync, counter_value);

                window->x11_configure_serial = 0;
            }
        }
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
