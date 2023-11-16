#include <poll.h>
#include <errno.h>
#include <locale.h>
#include <syscall.h>
#include <sys/sysinfo.h>

#ifndef CUI_NO_BACKEND

static const int32_t _CUI_DEFAULT_DOUBLE_CLICK_TIME  = 400; // ms
static const int32_t _CUI_DROP_SHADOW_PADDING_TOP    = 12;
static const int32_t _CUI_DROP_SHADOW_PADDING_RIGHT  = 16;
static const int32_t _CUI_DROP_SHADOW_PADDING_BOTTOM = 20;
static const int32_t _CUI_DROP_SHADOW_PADDING_LEFT   = 16;
static const int32_t _CUI_WINDOW_TITLEBAR_HEIGHT     = 28;
static const int32_t _CUI_WAYLAND_INPUT_MARGIN       = 8;
static const int32_t _CUI_WAYLAND_CORNER_MARGIN      = 16;
static const int32_t _CUI_WAYLAND_MINIMUM_WIDTH      = 200;
static const int32_t _CUI_WAYLAND_MINIMUM_HEIGHT     = 100;

#if CUI_RENDERER_OPENGLES2_ENABLED

static inline void
_cui_parse_egl_client_extensions(void)
{
    _cui_context.has_EGL_EXT_platform_base    = false;
    _cui_context.has_EGL_EXT_platform_x11     = false;
    _cui_context.has_EGL_EXT_platform_wayland = false;

    char *egl_extensions = (char *) eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);

    char *start = egl_extensions;

    while (*start)
    {
        while (*start == ' ')  start += 1;
        char *end = start;
        while (*end && *end != ' ')  end += 1;

        CuiString name = cui_make_string(start, end - start);

        if (cui_string_equals(name, CuiStringLiteral("EGL_EXT_platform_base")))
        {
            _cui_context.has_EGL_EXT_platform_base = true;
        }
        else if (cui_string_equals(name, CuiStringLiteral("EGL_EXT_platform_x11")) ||
                 cui_string_equals(name, CuiStringLiteral("EGL_KHR_platform_x11")))
        {
            _cui_context.has_EGL_EXT_platform_x11 = true;
        }
        else if (cui_string_equals(name, CuiStringLiteral("EGL_EXT_platform_wayland")) ||
                 cui_string_equals(name, CuiStringLiteral("EGL_KHR_platform_wayland")))
        {
            _cui_context.has_EGL_EXT_platform_wayland = true;
        }

        start = end;
    }
}

#endif

#if CUI_BACKEND_X11_ENABLED

#include <sys/shm.h>
#include <X11/Xutil.h>
#include <X11/Xresource.h>
#include <X11/cursorfont.h>
#include <X11/extensions/Xrandr.h>

static inline int64_t
_cui_get_current_ms(void)
{
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC_RAW, &time);
    return time.tv_sec * 1000 + time.tv_nsec / 1000000;
}

static CuiX11DesktopSettings
_cui_parse_desktop_settings(CuiString settings)
{
    CuiX11DesktopSettings result;

    result.ui_scale = 1;
    result.double_click_time = _CUI_DEFAULT_DOUBLE_CLICK_TIME;

    bool is_little_endian = !settings.data[0];

    cui_string_advance(&settings, 12);

    int32_t xft_dpi_scaling;
    int32_t gdk_window_scaling_factor;

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
                    gdk_window_scaling_factor = value;
                }
                else if (cui_string_equals(name, CuiStringLiteral("Xft/DPI")))
                {
                    has_xft_dpi_scaling = true;
                    xft_dpi_scaling = ((value / 1024) + 48) / 96;
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

#  if CUI_RENDERER_SOFTWARE_ENABLED

static Bool
_cui_is_shm_completion_event(Display *x11_display, XEvent *ev, XPointer arg)
{
    (void) x11_display;

    XShmCompletionEvent *event = (XShmCompletionEvent *) ev;
    CuiX11FrameCompletionData *frame_completion = (CuiX11FrameCompletionData *) arg;

    if (event->type == frame_completion->event_type)
    {
        for (uint32_t window_index = 0;
             window_index < _cui_context.common.window_count; window_index += 1)
        {
            CuiWindow *window = _cui_context.common.windows[window_index];

            if (window->x11_window == event->drawable)
            {
                for (uint32_t framebuffer_index = 0;
                     framebuffer_index < CuiArrayCount(window->renderer.software.framebuffers);
                     framebuffer_index += 1)
                {
                    CuiFramebuffer *framebuffer = window->renderer.software.framebuffers + framebuffer_index;

                    if (framebuffer->backend.x11.shared_memory_info.shmseg == event->shmseg)
                    {
                        framebuffer->is_busy = false;
                        break;
                    }
                }

                break;
            }
        }

        return True;
    }
    else
    {
        return False;
    }
}

static inline void
_cui_wait_for_frame_completion(CuiWindow *window)
{
    CuiAssert(_cui_context.has_shared_memory_extension);
    CuiAssert(window->base.renderer_type == CUI_RENDERER_TYPE_SOFTWARE);

    CuiX11FrameCompletionData frame_completion;
    frame_completion.event_type = _cui_context.x11_frame_completion_event;
    frame_completion.window = window->x11_window;

    XEvent ev;
    XIfEvent(_cui_context.x11_display, &ev, _cui_is_shm_completion_event, (XPointer) &frame_completion);
}

static void
_cui_x11_allocate_framebuffer(CuiFramebuffer *framebuffer, int32_t width, int32_t height)
{
    framebuffer->is_busy = false;

    framebuffer->bitmap.width  = width;
    framebuffer->bitmap.height = height;
    framebuffer->bitmap.stride = CuiAlign(framebuffer->bitmap.width * 4, 64);

    framebuffer->shared_memory_size = CuiAlign((int64_t) framebuffer->bitmap.stride * (int64_t) framebuffer->bitmap.height, CuiMiB(4));

    if (_cui_context.has_shared_memory_extension)
    {
        framebuffer->backend.x11.shared_memory_info.shmid = shmget(IPC_PRIVATE, framebuffer->shared_memory_size, IPC_CREAT | 0644);
        framebuffer->backend.x11.shared_memory_info.shmaddr = (char *) shmat(framebuffer->backend.x11.shared_memory_info.shmid, 0, 0);
        framebuffer->backend.x11.shared_memory_info.readOnly = True;

        framebuffer->bitmap.pixels = framebuffer->backend.x11.shared_memory_info.shmaddr;

        XShmAttach(_cui_context.x11_display, &framebuffer->backend.x11.shared_memory_info);
    }
    else
    {
        framebuffer->bitmap.pixels = cui_platform_allocate(framebuffer->shared_memory_size);
    }
}

static void
_cui_x11_resize_framebuffer(CuiFramebuffer *framebuffer, int32_t width, int32_t height)
{
    CuiAssert(!framebuffer->is_busy);

    framebuffer->bitmap.width  = width;
    framebuffer->bitmap.height = height;
    framebuffer->bitmap.stride = CuiAlign(framebuffer->bitmap.width * 4, 64);

    int64_t needed_size = (int64_t) framebuffer->bitmap.stride * (int64_t) framebuffer->bitmap.height;

    if (needed_size > framebuffer->shared_memory_size)
    {
        int64_t old_shared_memory_size = framebuffer->shared_memory_size;
        framebuffer->shared_memory_size = CuiAlign(needed_size, CuiMiB(4));

        if (_cui_context.has_shared_memory_extension)
        {
            XShmDetach(_cui_context.x11_display, &framebuffer->backend.x11.shared_memory_info);
            shmdt(framebuffer->backend.x11.shared_memory_info.shmaddr);
            shmctl(framebuffer->backend.x11.shared_memory_info.shmid, IPC_RMID, 0);

            framebuffer->backend.x11.shared_memory_info.shmid = shmget(IPC_PRIVATE, framebuffer->shared_memory_size, IPC_CREAT | 0644);
            framebuffer->backend.x11.shared_memory_info.shmaddr = (char *) shmat(framebuffer->backend.x11.shared_memory_info.shmid, 0, 0);
            framebuffer->backend.x11.shared_memory_info.readOnly = True;

            framebuffer->bitmap.pixels = framebuffer->backend.x11.shared_memory_info.shmaddr;

            XShmAttach(_cui_context.x11_display, &framebuffer->backend.x11.shared_memory_info);
        }
        else
        {
            cui_platform_deallocate(framebuffer->bitmap.pixels, old_shared_memory_size);
            framebuffer->bitmap.pixels = cui_platform_allocate(framebuffer->shared_memory_size);
        }
    }
}

static void
_cui_x11_acquire_framebuffer(CuiWindow *window, int32_t width, int32_t height)
{
    CuiAssert(window->base.renderer_type == CUI_RENDERER_TYPE_SOFTWARE);
    CuiAssert(!window->renderer.software.current_framebuffer);

    for (;;)
    {
        for (uint32_t i = 0; i < CuiArrayCount(window->renderer.software.framebuffers); i += 1)
        {
            CuiFramebuffer *framebuffer = window->renderer.software.framebuffers + i;

            if (!framebuffer->is_busy)
            {
                if ((framebuffer->bitmap.width != width) || (framebuffer->bitmap.height != height))
                {
                    _cui_x11_resize_framebuffer(framebuffer, width, height);
                }

                framebuffer->is_busy = true;
                window->renderer.software.current_framebuffer = framebuffer;
                return;
            }
        }

        CuiAssert(_cui_context.has_shared_memory_extension);

        printf("all framebuffers busy\n");
        _cui_wait_for_frame_completion(window);
    }
}

#  endif

static bool
_cui_initialize_x11(void)
{
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

    _cui_context.has_xsync_extension = (XSyncQueryExtension(_cui_context.x11_display, &event_base, &error_base) &&
                                        XSyncInitialize(_cui_context.x11_display, &major, &minor));
#  if CUI_RENDERER_SOFTWARE_ENABLED
    _cui_context.has_shared_memory_extension = (XShmQueryExtension(_cui_context.x11_display) == True);

    if (_cui_context.has_shared_memory_extension)
    {
        _cui_context.x11_frame_completion_event = XShmGetEventBase(_cui_context.x11_display) + ShmCompletion;
    }
#  endif

    _cui_context.x11_input_method = XOpenIM(_cui_context.x11_display, 0, 0, 0);

    if (!_cui_context.x11_input_method)
    {
        return false;
    }

    _cui_context.x11_desktop_scale = 1;
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

    _cui_context.x11_atom_atom = XInternAtom(_cui_context.x11_display, "ATOM", false);
    _cui_context.x11_atom_manager = XInternAtom(_cui_context.x11_display, "MANAGER", false);
    _cui_context.x11_atom_targets = XInternAtom(_cui_context.x11_display, "TARGETS", false);
    _cui_context.x11_atom_cardinal = XInternAtom(_cui_context.x11_display, "CARDINAL", false);
    _cui_context.x11_atom_xsel_data = XInternAtom(_cui_context.x11_display, "XSEL_DATA", false);
    _cui_context.x11_atom_clipboard = XInternAtom(_cui_context.x11_display, "CLIPBOARD", false);
    _cui_context.x11_atom_utf8_string = XInternAtom(_cui_context.x11_display, "UTF8_STRING", false);
    _cui_context.x11_atom_wm_protocols = XInternAtom(_cui_context.x11_display, "WM_PROTOCOLS", false);
    _cui_context.x11_atom_wm_delete_window = XInternAtom(_cui_context.x11_display, "WM_DELETE_WINDOW", false);
    _cui_context.x11_atom_wm_sync_request = XInternAtom(_cui_context.x11_display, "_NET_WM_SYNC_REQUEST", false);
    _cui_context.x11_atom_wm_sync_request_counter = XInternAtom(_cui_context.x11_display, "_NET_WM_SYNC_REQUEST_COUNTER", false);
    _cui_context.x11_atom_wm_state = XInternAtom(_cui_context.x11_display, "_NET_WM_STATE", false);
    _cui_context.x11_atom_wm_state_add = XInternAtom(_cui_context.x11_display, "_NET_WM_STATE_ADD", false);
    _cui_context.x11_atom_wm_state_remove = XInternAtom(_cui_context.x11_display, "_NET_WM_STATE_REMOVE", false);
    _cui_context.x11_atom_wm_state_fullscreen = XInternAtom(_cui_context.x11_display, "_NET_WM_STATE_FULLSCREEN", false);
    _cui_context.x11_atom_wm_fullscreen_monitors = XInternAtom(_cui_context.x11_display, "_NET_WM_FULLSCREEN_MONITORS", false);
    _cui_context.x11_atom_xsettings_screen = XInternAtom(_cui_context.x11_display, xsettings_screen_name, false);
    _cui_context.x11_atom_xsettings_settings = XInternAtom(_cui_context.x11_display, "_XSETTINGS_SETTINGS", false);

    XGrabServer(_cui_context.x11_display);

    _cui_context.x11_settings_window = XGetSelectionOwner(_cui_context.x11_display, _cui_context.x11_atom_xsettings_screen);

    if (_cui_context.x11_settings_window != None)
    {
        XSelectInput(_cui_context.x11_display, _cui_context.x11_settings_window, StructureNotifyMask | PropertyChangeMask);

        Atom type;
        int format;
        unsigned long length, remaining;
        unsigned char *settings_buffer = 0;

        int ret = XGetWindowProperty(_cui_context.x11_display, _cui_context.x11_settings_window, _cui_context.x11_atom_xsettings_settings,
                                     0, 2048, false, AnyPropertyType, &type, &format, &length, &remaining, &settings_buffer);

        if ((ret == Success) && (length > 0))
        {
            if ((remaining == 0) && (format == 8))
            {
                CuiX11DesktopSettings desktop_settings = _cui_parse_desktop_settings(cui_make_string(settings_buffer, length));

                _cui_context.x11_desktop_scale = desktop_settings.ui_scale;
                _cui_context.double_click_time = desktop_settings.double_click_time;
            }

            XFree(settings_buffer);
        }
    }
    else
    {
        XrmInitialize();

        char *resource_manager = XResourceManagerString(_cui_context.x11_display);

        if (resource_manager)
        {
            XrmDatabase database = XrmGetStringDatabase(resource_manager);

            if (database)
            {
                char *type;
                XrmValue value;

                if (XrmGetResource(database, "Xft.dpi", "String", &type, &value))
                {
                    CuiString type_str = CuiCString(type);

                    if (value.addr && cui_string_equals(type_str, CuiStringLiteral("String")))
                    {
                        CuiString value_str = CuiCString(value.addr);
                        int32_t desktop_scale = (cui_parse_int32(value_str) + 48) / 96;

                        if (desktop_scale > 0)
                        {
                            _cui_context.x11_desktop_scale = desktop_scale;
                        }
                    }
                }

                XrmDestroyDatabase(database);
            }
        }
    }

    XUngrabServer(_cui_context.x11_display);

    _cui_context.x11_clipboard_window = XCreateWindow(_cui_context.x11_display, _cui_context.x11_root_window, 0, 0, 1, 1, 0,
                                                      CopyFromParent, InputOnly, CopyFromParent, 0, 0);

    if (!_cui_context.x11_clipboard_window)
    {
        return false;
    }

    cui_arena_allocate(&_cui_context.clipboard_arena, CuiMiB(1));
    _cui_context.clipboard_data = cui_make_string(0, 0);

    XColor black = { 0 };
    char cursor_data = 0;
    Pixmap cursor_pixmap = XCreateBitmapFromData(_cui_context.x11_display, _cui_context.x11_root_window, &cursor_data, 1, 1);
    _cui_context.x11_cursor_none = XCreatePixmapCursor(_cui_context.x11_display, cursor_pixmap, cursor_pixmap, &black, &black, 0, 0);

    _cui_context.x11_cursor_arrow           = XCreateFontCursor(_cui_context.x11_display, XC_left_ptr);
    _cui_context.x11_cursor_hand            = XCreateFontCursor(_cui_context.x11_display, XC_hand1);
    _cui_context.x11_cursor_text            = XCreateFontCursor(_cui_context.x11_display, XC_xterm);
    _cui_context.x11_cursor_move_all        = XCreateFontCursor(_cui_context.x11_display, XC_fleur);
    _cui_context.x11_cursor_move_left_right = XCreateFontCursor(_cui_context.x11_display, XC_sb_h_double_arrow);
    _cui_context.x11_cursor_move_up_down    = XCreateFontCursor(_cui_context.x11_display, XC_sb_v_double_arrow);

#  if CUI_RENDERER_OPENGLES2_ENABLED

    _cui_parse_egl_client_extensions();

    _cui_context.egl_display = EGL_NO_DISPLAY;

    if (_cui_context.has_EGL_EXT_platform_base && _cui_context.has_EGL_EXT_platform_x11)
    {
        _cui_context.egl_display = eglGetPlatformDisplay(EGL_PLATFORM_X11_EXT, _cui_context.x11_display, 0);

        if (_cui_context.egl_display != EGL_NO_DISPLAY)
        {
            EGLint major_version;
            EGLint minor_version;

            if (eglInitialize(_cui_context.egl_display, &major_version, &minor_version))
            {
#if 0
                char *apis = (char *) eglQueryString(_cui_context.egl_display, EGL_CLIENT_APIS);
                char *vendor = (char *) eglQueryString(_cui_context.egl_display, EGL_VENDOR);
                char *extensions = (char *) eglQueryString(_cui_context.egl_display, EGL_EXTENSIONS);

                printf("EGL version %d.%d\n", major_version, minor_version);
                printf("EGL APIs: %s\n", apis);
                printf("EGL Vendor: %s\n", vendor);
                printf("EGL Extensions: %s\n", extensions);
#endif
            }
            else
            {
                _cui_context.egl_display = EGL_NO_DISPLAY;
            }
        }
    }

#  endif

    return true;
}

#  if CUI_RENDERER_OPENGLES2_ENABLED

static bool
_cui_initialize_opengles2_x11(CuiWindow *window, XSetWindowAttributes window_attr)
{
    if (_cui_context.egl_display == EGL_NO_DISPLAY)
    {
        return false;
    }

    EGLint buffer_attr[] = {
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };

    EGLConfig config;
    EGLint num_config;

    if (!eglChooseConfig(_cui_context.egl_display, buffer_attr, &config, 1, &num_config))
    {
        return false;
    }

    int32_t width = cui_rect_get_width(window->content_rect);
    int32_t height = cui_rect_get_height(window->content_rect);

    window->x11_window = XCreateWindow(_cui_context.x11_display, _cui_context.x11_root_window, 0, 0,
                                       width, height, 0, 0, InputOutput, 0, CWEventMask, &window_attr);

    EGLSurface egl_surface = eglCreatePlatformWindowSurface(_cui_context.egl_display, config, &window->x11_window, 0);

    EGLContext egl_context = EGL_NO_CONTEXT;

    if (egl_surface != EGL_NO_SURFACE)
    {
        EGLint context_attr[] = {
            EGL_CONTEXT_MAJOR_VERSION, 2,
            EGL_CONTEXT_MINOR_VERSION, 0,
            EGL_NONE
        };

        egl_context = eglCreateContext(_cui_context.egl_display, config, EGL_NO_CONTEXT, context_attr);

        if (egl_context != EGL_NO_CONTEXT)
        {
            EGLBoolean success = eglMakeCurrent(_cui_context.egl_display, egl_surface, egl_surface, egl_context);

            if (success)
            {
#if 0
                printf("OpenGL ES Version: %s\n", glGetString(GL_VERSION));
                printf("OpenGL ES Extensions: %s\n", glGetString(GL_EXTENSIONS));
#endif

                window->renderer.opengles2.egl_surface = egl_surface;
                window->renderer.opengles2.egl_context = egl_context;
                window->base.renderer_type = CUI_RENDERER_TYPE_OPENGLES2;
                window->renderer.opengles2.renderer_opengles2 = _cui_create_opengles2_renderer();

                if (!window->renderer.opengles2.renderer_opengles2)
                {
                    eglMakeCurrent(_cui_context.egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
                    eglDestroyContext(_cui_context.egl_display, egl_context);

                    XDestroyWindow(_cui_context.x11_display, window->x11_window);

                    return false;
                }

                return true;
            }
        }
    }

    eglMakeCurrent(_cui_context.egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

    if (egl_context != EGL_NO_CONTEXT)
    {
        eglDestroyContext(_cui_context.egl_display, egl_context);
    }

    if (egl_surface != EGL_NO_SURFACE)
    {
        eglDestroySurface(_cui_context.egl_display, egl_surface);
    }

    XDestroyWindow(_cui_context.x11_display, window->x11_window);

    return false;
}

#  endif

#endif

#if CUI_BACKEND_WAYLAND_ENABLED

#include "xdg-shell.c"
#include "xdg-decoration-unstable-v1.c"

#include <linux/input.h>
#include <wayland-cursor.h>

#  if CUI_RENDERER_SOFTWARE_ENABLED

static void
_cui_wayland_handle_buffer_release(void *data, struct wl_buffer *buffer)
{
    CuiFramebuffer *framebuffer = (CuiFramebuffer *) data;
    CuiAssert(framebuffer->backend.wayland.buffer == buffer);
    framebuffer->is_busy = false;
}

static const struct wl_buffer_listener _cui_wayland_buffer_listener = {
    .release = _cui_wayland_handle_buffer_release,
};

static void
_cui_wayland_allocate_framebuffer(CuiFramebuffer *framebuffer, int32_t width, int32_t height)
{
    framebuffer->is_busy = false;

    framebuffer->bitmap.width  = width;
    framebuffer->bitmap.height = height;
    framebuffer->bitmap.stride = CuiAlign(framebuffer->bitmap.width * 4, 64);

    framebuffer->shared_memory_size = CuiAlign((int64_t) framebuffer->bitmap.stride * (int64_t) framebuffer->bitmap.height, CuiMiB(4));

    framebuffer->backend.wayland.shared_memory_fd = syscall(SYS_memfd_create, "wayland_framebuffer", 0);

    ftruncate(framebuffer->backend.wayland.shared_memory_fd, framebuffer->shared_memory_size);
    framebuffer->bitmap.pixels = mmap(0, framebuffer->shared_memory_size, PROT_READ | PROT_WRITE,
                                      MAP_SHARED, framebuffer->backend.wayland.shared_memory_fd, 0);

    framebuffer->backend.wayland.shared_memory_pool = wl_shm_create_pool(_cui_context.wayland_shared_memory,
                                                                         framebuffer->backend.wayland.shared_memory_fd,
                                                                         framebuffer->shared_memory_size);

    framebuffer->backend.wayland.buffer = wl_shm_pool_create_buffer(framebuffer->backend.wayland.shared_memory_pool, 0, framebuffer->bitmap.width,
                                                                    framebuffer->bitmap.height, framebuffer->bitmap.stride, WL_SHM_FORMAT_ARGB8888);

    wl_buffer_add_listener(framebuffer->backend.wayland.buffer, &_cui_wayland_buffer_listener, framebuffer);
}

static void
_cui_wayland_resize_framebuffer(CuiFramebuffer *framebuffer, int32_t width, int32_t height)
{
    CuiAssert(!framebuffer->is_busy);

    wl_buffer_destroy(framebuffer->backend.wayland.buffer);

    framebuffer->bitmap.width  = width;
    framebuffer->bitmap.height = height;
    framebuffer->bitmap.stride = CuiAlign(framebuffer->bitmap.width * 4, 64);

    int64_t needed_size = (int64_t) framebuffer->bitmap.stride * (int64_t) framebuffer->bitmap.height;

    if (needed_size > framebuffer->shared_memory_size)
    {
        int64_t old_shared_memory_size = framebuffer->shared_memory_size;
        framebuffer->shared_memory_size = CuiAlign(needed_size, CuiMiB(4));

        munmap(framebuffer->bitmap.pixels, old_shared_memory_size);
        ftruncate(framebuffer->backend.wayland.shared_memory_fd, framebuffer->shared_memory_size);
        framebuffer->bitmap.pixels = mmap(0, framebuffer->shared_memory_size, PROT_READ | PROT_WRITE,
                                          MAP_SHARED, framebuffer->backend.wayland.shared_memory_fd, 0);

        wl_shm_pool_resize(framebuffer->backend.wayland.shared_memory_pool, framebuffer->shared_memory_size);
    }

    framebuffer->backend.wayland.buffer = wl_shm_pool_create_buffer(framebuffer->backend.wayland.shared_memory_pool, 0, framebuffer->bitmap.width,
                                                                    framebuffer->bitmap.height, framebuffer->bitmap.stride, WL_SHM_FORMAT_ARGB8888);

    wl_buffer_add_listener(framebuffer->backend.wayland.buffer, &_cui_wayland_buffer_listener, framebuffer);
}

static void
_cui_wayland_acquire_framebuffer(CuiWindow *window, int32_t width, int32_t height)
{
    CuiAssert(window->base.renderer_type == CUI_RENDERER_TYPE_SOFTWARE);
    CuiAssert(!window->renderer.software.current_framebuffer);

    for (;;)
    {
        for (uint32_t i = 0; i < CuiArrayCount(window->renderer.software.framebuffers); i += 1)
        {
            CuiFramebuffer *framebuffer = window->renderer.software.framebuffers + i;

            if (!framebuffer->is_busy)
            {
                if ((framebuffer->bitmap.width != width) || (framebuffer->bitmap.height != height))
                {
                    _cui_wayland_resize_framebuffer(framebuffer, width, height);
                }

                framebuffer->is_busy = true;
                window->renderer.software.current_framebuffer = framebuffer;
                return;
            }
        }

        printf("all framebuffers busy\n");
        wl_display_dispatch(_cui_context.wayland_display);
    }
}

#  endif

static void _cui_window_draw(CuiWindow *window);

static inline void
_cui_wayland_commit_frame(CuiWindow *window)
{
    _cui_window_draw(window);

    if (window->wayland_configure_serial)
    {
        xdg_surface_ack_configure(window->wayland_xdg_surface, window->wayland_configure_serial);
        window->wayland_configure_serial = 0;
    }

    wl_surface_set_buffer_scale(window->wayland_surface, window->backbuffer_scale);

    xdg_surface_set_window_geometry(window->wayland_xdg_surface, window->wayland_window_rect.min.x, window->wayland_window_rect.min.y,
                                    cui_rect_get_width(window->wayland_window_rect), cui_rect_get_height(window->wayland_window_rect));

    if (window->base.creation_flags & CUI_WINDOW_CREATION_FLAG_NOT_USER_RESIZABLE)
    {
        xdg_toplevel_set_min_size(window->wayland_xdg_toplevel,
                                  cui_rect_get_width(window->wayland_window_rect),
                                  cui_rect_get_height(window->wayland_window_rect));
        xdg_toplevel_set_max_size(window->wayland_xdg_toplevel,
                                  cui_rect_get_width(window->wayland_window_rect),
                                  cui_rect_get_height(window->wayland_window_rect));
    }

    struct wl_region *input_region = wl_compositor_create_region(_cui_context.wayland_compositor);
    wl_region_add(input_region, window->wayland_input_rect.min.x, window->wayland_input_rect.min.y,
                  cui_rect_get_width(window->wayland_input_rect), cui_rect_get_height(window->wayland_input_rect));
    wl_surface_set_input_region(window->wayland_surface, input_region);
    wl_region_destroy(input_region);

    switch (window->base.renderer_type)
    {
        case CUI_RENDERER_TYPE_SOFTWARE:
        {
#  if CUI_RENDERER_SOFTWARE_ENABLED
            CuiFramebuffer *framebuffer = window->renderer.software.current_framebuffer;

            wl_surface_attach(window->wayland_surface, framebuffer->backend.wayland.buffer, 0, 0);
            wl_surface_damage(window->wayland_surface, 0, 0, INT32_MAX, INT32_MAX);
            wl_surface_commit(window->wayland_surface);

            window->renderer.software.current_framebuffer = 0;
#  else
            CuiAssert(!"CUI_RENDERER_TYPE_SOFTWARE not enabled.");
#  endif
        } break;

        case CUI_RENDERER_TYPE_OPENGLES2:
        {
#if CUI_RENDERER_OPENGLES2_ENABLED
            eglSwapBuffers(_cui_context.egl_display, window->renderer.opengles2.egl_surface);
#else
            CuiAssert(!"CUI_RENDERER_TYPE_OPENGLES2 not enabled.");
#endif
        } break;

        case CUI_RENDERER_TYPE_METAL:
        {
            CuiAssert(!"CUI_RENDERER_TYPE_METAL not supported.");
        } break;

        case CUI_RENDERER_TYPE_DIRECT3D11:
        {
            CuiAssert(!"CUI_RENDERER_TYPE_DIRECT3D11 not supported.");
        } break;
    }
}

static inline CuiWaylandMonitor *
_cui_wayland_get_monitor_from_output(struct wl_output *output)
{
    CuiWaylandMonitor *result = 0;

    for (int32_t i = 0; i < cui_array_count(_cui_context.wayland_monitors); i += 1)
    {
        CuiWaylandMonitor *monitor = _cui_context.wayland_monitors + i;

        if (monitor->output == output)
        {
            result = monitor;
            break;
        }
    }

    return result;
}

static inline CuiWaylandMonitor *
_cui_wayland_get_monitor_from_name(uint32_t name)
{
    CuiWaylandMonitor *result = 0;

    for (int32_t i = 0; i < cui_array_count(_cui_context.wayland_monitors); i += 1)
    {
        CuiWaylandMonitor *monitor = _cui_context.wayland_monitors + i;

        if (monitor->id == name)
        {
            result = monitor;
            break;
        }
    }

    return result;
}

static inline int32_t
_cui_wayland_get_monitor_index_from_name(uint32_t name)
{
    int32_t result = -1;

    for (int32_t i = 0; i < cui_array_count(_cui_context.wayland_monitors); i += 1)
    {
        CuiWaylandMonitor *monitor = _cui_context.wayland_monitors + i;

        if (monitor->id == name)
        {
            result = i;
            break;
        }
    }

    return result;
}

static void
_cui_wayland_update_window_backbuffer_scale(CuiWindow *window)
{
    int32_t new_backbuffer_scale = 1;
    int32_t old_backbuffer_scale = window->backbuffer_scale;

    for (int32_t i = 0; i < cui_array_count(window->wayland_monitors); i += 1)
    {
        CuiWaylandMonitor *monitor = _cui_wayland_get_monitor_from_name(window->wayland_monitors[i]);

        if (monitor->scale > new_backbuffer_scale)
        {
            new_backbuffer_scale = monitor->scale;
        }
    }

    if (new_backbuffer_scale != old_backbuffer_scale)
    {
        window->backbuffer_scale = new_backbuffer_scale;

        if (_cui_context.common.scale_factor == 0)
        {
            _cui_window_set_ui_scale(window, (float) window->backbuffer_scale);

            int32_t old_width = cui_rect_get_width(window->content_rect);
            int32_t old_height = cui_rect_get_height(window->content_rect);

            int32_t new_width  = (old_width / old_backbuffer_scale) * window->backbuffer_scale;
            int32_t new_height = (old_height / old_backbuffer_scale) * window->backbuffer_scale;

            cui_window_resize(window, new_width, new_height);
        }
    }
}

static CuiWaylandMonitor *
_cui_wayland_get_fullscreen_monitor_for_window(CuiWindow *window)
{
    CuiAssert(cui_array_count(window->wayland_monitors));

    // TODO: better way to determin fullscreen monitor

    CuiWaylandMonitor *monitor = _cui_wayland_get_monitor_from_name(window->wayland_monitors[0]);

    return monitor;
}

static inline CuiWindow *
_cui_wayland_get_window_from_surface(struct wl_surface *surface)
{
    CuiWindow *result = 0;

    for (uint32_t window_index = 0;
         window_index < _cui_context.common.window_count; window_index += 1)
    {
        CuiWindow *window = _cui_context.common.windows[window_index];

        if (window->wayland_surface == surface)
        {
            result = window;
            break;
        }
    }

    return result;
}

static void
_cui_wayland_handle_output_geometry(void *data, struct wl_output *output, int32_t x, int32_t y, int32_t physical_width,
                                    int32_t physical_height, int32_t subpixel, const char *maker, const char *model, int32_t transform)
{
    (void) data;
    (void) output;
    (void) x;
    (void) y;
    (void) physical_width;
    (void) physical_height;
    (void) subpixel;
    (void) maker;
    (void) model;
    (void) transform;
}

static void
_cui_wayland_handle_output_mode(void *data, struct wl_output *output, uint32_t flags, int32_t width, int32_t height, int32_t refresh)
{
    (void) data;
    (void) output;
    (void) flags;
    (void) width;
    (void) height;
    (void) refresh;
}

static void
_cui_wayland_handle_output_done(void *data, struct wl_output *output)
{
    (void) data;

    CuiWaylandMonitor *monitor = _cui_wayland_get_monitor_from_output(output);

    if (monitor->scale != monitor->pending_scale)
    {
        monitor->scale = monitor->pending_scale;

        for (uint32_t window_index = 0;
             window_index < _cui_context.common.window_count; window_index += 1)
        {
            CuiWindow *window = _cui_context.common.windows[window_index];
            _cui_wayland_update_window_backbuffer_scale(window);
        }
    }
}

static void
_cui_wayland_handle_output_scale(void *data, struct wl_output *output, int32_t factor)
{
    (void) data;

    CuiWaylandMonitor *monitor = _cui_wayland_get_monitor_from_output(output);
    monitor->pending_scale = factor;
}

static const struct wl_output_listener _cui_wayland_output_listener = {
    .geometry = _cui_wayland_handle_output_geometry,
    .mode     = _cui_wayland_handle_output_mode,
    .done     = _cui_wayland_handle_output_done,
    .scale    = _cui_wayland_handle_output_scale,
};

static void
_cui_wayland_handle_surface_enter(void *data, struct wl_surface *surface, struct wl_output *output)
{
    CuiWindow *window = (CuiWindow *) data;
    CuiWaylandMonitor *monitor = _cui_wayland_get_monitor_from_output(output);

    CuiAssert(window->wayland_surface == surface);

    *cui_array_append(window->wayland_monitors) = monitor->id;
    _cui_wayland_update_window_backbuffer_scale(window);
}

static void
_cui_wayland_handle_surface_leave(void *data, struct wl_surface *surface, struct wl_output *output)
{
    CuiWindow *window = (CuiWindow *) data;
    CuiWaylandMonitor *monitor = _cui_wayland_get_monitor_from_output(output);

    CuiAssert(window->wayland_surface == surface);

    for (int32_t i = 0; i < cui_array_count(window->wayland_monitors); i += 1)
    {
        if (window->wayland_monitors[i] == monitor->id)
        {
            int32_t index = --_cui_array_header(window->wayland_monitors)->count;
            window->wayland_monitors[i] = window->wayland_monitors[index];
            break;
        }
    }

    _cui_wayland_update_window_backbuffer_scale(window);
}

static const struct wl_surface_listener _cui_wayland_surface_listener = {
    .enter = _cui_wayland_handle_surface_enter,
    .leave = _cui_wayland_handle_surface_leave,
};

static void
_cui_wayland_update_window_with_window_size(CuiWindow *window, int32_t width, int32_t height)
{
    if (window->do_client_side_decoration)
    {
        int32_t integer_ui_scale;

        if (_cui_context.common.scale_factor == 0)
        {
            integer_ui_scale = window->backbuffer_scale;
        }
        else
        {
            integer_ui_scale = _cui_context.common.scale_factor;
        }

        int32_t outline_width = integer_ui_scale;
        int32_t titlebar_height = integer_ui_scale * _CUI_WINDOW_TITLEBAR_HEIGHT;

        int32_t drop_shadow_padding_top    = integer_ui_scale * _CUI_DROP_SHADOW_PADDING_TOP;
        int32_t drop_shadow_padding_right  = integer_ui_scale * _CUI_DROP_SHADOW_PADDING_RIGHT;
        int32_t drop_shadow_padding_bottom = integer_ui_scale * _CUI_DROP_SHADOW_PADDING_BOTTOM;
        int32_t drop_shadow_padding_left   = integer_ui_scale * _CUI_DROP_SHADOW_PADDING_LEFT;

        if (cui_window_is_fullscreen(window))
        {
            titlebar_height = 0;
            outline_width = 0;
            drop_shadow_padding_top = 0;
            drop_shadow_padding_right = 0;
            drop_shadow_padding_bottom = 0;
            drop_shadow_padding_left = 0;
        }

        window->backbuffer_rect.min.x = 0;
        window->backbuffer_rect.min.y = 0;
        window->backbuffer_rect.max.x = (2 * outline_width) + drop_shadow_padding_left + drop_shadow_padding_right + width;
        window->backbuffer_rect.max.y = (2 * outline_width) + drop_shadow_padding_top + drop_shadow_padding_bottom + height;

        window->layout_rect.min.x = window->backbuffer_rect.min.x + drop_shadow_padding_left;
        window->layout_rect.min.y = window->backbuffer_rect.min.y + drop_shadow_padding_top;
        window->layout_rect.max.x = window->backbuffer_rect.max.x - drop_shadow_padding_right;
        window->layout_rect.max.y = window->backbuffer_rect.max.y - drop_shadow_padding_bottom;

        window->window_rect.min.x = window->layout_rect.min.x + outline_width;
        window->window_rect.min.y = window->layout_rect.min.y + outline_width;
        window->window_rect.max.x = window->layout_rect.max.x - outline_width;
        window->window_rect.max.y = window->layout_rect.max.y - outline_width;

        window->content_rect.min.x = window->window_rect.min.x;
        window->content_rect.min.y = window->window_rect.min.y + titlebar_height;
        window->content_rect.max.x = window->window_rect.max.x;
        window->content_rect.max.y = window->window_rect.max.y;

        window->wayland_window_rect.min.x = window->window_rect.min.x / window->backbuffer_scale;
        window->wayland_window_rect.min.y = window->window_rect.min.y / window->backbuffer_scale;
        window->wayland_window_rect.max.x = window->window_rect.max.x / window->backbuffer_scale;
        window->wayland_window_rect.max.y = window->window_rect.max.y / window->backbuffer_scale;

        int32_t offset = integer_ui_scale * _CUI_WAYLAND_INPUT_MARGIN;

        window->wayland_input_rect.min.x = window->window_rect.min.x - offset;
        window->wayland_input_rect.min.y = window->window_rect.min.y - offset;
        window->wayland_input_rect.max.x = window->window_rect.max.x + offset;
        window->wayland_input_rect.max.y = window->window_rect.max.y + offset;

        window->wayland_input_rect.min.x /= window->backbuffer_scale;
        window->wayland_input_rect.min.y /= window->backbuffer_scale;
        window->wayland_input_rect.max.x /= window->backbuffer_scale;
        window->wayland_input_rect.max.y /= window->backbuffer_scale;
    }
    else
    {
        window->backbuffer_rect.min.x = 0;
        window->backbuffer_rect.min.y = 0;
        window->backbuffer_rect.max.x = width;
        window->backbuffer_rect.max.y = height;

        window->layout_rect = window->backbuffer_rect;
        window->window_rect = window->backbuffer_rect;
        window->content_rect = window->backbuffer_rect;

        window->wayland_window_rect.min.x = 0;
        window->wayland_window_rect.min.y = 0;
        window->wayland_window_rect.max.x = width / window->backbuffer_scale;
        window->wayland_window_rect.max.y = height / window->backbuffer_scale;

        window->wayland_input_rect = window->wayland_window_rect;
    }

    window->pending_window_width = cui_rect_get_width(window->window_rect);
    window->pending_window_height = cui_rect_get_height(window->window_rect);

    if (!cui_window_is_fullscreen(window) && !cui_window_is_maximized(window) && !cui_window_is_tiled(window))
    {
        window->windowed_width = cui_rect_get_width(window->window_rect);
        window->windowed_height = cui_rect_get_height(window->window_rect);
    }

    if (window->base.platform_root_widget)
    {
        cui_widget_layout(window->base.platform_root_widget, window->layout_rect);
    }

    window->base.needs_redraw = true;
}

static void
_cui_wayland_update_window_with_content_size(CuiWindow *window, int32_t width, int32_t height)
{
    if (window->do_client_side_decoration)
    {
        int32_t integer_ui_scale;

        if (_cui_context.common.scale_factor == 0)
        {
            integer_ui_scale = window->backbuffer_scale;
        }
        else
        {
            integer_ui_scale = _cui_context.common.scale_factor;
        }

        int32_t outline_width = integer_ui_scale;
        int32_t titlebar_height = integer_ui_scale * _CUI_WINDOW_TITLEBAR_HEIGHT;

        int32_t drop_shadow_padding_top    = integer_ui_scale * _CUI_DROP_SHADOW_PADDING_TOP;
        int32_t drop_shadow_padding_right  = integer_ui_scale * _CUI_DROP_SHADOW_PADDING_RIGHT;
        int32_t drop_shadow_padding_bottom = integer_ui_scale * _CUI_DROP_SHADOW_PADDING_BOTTOM;
        int32_t drop_shadow_padding_left   = integer_ui_scale * _CUI_DROP_SHADOW_PADDING_LEFT;

        if (cui_window_is_fullscreen(window))
        {
            titlebar_height = 0;
            outline_width = 0;
            drop_shadow_padding_top = 0;
            drop_shadow_padding_right = 0;
            drop_shadow_padding_bottom = 0;
            drop_shadow_padding_left = 0;
        }

        window->backbuffer_rect.min.x = 0;
        window->backbuffer_rect.min.y = 0;
        window->backbuffer_rect.max.x = (2 * outline_width) + drop_shadow_padding_left + drop_shadow_padding_right + width;
        window->backbuffer_rect.max.y = (2 * outline_width) + drop_shadow_padding_top + drop_shadow_padding_bottom + titlebar_height + height;

        window->layout_rect.min.x = window->backbuffer_rect.min.x + drop_shadow_padding_left;
        window->layout_rect.min.y = window->backbuffer_rect.min.y + drop_shadow_padding_top;
        window->layout_rect.max.x = window->backbuffer_rect.max.x - drop_shadow_padding_right;
        window->layout_rect.max.y = window->backbuffer_rect.max.y - drop_shadow_padding_bottom;

        window->window_rect.min.x = window->layout_rect.min.x + outline_width;
        window->window_rect.min.y = window->layout_rect.min.y + outline_width;
        window->window_rect.max.x = window->layout_rect.max.x - outline_width;
        window->window_rect.max.y = window->layout_rect.max.y - outline_width;

        window->content_rect.min.x = window->window_rect.min.x;
        window->content_rect.min.y = window->window_rect.min.y + titlebar_height;
        window->content_rect.max.x = window->window_rect.max.x;
        window->content_rect.max.y = window->window_rect.max.y;

        window->wayland_window_rect.min.x = window->window_rect.min.x / window->backbuffer_scale;
        window->wayland_window_rect.min.y = window->window_rect.min.y / window->backbuffer_scale;
        window->wayland_window_rect.max.x = window->window_rect.max.x / window->backbuffer_scale;
        window->wayland_window_rect.max.y = window->window_rect.max.y / window->backbuffer_scale;

        int32_t offset = integer_ui_scale * _CUI_WAYLAND_INPUT_MARGIN;

        window->wayland_input_rect.min.x = window->window_rect.min.x - offset;
        window->wayland_input_rect.min.y = window->window_rect.min.y - offset;
        window->wayland_input_rect.max.x = window->window_rect.max.x + offset;
        window->wayland_input_rect.max.y = window->window_rect.max.y + offset;

        window->wayland_input_rect.min.x /= window->backbuffer_scale;
        window->wayland_input_rect.min.y /= window->backbuffer_scale;
        window->wayland_input_rect.max.x /= window->backbuffer_scale;
        window->wayland_input_rect.max.y /= window->backbuffer_scale;
    }
    else
    {
        window->backbuffer_rect.min.x = 0;
        window->backbuffer_rect.min.y = 0;
        window->backbuffer_rect.max.x = width;
        window->backbuffer_rect.max.y = height;

        window->layout_rect = window->backbuffer_rect;
        window->window_rect = window->backbuffer_rect;
        window->content_rect = window->backbuffer_rect;

        window->wayland_window_rect.min.x = 0;
        window->wayland_window_rect.min.y = 0;
        window->wayland_window_rect.max.x = width / window->backbuffer_scale;
        window->wayland_window_rect.max.y = height / window->backbuffer_scale;

        window->wayland_input_rect = window->wayland_window_rect;
    }

    window->pending_window_width = cui_rect_get_width(window->window_rect);
    window->pending_window_height = cui_rect_get_height(window->window_rect);

    if (!cui_window_is_fullscreen(window) && !cui_window_is_maximized(window) && !cui_window_is_tiled(window))
    {
        window->windowed_width = cui_rect_get_width(window->window_rect);
        window->windowed_height = cui_rect_get_height(window->window_rect);
    }

    if (window->base.platform_root_widget)
    {
        cui_widget_layout(window->base.platform_root_widget, window->layout_rect);
    }

    window->base.needs_redraw = true;
}

static void
_cui_wayland_handle_xdg_surface_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial)
{
    CuiWindow *window = (CuiWindow *) data;
    CuiAssert(window->wayland_xdg_surface == xdg_surface);

#if 0
    printf("configure\n");
#endif

    window->wayland_configure_serial = serial;

    int32_t window_width = cui_rect_get_width(window->window_rect);
    int32_t window_height = cui_rect_get_height(window->window_rect);

    if ((window->pending_window_state != window->base.state) ||
        (window->pending_do_client_side_decoration != window->do_client_side_decoration) ||
        (window->pending_window_width != window_width) || (window->pending_window_height != window_height))
    {
        if (window->pending_do_client_side_decoration != window->do_client_side_decoration)
        {
            window->do_client_side_decoration = window->pending_do_client_side_decoration;

            if (window->do_client_side_decoration)
            {
                CuiAssert(window->base.platform_root_widget == window->base.user_root_widget);

                if (window->base.user_root_widget)
                {
                    cui_widget_replace_child(window->client_side_decoration_root_widget,
                                             window->client_side_decoration_dummy_user_root_widget,
                                             window->base.user_root_widget);
                }
                else
                {
                    window->base.user_root_widget = window->client_side_decoration_dummy_user_root_widget;
                }

                window->base.platform_root_widget = window->client_side_decoration_root_widget;
                cui_widget_set_ui_scale(window->base.platform_root_widget, window->base.ui_scale);
            }
            else
            {
                CuiAssert(window->base.platform_root_widget == window->client_side_decoration_root_widget);

                if (window->base.user_root_widget == window->client_side_decoration_dummy_user_root_widget)
                {
                    window->base.user_root_widget = 0;
                }
                else
                {
                    cui_widget_replace_child(window->client_side_decoration_root_widget, window->base.user_root_widget,
                                             window->client_side_decoration_dummy_user_root_widget);
                }

                window->base.platform_root_widget = window->base.user_root_widget;
            }
        }

        window->base.state = window->pending_window_state;

        if (window->do_client_side_decoration)
        {
            if (cui_window_is_maximized(window) || cui_window_is_fullscreen(window) || cui_window_is_tiled(window))
            {
                if (cui_window_is_fullscreen(window))
                {
                    cui_widget_set_preferred_size(window->titlebar, 0.0f, 0.0f);
                    cui_widget_set_preferred_size(window->button_layer, 0.0f, 0.0f);

                    cui_widget_set_border_width(window->base.platform_root_widget, 0.0f, 0.0f, 0.0f, 0.0f);
                    cui_widget_set_box_shadow(window->base.platform_root_widget, 0.0f, 0.0f, 0.0f);
                }
                else
                {
                    cui_widget_set_preferred_size(window->titlebar, 0.0f, (float) _CUI_WINDOW_TITLEBAR_HEIGHT);
                    cui_widget_set_preferred_size(window->button_layer, 0.0f, (float) _CUI_WINDOW_TITLEBAR_HEIGHT);

                    cui_widget_set_border_width(window->base.platform_root_widget, 1.0f, 1.0f, 1.0f, 1.0f);
                    cui_widget_set_box_shadow(window->base.platform_root_widget, 0.0f, 4.0f, 16.0f);
                }

                cui_widget_set_border_radius(window->base.platform_root_widget, 0.0f, 0.0f, 0.0f, 0.0f);

                cui_widget_set_border_radius(window->titlebar, 0.0f, 0.0f, 0.0f, 0.0f);
                cui_widget_set_border_radius(window->close_button, 0.0f, 0.0f, 0.0f, 0.0f);
            }
            else
            {
                cui_widget_set_preferred_size(window->titlebar, 0.0f, (float) _CUI_WINDOW_TITLEBAR_HEIGHT);
                cui_widget_set_preferred_size(window->button_layer, 0.0f, (float) _CUI_WINDOW_TITLEBAR_HEIGHT);

                cui_widget_set_border_radius(window->base.platform_root_widget, 4.0f, 4.0f, 0.0f, 0.0f);
                cui_widget_set_border_width(window->base.platform_root_widget, 1.0f, 1.0f, 1.0f, 1.0f);
                cui_widget_set_box_shadow(window->base.platform_root_widget, 0.0f, 4.0f, 16.0f);

                cui_widget_set_border_radius(window->titlebar, 3.0f, 3.0f, 0.0f, 0.0f);
                cui_widget_set_border_radius(window->close_button, 0.0f, 3.0f, 0.0f, 0.0f);
            }
        }

        _cui_wayland_update_window_with_window_size(window, window->pending_window_width, window->pending_window_height);
    }

    if (!window->is_mapped)
    {
        _cui_wayland_commit_frame(window);
        window->is_mapped = true;
        window->base.needs_redraw = false;
    }
}

static const struct xdg_surface_listener _cui_wayland_xdg_surface_listener = {
    .configure = _cui_wayland_handle_xdg_surface_configure,
};

static void
_cui_wayland_xdg_toplevel_configure(void *data, struct xdg_toplevel *xdg_toplevel, int32_t width,
                                    int32_t height, struct wl_array *states)
{
    CuiWindow *window = (CuiWindow *) data;
    CuiAssert(window->wayland_xdg_toplevel == xdg_toplevel);

    if ((width > 0) && (height > 0))
    {
        width = cui_max_int32(width, _CUI_WAYLAND_MINIMUM_WIDTH);
        height = cui_max_int32(height, _CUI_WAYLAND_MINIMUM_HEIGHT);

        window->pending_window_width = window->backbuffer_scale * width;
        window->pending_window_height = window->backbuffer_scale * height;
    }
    else
    {
        window->pending_window_width = window->windowed_width;
        window->pending_window_height = window->windowed_height;
    }

    window->pending_window_state = 0;

    uint32_t *first_state = (uint32_t *) states->data;
    uint32_t *last_state  = (uint32_t *) ((uint8_t *) states->data + states->size);

    for (uint32_t *state = first_state; state < last_state; state += 1)
    {
        switch (*state)
        {
            case XDG_TOPLEVEL_STATE_MAXIMIZED:
            {
                window->pending_window_state |= CUI_WINDOW_STATE_MAXIMIZED;
            } break;

            case XDG_TOPLEVEL_STATE_FULLSCREEN:
            {
                window->pending_window_state |= CUI_WINDOW_STATE_FULLSCREEN;
            } break;

            case XDG_TOPLEVEL_STATE_TILED_LEFT:
            {
                window->pending_window_state |= CUI_WINDOW_STATE_TILED_LEFT;
            } break;

            case XDG_TOPLEVEL_STATE_TILED_RIGHT:
            {
                window->pending_window_state |= CUI_WINDOW_STATE_TILED_RIGHT;
            } break;

            case XDG_TOPLEVEL_STATE_TILED_TOP:
            {
                window->pending_window_state |= CUI_WINDOW_STATE_TILED_TOP;
            } break;

            case XDG_TOPLEVEL_STATE_TILED_BOTTOM:
            {
                window->pending_window_state |= CUI_WINDOW_STATE_TILED_BOTTOM;
            } break;
        }
    }
}

static void
_cui_wayland_xdg_toplevel_close(void *data, struct xdg_toplevel *toplevel)
{
    CuiWindow *window = (CuiWindow *) data;
    CuiAssert(window->wayland_xdg_toplevel == toplevel);

    cui_window_close(window);
}

static const struct xdg_toplevel_listener _cui_wayland_xdg_toplevel_listener = {
    .configure = _cui_wayland_xdg_toplevel_configure,
    .close     = _cui_wayland_xdg_toplevel_close,
};

static void
_cui_wayland_xdg_decoration_configure(void *data, struct zxdg_toplevel_decoration_v1 *decoration, uint32_t mode)
{
    CuiWindow *window = (CuiWindow *) data;
    CuiAssert(window->wayland_xdg_decoration == decoration);

    if (mode == ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE)
    {
        window->pending_do_client_side_decoration = true;
    }
    else
    {
        CuiAssert(mode == ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
        window->pending_do_client_side_decoration = false;
    }
}

static const struct zxdg_toplevel_decoration_v1_listener _cui_wayland_xdg_decoration_listener = {
    .configure = _cui_wayland_xdg_decoration_configure,
};

static inline CuiWaylandCursorTheme *
_cui_wayland_get_cursor_theme(int32_t cursor_buffer_scale)
{
    CuiAssert((cursor_buffer_scale > 0) && (cursor_buffer_scale <= (int32_t) CuiArrayCount(_cui_context.wayland_cursor_themes)));

    CuiWaylandCursorTheme *cursor_theme = _cui_context.wayland_cursor_themes + (cursor_buffer_scale - 1);

    CuiAssert(cursor_theme->ref_count && cursor_theme->cursor_theme);

    return cursor_theme;
}

static inline CuiWaylandCursorTheme *
_cui_wayland_ref_cursor_theme(int32_t cursor_buffer_scale)
{
    CuiAssert((cursor_buffer_scale > 0) && (cursor_buffer_scale <= (int32_t) CuiArrayCount(_cui_context.wayland_cursor_themes)));

    CuiWaylandCursorTheme *cursor_theme = _cui_context.wayland_cursor_themes + (cursor_buffer_scale - 1);

    if (!cursor_theme->ref_count)
    {
        cursor_theme->cursor_theme = wl_cursor_theme_load(0, cursor_buffer_scale * 24, _cui_context.wayland_shared_memory);

        cursor_theme->wayland_cursor_arrow               = wl_cursor_theme_get_cursor(cursor_theme->cursor_theme, "left_ptr");
        cursor_theme->wayland_cursor_hand                = wl_cursor_theme_get_cursor(cursor_theme->cursor_theme, "hand1");
        cursor_theme->wayland_cursor_text                = wl_cursor_theme_get_cursor(cursor_theme->cursor_theme, "xterm");
        cursor_theme->wayland_cursor_move_all            = wl_cursor_theme_get_cursor(cursor_theme->cursor_theme, "fleur");
        cursor_theme->wayland_cursor_move_left_right     = wl_cursor_theme_get_cursor(cursor_theme->cursor_theme, "sb_h_double_arrow");
        cursor_theme->wayland_cursor_move_up_down        = wl_cursor_theme_get_cursor(cursor_theme->cursor_theme, "sb_v_double_arrow");
        cursor_theme->wayland_cursor_resize_top          = wl_cursor_theme_get_cursor(cursor_theme->cursor_theme, "top_side");
        cursor_theme->wayland_cursor_resize_bottom       = wl_cursor_theme_get_cursor(cursor_theme->cursor_theme, "bottom_side");
        cursor_theme->wayland_cursor_resize_left         = wl_cursor_theme_get_cursor(cursor_theme->cursor_theme, "left_side");
        cursor_theme->wayland_cursor_resize_right        = wl_cursor_theme_get_cursor(cursor_theme->cursor_theme, "right_side");
        cursor_theme->wayland_cursor_resize_top_left     = wl_cursor_theme_get_cursor(cursor_theme->cursor_theme, "top_left_corner");
        cursor_theme->wayland_cursor_resize_top_right    = wl_cursor_theme_get_cursor(cursor_theme->cursor_theme, "top_right_corner");
        cursor_theme->wayland_cursor_resize_bottom_left  = wl_cursor_theme_get_cursor(cursor_theme->cursor_theme, "bottom_left_corner");
        cursor_theme->wayland_cursor_resize_bottom_right = wl_cursor_theme_get_cursor(cursor_theme->cursor_theme, "bottom_right_corner");
    }

    cursor_theme->ref_count += 1;

    return cursor_theme;
}

static inline void
_cui_wayland_unref_cursor_theme(int32_t cursor_buffer_scale)
{
    CuiAssert((cursor_buffer_scale > 0) && (cursor_buffer_scale <= (int32_t) CuiArrayCount(_cui_context.wayland_cursor_themes)));

    CuiWaylandCursorTheme *cursor_theme = _cui_context.wayland_cursor_themes + (cursor_buffer_scale - 1);

    CuiAssert(cursor_theme->ref_count && cursor_theme->cursor_theme);

    cursor_theme->ref_count -= 1;

    if (!cursor_theme->ref_count)
    {
        wl_cursor_theme_destroy(cursor_theme->cursor_theme);
        cursor_theme->cursor_theme = 0;
    }
}

static inline void
_cui_wayland_update_cursor(CuiWindow *window)
{
    CuiWaylandCursorTheme *cursor_theme = 0;

    if (window->wayland_cursor_buffer_scale != window->backbuffer_scale)
    {
        if (window->wayland_cursor_buffer_scale != 0)
        {
            _cui_wayland_unref_cursor_theme(window->wayland_cursor_buffer_scale);
        }

        window->wayland_cursor_buffer_scale = window->backbuffer_scale;
        cursor_theme = _cui_wayland_ref_cursor_theme(window->wayland_cursor_buffer_scale);
    }
    else
    {
        cursor_theme = _cui_wayland_get_cursor_theme(window->wayland_cursor_buffer_scale);
    }

    CuiCursorType target_cursor = CUI_CURSOR_ARROW;

    if (window->base.cursor)
    {
        target_cursor = window->base.cursor;
    }

    if (!window->pointer_button_mask && _cui_context.wayland_platform_cursor)
    {
        target_cursor = _cui_context.wayland_platform_cursor;
    }

    if (_cui_context.current_cursor != target_cursor)
    {
        if (target_cursor == CUI_CURSOR_NONE)
        {
            wl_pointer_set_cursor(_cui_context.wayland_pointer, _cui_context.wayland_pointer_serial, 0, 0, 0);
        }
        else
        {
            struct wl_cursor *cursor = 0;

            switch (target_cursor)
            {
                case CUI_CURSOR_NONE:                CuiAssert(!"should be handled in if above");               break;
                case CUI_CURSOR_ARROW:               cursor = cursor_theme->wayland_cursor_arrow;               break;
                case CUI_CURSOR_HAND:                cursor = cursor_theme->wayland_cursor_hand;                break;
                case CUI_CURSOR_TEXT:                cursor = cursor_theme->wayland_cursor_text;                break;
                case CUI_CURSOR_MOVE_ALL:            cursor = cursor_theme->wayland_cursor_move_all;            break;
                case CUI_CURSOR_MOVE_LEFT_RIGHT:     cursor = cursor_theme->wayland_cursor_move_left_right;     break;
                case CUI_CURSOR_MOVE_UP_DOWN:        cursor = cursor_theme->wayland_cursor_move_up_down;        break;
                case CUI_CURSOR_RESIZE_TOP:          cursor = cursor_theme->wayland_cursor_resize_top;          break;
                case CUI_CURSOR_RESIZE_BOTTOM:       cursor = cursor_theme->wayland_cursor_resize_bottom;       break;
                case CUI_CURSOR_RESIZE_LEFT:         cursor = cursor_theme->wayland_cursor_resize_left;         break;
                case CUI_CURSOR_RESIZE_RIGHT:        cursor = cursor_theme->wayland_cursor_resize_right;        break;
                case CUI_CURSOR_RESIZE_TOP_LEFT:     cursor = cursor_theme->wayland_cursor_resize_top_left;     break;
                case CUI_CURSOR_RESIZE_TOP_RIGHT:    cursor = cursor_theme->wayland_cursor_resize_top_right;    break;
                case CUI_CURSOR_RESIZE_BOTTOM_LEFT:  cursor = cursor_theme->wayland_cursor_resize_bottom_left;  break;
                case CUI_CURSOR_RESIZE_BOTTOM_RIGHT: cursor = cursor_theme->wayland_cursor_resize_bottom_right; break;
            }

            struct wl_cursor_image *image = cursor->images[0];
            struct wl_buffer *cursor_buffer = wl_cursor_image_get_buffer(image);

            wl_surface_set_buffer_scale(_cui_context.wayland_cursor_surface, window->wayland_cursor_buffer_scale);

            int32_t hotspot_x = image->hotspot_x / window->wayland_cursor_buffer_scale;
            int32_t hotspot_y = image->hotspot_y / window->wayland_cursor_buffer_scale;

            wl_pointer_set_cursor(_cui_context.wayland_pointer, _cui_context.wayland_pointer_serial,
                                  _cui_context.wayland_cursor_surface, hotspot_x, hotspot_y);

            wl_surface_attach(_cui_context.wayland_cursor_surface, cursor_buffer, 0, 0);
            wl_surface_damage(_cui_context.wayland_cursor_surface, 0, 0, image->width, image->height);
            wl_surface_commit(_cui_context.wayland_cursor_surface);
        }

        _cui_context.current_cursor = target_cursor;
    }
}

static inline void
_cui_wayland_set_platform_cursor(CuiWindow *window, CuiCursorType type)
{
    _cui_context.wayland_platform_cursor = type;
    _cui_wayland_update_cursor(window);
}

static inline enum xdg_toplevel_resize_edge
_cui_wayland_get_toplevel_resize_edge(CuiWindow *window, CuiPoint point)
{
    if (cui_rect_has_point_inside(window->wayland_input_rect, point))
    {
        if (point.x < window->wayland_window_rect.min.x)
        {
            if (point.y < (window->wayland_window_rect.min.y + _CUI_WAYLAND_CORNER_MARGIN))
            {
                return XDG_TOPLEVEL_RESIZE_EDGE_TOP_LEFT;
            }
            else if (point.y >= (window->wayland_window_rect.max.y - _CUI_WAYLAND_CORNER_MARGIN))
            {
                return XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_LEFT;
            }
            else
            {
                return XDG_TOPLEVEL_RESIZE_EDGE_LEFT;
            }
        }
        else if (point.x >= window->wayland_window_rect.max.x)
        {
            if (point.y < (window->wayland_window_rect.min.y + _CUI_WAYLAND_CORNER_MARGIN))
            {
                return XDG_TOPLEVEL_RESIZE_EDGE_TOP_RIGHT;
            }
            else if (point.y >= (window->wayland_window_rect.max.y - _CUI_WAYLAND_CORNER_MARGIN))
            {
                return XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_RIGHT;
            }
            else
            {
                return XDG_TOPLEVEL_RESIZE_EDGE_RIGHT;
            }
        }
        else if (point.y < window->wayland_window_rect.min.y)
        {
            if (point.x < (window->wayland_window_rect.min.x + _CUI_WAYLAND_CORNER_MARGIN))
            {
                return XDG_TOPLEVEL_RESIZE_EDGE_TOP_LEFT;
            }
            else if (point.x >= (window->wayland_window_rect.max.x - _CUI_WAYLAND_CORNER_MARGIN))
            {
                return XDG_TOPLEVEL_RESIZE_EDGE_TOP_RIGHT;
            }
            else
            {
                return XDG_TOPLEVEL_RESIZE_EDGE_TOP;
            }
        }
        else if (point.y >= window->wayland_window_rect.max.y)
        {
            if (point.x < (window->wayland_window_rect.min.x + _CUI_WAYLAND_CORNER_MARGIN))
            {
                return XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_LEFT;
            }
            else if (point.x >= (window->wayland_window_rect.max.x - _CUI_WAYLAND_CORNER_MARGIN))
            {
                return XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_RIGHT;
            }
            else
            {
                return XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM;
            }
        }
    }

    return XDG_TOPLEVEL_RESIZE_EDGE_NONE;
}

static void
_cui_wayland_update_platform_cursor(CuiWindow *window, CuiPoint point)
{
    if (window->base.creation_flags & CUI_WINDOW_CREATION_FLAG_NOT_USER_RESIZABLE)
    {
        _cui_wayland_set_platform_cursor(window, (CuiCursorType) 0);
    }
    else
    {
        enum xdg_toplevel_resize_edge resize_edge = _cui_wayland_get_toplevel_resize_edge(window, point);

        switch (resize_edge)
        {
            case XDG_TOPLEVEL_RESIZE_EDGE_NONE:         _cui_wayland_set_platform_cursor(window, (CuiCursorType) 0);              break;
            case XDG_TOPLEVEL_RESIZE_EDGE_TOP:          _cui_wayland_set_platform_cursor(window, CUI_CURSOR_RESIZE_TOP);          break;
            case XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM:       _cui_wayland_set_platform_cursor(window, CUI_CURSOR_RESIZE_BOTTOM);       break;
            case XDG_TOPLEVEL_RESIZE_EDGE_LEFT:         _cui_wayland_set_platform_cursor(window, CUI_CURSOR_RESIZE_LEFT);         break;
            case XDG_TOPLEVEL_RESIZE_EDGE_TOP_LEFT:     _cui_wayland_set_platform_cursor(window, CUI_CURSOR_RESIZE_TOP_LEFT);     break;
            case XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_LEFT:  _cui_wayland_set_platform_cursor(window, CUI_CURSOR_RESIZE_BOTTOM_LEFT);  break;
            case XDG_TOPLEVEL_RESIZE_EDGE_RIGHT:        _cui_wayland_set_platform_cursor(window, CUI_CURSOR_RESIZE_RIGHT);        break;
            case XDG_TOPLEVEL_RESIZE_EDGE_TOP_RIGHT:    _cui_wayland_set_platform_cursor(window, CUI_CURSOR_RESIZE_TOP_RIGHT);    break;
            case XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_RIGHT: _cui_wayland_set_platform_cursor(window, CUI_CURSOR_RESIZE_BOTTOM_RIGHT); break;
        }
    }
}

static void
_cui_wayland_handle_touch_down(void *data, struct wl_touch *touch, uint32_t serial, uint32_t time,
                               struct wl_surface *surface, int32_t id, wl_fixed_t x, wl_fixed_t y)
{
    (void) data;
    (void) touch;
    (void) serial;
    (void) time;

    CuiWindow *window =_cui_wayland_get_window_from_surface(surface);
    CuiAssert(window);

    // TODO: this should all be handled in handle_touch_frame

    // TODO: handle window decoration

    CuiPoint touch_position = cui_make_point(wl_fixed_to_int(wl_fixed_from_double((double) window->backbuffer_scale * wl_fixed_to_double(x))),
                                             wl_fixed_to_int(wl_fixed_from_double((double) window->backbuffer_scale * wl_fixed_to_double(y))));

    window->base.event.mouse.x = _cui_context.wayland_application_mouse_position.x;
    window->base.event.mouse.y = _cui_context.wayland_application_mouse_position.y;

    window->base.event.pointer.index = id;
    window->base.event.pointer.position = touch_position;
    cui_window_handle_event(window, CUI_EVENT_TYPE_POINTER_DOWN);
}

static void
_cui_wayland_handle_touch_up(void *data, struct wl_touch *touch, uint32_t serial, uint32_t time, int32_t id)
{
    (void) data;
    (void) touch;
    (void) serial;
    (void) time;
    (void) id;
}

static void
_cui_wayland_handle_touch_motion(void *data, struct wl_touch *touch, uint32_t time, int32_t id, wl_fixed_t x, wl_fixed_t y)
{
    (void) data;
    (void) touch;
    (void) time;
    (void) id;
    (void) x;
    (void) y;
}

static void
_cui_wayland_handle_touch_frame(void *data, struct wl_touch *touch)
{
    (void) data;
    (void) touch;
}

static void
_cui_wayland_handle_touch_cancel(void *data, struct wl_touch *touch)
{
    (void) data;
    (void) touch;
}

static void
_cui_wayland_handle_touch_shape(void *data, struct wl_touch *touch, int32_t id, wl_fixed_t major, wl_fixed_t minor)
{
    (void) data;
    (void) touch;
    (void) id;
    (void) major;
    (void) minor;
}

static void
_cui_wayland_handle_touch_orientation(void *data, struct wl_touch *touch, int32_t id, wl_fixed_t orientation)
{
    (void) data;
    (void) touch;
    (void) id;
    (void) orientation;
}

static const struct wl_touch_listener _cui_wayland_touch_listener = {
    .down        = _cui_wayland_handle_touch_down,
    .up          = _cui_wayland_handle_touch_up,
    .motion      = _cui_wayland_handle_touch_motion,
    .frame       = _cui_wayland_handle_touch_frame,
    .cancel      = _cui_wayland_handle_touch_cancel,
    .shape       = _cui_wayland_handle_touch_shape,
    .orientation = _cui_wayland_handle_touch_orientation,
};

static void
_cui_handle_pointer_enter(CuiWindow *window, uint32_t serial, wl_fixed_t x, wl_fixed_t y)
{
    _cui_context.wayland_pointer_serial = serial;
    _cui_context.wayland_window_under_cursor = window;

    _cui_context.wayland_platform_mouse_position    = cui_make_point(wl_fixed_to_int(x), wl_fixed_to_int(y));
    _cui_context.wayland_application_mouse_position = cui_make_point(wl_fixed_to_int(wl_fixed_from_double((double) window->backbuffer_scale * wl_fixed_to_double(x))),
                                                                     wl_fixed_to_int(wl_fixed_from_double((double) window->backbuffer_scale * wl_fixed_to_double(y))));

    _cui_wayland_update_platform_cursor(window, _cui_context.wayland_platform_mouse_position);

    window->base.event.mouse.x = _cui_context.wayland_application_mouse_position.x;
    window->base.event.mouse.y = _cui_context.wayland_application_mouse_position.y;

    cui_window_handle_event(window, CUI_EVENT_TYPE_MOUSE_MOVE);
}

static void
_cui_handle_pointer_leave(CuiWindow *window)
{
    CuiAssert(_cui_context.wayland_window_under_cursor == window);

    _cui_context.current_cursor = (CuiCursorType) 0;
    _cui_context.wayland_pointer_serial = 0;
    _cui_context.wayland_window_under_cursor = 0;

    if (window)
    {
        cui_window_handle_event(window, CUI_EVENT_TYPE_MOUSE_LEAVE);
    }
}

static void
_cui_handle_pointer_motion(CuiWindow *window, wl_fixed_t x, wl_fixed_t y)
{
    _cui_context.wayland_platform_mouse_position    = cui_make_point(wl_fixed_to_int(x), wl_fixed_to_int(y));
    _cui_context.wayland_application_mouse_position = cui_make_point(wl_fixed_to_int(wl_fixed_from_double((double) window->backbuffer_scale * wl_fixed_to_double(x))),
                                                                     wl_fixed_to_int(wl_fixed_from_double((double) window->backbuffer_scale * wl_fixed_to_double(y))));

    _cui_wayland_update_platform_cursor(window, _cui_context.wayland_platform_mouse_position);

    window->base.event.mouse.x = _cui_context.wayland_application_mouse_position.x;
    window->base.event.mouse.y = _cui_context.wayland_application_mouse_position.y;

    cui_window_handle_event(window, CUI_EVENT_TYPE_MOUSE_MOVE);
}

static void
_cui_handle_pointer_button(CuiWindow *window, uint32_t serial, uint32_t time, uint32_t button, uint32_t state)
{
    switch (state)
    {
        case WL_POINTER_BUTTON_STATE_PRESSED:
        {
            switch (button)
            {
                case BTN_LEFT:
                {
                    enum xdg_toplevel_resize_edge resize_edge = _cui_wayland_get_toplevel_resize_edge(window, _cui_context.wayland_platform_mouse_position);

                    if (((int64_t) time - window->last_left_click_time) <= (int64_t) _cui_context.double_click_time)
                    {
                        if (resize_edge == XDG_TOPLEVEL_RESIZE_EDGE_NONE)
                        {
                            window->pointer_button_mask |= CUI_WAYLAND_POINTER_BUTTON_LEFT;

                            if (window->title && (window->base.hovered_widget == window->title))
                            {
                                if (cui_window_is_maximized(window))
                                {
                                    xdg_toplevel_unset_maximized(window->wayland_xdg_toplevel);
                                }
                                else if (!(window->base.creation_flags & CUI_WINDOW_CREATION_FLAG_NOT_USER_RESIZABLE))
                                {
                                    xdg_toplevel_set_maximized(window->wayland_xdg_toplevel);
                                }
                            }
                            else
                            {
                                window->base.event.mouse.x = _cui_context.wayland_application_mouse_position.x;
                                window->base.event.mouse.y = _cui_context.wayland_application_mouse_position.y;

                                cui_window_handle_event(window, CUI_EVENT_TYPE_DOUBLE_CLICK);
                            }
                        }
                        else
                        {
                            if (window->base.creation_flags & CUI_WINDOW_CREATION_FLAG_NOT_USER_RESIZABLE)
                            {
                                window->pointer_button_mask |= CUI_WAYLAND_POINTER_BUTTON_LEFT;
                            }
                            else
                            {
                                xdg_toplevel_resize(window->wayland_xdg_toplevel, _cui_context.wayland_seat, serial, resize_edge);
                            }
                        }

                        window->last_left_click_time = INT16_MIN;
                    }
                    else
                    {
                        if (resize_edge == XDG_TOPLEVEL_RESIZE_EDGE_NONE)
                        {
                            if (window->title && (window->base.hovered_widget == window->title))
                            {
                                xdg_toplevel_move(window->wayland_xdg_toplevel, _cui_context.wayland_seat, serial);
                            }
                            else
                            {
                                window->pointer_button_mask |= CUI_WAYLAND_POINTER_BUTTON_LEFT;

                                window->base.event.mouse.x = _cui_context.wayland_application_mouse_position.x;
                                window->base.event.mouse.y = _cui_context.wayland_application_mouse_position.y;

                                cui_window_handle_event(window, CUI_EVENT_TYPE_LEFT_DOWN);
                            }
                        }
                        else
                        {
                            if (window->base.creation_flags & CUI_WINDOW_CREATION_FLAG_NOT_USER_RESIZABLE)
                            {
                                window->pointer_button_mask |= CUI_WAYLAND_POINTER_BUTTON_LEFT;
                            }
                            else
                            {
                                xdg_toplevel_resize(window->wayland_xdg_toplevel, _cui_context.wayland_seat, serial, resize_edge);
                            }
                        }

                        window->last_left_click_time = (int64_t) time;
                    }
                } break;

                case BTN_MIDDLE:
                {
                    window->pointer_button_mask |= CUI_WAYLAND_POINTER_BUTTON_MIDDLE;
                } break;

                case BTN_RIGHT:
                {
                    window->pointer_button_mask |= CUI_WAYLAND_POINTER_BUTTON_RIGHT;

                    enum xdg_toplevel_resize_edge resize_edge = _cui_wayland_get_toplevel_resize_edge(window, _cui_context.wayland_platform_mouse_position);

                    if (resize_edge == XDG_TOPLEVEL_RESIZE_EDGE_NONE)
                    {
                        if (window->title && (window->base.hovered_widget == window->title))
                        {
                            xdg_toplevel_show_window_menu(window->wayland_xdg_toplevel, _cui_context.wayland_seat, serial,
                                                          _cui_context.wayland_platform_mouse_position.x,
                                                          _cui_context.wayland_platform_mouse_position.y);
                        }
                        else
                        {
#if 0
                            window->base.event.mouse.x = _cui_context.wayland_application_mouse_position.x;
                            window->base.event.mouse.y = _cui_context.wayland_application_mouse_position.y;

                            cui_window_handle_event(window, CUI_EVENT_TYPE_RIGHT_DOWN);
#endif
                        }
                    }
                } break;
            }
        } break;

        case WL_POINTER_BUTTON_STATE_RELEASED:
        {
            switch (button)
            {
                case BTN_LEFT:
                {
                    window->base.event.mouse.x = _cui_context.wayland_application_mouse_position.x;
                    window->base.event.mouse.y = _cui_context.wayland_application_mouse_position.y;

                    cui_window_handle_event(window, CUI_EVENT_TYPE_LEFT_UP);

                    window->pointer_button_mask &= ~CUI_WAYLAND_POINTER_BUTTON_LEFT;
                    _cui_wayland_update_platform_cursor(window, _cui_context.wayland_platform_mouse_position);
                } break;

                case BTN_MIDDLE:
                {
                    window->pointer_button_mask &= ~CUI_WAYLAND_POINTER_BUTTON_MIDDLE;
                    _cui_wayland_update_platform_cursor(window, _cui_context.wayland_platform_mouse_position);
                } break;

                case BTN_RIGHT:
                {
#if 0
                    window->base.event.mouse.x = _cui_context.wayland_application_mouse_position.x;
                    window->base.event.mouse.y = _cui_context.wayland_application_mouse_position.y;

                    cui_window_handle_event(window, CUI_EVENT_TYPE_RIGHT_UP);
#endif
                    window->pointer_button_mask &= ~CUI_WAYLAND_POINTER_BUTTON_RIGHT;
                    _cui_wayland_update_platform_cursor(window, _cui_context.wayland_platform_mouse_position);
                } break;
            }
        } break;
    }
}

static void
_cui_handle_pointer_axis(CuiWindow *window, float dx, float dy)
{
    window->base.event.wheel.is_precise_scrolling = true;
    window->base.event.wheel.dx = dx;
    window->base.event.wheel.dy = dy;
    cui_window_handle_event(window, CUI_EVENT_TYPE_MOUSE_WHEEL);
}

static void
_cui_handle_pointer_axis_discrete(CuiWindow *window, int32_t dx, int32_t dy)
{
    window->base.event.wheel.is_precise_scrolling = false;
    window->base.event.wheel.dx = (float) dx;
    window->base.event.wheel.dy = (float) dy;
    cui_window_handle_event(window, CUI_EVENT_TYPE_MOUSE_WHEEL);
}

static void
_cui_wayland_handle_pointer_enter(void *data, struct wl_pointer *pointer, uint32_t serial, struct wl_surface *surface, wl_fixed_t x, wl_fixed_t y)
{
    (void) data;
    (void) pointer;

    CuiWindow *window =_cui_wayland_get_window_from_surface(surface);
    CuiAssert(window);

    if (_cui_context.wayland_seat_version == 4)
    {
        _cui_handle_pointer_enter(window, serial, x, y);
    }
    else
    {
        CuiAssert(_cui_context.wayland_seat_version == 5);

        if (_cui_context.wayland_pointer_event.flags)
        {
            printf("error (pointer_enter): event flags should not be set.\n");
        }

        _cui_context.wayland_pointer_event.flags  = CUI_WAYLAND_POINTER_EVENT_ENTER;
        _cui_context.wayland_pointer_event.serial = serial;
        _cui_context.wayland_pointer_event.x      = x;
        _cui_context.wayland_pointer_event.y      = y;
        _cui_context.wayland_pointer_event.window = window;
    }
}

static void
_cui_wayland_handle_pointer_leave(void *data, struct wl_pointer *pointer, uint32_t serial, struct wl_surface *surface)
{
    (void) data;
    (void) pointer;
    (void) serial;

    CuiWindow *window =_cui_wayland_get_window_from_surface(surface);

    if (_cui_context.wayland_seat_version == 4)
    {
        _cui_handle_pointer_leave(window);
    }
    else
    {
        CuiAssert(_cui_context.wayland_seat_version == 5);

        if (_cui_context.wayland_pointer_event.flags & ~CUI_WAYLAND_POINTER_EVENT_BUTTON)
        {
            printf("error (pointer_leave): event flags should not be set.\n");
        }

        _cui_context.wayland_pointer_event.flags |= CUI_WAYLAND_POINTER_EVENT_LEAVE;
        _cui_context.wayland_pointer_event.window = window;
    }
}

static void
_cui_wayland_handle_pointer_motion(void *data, struct wl_pointer *pointer, uint32_t time, wl_fixed_t x, wl_fixed_t y)
{
    (void) data;
    (void) pointer;
    (void) time;

    CuiWindow *window = _cui_context.wayland_window_under_cursor;
    CuiAssert(window);

    if (_cui_context.wayland_seat_version == 4)
    {
        _cui_handle_pointer_motion(window, x, y);
    }
    else
    {
        CuiAssert(_cui_context.wayland_seat_version == 5);

        if (_cui_context.wayland_pointer_event.flags)
        {
            printf("error (pointer_motion): event flags should not be set.\n");
        }

        _cui_context.wayland_pointer_event.flags  = CUI_WAYLAND_POINTER_EVENT_MOTION;
        _cui_context.wayland_pointer_event.x      = x;
        _cui_context.wayland_pointer_event.y      = y;
        _cui_context.wayland_pointer_event.window = window;
    }
}

static void
_cui_wayland_handle_pointer_button(void *data, struct wl_pointer *pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state)
{
    (void) data;
    (void) pointer;

    CuiWindow *window = _cui_context.wayland_window_under_cursor;
    CuiAssert(window);

    if (_cui_context.wayland_seat_version == 4)
    {
        _cui_handle_pointer_button(window, serial, time, button, state);
    }
    else
    {
        CuiAssert(_cui_context.wayland_seat_version == 5);

        if (_cui_context.wayland_pointer_event.flags)
        {
            printf("error (pointer_button): event flags should not be set.\n");
        }

        _cui_context.wayland_pointer_event.flags  = CUI_WAYLAND_POINTER_EVENT_BUTTON;
        _cui_context.wayland_pointer_event.serial = serial;
        _cui_context.wayland_pointer_event.time   = time;
        _cui_context.wayland_pointer_event.button = button;
        _cui_context.wayland_pointer_event.state  = state;
        _cui_context.wayland_pointer_event.window = window;
    }
}

static void
_cui_wayland_handle_pointer_axis(void *data, struct wl_pointer *pointer, uint32_t time, uint32_t axis, wl_fixed_t value)
{
    (void) data;
    (void) pointer;
    (void) time;

    CuiWindow *window = _cui_context.wayland_window_under_cursor;
    CuiAssert(window);

    float delta = -((float) wl_fixed_to_double(value) * window->backbuffer_scale);

    if (_cui_context.wayland_seat_version == 4)
    {
        switch (axis)
        {
            case WL_POINTER_AXIS_VERTICAL_SCROLL:
            {
                _cui_handle_pointer_axis(window, 0.0f, delta);
            } break;

            case WL_POINTER_AXIS_HORIZONTAL_SCROLL:
            {
                _cui_handle_pointer_axis(window, delta, 0.0f);
            } break;
        }
    }
    else
    {
        CuiAssert(_cui_context.wayland_seat_version == 5);

        if (_cui_context.wayland_pointer_event.flags & ~(CUI_WAYLAND_POINTER_EVENT_AXIS | CUI_WAYLAND_POINTER_EVENT_AXIS_DISCRETE))
        {
            printf("error (pointer_axis): event flags should not be set.\n");
        }

        switch (axis)
        {
            case WL_POINTER_AXIS_VERTICAL_SCROLL:
            {
                _cui_context.wayland_pointer_event.axis_y += delta;
            } break;

            case WL_POINTER_AXIS_HORIZONTAL_SCROLL:
            {
                _cui_context.wayland_pointer_event.axis_x += delta;
            } break;
        }

        _cui_context.wayland_pointer_event.flags |= CUI_WAYLAND_POINTER_EVENT_AXIS;
        _cui_context.wayland_pointer_event.window = window;
    }
}

static void
_cui_wayland_handle_pointer_axis_source(void *data, struct wl_pointer *pointer, uint32_t axis_source)
{
    (void) data;
    (void) pointer;

    CuiAssert(_cui_context.wayland_seat_version == 5);

    if (_cui_context.wayland_pointer_event.flags & ~(CUI_WAYLAND_POINTER_EVENT_AXIS | CUI_WAYLAND_POINTER_EVENT_AXIS_DISCRETE))
    {
        printf("error (pointer_axis_source): event flags should not be set.\n");
    }

    _cui_context.wayland_pointer_event.axis_source = axis_source;
}

static void
_cui_wayland_handle_pointer_axis_stop(void *data, struct wl_pointer *pointer, uint32_t time, uint32_t axis)
{
    (void) data;
    (void) pointer;
    (void) time;
    (void) axis;
}

static void
_cui_wayland_handle_pointer_axis_discrete(void *data, struct wl_pointer *pointer, uint32_t axis, int32_t discrete)
{
    (void) data;
    (void) pointer;

    CuiWindow *window = _cui_context.wayland_window_under_cursor;
    CuiAssert(window);

    CuiAssert(_cui_context.wayland_seat_version == 5);

    if (_cui_context.wayland_pointer_event.flags & ~(CUI_WAYLAND_POINTER_EVENT_AXIS | CUI_WAYLAND_POINTER_EVENT_AXIS_DISCRETE))
    {
        printf("error (pointer_axis_discrete): event flags should not be set.\n");
    }

    switch (axis)
    {
        case WL_POINTER_AXIS_VERTICAL_SCROLL:
        {
            _cui_context.wayland_pointer_event.axis_discrete_y -= 3 * discrete;
        } break;

        case WL_POINTER_AXIS_HORIZONTAL_SCROLL:
        {
            _cui_context.wayland_pointer_event.axis_discrete_x -= 3 * discrete;
        } break;
    }

    _cui_context.wayland_pointer_event.flags |= CUI_WAYLAND_POINTER_EVENT_AXIS_DISCRETE;
    _cui_context.wayland_pointer_event.window = window;
}

static void
_cui_wayland_handle_pointer_frame(void *data, struct wl_pointer *pointer)
{
    (void) data;
    (void) pointer;

    if (_cui_context.wayland_pointer_event.flags & CUI_WAYLAND_POINTER_EVENT_ENTER)
    {
        _cui_handle_pointer_enter(_cui_context.wayland_pointer_event.window,
                                  _cui_context.wayland_pointer_event.serial,
                                  _cui_context.wayland_pointer_event.x,
                                  _cui_context.wayland_pointer_event.y);
    }
    else if (_cui_context.wayland_pointer_event.flags & CUI_WAYLAND_POINTER_EVENT_MOTION)
    {
        _cui_handle_pointer_motion(_cui_context.wayland_pointer_event.window,
                                   _cui_context.wayland_pointer_event.x,
                                   _cui_context.wayland_pointer_event.y);
    }
    else if (_cui_context.wayland_pointer_event.flags & (CUI_WAYLAND_POINTER_EVENT_BUTTON | CUI_WAYLAND_POINTER_EVENT_LEAVE))
    {
        if (_cui_context.wayland_pointer_event.flags & CUI_WAYLAND_POINTER_EVENT_BUTTON)
        {
            _cui_handle_pointer_button(_cui_context.wayland_pointer_event.window,
                                       _cui_context.wayland_pointer_event.serial,
                                       _cui_context.wayland_pointer_event.time,
                                       _cui_context.wayland_pointer_event.button,
                                       _cui_context.wayland_pointer_event.state);
        }

        if (_cui_context.wayland_pointer_event.flags & CUI_WAYLAND_POINTER_EVENT_LEAVE)
        {
            _cui_handle_pointer_leave(_cui_context.wayland_pointer_event.window);
        }
    }
    else if (_cui_context.wayland_pointer_event.flags & (CUI_WAYLAND_POINTER_EVENT_AXIS | CUI_WAYLAND_POINTER_EVENT_AXIS_DISCRETE))
    {
        switch (_cui_context.wayland_pointer_event.axis_source)
        {
            case WL_POINTER_AXIS_SOURCE_WHEEL:
            {
                if (_cui_context.wayland_pointer_event.flags & CUI_WAYLAND_POINTER_EVENT_AXIS_DISCRETE)
                {
                    _cui_handle_pointer_axis_discrete(_cui_context.wayland_pointer_event.window,
                                                      _cui_context.wayland_pointer_event.axis_discrete_x,
                                                      _cui_context.wayland_pointer_event.axis_discrete_y);
                }
                else
                {
                    _cui_handle_pointer_axis(_cui_context.wayland_pointer_event.window,
                                             _cui_context.wayland_pointer_event.axis_x,
                                             _cui_context.wayland_pointer_event.axis_y);
                }
            } break;

            case WL_POINTER_AXIS_SOURCE_FINGER:
            {
                _cui_handle_pointer_axis(_cui_context.wayland_pointer_event.window,
                                         _cui_context.wayland_pointer_event.axis_x,
                                         _cui_context.wayland_pointer_event.axis_y);
            } break;

            case WL_POINTER_AXIS_SOURCE_CONTINUOUS:
            {
                _cui_handle_pointer_axis(_cui_context.wayland_pointer_event.window,
                                         _cui_context.wayland_pointer_event.axis_x,
                                         _cui_context.wayland_pointer_event.axis_y);
            } break;

            case WL_POINTER_AXIS_SOURCE_WHEEL_TILT:
            {
                CuiAssert(!"This should not appear as we're using version 4 or 5.");
            } break;

            default:
            {
                CuiAssert(_cui_context.wayland_pointer_event.axis_source == 0xFFFFFFFF);

                if (_cui_context.wayland_pointer_event.flags & CUI_WAYLAND_POINTER_EVENT_AXIS_DISCRETE)
                {
                    _cui_handle_pointer_axis_discrete(_cui_context.wayland_pointer_event.window,
                                                      _cui_context.wayland_pointer_event.axis_discrete_x,
                                                      _cui_context.wayland_pointer_event.axis_discrete_y);
                }
                else
                {
                    _cui_handle_pointer_axis(_cui_context.wayland_pointer_event.window,
                                             _cui_context.wayland_pointer_event.axis_x,
                                             _cui_context.wayland_pointer_event.axis_y);
                }
            } break;
        }
    }

    CuiClearStruct(_cui_context.wayland_pointer_event);
    _cui_context.wayland_pointer_event.axis_source = 0xFFFFFFFF;
}

static const struct wl_pointer_listener _cui_wayland_pointer_listener = {
    .enter         = _cui_wayland_handle_pointer_enter,
    .leave         = _cui_wayland_handle_pointer_leave,
    .motion        = _cui_wayland_handle_pointer_motion,
    .button        = _cui_wayland_handle_pointer_button,
    .axis          = _cui_wayland_handle_pointer_axis,
    .frame         = _cui_wayland_handle_pointer_frame,
    .axis_source   = _cui_wayland_handle_pointer_axis_source,
    .axis_stop     = _cui_wayland_handle_pointer_axis_stop,
    .axis_discrete = _cui_wayland_handle_pointer_axis_discrete,
};

static inline void
_cui_wayland_handle_key_down(CuiWindow *window, uint32_t keycode)
{
    char buffer[32];
    xkb_keysym_t sym = xkb_state_key_get_one_sym(_cui_context.xkb_state, keycode);

#define _CUI_KEY_DOWN_EVENT(key_id)                                                                                                             \
    window->base.event.key.codepoint       = (key_id);                                                                                          \
    window->base.event.key.alt_is_down     = xkb_state_mod_name_is_active(_cui_context.xkb_state, XKB_MOD_NAME_ALT, XKB_STATE_MODS_EFFECTIVE);  \
    window->base.event.key.ctrl_is_down    = xkb_state_mod_name_is_active(_cui_context.xkb_state, XKB_MOD_NAME_CTRL, XKB_STATE_MODS_EFFECTIVE); \
    window->base.event.key.shift_is_down   = xkb_state_mod_name_is_active(_cui_context.xkb_state, XKB_MOD_NAME_SHIFT, XKB_STATE_MODS_EFFECTIVE);\
    window->base.event.key.command_is_down = false;                                                                                             \
    cui_window_handle_event(window, CUI_EVENT_TYPE_KEY_DOWN);

    if ((xkb_compose_state_feed(_cui_context.xkb_compose_state, sym) == XKB_COMPOSE_FEED_IGNORED) ||
        (xkb_compose_state_get_status(_cui_context.xkb_compose_state) == XKB_COMPOSE_NOTHING))
    {

        switch (sym)
        {
            case XKB_KEY_BackSpace: { _CUI_KEY_DOWN_EVENT(CUI_KEY_BACKSPACE); } break;
            case XKB_KEY_Tab:       { _CUI_KEY_DOWN_EVENT(CUI_KEY_TAB);       } break;
            case XKB_KEY_Linefeed:  { _CUI_KEY_DOWN_EVENT(CUI_KEY_LINEFEED);  } break;
            case XKB_KEY_Return:    { _CUI_KEY_DOWN_EVENT(CUI_KEY_ENTER);     } break;
            case XKB_KEY_Escape:    { _CUI_KEY_DOWN_EVENT(CUI_KEY_ESCAPE);    } break;
            case XKB_KEY_Delete:    { _CUI_KEY_DOWN_EVENT(CUI_KEY_DELETE);    } break;
            case XKB_KEY_Up:        { _CUI_KEY_DOWN_EVENT(CUI_KEY_UP);        } break;
            case XKB_KEY_Down:      { _CUI_KEY_DOWN_EVENT(CUI_KEY_DOWN);      } break;
            case XKB_KEY_Left:      { _CUI_KEY_DOWN_EVENT(CUI_KEY_LEFT);      } break;
            case XKB_KEY_Right:     { _CUI_KEY_DOWN_EVENT(CUI_KEY_RIGHT);     } break;

            case XKB_KEY_F1:  case XKB_KEY_F2:  case XKB_KEY_F3:  case XKB_KEY_F4:
            case XKB_KEY_F5:  case XKB_KEY_F6:  case XKB_KEY_F7:  case XKB_KEY_F8:
            case XKB_KEY_F9:  case XKB_KEY_F10: case XKB_KEY_F11: case XKB_KEY_F12:
            {
#if CUI_FRAMEBUFFER_SCREENSHOT_ENABLED
                if (sym == XKB_KEY_F2)
                {
                    window->take_screenshot = true;
                    window->base.needs_redraw = true;
                }
                else
#endif
                {
                    _CUI_KEY_DOWN_EVENT(CUI_KEY_F1 + (sym - XKB_KEY_F1));
                }
            } break;

            default:
            {
                if (sym >= 32)
                {
                    bool ctrl_is_down = xkb_state_mod_name_is_active(_cui_context.xkb_state, XKB_MOD_NAME_CTRL, XKB_STATE_MODS_EFFECTIVE) ? true : false;

                    if (ctrl_is_down && (sym < 127))
                    {
                        _CUI_KEY_DOWN_EVENT(sym);
                    }
                    else
                    {
                        int ret = xkb_state_key_get_utf8(_cui_context.xkb_state, keycode, buffer, sizeof(buffer));

                        if (ret)
                        {
                            int64_t i = 0;
                            CuiString str = cui_make_string(buffer, ret);

                            while (i < str.count)
                            {
                                CuiUnicodeResult utf8 = cui_utf8_decode(str, i);

                                _CUI_KEY_DOWN_EVENT(utf8.codepoint);

                                i += utf8.byte_count;
                            }
                        }
                    }
                }
            } break;
        }
    }
    else if (xkb_compose_state_get_status(_cui_context.xkb_compose_state) == XKB_COMPOSE_COMPOSED)
    {
        int ret = xkb_compose_state_get_utf8(_cui_context.xkb_compose_state, buffer, sizeof(buffer));

        if (ret)
        {
            int64_t i = 0;
            CuiString str = cui_make_string(buffer, ret);

            while (i < str.count)
            {
                CuiUnicodeResult utf8 = cui_utf8_decode(str, i);

                _CUI_KEY_DOWN_EVENT(utf8.codepoint);

                i += utf8.byte_count;
            }
        }
    }

#undef _CUI_KEY_DOWN_EVENT

}

static void
_cui_wayland_handle_keyboard_keymap(void *data, struct wl_keyboard *keyboard, uint32_t format, int32_t fd, uint32_t size)
{
    (void) data;
    (void) keyboard;

    CuiAssert(format == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1);

    xkb_compose_state_unref(_cui_context.xkb_compose_state);
    xkb_compose_table_unref(_cui_context.xkb_compose_table);

    xkb_state_unref(_cui_context.xkb_state);
    xkb_keymap_unref(_cui_context.xkb_keymap);

    char *keymap = (char *) mmap(0, size, PROT_READ, MAP_PRIVATE, fd, 0);

    CuiAssert(keymap != MAP_FAILED);

    _cui_context.xkb_keymap = xkb_keymap_new_from_string(_cui_context.xkb_context, keymap, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
    _cui_context.xkb_state = xkb_state_new(_cui_context.xkb_keymap);

    munmap(keymap, size);
    close(fd);

    _cui_context.xkb_compose_table = xkb_compose_table_new_from_locale(_cui_context.xkb_context, setlocale(LC_CTYPE, 0), XKB_COMPOSE_COMPILE_NO_FLAGS);
    _cui_context.xkb_compose_state = xkb_compose_state_new(_cui_context.xkb_compose_table, XKB_COMPOSE_STATE_NO_FLAGS);
}

static void
_cui_wayland_handle_keyboard_enter(void *data, struct wl_keyboard *keyboard, uint32_t serial,
                                   struct wl_surface *surface, struct wl_array *keys)
{
    (void) data;
    (void) keyboard;
    (void) serial;

    CuiWindow *window =_cui_wayland_get_window_from_surface(surface);
    CuiAssert(window);

    _cui_context.wayland_keyboard_focused_window = window;

    // TODO: handle keys
    (void) keys;
}

static void
_cui_wayland_handle_keyboard_leave(void *data, struct wl_keyboard *keyboard, uint32_t serial, struct wl_surface *surface)
{
    (void) data;
    (void) keyboard;
    (void) serial;

#if CUI_DEBUG_BUILD
    CuiWindow *window =_cui_wayland_get_window_from_surface(surface);
    CuiAssert(_cui_context.wayland_keyboard_focused_window == window);
#else
    (void) surface;
#endif

    _cui_context.wayland_keyboard_focused_window = 0;
}

static void
_cui_wayland_handle_keyboard_key(void *data, struct wl_keyboard *keyboard, uint32_t serial,
                                 uint32_t time, uint32_t key, uint32_t state)
{
    (void) data;
    (void) keyboard;
    (void) serial;
    (void) time;

    CuiAssert(_cui_context.wayland_keyboard_focused_window);
    CuiWindow *window = _cui_context.wayland_keyboard_focused_window;

    uint32_t key_code = key + 8;

    if (state == WL_KEYBOARD_KEY_STATE_PRESSED)
    {
        _cui_wayland_handle_key_down(window, key_code);
    }
    else
    {
        CuiAssert(state == WL_KEYBOARD_KEY_STATE_RELEASED);
    }
}

static void
_cui_wayland_handle_keyboard_modifiers(void *data, struct wl_keyboard *keyboard, uint32_t serial, uint32_t mods_depressed,
                                       uint32_t mods_latched, uint32_t mods_locked, uint32_t group)
{
    (void) data;
    (void) keyboard;
    (void) serial;

    xkb_state_update_mask(_cui_context.xkb_state, mods_depressed, mods_latched, mods_locked, 0, 0, group);
}

static void
_cui_wayland_handle_keyboard_repeat_info(void *data, struct wl_keyboard *keyboard, int32_t rate, int32_t delay)
{
    (void) data;
    (void) keyboard;

    _cui_context.wayland_keyboard_repeat_period = 1000 / rate;
    _cui_context.wayland_keyboard_repeat_delay = delay;
}

static const struct wl_keyboard_listener _cui_wayland_keyboard_listener = {
    .keymap      = _cui_wayland_handle_keyboard_keymap,
    .enter       = _cui_wayland_handle_keyboard_enter,
    .leave       = _cui_wayland_handle_keyboard_leave,
    .key         = _cui_wayland_handle_keyboard_key,
    .modifiers   = _cui_wayland_handle_keyboard_modifiers,
    .repeat_info = _cui_wayland_handle_keyboard_repeat_info,
};

static void
_cui_wayland_handle_seat_capabilities(void *data, struct wl_seat *seat, uint32_t capabilities)
{
    (void) data;
    (void) seat;

    if (capabilities & WL_SEAT_CAPABILITY_TOUCH)
    {
        if (!_cui_context.wayland_touch)
        {
            _cui_context.wayland_touch = wl_seat_get_touch(seat);
            wl_touch_add_listener(_cui_context.wayland_touch, &_cui_wayland_touch_listener, 0);
        }
    }
    else
    {
        if (_cui_context.wayland_touch)
        {
            wl_touch_release(_cui_context.wayland_touch);
            _cui_context.wayland_touch = 0;
        }
    }

    if (capabilities & WL_SEAT_CAPABILITY_POINTER)
    {
        if (!_cui_context.wayland_pointer)
        {
            _cui_context.wayland_pointer = wl_seat_get_pointer(seat);
            wl_pointer_add_listener(_cui_context.wayland_pointer, &_cui_wayland_pointer_listener, 0);
        }
    }
    else
    {
        if (_cui_context.wayland_pointer)
        {
            wl_pointer_release(_cui_context.wayland_pointer);
            _cui_context.wayland_pointer = 0;
        }
    }

    if (capabilities & WL_SEAT_CAPABILITY_KEYBOARD)
    {
        if (!_cui_context.wayland_keyboard)
        {
            _cui_context.wayland_keyboard = wl_seat_get_keyboard(seat);
            wl_keyboard_add_listener(_cui_context.wayland_keyboard, &_cui_wayland_keyboard_listener, 0);
        }
    }
    else
    {
        if (_cui_context.wayland_keyboard)
        {
            wl_keyboard_release(_cui_context.wayland_keyboard);
            _cui_context.wayland_keyboard = 0;
        }
    }
}

static void
_cui_wayland_handle_seat_name(void *data, struct wl_seat *seat, const char *name)
{
    (void) data;
    (void) seat;
    (void) name;
}


static const struct wl_seat_listener _cui_wayland_seat_listener = {
    .capabilities = _cui_wayland_handle_seat_capabilities,
    .name         = _cui_wayland_handle_seat_name,
};

static void
_cui_wayland_handle_registry_add(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version)
{
    (void) data;

#if 0
    printf("interface: '%s', version: %u, name %u\n", interface, version, name);
#endif

    CuiString interface_name = CuiCString(interface);

    if (cui_string_equals(interface_name, CuiCString(wl_compositor_interface.name)))
    {
        _cui_context.wayland_compositor = (struct wl_compositor *) wl_registry_bind(registry, name, &wl_compositor_interface, 3);
    }
    else if (cui_string_equals(interface_name, CuiCString(xdg_wm_base_interface.name)))
    {
        if (version >= 1)
        {
            _cui_context.wayland_xdg_wm_base = (struct xdg_wm_base *) wl_registry_bind(registry, name, &xdg_wm_base_interface, (version < 2) ? version : 2);
        }
    }
    else if (cui_string_equals(interface_name, CuiCString(wl_shm_interface.name)))
    {
        _cui_context.wayland_shared_memory = (struct wl_shm *) wl_registry_bind(registry, name, &wl_shm_interface, 1);
    }
    else if (cui_string_equals(interface_name, CuiCString(wl_seat_interface.name)))
    {
        if (version >= 4)
        {
            _cui_context.wayland_seat_version = cui_min_uint32(version, 5);
            _cui_context.wayland_seat = (struct wl_seat *) wl_registry_bind(registry, name, &wl_seat_interface, _cui_context.wayland_seat_version);
            wl_seat_add_listener(_cui_context.wayland_seat, &_cui_wayland_seat_listener, 0);
        }
    }
    else if (cui_string_equals(interface_name, CuiCString(wl_output_interface.name)))
    {
        CuiWaylandMonitor *monitor = cui_array_append(_cui_context.wayland_monitors);

        monitor->id = name;
        monitor->scale = 1;
        monitor->pending_scale = 1;
        monitor->output = (struct wl_output *) wl_registry_bind(registry, name, &wl_output_interface, 2);

        wl_output_add_listener(monitor->output, &_cui_wayland_output_listener, 0);
    }
    else if (cui_string_equals(interface_name, CuiCString(wl_data_device_manager_interface.name)))
    {
        if (version >= 3)
        {
            _cui_context.wayland_data_device_manager = (struct wl_data_device_manager *) wl_registry_bind(registry, name, &wl_data_device_manager_interface, cui_min_uint32(version, 3));
        }
    }
    else if (cui_string_equals(interface_name, CuiCString(zxdg_decoration_manager_v1_interface.name)))
    {
        if (version >= 1)
        {
            _cui_context.wayland_xdg_decoration_manager = (struct zxdg_decoration_manager_v1 *) wl_registry_bind(registry, name, &zxdg_decoration_manager_v1_interface, 1);
        }
    }
}

static void
_cui_wayland_handle_registry_remove(void *data, struct wl_registry *registry, uint32_t name)
{
    (void) data;
    (void) registry;

    int32_t monitor_index = _cui_wayland_get_monitor_index_from_name(name);

    if (monitor_index >= 0)
    {
        CuiAssert(monitor_index < cui_array_count(_cui_context.wayland_monitors));

        CuiWaylandMonitor *monitor = _cui_context.wayland_monitors + monitor_index;
        wl_output_destroy(monitor->output);

        int32_t index = --_cui_array_header(_cui_context.wayland_monitors)->count;
        _cui_context.wayland_monitors[monitor_index] = _cui_context.wayland_monitors[index];

        // TODO: remove monitor from all windows
    }
}

static const struct wl_registry_listener _cui_wayland_registry_listener = {
    .global        = _cui_wayland_handle_registry_add,
    .global_remove = _cui_wayland_handle_registry_remove,
};

static void
_cui_wayland_handle_xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial)
{
    (void) data;
    xdg_wm_base_pong(xdg_wm_base, serial);
}

static const struct xdg_wm_base_listener _cui_wayland_xdg_wm_base_listener = {
    .ping = _cui_wayland_handle_xdg_wm_base_ping,
};

static void
_cui_wayland_handle_data_source_target(void *data, struct wl_data_source *data_source, const char *mime_type)
{
    (void) data;
    (void) data_source;
    (void) mime_type;

#if 0
    printf("data_source_target\n");
#endif
}

static void
_cui_wayland_handle_data_source_send(void *data, struct wl_data_source *data_source, const char *mime_type, int32_t fd)
{
    (void) data;
    (void) data_source;
    (void) mime_type;
    (void) fd;

#if 0
    printf("data_source_send\n");
#endif
}

static void
_cui_wayland_handle_data_source_cancelled(void *data, struct wl_data_source *data_source)
{
    (void) data;
    (void) data_source;

#if 0
    printf("data_source_cancelled\n");
#endif
}

static void
_cui_wayland_handle_data_source_dnd_drop_performed(void *data, struct wl_data_source *data_source)
{
    (void) data;
    (void) data_source;

#if 0
    printf("data_source_dnd_drop_performed\n");
#endif
}

static void
_cui_wayland_handle_data_source_dnd_finished(void *data, struct wl_data_source *data_source)
{
    (void) data;
    (void) data_source;

#if 0
    printf("data_source_dnd_finished\n");
#endif
}

static void
_cui_wayland_handle_data_source_action(void *data, struct wl_data_source *data_source, uint32_t dnd_action)
{
    (void) data;
    (void) data_source;
    (void) dnd_action;

#if 0
    printf("data_source_action\n");
#endif
}

static const struct wl_data_source_listener _cui_wayland_data_source_listener = {
    .target             = _cui_wayland_handle_data_source_target,
    .send               = _cui_wayland_handle_data_source_send,
    .cancelled          = _cui_wayland_handle_data_source_cancelled,
    .dnd_drop_performed = _cui_wayland_handle_data_source_dnd_drop_performed,
    .dnd_finished       = _cui_wayland_handle_data_source_dnd_finished,
    .action             = _cui_wayland_handle_data_source_action,
};

static void
_cui_wayland_handle_data_offer_offer(void *data, struct wl_data_offer *data_offer, const char *mime_type)
{
    (void) data;
    (void) data_offer;

#if 0
    printf("data_offer_offer(%s)\n", mime_type);
#else
    (void) mime_type;
#endif
}

static void
_cui_wayland_handle_data_offer_source_actions(void *data, struct wl_data_offer *data_offer, uint32_t source_actions)
{
    (void) data;
    (void) data_offer;
    (void) source_actions;
}

static void
_cui_wayland_handle_data_offer_action(void *data, struct wl_data_offer *data_offer, uint32_t dnd_action)
{
    (void) data;
    (void) data_offer;
    (void) dnd_action;
}

static const struct wl_data_offer_listener _cui_wayland_data_offer_listener = {
    .offer          = _cui_wayland_handle_data_offer_offer,
    .source_actions = _cui_wayland_handle_data_offer_source_actions,
    .action         = _cui_wayland_handle_data_offer_action,
};

static void
_cui_wayland_handle_data_device_data_offer(void *data, struct wl_data_device *data_device, struct wl_data_offer *data_offer)
{
    (void) data;
    (void) data_device;
    (void) data_offer;

    // TODO: what to do when there is already an offer
    _cui_context.wayland_data_offer = data_offer;

    wl_data_offer_add_listener(_cui_context.wayland_data_offer, &_cui_wayland_data_offer_listener, 0);
}

static void
_cui_wayland_handle_data_device_enter(void *data, struct wl_data_device *data_device, uint32_t serial,
                                      struct wl_surface *surface, wl_fixed_t x, wl_fixed_t y, struct wl_data_offer *data_offer)
{
    (void) data;
    (void) data_device;
    (void) serial;
    (void) surface;
    (void) x;
    (void) y;
    (void) data_offer;
}

static void
_cui_wayland_handle_data_device_leave(void *data, struct wl_data_device *data_device)
{
    (void) data;
    (void) data_device;
}

static void
_cui_wayland_handle_data_device_motion(void *data, struct wl_data_device *data_device, uint32_t time, wl_fixed_t x, wl_fixed_t y)
{
    (void) data;
    (void) data_device;
    (void) time;
    (void) x;
    (void) y;
}

static void
_cui_wayland_handle_data_device_drop(void *data, struct wl_data_device *data_device)
{
    (void) data;
    (void) data_device;
}

static void
_cui_wayland_handle_data_device_selection(void *data, struct wl_data_device *data_device, struct wl_data_offer *data_offer)
{
    (void) data;
    (void) data_device;
    (void) data_offer;
}

static const struct wl_data_device_listener _cui_wayland_data_device_listener = {
    .data_offer = _cui_wayland_handle_data_device_data_offer,
    .enter      = _cui_wayland_handle_data_device_enter,
    .leave      = _cui_wayland_handle_data_device_leave,
    .motion     = _cui_wayland_handle_data_device_motion,
    .drop       = _cui_wayland_handle_data_device_drop,
    .selection  = _cui_wayland_handle_data_device_selection,
};

static bool
_cui_initialize_wayland(void)
{
    _cui_context.wayland_display = wl_display_connect(0);

    if (!_cui_context.wayland_display)
    {
        return false;
    }

    _cui_context.xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);

    _cui_context.wayland_pointer_event.axis_source = 0xFFFFFFFF;

    _cui_context.double_click_time = _CUI_DEFAULT_DOUBLE_CLICK_TIME;

    _cui_context.wayland_keyboard_repeat_period = 25;
    _cui_context.wayland_keyboard_repeat_delay = 400;

    cui_array_init(_cui_context.wayland_monitors, 4, &_cui_context.common.arena);

    struct wl_registry *wayland_registry = wl_display_get_registry(_cui_context.wayland_display);

    wl_registry_add_listener(wayland_registry, &_cui_wayland_registry_listener, 0);
    wl_display_roundtrip(_cui_context.wayland_display);

    if (!_cui_context.wayland_compositor || !_cui_context.wayland_xdg_wm_base || !_cui_context.wayland_shared_memory)
    {
        wl_display_disconnect(_cui_context.wayland_display);
        return false;
    }

    if (_cui_context.wayland_seat && _cui_context.wayland_data_device_manager)
    {
        _cui_context.wayland_data_device =
            wl_data_device_manager_get_data_device(_cui_context.wayland_data_device_manager,
                                                   _cui_context.wayland_seat);
        wl_data_device_add_listener(_cui_context.wayland_data_device, &_cui_wayland_data_device_listener, 0);
    }

    xdg_wm_base_add_listener(_cui_context.wayland_xdg_wm_base, &_cui_wayland_xdg_wm_base_listener, 0);

    _cui_context.wayland_cursor_surface = wl_compositor_create_surface(_cui_context.wayland_compositor);

    wl_display_roundtrip(_cui_context.wayland_display);

#  if CUI_RENDERER_OPENGLES2_ENABLED

    _cui_parse_egl_client_extensions();

    _cui_context.egl_display = EGL_NO_DISPLAY;

    if (_cui_context.has_EGL_EXT_platform_base && _cui_context.has_EGL_EXT_platform_wayland)
    {
        _cui_context.egl_display = eglGetPlatformDisplay(EGL_PLATFORM_WAYLAND_EXT, _cui_context.wayland_display, 0);

        if (_cui_context.egl_display != EGL_NO_DISPLAY)
        {
            EGLint major_version;
            EGLint minor_version;

            if (eglInitialize(_cui_context.egl_display, &major_version, &minor_version))
            {
#if 0
                char *apis = (char *) eglQueryString(_cui_context.egl_display, EGL_CLIENT_APIS);
                char *vendor = (char *) eglQueryString(_cui_context.egl_display, EGL_VENDOR);
                char *extensions = (char *) eglQueryString(_cui_context.egl_display, EGL_EXTENSIONS);

                printf("EGL version %d.%d\n", major_version, minor_version);
                printf("EGL APIs: %s\n", apis);
                printf("EGL Vendor: %s\n", vendor);
                printf("EGL Extensions: %s\n", extensions);
#endif
            }
            else
            {
                _cui_context.egl_display = EGL_NO_DISPLAY;
            }
        }
    }

#  endif

    return true;
}

static void
_cui_window_on_close_button(CuiWidget *widget)
{
    CuiWindow *window = widget->window;
    CuiAssert(window);

    cui_window_close(window);
}

static void
_cui_window_on_maximize_button(CuiWidget *widget)
{
    CuiWindow *window = widget->window;
    CuiAssert(window);

    if (cui_window_is_maximized(window))
    {
        xdg_toplevel_unset_maximized(window->wayland_xdg_toplevel);
    }
    else
    {
        xdg_toplevel_set_maximized(window->wayland_xdg_toplevel);
    }
}

static void
_cui_window_on_minimize_button(CuiWidget *widget)
{
    CuiWindow *window = widget->window;
    CuiAssert(window);

    xdg_toplevel_set_minimized(window->wayland_xdg_toplevel);
}

#  if CUI_RENDERER_OPENGLES2_ENABLED

static bool
_cui_initialize_opengles2_wayland(CuiWindow *window)
{
    if (_cui_context.egl_display == EGL_NO_DISPLAY)
    {
        return false;
    }

    EGLint buffer_attr[] = {
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };

    EGLConfig config;
    EGLint num_config;

    if (!eglChooseConfig(_cui_context.egl_display, buffer_attr, &config, 1, &num_config))
    {
        return false;
    }

    int32_t framebuffer_width = cui_rect_get_width(window->backbuffer_rect);
    int32_t framebuffer_height = cui_rect_get_height(window->backbuffer_rect);

    window->wayland_egl_window = wl_egl_window_create(window->wayland_surface, framebuffer_width, framebuffer_height);

    if (!window->wayland_egl_window)
    {
        return false;
    }

    EGLSurface egl_surface = eglCreatePlatformWindowSurface(_cui_context.egl_display, config, window->wayland_egl_window, 0);

    EGLContext egl_context = EGL_NO_CONTEXT;

    if (egl_surface != EGL_NO_SURFACE)
    {
        EGLint context_attr[] = {
            EGL_CONTEXT_MAJOR_VERSION, 2,
            EGL_CONTEXT_MINOR_VERSION, 0,
            EGL_NONE
        };

        egl_context = eglCreateContext(_cui_context.egl_display, config, EGL_NO_CONTEXT, context_attr);

        if (egl_context != EGL_NO_CONTEXT)
        {
            EGLBoolean success = eglMakeCurrent(_cui_context.egl_display, egl_surface, egl_surface, egl_context);

            if (success)
            {
#if 0
                printf("OpenGL ES Version: %s\n", glGetString(GL_VERSION));
                printf("OpenGL ES Extensions: %s\n", glGetString(GL_EXTENSIONS));
#endif

                window->renderer.opengles2.egl_surface = egl_surface;
                window->renderer.opengles2.egl_context = egl_context;
                window->base.renderer_type = CUI_RENDERER_TYPE_OPENGLES2;
                window->renderer.opengles2.renderer_opengles2 = _cui_create_opengles2_renderer();

                if (!window->renderer.opengles2.renderer_opengles2)
                {
                    eglMakeCurrent(_cui_context.egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
                    eglDestroyContext(_cui_context.egl_display, egl_context);

                    wl_egl_window_destroy(window->wayland_egl_window);

                    return false;
                }

                return true;
            }
        }
    }

    eglMakeCurrent(_cui_context.egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

    if (egl_context != EGL_NO_CONTEXT)
    {
        eglDestroyContext(_cui_context.egl_display, egl_context);
    }

    if (egl_surface != EGL_NO_SURFACE)
    {
        eglDestroySurface(_cui_context.egl_display, egl_surface);
    }

    wl_egl_window_destroy(window->wayland_egl_window);

    return false;
}

#  endif

#endif

static void
_cui_window_draw(CuiWindow *window)
{
    CuiCommandBuffer *command_buffer = 0;

    switch (window->base.renderer_type)
    {
        case CUI_RENDERER_TYPE_SOFTWARE:
        {
#if CUI_RENDERER_SOFTWARE_ENABLED
            command_buffer = _cui_software_renderer_begin_command_buffer(window->renderer.software.renderer_software);
#else
            CuiAssert(!"CUI_RENDERER_TYPE_SOFTWARE not enabled.");
#endif
        } break;

        case CUI_RENDERER_TYPE_OPENGLES2:
        {
#if CUI_RENDERER_OPENGLES2_ENABLED
            command_buffer = _cui_opengles2_renderer_begin_command_buffer(window->renderer.opengles2.renderer_opengles2);
#else
            CuiAssert(!"CUI_RENDERER_TYPE_OPENGLES2 not enabled.");
#endif
        } break;

        case CUI_RENDERER_TYPE_METAL:
        {
            CuiAssert(!"CUI_RENDERER_TYPE_METAL not supported.");
        } break;

        case CUI_RENDERER_TYPE_DIRECT3D11:
        {
            CuiAssert(!"CUI_RENDERER_TYPE_DIRECT3D11 not supported.");
        } break;
    }

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

        CuiGraphicsContext ctx;
        ctx.clip_rect_offset = 0;
        ctx.clip_rect = window->backbuffer_rect;
        ctx.window_rect = window->backbuffer_rect;
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

        cui_end_temporary_memory(temp_memory);
    }

    int32_t framebuffer_width = cui_rect_get_width(window->backbuffer_rect);
    int32_t framebuffer_height = cui_rect_get_height(window->backbuffer_rect);

#if CUI_FRAMEBUFFER_SCREENSHOT_ENABLED
    bool bgra = true;
    bool top_to_bottom = true;
    CuiArena screenshot_arena;
    CuiBitmap screenshot_bitmap;

    if (window->take_screenshot)
    {
        cui_arena_allocate(&screenshot_arena, CuiMiB(32));
    }
#endif

    switch (window->base.renderer_type)
    {
        case CUI_RENDERER_TYPE_SOFTWARE:
        {
#if CUI_RENDERER_SOFTWARE_ENABLED

            switch (_cui_context.backend)
            {
                case CUI_LINUX_BACKEND_NONE:
                {
                    CuiAssert(!"unsupported");
                } break;

#  if CUI_BACKEND_WAYLAND_ENABLED

                case CUI_LINUX_BACKEND_WAYLAND:
                {
                    _cui_wayland_acquire_framebuffer(window, framebuffer_width, framebuffer_height);

                    CuiFramebuffer *framebuffer = window->renderer.software.current_framebuffer;

                    _cui_software_renderer_render(window->renderer.software.renderer_software,
                                                  &framebuffer->bitmap, command_buffer, CuiHexColor(0x00000000));

#    if CUI_FRAMEBUFFER_SCREENSHOT_ENABLED
                    screenshot_bitmap = framebuffer->bitmap;
#    endif
                } break;

#  endif

#  if CUI_BACKEND_X11_ENABLED

                case CUI_LINUX_BACKEND_X11:
                {
                    _cui_x11_acquire_framebuffer(window, framebuffer_width, framebuffer_height);

                    CuiFramebuffer *framebuffer = window->renderer.software.current_framebuffer;

                    _cui_software_renderer_render(window->renderer.software.renderer_software,
                                                  &framebuffer->bitmap, command_buffer, CuiHexColor(0xFF000000));

#    if CUI_FRAMEBUFFER_SCREENSHOT_ENABLED
                    screenshot_bitmap = framebuffer->bitmap;
#    endif
                } break;

#  endif

            }

#else
            CuiAssert(!"CUI_RENDERER_TYPE_SOFTWARE not enabled.");
#endif
        } break;

        case CUI_RENDERER_TYPE_OPENGLES2:
        {
#if CUI_RENDERER_OPENGLES2_ENABLED

            switch (_cui_context.backend)
            {
                case CUI_LINUX_BACKEND_NONE:
                {
                    CuiAssert(!"unsupported");
                } break;

#  if CUI_BACKEND_WAYLAND_ENABLED

                case CUI_LINUX_BACKEND_WAYLAND:
                {
                    // TODO: change only if needed
#if CUI_DEBUG_BUILD
                    CuiAssert(eglMakeCurrent(_cui_context.egl_display, window->renderer.opengles2.egl_surface,
                                             window->renderer.opengles2.egl_surface, window->renderer.opengles2.egl_context));
#else
                    eglMakeCurrent(_cui_context.egl_display, window->renderer.opengles2.egl_surface,
                                   window->renderer.opengles2.egl_surface, window->renderer.opengles2.egl_context);
#endif

                    int32_t surface_width, surface_height;

                    wl_egl_window_get_attached_size(window->wayland_egl_window, &surface_width, &surface_height);

                    if ((framebuffer_width != surface_width) || (framebuffer_height != surface_height))
                    {
                        wl_egl_window_resize(window->wayland_egl_window, framebuffer_width, framebuffer_height, 0, 0);
                    }

                    _cui_opengles2_renderer_render(window->renderer.opengles2.renderer_opengles2,
                                                   command_buffer, framebuffer_width, framebuffer_height,
                                                   CuiHexColor(0x00000000));

#    if CUI_FRAMEBUFFER_SCREENSHOT_ENABLED
                    if (window->take_screenshot)
                    {
                        bgra = false;
                        top_to_bottom = false;

                        screenshot_bitmap.width = framebuffer_width;
                        screenshot_bitmap.height = framebuffer_height;
                        screenshot_bitmap.stride = screenshot_bitmap.width * 4;
                        screenshot_bitmap.pixels = cui_alloc(&screenshot_arena, screenshot_bitmap.stride * screenshot_bitmap.height,
                                                             CuiDefaultAllocationParams());

                        glReadPixels(0, 0, screenshot_bitmap.width, screenshot_bitmap.height, GL_RGBA, GL_UNSIGNED_BYTE,
                                     screenshot_bitmap.pixels);
                    }
#    endif
                } break;

#  endif

#  if CUI_BACKEND_X11_ENABLED

                case CUI_LINUX_BACKEND_X11:
                {
                    // TODO: change only if needed
#if CUI_DEBUG_BUILD
                    CuiAssert(eglMakeCurrent(_cui_context.egl_display, window->renderer.opengles2.egl_surface,
                                             window->renderer.opengles2.egl_surface, window->renderer.opengles2.egl_context));
#else
                    eglMakeCurrent(_cui_context.egl_display, window->renderer.opengles2.egl_surface,
                                   window->renderer.opengles2.egl_surface, window->renderer.opengles2.egl_context);
#endif

                    _cui_opengles2_renderer_render(window->renderer.opengles2.renderer_opengles2,
                                                   command_buffer, framebuffer_width, framebuffer_height,
                                                   CuiHexColor(0xFF000000));

#    if CUI_FRAMEBUFFER_SCREENSHOT_ENABLED
                    if (window->take_screenshot)
                    {
                        bgra = false;
                        top_to_bottom = false;

                        screenshot_bitmap.width = framebuffer_width;
                        screenshot_bitmap.height = framebuffer_height;
                        screenshot_bitmap.stride = screenshot_bitmap.width * 4;
                        screenshot_bitmap.pixels = cui_alloc(&screenshot_arena, screenshot_bitmap.stride * screenshot_bitmap.height,
                                                             CuiDefaultAllocationParams());

                        glReadPixels(0, 0, screenshot_bitmap.width, screenshot_bitmap.height, GL_RGBA, GL_UNSIGNED_BYTE,
                                     screenshot_bitmap.pixels);
                    }
#    endif
                } break;

#  endif

            }
#else
            CuiAssert(!"CUI_RENDERER_TYPE_OPENGLES2 not enabled.");
#endif
        } break;

        case CUI_RENDERER_TYPE_METAL:
        {
            CuiAssert(!"CUI_RENDERER_TYPE_METAL not supported.");
        } break;

        case CUI_RENDERER_TYPE_DIRECT3D11:
        {
            CuiAssert(!"CUI_RENDERER_TYPE_DIRECT3D11 not supported.");
        } break;
    }

#if CUI_FRAMEBUFFER_SCREENSHOT_ENABLED

    if (window->take_screenshot)
    {
        CuiString bmp_data = cui_image_encode_bmp(screenshot_bitmap, top_to_bottom, bgra, &screenshot_arena);

        CuiFile *screenshot_file = cui_platform_file_create(&screenshot_arena, CuiStringLiteral("screenshot_cui.bmp"));

        if (screenshot_file)
        {
            cui_platform_file_truncate(screenshot_file, bmp_data.count);
            cui_platform_file_write(screenshot_file, bmp_data.data, 0, bmp_data.count);
            cui_platform_file_close(screenshot_file);
        }

        cui_arena_deallocate(&screenshot_arena);

        window->take_screenshot = false;
    }

#endif

}

void
cui_signal_main_thread(void)
{
    int ret;
    uint8_t value = 1;

    do {
        ret = write(_cui_context.signal_fd[1], &value, sizeof(value));
    } while (__builtin_expect(!!((ret == -1) && (errno == EINTR)), 0));
}

void
cui_platform_set_clipboard_text(CuiArena *temporary_memory, CuiString text)
{
    (void) temporary_memory;

    switch (_cui_context.backend)
    {
        case CUI_LINUX_BACKEND_NONE:
        {
            CuiAssert(!"unsupported");
        } break;

#if CUI_BACKEND_WAYLAND_ENABLED

        case CUI_LINUX_BACKEND_WAYLAND:
        {
            if (_cui_context.wayland_data_device)
            {
                printf("create data source\n");
                _cui_context.wayland_data_source =
                // struct wl_data_source *clipboard_source =
                    wl_data_device_manager_create_data_source(_cui_context.wayland_data_device_manager);

                wl_data_source_offer(_cui_context.wayland_data_source, "text/plain");
                wl_data_source_add_listener(_cui_context.wayland_data_source, &_cui_wayland_data_source_listener, 0);

                uint32_t serial = 0;
                wl_data_device_set_selection(_cui_context.wayland_data_device, _cui_context.wayland_data_source, serial);
            }
        } break;

#endif

#if CUI_BACKEND_X11_ENABLED

        case CUI_LINUX_BACKEND_X11:
        {
            XSetSelectionOwner(_cui_context.x11_display, _cui_context.x11_atom_clipboard, _cui_context.x11_clipboard_window, CurrentTime);

            if (XGetSelectionOwner(_cui_context.x11_display, _cui_context.x11_atom_clipboard) == _cui_context.x11_clipboard_window)
            {
                cui_arena_clear(&_cui_context.clipboard_arena);
                _cui_context.clipboard_data = cui_copy_string(&_cui_context.clipboard_arena, text);
            }
        } break;

#endif

    }
}

CuiString
cui_platform_get_clipboard_text(CuiArena *arena)
{
    CuiString result = { 0, 0 };

    switch (_cui_context.backend)
    {
        case CUI_LINUX_BACKEND_NONE:
        {
            CuiAssert(!"unsupported");
        } break;

#if CUI_BACKEND_WAYLAND_ENABLED

        case CUI_LINUX_BACKEND_WAYLAND:
        {
            // TODO
        } break;

#endif

#if CUI_BACKEND_X11_ENABLED

        case CUI_LINUX_BACKEND_X11:
        {
            if (XGetSelectionOwner(_cui_context.x11_display, _cui_context.x11_atom_clipboard) == _cui_context.x11_clipboard_window)
            {
                if (_cui_context.clipboard_data.count)
                {
                    result = cui_copy_string(arena, _cui_context.clipboard_data);
                }
            }
            else
            {
                XConvertSelection(_cui_context.x11_display, _cui_context.x11_atom_clipboard, _cui_context.x11_atom_utf8_string,
                                  _cui_context.x11_atom_xsel_data, _cui_context.x11_clipboard_window, CurrentTime);

                XEvent ev = { 0 };

                int64_t target_timeout = _cui_get_current_ms() + 4; // timeout in 4 ms

                while (!XCheckTypedWindowEvent(_cui_context.x11_display, _cui_context.x11_clipboard_window, SelectionNotify, &ev) &&
                       (ev.xselection.selection != _cui_context.x11_atom_clipboard))
                {
                    struct pollfd x11_poll;
                    x11_poll.fd      = XConnectionNumber(_cui_context.x11_display);
                    x11_poll.events  = POLLIN;
                    x11_poll.revents = 0;

                    int timeout = (int) (target_timeout - _cui_get_current_ms());

                    if (timeout < 0)
                    {
                        timeout = 0;
                    }

                    int ret = poll(&x11_poll, 1, timeout);

                    if (ret <= 0) break;
                }

                if (ev.xselection.property != None)
                {
                    Atom type;
                    int format;
                    unsigned long length, remaining;
                    unsigned char *buffer = 0;

                    int ret = XGetWindowProperty(_cui_context.x11_display, ev.xselection.requestor, ev.xselection.property,
                                                 0, INT64_MAX, false, AnyPropertyType, &type, &format, &length, &remaining, &buffer);

                    if ((ret == Success) && (length > 0))
                    {
                        // TODO: handle chunks
                        if ((remaining == 0) && (type == _cui_context.x11_atom_utf8_string))
                        {
                            result = cui_copy_string(arena, cui_make_string(buffer, length));
                        }

                        XFree(buffer);
                    }

                    XDeleteProperty(_cui_context.x11_display, ev.xselection.requestor, ev.xselection.property);
                }
            }
        } break;

#endif

    }

    return result;
}

CuiString *
cui_get_files_to_open(void)
{
    return 0;
}

bool
cui_init(int argument_count, char **arguments)
{
    setlocale(LC_ALL, "");

    if (!_cui_common_init(argument_count, arguments))
    {
        return false;
    }

    if (argument_count > 0)
    {
        arguments += 1;
        argument_count -= 1;

        for (int i = 0; i < argument_count; i += 1)
        {
            *cui_array_append(_cui_context.common.command_line_arguments) = CuiCString(arguments[i]);
        }
    }

    if (pipe(_cui_context.signal_fd))
    {
        return false;
    }

    if (!_cui_set_fd_non_blocking(_cui_context.signal_fd[0]) ||
        !_cui_set_fd_non_blocking(_cui_context.signal_fd[1]))
    {
        return false;
    }

    int32_t worker_thread_count = cui_platform_get_performance_core_count() - 1;

    worker_thread_count = cui_max_int32(1, cui_min_int32(worker_thread_count, 15));

    pthread_cond_init(&_cui_context.common.worker_thread_queue.semaphore_cond, 0);
    pthread_mutex_init(&_cui_context.common.worker_thread_queue.semaphore_mutex, 0);

    pthread_cond_init(&_cui_context.common.interactive_background_thread_queue.semaphore_cond, 0);
    pthread_mutex_init(&_cui_context.common.interactive_background_thread_queue.semaphore_mutex, 0);

    pthread_cond_init(&_cui_context.common.non_interactive_background_thread_queue.semaphore_cond, 0);
    pthread_mutex_init(&_cui_context.common.non_interactive_background_thread_queue.semaphore_mutex, 0);

    for (int32_t worker_thread_index = 0;
         worker_thread_index < worker_thread_count;
         worker_thread_index += 1)
    {
        pthread_t worker_thread;
        pthread_create(&worker_thread, 0, _cui_worker_thread_proc, 0);
    }

    pthread_t interactive_background_thread;
    pthread_create(&interactive_background_thread, 0, _cui_background_thread_proc, &_cui_context.common.interactive_background_thread_queue);

    pthread_t non_interactive_background_thread;
    pthread_create(&non_interactive_background_thread, 0, _cui_background_thread_proc, &_cui_context.common.non_interactive_background_thread_queue);

    {
        CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(&_cui_context.common.temporary_memory);

        size_t buffer_size = CuiKiB(1);
        char *executable_directory = cui_alloc_array(&_cui_context.common.temporary_memory, char, buffer_size, CuiDefaultAllocationParams());

        ssize_t len = readlink("/proc/self/exe", executable_directory, buffer_size);
        CuiString executable_path = cui_make_string(executable_directory, len);
        cui_path_remove_last_directory(&executable_path);
        _cui_context.common.executable_directory = cui_copy_string(&_cui_context.common.arena, executable_path);

        cui_end_temporary_memory(temp_memory);
    }

    bool backend_initialized = false;

#if CUI_BACKEND_WAYLAND_ENABLED
    if (!backend_initialized)
    {
        _cui_context.backend = CUI_LINUX_BACKEND_WAYLAND;
        backend_initialized = _cui_initialize_wayland();
    }
#endif

#if CUI_BACKEND_X11_ENABLED
    if (!backend_initialized)
    {
        _cui_context.backend = CUI_LINUX_BACKEND_X11;
        backend_initialized = _cui_initialize_x11();
    }
#endif

    if (!backend_initialized)
    {
        _cui_context.backend = CUI_LINUX_BACKEND_NONE;
        return false;
    }

    return true;
}

CuiWindow *
cui_window_create(uint32_t creation_flags)
{
    switch (_cui_context.backend)
    {
        case CUI_LINUX_BACKEND_NONE:
        {
            CuiAssert(!"unsupported");
            return 0;
        } break;

#if CUI_BACKEND_WAYLAND_ENABLED

        case CUI_LINUX_BACKEND_WAYLAND:
        {
            if (!_cui_context.wayland_display)
            {
                return 0;
            }
        } break;

#endif

#if CUI_BACKEND_X11_ENABLED

        case CUI_LINUX_BACKEND_X11:
        {
            if (!_cui_context.x11_display)
            {
                return 0;
            }
        } break;

#endif

    }

    CuiWindow *window = _cui_add_window(creation_flags);

    switch (_cui_context.backend)
    {
        case CUI_LINUX_BACKEND_NONE:
        {
            CuiAssert(!"unsupported");
            return 0;
        } break;

#if CUI_BACKEND_WAYLAND_ENABLED

        case CUI_LINUX_BACKEND_WAYLAND:
        {
            int32_t backbuffer_scale = 1;

            if (cui_array_count(_cui_context.wayland_monitors) > 0)
            {
                CuiWaylandMonitor *monitor = _cui_context.wayland_monitors;
                backbuffer_scale = monitor->scale;

                for (int32_t i = 1; i < cui_array_count(_cui_context.wayland_monitors); i += 1)
                {
                    CuiWaylandMonitor *monitor = _cui_context.wayland_monitors + i;
                    backbuffer_scale = cui_min_int32(backbuffer_scale, monitor->scale);
                }
            }

            window->backbuffer_scale = backbuffer_scale;
        } break;

#endif

#if CUI_BACKEND_X11_ENABLED

        case CUI_LINUX_BACKEND_X11:
        {
            window->backbuffer_scale = _cui_context.x11_desktop_scale;
        } break;

#endif

    }

    int32_t content_width;
    int32_t content_height;

    if (_cui_context.common.scale_factor == 0)
    {
        window->base.ui_scale = (float) window->backbuffer_scale;
        content_width  = window->backbuffer_scale * CUI_DEFAULT_WINDOW_WIDTH;
        content_height = window->backbuffer_scale * CUI_DEFAULT_WINDOW_HEIGHT;
    }
    else
    {
        window->base.ui_scale = (float) _cui_context.common.scale_factor;
        content_width  = _cui_context.common.scale_factor * CUI_DEFAULT_WINDOW_WIDTH;
        content_height = _cui_context.common.scale_factor * CUI_DEFAULT_WINDOW_HEIGHT;
    }

    window->font_id = _cui_font_manager_find_font(&window->base.temporary_memory, &window->base.font_manager, window->base.ui_scale,
                                                  cui_make_sized_font_spec(CuiStringLiteral("Inter-Regular"),  14.0f, 1.0f),
                                                  cui_make_sized_font_spec(CuiStringLiteral("Roboto-Regular"), 14.0f, 1.0f),
                                                  cui_make_sized_font_spec(CuiStringLiteral("DejaVuSans"),     13.0f, 1.0f),
                                                  cui_make_sized_font_spec(CuiStringLiteral("DroidSans"),      14.0f, 1.0f),
                                                  cui_make_sized_font_spec(CuiStringLiteral("TwemojiMozilla"), 14.0f, 1.0f));

    switch (_cui_context.backend)
    {
        case CUI_LINUX_BACKEND_NONE:
        {
            CuiAssert(!"unsupported");
            return 0;
        } break;

#if CUI_BACKEND_WAYLAND_ENABLED

        case CUI_LINUX_BACKEND_WAYLAND:
        {
            CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(&window->base.temporary_memory);

            if (cui_platform_get_environment_variable_int32(&window->base.temporary_memory, CuiStringLiteral("CUI_WAYLAND_USE_SERVER_SIDE_DECORATION")) != 0)
            {
                window->base.creation_flags |= CUI_WINDOW_CREATION_FLAG_PREFER_SYSTEM_DECORATION;
                window->do_client_side_decoration = false;
            }
            else
            {
                window->do_client_side_decoration = true;
            }

            window->pending_do_client_side_decoration = window->do_client_side_decoration;

            cui_end_temporary_memory(temp_memory);

            _cui_wayland_update_window_with_content_size(window, content_width, content_height);

            cui_arena_allocate(&window->arena, CuiKiB(8));

            CuiWidget *root_widget = cui_alloc_type(&window->arena, CuiWidget, CuiDefaultAllocationParams());

            cui_widget_init(root_widget, CUI_WIDGET_TYPE_BOX);
            cui_widget_set_main_axis(root_widget, CUI_AXIS_Y);
            cui_widget_set_y_axis_gravity(root_widget, CUI_GRAVITY_START);
            cui_widget_add_flags(root_widget, CUI_WIDGET_FLAG_DRAW_BACKGROUND | CUI_WIDGET_FLAG_CLIP_CONTENT);
            cui_widget_set_border_radius(root_widget, 4.0f, 4.0f, 0.0f, 0.0f);
            cui_widget_set_border_width(root_widget, 1.0f, 1.0f, 1.0f, 1.0f);
            cui_widget_set_box_shadow(root_widget, 0.0f, 4.0f, 16.0f);

            root_widget->color_normal_box_shadow = CUI_COLOR_WINDOW_DROP_SHADOW;
            root_widget->color_normal_border = CUI_COLOR_WINDOW_OUTLINE;

            CuiWidget *titlebar_stack = cui_alloc_type(&window->arena, CuiWidget, CuiDefaultAllocationParams());

            cui_widget_init(titlebar_stack, CUI_WIDGET_TYPE_STACK);

            cui_widget_append_child(root_widget, titlebar_stack);

            window->titlebar = cui_alloc_type(&window->arena, CuiWidget, CuiDefaultAllocationParams());

            cui_widget_init(window->titlebar, CUI_WIDGET_TYPE_BOX);
            cui_widget_set_main_axis(window->titlebar, CUI_AXIS_X);
            cui_widget_set_x_axis_gravity(window->titlebar, CUI_GRAVITY_END);
            cui_widget_add_flags(window->titlebar, CUI_WIDGET_FLAG_DRAW_BACKGROUND | CUI_WIDGET_FLAG_FIXED_HEIGHT);
            cui_widget_set_preferred_size(window->titlebar, 0.0f, (float) _CUI_WINDOW_TITLEBAR_HEIGHT);
            cui_widget_set_border_width(window->titlebar, 0.0f, 0.0f, 0.0f, 0.0f);
            cui_widget_set_border_radius(window->titlebar, 3.0f, 3.0f, 0.0f, 0.0f);
            cui_widget_set_inline_padding(window->titlebar, 1.0f);

            window->titlebar->color_normal_background = CUI_COLOR_WINDOW_TITLEBAR_BACKGROUND;
            window->titlebar->color_normal_border = CUI_COLOR_WINDOW_TITLEBAR_BORDER;

            cui_widget_append_child(titlebar_stack, window->titlebar);

            // window title
            window->title = cui_alloc_type(&window->arena, CuiWidget, CuiDefaultAllocationParams());

            cui_widget_init(window->title, CUI_WIDGET_TYPE_LABEL);
            cui_widget_set_label(window->title, CuiStringLiteral(""));
            cui_widget_set_x_axis_gravity(window->title, CUI_GRAVITY_CENTER);
            cui_widget_set_y_axis_gravity(window->title, CUI_GRAVITY_CENTER);
            cui_widget_set_padding(window->title, 0.0f, 0.0f, 0.0f, 8.0f);
            cui_widget_append_child(window->titlebar, window->title);

            window->button_layer = cui_alloc_type(&window->arena, CuiWidget, CuiDefaultAllocationParams());

            cui_widget_init(window->button_layer, CUI_WIDGET_TYPE_BOX);
            cui_widget_set_main_axis(window->button_layer, CUI_AXIS_X);
            cui_widget_set_x_axis_gravity(window->button_layer, CUI_GRAVITY_END);
            cui_widget_add_flags(window->button_layer, CUI_WIDGET_FLAG_FIXED_HEIGHT);
            cui_widget_set_preferred_size(window->button_layer, 0.0f, (float) _CUI_WINDOW_TITLEBAR_HEIGHT);
            cui_widget_append_child(titlebar_stack, window->button_layer);

            // close button
            window->close_button = cui_alloc_type(&window->arena, CuiWidget, CuiDefaultAllocationParams());

            cui_widget_init(window->close_button, CUI_WIDGET_TYPE_BUTTON);
            cui_widget_set_icon(window->close_button, CUI_ICON_WINDOWS_CLOSE);
            cui_widget_set_border_width(window->close_button, 0.0f, 0.0f, 0.0f, 0.0f);
            cui_widget_set_border_radius(window->close_button, 0.0f, 3.0f, 0.0f, 0.0f);
            cui_widget_set_padding(window->close_button, 8.0f, 14.0f, 8.0f, 14.0f);
            cui_widget_set_box_shadow(window->close_button, 0.0f, 0.0f, 0.0f);

            window->close_button->color_normal_background = CUI_COLOR_WINDOW_TITLEBAR_BACKGROUND;
            window->close_button->color_normal_icon = CUI_COLOR_WINDOW_TITLEBAR_ICON;

            cui_widget_append_child(window->button_layer, window->close_button);

            window->close_button->on_action = _cui_window_on_close_button;

            if (!(window->base.creation_flags & CUI_WINDOW_CREATION_FLAG_NOT_USER_RESIZABLE))
            {
                // maximize button
                window->maximize_button = cui_alloc_type(&window->arena, CuiWidget, CuiDefaultAllocationParams());

                cui_widget_init(window->maximize_button, CUI_WIDGET_TYPE_BUTTON);
                cui_widget_set_icon(window->maximize_button, CUI_ICON_WINDOWS_MAXIMIZE);
                cui_widget_set_border_width(window->maximize_button, 0.0f, 0.0f, 0.0f, 0.0f);
                cui_widget_set_border_radius(window->maximize_button, 0.0f, 0.0f, 0.0f, 0.0f);
                cui_widget_set_padding(window->maximize_button, 8.0f, 14.0f, 8.0f, 14.0f);
                cui_widget_set_box_shadow(window->maximize_button, 0.0f, 0.0f, 0.0f);

                window->maximize_button->color_normal_background = CUI_COLOR_WINDOW_TITLEBAR_BACKGROUND;
                window->maximize_button->color_normal_icon = CUI_COLOR_WINDOW_TITLEBAR_ICON;

                cui_widget_append_child(window->button_layer, window->maximize_button);

                window->maximize_button->on_action = _cui_window_on_maximize_button;
            }

            // minimize button
            window->minimize_button = cui_alloc_type(&window->arena, CuiWidget, CuiDefaultAllocationParams());

            cui_widget_init(window->minimize_button, CUI_WIDGET_TYPE_BUTTON);
            cui_widget_set_icon(window->minimize_button, CUI_ICON_WINDOWS_MINIMIZE);
            cui_widget_set_border_width(window->minimize_button, 0.0f, 0.0f, 0.0f, 0.0f);
            cui_widget_set_border_radius(window->minimize_button, 0.0f, 0.0f, 0.0f, 0.0f);
            cui_widget_set_padding(window->minimize_button, 8.0f, 14.0f, 8.0f, 14.0f);
            cui_widget_set_box_shadow(window->minimize_button, 0.0f, 0.0f, 0.0f);

            window->minimize_button->color_normal_background = CUI_COLOR_WINDOW_TITLEBAR_BACKGROUND;
            window->minimize_button->color_normal_icon = CUI_COLOR_WINDOW_TITLEBAR_ICON;

            cui_widget_append_child(window->button_layer, window->minimize_button);

            window->minimize_button->on_action = _cui_window_on_minimize_button;

            CuiWidget *button_padding = cui_alloc_type(&window->arena, CuiWidget, CuiDefaultAllocationParams());

            cui_widget_init(button_padding, CUI_WIDGET_TYPE_BOX);
            cui_widget_append_child(window->button_layer, button_padding);

            CuiWidget *dummy_user_root_widget = cui_alloc_type(&window->arena, CuiWidget, CuiDefaultAllocationParams());
            cui_widget_init(dummy_user_root_widget, CUI_WIDGET_TYPE_BOX);

            cui_widget_append_child(root_widget, dummy_user_root_widget);

            cui_widget_set_window(root_widget, window);
            cui_widget_set_ui_scale(root_widget, window->base.ui_scale);

            window->client_side_decoration_root_widget = root_widget;
            window->client_side_decoration_dummy_user_root_widget = dummy_user_root_widget;

            if (window->do_client_side_decoration)
            {
                cui_widget_layout(root_widget, window->layout_rect);

                window->base.user_root_widget = dummy_user_root_widget;
                window->base.platform_root_widget = root_widget;
            }

            cui_array_init(window->wayland_monitors, 4, &window->arena);

            window->wayland_surface      = wl_compositor_create_surface(_cui_context.wayland_compositor);
            window->wayland_xdg_surface  = xdg_wm_base_get_xdg_surface(_cui_context.wayland_xdg_wm_base, window->wayland_surface);
            window->wayland_xdg_toplevel = xdg_surface_get_toplevel(window->wayland_xdg_surface);

            if (_cui_context.wayland_xdg_decoration_manager)
            {
                window->wayland_xdg_decoration = zxdg_decoration_manager_v1_get_toplevel_decoration(_cui_context.wayland_xdg_decoration_manager, window->wayland_xdg_toplevel);
                zxdg_toplevel_decoration_v1_set_mode(window->wayland_xdg_decoration, (window->base.creation_flags & CUI_WINDOW_CREATION_FLAG_PREFER_SYSTEM_DECORATION) ?
                                                                                     ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE : ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
                zxdg_toplevel_decoration_v1_add_listener(window->wayland_xdg_decoration, &_cui_wayland_xdg_decoration_listener, window);
            }

            wl_surface_add_listener(window->wayland_surface, &_cui_wayland_surface_listener, window);
            xdg_surface_add_listener(window->wayland_xdg_surface, &_cui_wayland_xdg_surface_listener, window);
            xdg_toplevel_add_listener(window->wayland_xdg_toplevel, &_cui_wayland_xdg_toplevel_listener, window);

            // everything has to be allocated at this point
            window->title_temp_memory = cui_begin_temporary_memory(&window->arena);

            bool renderer_initialized = false;

#  if CUI_RENDERER_OPENGLES2_ENABLED

            if (!renderer_initialized)
            {
                renderer_initialized = _cui_initialize_opengles2_wayland(window);
            }

#  endif

#  if CUI_RENDERER_SOFTWARE_ENABLED

            if (!renderer_initialized)
            {
                window->base.renderer_type = CUI_RENDERER_TYPE_SOFTWARE;
                window->renderer.software.renderer_software = _cui_create_software_renderer();

                int32_t framebuffer_width = cui_rect_get_width(window->backbuffer_rect);
                int32_t framebuffer_height = cui_rect_get_height(window->backbuffer_rect);

                for (uint32_t i = 0; i < CuiArrayCount(window->renderer.software.framebuffers); i += 1)
                {
                    CuiFramebuffer *framebuffer = window->renderer.software.framebuffers + i;
                    _cui_wayland_allocate_framebuffer(framebuffer, framebuffer_width, framebuffer_height);
                }

                window->renderer.software.current_framebuffer = 0;

                renderer_initialized = true;
            }

#  endif

            if (!renderer_initialized)
            {
                cui_arena_deallocate(&window->arena);
                _cui_remove_window(window);
                return 0;
            }
        } break;

#endif

#if CUI_BACKEND_X11_ENABLED

        case CUI_LINUX_BACKEND_X11:
        {
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

            window->content_rect = cui_make_rect(0, 0, content_width, content_height);
            window->layout_rect = window->content_rect;
            window->window_rect = window->content_rect;
            window->backbuffer_rect = window->content_rect;

            bool renderer_initialized = false;

#  if CUI_RENDERER_OPENGLES2_ENABLED

            if (!renderer_initialized)
            {
                renderer_initialized = _cui_initialize_opengles2_x11(window, window_attr);
            }

#  endif

#  if CUI_RENDERER_SOFTWARE_ENABLED

            if (!renderer_initialized)
            {
                int32_t width = cui_rect_get_width(window->content_rect);
                int32_t height = cui_rect_get_height(window->content_rect);

                window->x11_window = XCreateWindow(_cui_context.x11_display, _cui_context.x11_root_window, 0, 0,
                                                   width, height, 0, 0, InputOutput, 0, CWEventMask, &window_attr);

                window->base.renderer_type = CUI_RENDERER_TYPE_SOFTWARE;
                window->renderer.software.renderer_software = _cui_create_software_renderer();

                int32_t framebuffer_width = cui_rect_get_width(window->backbuffer_rect);
                int32_t framebuffer_height = cui_rect_get_height(window->backbuffer_rect);

                for (uint32_t i = 0; i < CuiArrayCount(window->renderer.software.framebuffers); i += 1)
                {
                    CuiFramebuffer *framebuffer = window->renderer.software.framebuffers + i;
                    _cui_x11_allocate_framebuffer(framebuffer, framebuffer_width, framebuffer_height);
                }

                window->renderer.software.current_framebuffer = 0;
            }

#  endif

            if (!window->x11_window)
            {
                _cui_remove_window(window);
                return 0;
            }

            Atom protocols[] = {
                _cui_context.x11_atom_wm_delete_window,
                _cui_context.x11_atom_wm_sync_request,
            };

            XSetWMProtocols(_cui_context.x11_display, window->x11_window, protocols, CuiArrayCount(protocols));

            if (_cui_context.has_xsync_extension)
            {
                XSyncValue counter_value;
                XSyncIntToValue(&counter_value, 0);

                window->basic_frame_counter = XSyncCreateCounter(_cui_context.x11_display, counter_value);

                XChangeProperty(_cui_context.x11_display, window->x11_window, _cui_context.x11_atom_wm_sync_request_counter,
                                _cui_context.x11_atom_cardinal, 32, PropModeReplace, (unsigned char *) &window->basic_frame_counter, 1);
            }

            window->last_left_click_time = INT16_MIN;

            XSizeHints sh = { 0 };
            sh.flags = PMinSize | PResizeInc | PWinGravity;
            sh.win_gravity = StaticGravity;
            sh.min_width  = window->backbuffer_scale * 100;
            sh.min_height = window->backbuffer_scale * 75;
            sh.width_inc  = window->backbuffer_scale;
            sh.height_inc = window->backbuffer_scale;

            if (window->base.creation_flags & CUI_WINDOW_CREATION_FLAG_NOT_USER_RESIZABLE)
            {
                int32_t width = cui_rect_get_width(window->content_rect);
                int32_t height = cui_rect_get_height(window->content_rect);

                sh.flags |= PMaxSize;
                sh.min_width  = width;
                sh.min_height = height;
                sh.max_width  = width;
                sh.max_height = height;
            }

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
        } break;

#endif

    }

    return window;
}

void
cui_window_set_title(CuiWindow *window, CuiString title)
{
    switch (_cui_context.backend)
    {
        case CUI_LINUX_BACKEND_NONE:
        {
            CuiAssert(!"unsupported");
        } break;

#if CUI_BACKEND_WAYLAND_ENABLED

        case CUI_LINUX_BACKEND_WAYLAND:
        {
            if (window->do_client_side_decoration)
            {
                cui_end_temporary_memory(window->title_temp_memory);
                window->title_temp_memory = cui_begin_temporary_memory(&window->arena);

                cui_widget_set_label(window->title, cui_copy_string(&window->arena, title));
                cui_widget_layout(window->title, window->title->rect);
            }

            CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(&window->base.temporary_memory);

            xdg_toplevel_set_title(window->wayland_xdg_toplevel, cui_to_c_string(&window->base.temporary_memory, title));

            cui_end_temporary_memory(temp_memory);
        } break;

#endif

#if CUI_BACKEND_X11_ENABLED

        case CUI_LINUX_BACKEND_X11:
        {
            XTextProperty text_prop;
            text_prop.value    = (unsigned char *) title.data;
            text_prop.encoding = _cui_context.x11_atom_utf8_string;
            text_prop.format   = 8;
            text_prop.nitems   = title.count;

            XSetWMName(_cui_context.x11_display, window->x11_window, &text_prop);
        } break;

#endif

    }
}

void
cui_window_resize(CuiWindow *window, int32_t width, int32_t height)
{
    switch (_cui_context.backend)
    {
        case CUI_LINUX_BACKEND_NONE:
        {
            CuiAssert(!"unsupported");
        } break;

#if CUI_BACKEND_WAYLAND_ENABLED

        case CUI_LINUX_BACKEND_WAYLAND:
        {
            _cui_wayland_update_window_with_content_size(window, width, height);
        } break;

#endif

#if CUI_BACKEND_X11_ENABLED

        case CUI_LINUX_BACKEND_X11:
        {
            if (window->base.creation_flags & CUI_WINDOW_CREATION_FLAG_NOT_USER_RESIZABLE)
            {
                long hints;
                XSizeHints sh;

                XGetWMNormalHints(_cui_context.x11_display, window->x11_window, &sh, &hints);

                sh.flags |= PMinSize | PMaxSize;
                sh.min_width  = width;
                sh.min_height = height;
                sh.max_width  = width;
                sh.max_height = height;

                XSetWMNormalHints(_cui_context.x11_display, window->x11_window, &sh);
                XResizeWindow(_cui_context.x11_display, window->x11_window, width, height);
            }
            else
            {
                XResizeWindow(_cui_context.x11_display, window->x11_window, width, height);
            }
        } break;

#endif

    }
}

void
cui_window_show(CuiWindow *window)
{
    switch (_cui_context.backend)
    {
        case CUI_LINUX_BACKEND_NONE:
        {
            CuiAssert(!"unsupported");
        } break;

#if CUI_BACKEND_WAYLAND_ENABLED

        case CUI_LINUX_BACKEND_WAYLAND:
        {
            if (!window->is_mapped)
            {
                wl_surface_commit(window->wayland_surface);
            }
        } break;

#endif

#if CUI_BACKEND_X11_ENABLED

        case CUI_LINUX_BACKEND_X11:
        {
            XMapWindow(_cui_context.x11_display, window->x11_window);
        } break;

#endif

    }
}

void
cui_window_set_fullscreen(CuiWindow *window, bool fullscreen)
{
    switch (_cui_context.backend)
    {
        case CUI_LINUX_BACKEND_NONE:
        {
            CuiAssert(!"unsupported");
        } break;

#if CUI_BACKEND_WAYLAND_ENABLED

        case CUI_LINUX_BACKEND_WAYLAND:
        {
            if (cui_window_is_fullscreen(window))
            {
                if (!fullscreen)
                {
                    xdg_toplevel_unset_fullscreen(window->wayland_xdg_toplevel);
                }
            }
            else
            {
                if (fullscreen)
                {
                    CuiWaylandMonitor *monitor = _cui_wayland_get_fullscreen_monitor_for_window(window);
                    xdg_toplevel_set_fullscreen(window->wayland_xdg_toplevel, monitor->output);
                }
            }
        } break;

#endif

#if CUI_BACKEND_X11_ENABLED

        case CUI_LINUX_BACKEND_X11:
        {
            if (cui_window_is_fullscreen(window))
            {
                if (!fullscreen)
                {
                    XDeleteProperty(_cui_context.x11_display, window->x11_window, _cui_context.x11_atom_wm_fullscreen_monitors);

                    {
                        XEvent e = { 0 };
                        e.xclient.type = ClientMessage;
                        e.xclient.message_type = _cui_context.x11_atom_wm_state;
                        e.xclient.format       = 32;
                        e.xclient.window       = window->x11_window;
                        e.xclient.data.l[0]    = _cui_context.x11_atom_wm_state_remove;
                        e.xclient.data.l[1]    = _cui_context.x11_atom_wm_state_fullscreen;
                        e.xclient.data.l[3]    = 1;

                        XSendEvent(_cui_context.x11_display, _cui_context.x11_root_window, 0,
                                   SubstructureNotifyMask | SubstructureRedirectMask, &e);
                    }

                    XMoveResizeWindow(_cui_context.x11_display, window->x11_window, window->windowed_x,
                                      window->windowed_y, window->windowed_width, window->windowed_height);

                    window->base.state &= ~CUI_WINDOW_STATE_FULLSCREEN;
                }
            }
            else
            {
                if (fullscreen)
                {
                    window->windowed_width = cui_rect_get_width(window->content_rect);
                    window->windowed_height = cui_rect_get_height(window->content_rect);

                    XWindowAttributes window_attr = { 0 };
                    XGetWindowAttributes(_cui_context.x11_display, window->x11_window, &window_attr);

                    Window child;
                    XTranslateCoordinates(_cui_context.x11_display, window->x11_window, _cui_context.x11_root_window,
                                          0, 0, &window->windowed_x, &window->windowed_y, &child);

                    int32_t center_x = window->windowed_x + window->windowed_width / 2;
                    int32_t center_y = window->windowed_y + window->windowed_height / 2;

                    int32_t target_x = 0;
                    int32_t target_y = 0;
                    int32_t target_width = 0;
                    int32_t target_height = 0;
                    int32_t target_monitor_index = 0;

                    int monitor_count = 0;
                    XRRMonitorInfo *monitors = XRRGetMonitors(_cui_context.x11_display, _cui_context.x11_root_window, false, &monitor_count);

                    for (int monitor_index = 0; monitor_index < monitor_count; monitor_index += 1)
                    {
                        XRRMonitorInfo *monitor_info = monitors + monitor_index;

                        if ((monitor_info->width > 0) && (monitor_info->height > 0))
                        {
                            if ((center_x >= monitor_info->x) && (center_y >= monitor_info->y) &&
                                (center_x < (monitor_info->x + monitor_info->width)) &&
                                (center_y < (monitor_info->y + monitor_info->height)))
                            {
                                target_x = monitor_info->x;
                                target_y = monitor_info->y;
                                target_width = monitor_info->width;
                                target_height = monitor_info->height;
                                target_monitor_index = monitor_index;
                                break;
                            }
                        }
                    }

                    XRRFreeMonitors(monitors);

                    {
                        XEvent e = { 0 };
                        e.xclient.type         = ClientMessage;
                        e.xclient.message_type = _cui_context.x11_atom_wm_fullscreen_monitors;
                        e.xclient.format       = 32;
                        e.xclient.window       = window->x11_window;
                        e.xclient.data.l[0]    = target_monitor_index; /* top */
                        e.xclient.data.l[1]    = target_monitor_index; /* bottom */
                        e.xclient.data.l[2]    = target_monitor_index; /* left */
                        e.xclient.data.l[3]    = target_monitor_index; /* right */
                        e.xclient.data.l[4]    = 0;

                        XSendEvent(_cui_context.x11_display, _cui_context.x11_root_window, 0,
                                   SubstructureNotifyMask | SubstructureRedirectMask, &e);
                    }

                    {
                        XEvent e = { 0 };
                        e.xclient.type = ClientMessage;
                        e.xclient.message_type = _cui_context.x11_atom_wm_state;
                        e.xclient.format       = 32;
                        e.xclient.window       = window->x11_window;
                        e.xclient.data.l[0]    = _cui_context.x11_atom_wm_state_add;
                        e.xclient.data.l[1]    = _cui_context.x11_atom_wm_state_fullscreen;
                        e.xclient.data.l[3]    = 1;

                        XSendEvent(_cui_context.x11_display, _cui_context.x11_root_window, 0,
                                   SubstructureNotifyMask | SubstructureRedirectMask, &e);
                    }

                    XMoveResizeWindow(_cui_context.x11_display, window->x11_window, target_x, target_y, target_width, target_height);

                    window->base.state |= CUI_WINDOW_STATE_FULLSCREEN;
                }
            }
        } break;

#endif

    }
}

float
cui_window_get_titlebar_height(CuiWindow *window)
{
    float titlebar_height = 0.0f;

    switch (_cui_context.backend)
    {
        case CUI_LINUX_BACKEND_NONE:
        {
            CuiAssert(!"unsupported");
        } break;

#if CUI_BACKEND_WAYLAND_ENABLED

        case CUI_LINUX_BACKEND_WAYLAND:
        {
            if (window->do_client_side_decoration)
            {
                titlebar_height = _CUI_WINDOW_TITLEBAR_HEIGHT;
            }
        } break;

#endif

#if CUI_BACKEND_X11_ENABLED

        case CUI_LINUX_BACKEND_X11:
        {
        } break;

#endif

    }

    return titlebar_height;
}

void
cui_window_close(CuiWindow *window)
{
    switch (_cui_context.backend)
    {
        case CUI_LINUX_BACKEND_NONE:
        {
            CuiAssert(!"unsupported");
        } break;

#if CUI_BACKEND_WAYLAND_ENABLED

        case CUI_LINUX_BACKEND_WAYLAND:
        {
            cui_window_destroy(window);
        } break;

#endif

#if CUI_BACKEND_X11_ENABLED

        case CUI_LINUX_BACKEND_X11:
        {
            XDestroyWindow(_cui_context.x11_display, window->x11_window);
        } break;

#endif

    }
}

void
cui_window_destroy(CuiWindow *window)
{
    switch (_cui_context.backend)
    {
        case CUI_LINUX_BACKEND_NONE:
        {
            CuiAssert(!"unsupported");
        } break;

#if CUI_BACKEND_WAYLAND_ENABLED

        case CUI_LINUX_BACKEND_WAYLAND:
        {
            if (_cui_context.wayland_keyboard_focused_window == window)
            {
                _cui_context.wayland_keyboard_focused_window = 0;
            }

            if (_cui_context.wayland_window_under_cursor == window)
            {
                _cui_context.wayland_window_under_cursor = 0;
            }

            switch (window->base.renderer_type)
            {
                case CUI_RENDERER_TYPE_SOFTWARE:
                {
#  if CUI_RENDERER_SOFTWARE_ENABLED
                    for (uint32_t i = 0; i < CuiArrayCount(window->renderer.software.framebuffers); i += 1)
                    {
                        CuiFramebuffer *framebuffer = window->renderer.software.framebuffers + i;

                        if (framebuffer->is_busy)
                        {
                            // TODO: wait for framebuffer not busy
                        }

                        wl_buffer_destroy(framebuffer->backend.wayland.buffer);
                        wl_shm_pool_destroy(framebuffer->backend.wayland.shared_memory_pool);
                        munmap(framebuffer->bitmap.pixels, framebuffer->shared_memory_size);
                        close(framebuffer->backend.wayland.shared_memory_fd);
                    }

                    _cui_destroy_software_renderer(window->renderer.software.renderer_software);
#  else
                    CuiAssert(!"CUI_RENDERER_TYPE_SOFTWARE not enabled.");
#  endif
                } break;

                case CUI_RENDERER_TYPE_OPENGLES2:
                {
#  if CUI_RENDERER_OPENGLES2_ENABLED
                    // TODO: change only if needed
#if CUI_DEBUG_BUILD
                    CuiAssert(eglMakeCurrent(_cui_context.egl_display, window->renderer.opengles2.egl_surface,
                                             window->renderer.opengles2.egl_surface, window->renderer.opengles2.egl_context));
#else
                    eglMakeCurrent(_cui_context.egl_display, window->renderer.opengles2.egl_surface,
                                   window->renderer.opengles2.egl_surface, window->renderer.opengles2.egl_context);
#endif

                    _cui_destroy_opengles2_renderer(window->renderer.opengles2.renderer_opengles2);

                    eglMakeCurrent(_cui_context.egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

                    CuiAssert(window->renderer.opengles2.egl_context != EGL_NO_CONTEXT);
                    eglDestroyContext(_cui_context.egl_display, window->renderer.opengles2.egl_context);

                    CuiAssert(window->renderer.opengles2.egl_surface != EGL_NO_SURFACE);
                    eglDestroySurface(_cui_context.egl_display, window->renderer.opengles2.egl_surface);

                    wl_egl_window_destroy(window->wayland_egl_window);
#  else
                    CuiAssert(!"CUI_RENDERER_TYPE_OPENGLES2 not enabled.");
#  endif
                } break;

                case CUI_RENDERER_TYPE_METAL:
                {
                    CuiAssert(!"CUI_RENDERER_TYPE_METAL not supported.");
                } break;

                case CUI_RENDERER_TYPE_DIRECT3D11:
                {
                    CuiAssert(!"CUI_RENDERER_TYPE_DIRECT3D11 not supported.");
                } break;
            }

            if (window->wayland_xdg_decoration)
            {
                zxdg_toplevel_decoration_v1_destroy(window->wayland_xdg_decoration);
            }

            xdg_toplevel_destroy(window->wayland_xdg_toplevel);
            xdg_surface_destroy(window->wayland_xdg_surface);
            wl_surface_destroy(window->wayland_surface);

            cui_arena_deallocate(&window->arena);
        } break;

#endif

#if CUI_BACKEND_X11_ENABLED

        case CUI_LINUX_BACKEND_X11:
        {
            switch (window->base.renderer_type)
            {
                case CUI_RENDERER_TYPE_SOFTWARE:
                {
#  if CUI_RENDERER_SOFTWARE_ENABLED
                    if (_cui_context.has_shared_memory_extension)
                    {
                        for (uint32_t i = 0; i < CuiArrayCount(window->renderer.software.framebuffers); i += 1)
                        {
                            CuiFramebuffer *framebuffer = window->renderer.software.framebuffers + i;

                            while (framebuffer->is_busy)
                            {
                                _cui_wait_for_frame_completion(window);
                            }

                            XShmDetach(_cui_context.x11_display, &framebuffer->backend.x11.shared_memory_info);
                            shmdt(framebuffer->backend.x11.shared_memory_info.shmaddr);
                            shmctl(framebuffer->backend.x11.shared_memory_info.shmid, IPC_RMID, 0);
                        }
                    }
                    else
                    {
                        CuiFramebuffer *framebuffer = window->renderer.software.framebuffers;
                        cui_platform_deallocate(framebuffer->bitmap.pixels, framebuffer->shared_memory_size);
                    }

                    _cui_destroy_software_renderer(window->renderer.software.renderer_software);
#  else
                    CuiAssert(!"CUI_RENDERER_TYPE_SOFTWARE not enabled.");
#  endif
                } break;

                case CUI_RENDERER_TYPE_OPENGLES2:
                {
#  if CUI_RENDERER_OPENGLES2_ENABLED
                    // TODO: change only if needed
#if CUI_DEBUG_BUILD
                    CuiAssert(eglMakeCurrent(_cui_context.egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, window->renderer.opengles2.egl_context));
#else
                    eglMakeCurrent(_cui_context.egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, window->renderer.opengles2.egl_context);
#endif

                    _cui_destroy_opengles2_renderer(window->renderer.opengles2.renderer_opengles2);

                    eglMakeCurrent(_cui_context.egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

                    CuiAssert(window->renderer.opengles2.egl_context != EGL_NO_CONTEXT);
                    eglDestroyContext(_cui_context.egl_display, window->renderer.opengles2.egl_context);

                    CuiAssert(window->renderer.opengles2.egl_surface != EGL_NO_SURFACE);
                    eglDestroySurface(_cui_context.egl_display, window->renderer.opengles2.egl_surface);
#  else
                    CuiAssert(!"CUI_RENDERER_TYPE_OPENGLES2 not enabled.");
#  endif
                } break;

                case CUI_RENDERER_TYPE_METAL:
                {
                    CuiAssert(!"CUI_RENDERER_TYPE_METAL not supported.");
                } break;

                case CUI_RENDERER_TYPE_DIRECT3D11:
                {
                    CuiAssert(!"CUI_RENDERER_TYPE_DIRECT3D11 not supported.");
                } break;
            }
        } break;

#endif

    }

    _cui_remove_window(window);
}

void
cui_window_set_root_widget(CuiWindow *window, CuiWidget *widget)
{
    CuiAssert(widget);

    switch (_cui_context.backend)
    {
        case CUI_LINUX_BACKEND_NONE:
        {
            CuiAssert(!"unsupported");
        } break;

#if CUI_BACKEND_WAYLAND_ENABLED

        case CUI_LINUX_BACKEND_WAYLAND:
        {
            if (window->do_client_side_decoration)
            {
                CuiAssert(window->base.user_root_widget);
                cui_widget_replace_child(window->base.platform_root_widget, window->base.user_root_widget, widget);

                window->base.user_root_widget = widget;

                cui_widget_set_window(window->base.user_root_widget, window);
                cui_widget_set_ui_scale(window->base.user_root_widget, window->base.ui_scale);
                cui_widget_layout(window->base.platform_root_widget, window->layout_rect);
            }
            else
            {
                window->base.user_root_widget = widget;
                window->base.platform_root_widget = widget;

                cui_widget_set_window(window->base.user_root_widget, window);
                cui_widget_set_ui_scale(window->base.user_root_widget, window->base.ui_scale);
                cui_widget_layout(window->base.platform_root_widget, window->layout_rect);
            }
        } break;

#endif

#if CUI_BACKEND_X11_ENABLED

        case CUI_LINUX_BACKEND_X11:
        {
            window->base.user_root_widget = widget;
            window->base.platform_root_widget = widget;

            cui_widget_set_window(window->base.user_root_widget, window);
            cui_widget_set_ui_scale(window->base.user_root_widget, window->base.ui_scale);
            cui_widget_layout(window->base.platform_root_widget, window->layout_rect);
        } break;

#endif

    }
}

void
cui_window_set_cursor(CuiWindow *window, CuiCursorType cursor_type)
{
    if (window->base.cursor == cursor_type)
    {
        return;
    }

    window->base.cursor = cursor_type;

    switch (_cui_context.backend)
    {
        case CUI_LINUX_BACKEND_NONE:
        {
            CuiAssert(!"unsupported");
        } break;

#if CUI_BACKEND_WAYLAND_ENABLED

        case CUI_LINUX_BACKEND_WAYLAND:
        {
            if (_cui_context.wayland_window_under_cursor == window)
            {
                _cui_wayland_update_cursor(window);
            }
        } break;

#endif

#if CUI_BACKEND_X11_ENABLED

        case CUI_LINUX_BACKEND_X11:
        {
            if (window->base.cursor)
            {
                switch (window->base.cursor)
                {
                    case CUI_CURSOR_NONE:            XDefineCursor(_cui_context.x11_display, window->x11_window, _cui_context.x11_cursor_none);            break;
                    case CUI_CURSOR_ARROW:           XDefineCursor(_cui_context.x11_display, window->x11_window, _cui_context.x11_cursor_arrow);           break;
                    case CUI_CURSOR_HAND:            XDefineCursor(_cui_context.x11_display, window->x11_window, _cui_context.x11_cursor_hand);            break;
                    case CUI_CURSOR_TEXT:            XDefineCursor(_cui_context.x11_display, window->x11_window, _cui_context.x11_cursor_text);            break;
                    case CUI_CURSOR_MOVE_ALL:        XDefineCursor(_cui_context.x11_display, window->x11_window, _cui_context.x11_cursor_move_all);        break;
                    case CUI_CURSOR_MOVE_LEFT_RIGHT: XDefineCursor(_cui_context.x11_display, window->x11_window, _cui_context.x11_cursor_move_left_right); break;
                    case CUI_CURSOR_MOVE_UP_DOWN:    XDefineCursor(_cui_context.x11_display, window->x11_window, _cui_context.x11_cursor_move_up_down);    break;
                    case CUI_CURSOR_RESIZE_TOP:          /* FALLTHRU */
                    case CUI_CURSOR_RESIZE_BOTTOM:       /* FALLTHRU */
                    case CUI_CURSOR_RESIZE_LEFT:         /* FALLTHRU */
                    case CUI_CURSOR_RESIZE_RIGHT:        /* FALLTHRU */
                    case CUI_CURSOR_RESIZE_TOP_LEFT:     /* FALLTHRU */
                    case CUI_CURSOR_RESIZE_TOP_RIGHT:    /* FALLTHRU */
                    case CUI_CURSOR_RESIZE_BOTTOM_LEFT:  /* FALLTHRU */
                    case CUI_CURSOR_RESIZE_BOTTOM_RIGHT: /* FALLTHRU */
                        CuiAssert(!"Not supported");
                        break;
                }
            }
            else
            {
                XUndefineCursor(_cui_context.x11_display, window->x11_window);
            }
        } break;

#endif

    }
}

void
cui_step(void)
{
    switch (_cui_context.backend)
    {
        case CUI_LINUX_BACKEND_NONE:
        {
            CuiAssert(!"unsupported");
        } break;

#if CUI_BACKEND_WAYLAND_ENABLED

        case CUI_LINUX_BACKEND_WAYLAND:
        {
            while (wl_display_prepare_read(_cui_context.wayland_display) != 0)
            {
                wl_display_dispatch_pending(_cui_context.wayland_display);
            }

            wl_display_flush(_cui_context.wayland_display);

            bool has_wayland_events = false;

            for (;;)
            {
                struct pollfd polling[2];

                polling[0].fd      = wl_display_get_fd(_cui_context.wayland_display);
                polling[0].events  = POLLIN;
                polling[0].revents = 0;

                polling[1].fd      = _cui_context.signal_fd[0];
                polling[1].events  = POLLIN;
                polling[1].revents = 0;

                int ret = poll(polling, CuiArrayCount(polling), -1);

                if (ret > 0)
                {
                    if (polling[0].revents & POLLIN)
                    {
                        has_wayland_events = true;
                    }

                    if (polling[1].revents & POLLIN)
                    {
                        uint8_t buffer[8];
                        while (read(_cui_context.signal_fd[0], buffer, sizeof(buffer)) == sizeof(buffer));

                        if (_cui_context.common.signal_callback)
                        {
                            _cui_context.common.signal_callback();
                        }
                    }

                    break;
                }
            }

            if (has_wayland_events)
            {
                wl_display_read_events(_cui_context.wayland_display);
            }
            else
            {
                wl_display_cancel_read(_cui_context.wayland_display);
            }

            wl_display_dispatch_pending(_cui_context.wayland_display);

            for (uint32_t window_index = 0;
                 window_index < _cui_context.common.window_count; window_index += 1)
            {
                CuiWindow *window = _cui_context.common.windows[window_index];

                if (window->is_mapped && window->base.needs_redraw)
                {
                    _cui_wayland_commit_frame(window);
                    window->base.needs_redraw = false;
                }
            }
        } break;

#endif

#if CUI_BACKEND_X11_ENABLED

        case CUI_LINUX_BACKEND_X11:
        {
            if (!XPending(_cui_context.x11_display))
            {
                bool blocking = true;

                while (blocking)
                {
                    struct pollfd polling[2];

                    polling[0].fd      = XConnectionNumber(_cui_context.x11_display);
                    polling[0].events  = POLLIN;
                    polling[0].revents = 0;

                    polling[1].fd      = _cui_context.signal_fd[0];
                    polling[1].events  = POLLIN;
                    polling[1].revents = 0;

                    int ret = poll(polling, CuiArrayCount(polling), -1);

                    if (ret > 0)
                    {
                        if (polling[0].revents & POLLIN)
                        {
                            if (XPending(_cui_context.x11_display))
                            {
                                blocking = false;
                            }
                        }

                        if (polling[1].revents & POLLIN)
                        {
                            uint8_t buffer[8];
                            while (read(_cui_context.signal_fd[0], buffer, sizeof(buffer)) == sizeof(buffer));

                            if (_cui_context.common.signal_callback)
                            {
                                _cui_context.common.signal_callback();
                            }

                            blocking = false;
                        }
                    }
                }
            }

            while (XPending(_cui_context.x11_display))
            {
                XEvent ev;
                XNextEvent(_cui_context.x11_display, &ev);

                Window x11_window = ev.xany.window;

                if (XFilterEvent(&ev, x11_window)) continue;

                if (x11_window == _cui_context.x11_settings_window)
                {
                    if ((ev.type == PropertyNotify) &&
                        (ev.xproperty.window == _cui_context.x11_settings_window) &&
                        (ev.xproperty.state == PropertyNewValue) &&
                        (ev.xproperty.atom == _cui_context.x11_atom_xsettings_settings))
                    {
                        Atom type;
                        int format;
                        unsigned long length, remaining;
                        unsigned char *settings_buffer = 0;

                        int ret = XGetWindowProperty(_cui_context.x11_display, _cui_context.x11_settings_window, _cui_context.x11_atom_xsettings_settings,
                                                     0, 2048, false, AnyPropertyType, &type, &format, &length, &remaining, &settings_buffer);

                        if ((ret == Success) && (length > 0))
                        {
                            if ((remaining == 0) && (format == 8))
                            {
                                CuiX11DesktopSettings desktop_settings = _cui_parse_desktop_settings(cui_make_string(settings_buffer, length));

                                if (_cui_context.x11_desktop_scale != desktop_settings.ui_scale)
                                {
                                    _cui_context.x11_desktop_scale = desktop_settings.ui_scale;

                                    for (uint32_t window_index = 0; window_index < _cui_context.common.window_count; window_index += 1)
                                    {
                                        CuiWindow *window = _cui_context.common.windows[window_index];

                                        int32_t old_backbuffer_scale = window->backbuffer_scale;

                                        window->backbuffer_scale = _cui_context.x11_desktop_scale;

                                        if (_cui_context.common.scale_factor == 0)
                                        {
                                            _cui_window_set_ui_scale(window, (float) window->backbuffer_scale);

                                            int32_t old_width = cui_rect_get_width(window->content_rect);
                                            int32_t old_height = cui_rect_get_height(window->content_rect);

                                            int32_t new_width  = (old_width / old_backbuffer_scale) * window->backbuffer_scale;
                                            int32_t new_height = (old_height / old_backbuffer_scale) * window->backbuffer_scale;

                                            XSizeHints sh = { 0 };
                                            sh.flags = PMinSize | PResizeInc | PWinGravity;
                                            sh.win_gravity = StaticGravity;
                                            sh.min_width  = window->backbuffer_scale * 100;
                                            sh.min_height = window->backbuffer_scale * 75;
                                            sh.width_inc  = window->backbuffer_scale;
                                            sh.height_inc = window->backbuffer_scale;

                                            XSetWMNormalHints(_cui_context.x11_display, window->x11_window, &sh);

                                            cui_window_resize(window, new_width, new_height);
                                        }
                                    }
                                }

                                if (_cui_context.double_click_time != desktop_settings.double_click_time)
                                {
                                    _cui_context.double_click_time = desktop_settings.double_click_time;
                                }
                            }

                            XFree(settings_buffer);
                        }
                    }

                    continue;
                }
                else if (x11_window == _cui_context.x11_clipboard_window)
                {
                    if (ev.type == SelectionRequest)
                    {
                        if ((ev.xselectionrequest.selection == _cui_context.x11_atom_clipboard) && _cui_context.clipboard_data.count)
                        {
                            XEvent reply = { 0 };
                            reply.xselection.type      = SelectionNotify;
                            reply.xselection.requestor = ev.xselectionrequest.requestor;
                            reply.xselection.selection = ev.xselectionrequest.selection;
                            reply.xselection.target    = ev.xselectionrequest.target;
                            reply.xselection.time      = ev.xselectionrequest.time;

                            if (ev.xselectionrequest.target == _cui_context.x11_atom_targets)
                            {
                                Atom targets = _cui_context.x11_atom_utf8_string;
                                reply.xselection.property = ev.xselectionrequest.property;
                                XChangeProperty(_cui_context.x11_display, ev.xselectionrequest.requestor,
                                                reply.xselection.property, _cui_context.x11_atom_atom, 32,
                                                PropModeReplace, (unsigned char *) &targets, 1);
                            }
                            else if (ev.xselectionrequest.target == _cui_context.x11_atom_utf8_string)
                            {
                                reply.xselection.property = ev.xselectionrequest.property;
                                XChangeProperty(_cui_context.x11_display, ev.xselectionrequest.requestor,
                                                reply.xselection.property, _cui_context.x11_atom_utf8_string, 8,
                                                PropModeReplace, _cui_context.clipboard_data.data, _cui_context.clipboard_data.count);
                            }

                            XSendEvent(_cui_context.x11_display, ev.xselectionrequest.requestor, 0, 0, &reply);
                            XFlush(_cui_context.x11_display);
                        }
                    }
                    else if (ev.type == SelectionClear)
                    {
                        cui_arena_clear(&_cui_context.clipboard_arena);
                        _cui_context.clipboard_data = cui_make_string(0, 0);
                    }

                    continue;
                }

                CuiWindow *window = _cui_window_get_from_x11_window(x11_window);

                CuiAssert(window);

#  if CUI_RENDERER_SOFTWARE_ENABLED

                if (_cui_context.has_shared_memory_extension && (ev.type == _cui_context.x11_frame_completion_event))
                {
                    XShmCompletionEvent *event = (XShmCompletionEvent *) &ev;

                    for (uint32_t window_index = 0;
                         window_index < _cui_context.common.window_count; window_index += 1)
                    {
                        CuiWindow *window = _cui_context.common.windows[window_index];

                        if (window->x11_window == event->drawable)
                        {
                            for (uint32_t framebuffer_index = 0;
                                 framebuffer_index < CuiArrayCount(window->renderer.software.framebuffers);
                                 framebuffer_index += 1)
                            {
                                CuiFramebuffer *framebuffer = window->renderer.software.framebuffers + framebuffer_index;

                                if (framebuffer->backend.x11.shared_memory_info.shmseg == event->shmseg)
                                {
                                    framebuffer->is_busy = false;
                                    break;
                                }
                            }

                            break;
                        }
                    }
                    continue;
                }

#  endif

                switch (ev.type)
                {
                    case MapNotify:
                    {
                        window->is_mapped = true;
                        window->first_frame = true;
                        window->base.needs_redraw = true;
                    } break;

                    case UnmapNotify:
                    {
                        window->is_mapped = false;
                    } break;

                    case ClientMessage:
                    {
                        if (ev.xclient.message_type == _cui_context.x11_atom_wm_protocols)
                        {
                            if ((Atom) ev.xclient.data.l[0] == _cui_context.x11_atom_wm_delete_window)
                            {
                                window->is_mapped = false;
                                cui_window_close(window);
                            }
                            else if ((Atom) ev.xclient.data.l[0] == _cui_context.x11_atom_wm_sync_request)
                            {
                                window->x11_sync_request_serial = ((uint64_t) ev.xclient.data.l[3] << 32) | (uint64_t) ev.xclient.data.l[2];
                            }
                        }
                        else if (ev.xclient.message_type == _cui_context.x11_atom_manager)
                        {
                            if ((Atom) ev.xclient.data.l[1] == _cui_context.x11_atom_xsettings_screen)
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

                        int32_t width = cui_rect_get_width(window->backbuffer_rect);
                        int32_t height = cui_rect_get_height(window->backbuffer_rect);

                        if ((width != ev.xconfigure.width) || (height != ev.xconfigure.height))
                        {
                            window->content_rect = cui_make_rect(0, 0, ev.xconfigure.width, ev.xconfigure.height);
                            window->layout_rect = window->content_rect;
                            window->window_rect = window->content_rect;
                            window->backbuffer_rect = window->content_rect;

                            if (window->base.platform_root_widget)
                            {
                                cui_widget_layout(window->base.platform_root_widget, window->layout_rect);
                            }

                            window->base.needs_redraw = true;
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

                        cui_window_handle_event(window, CUI_EVENT_TYPE_MOUSE_MOVE);
                    } break;

                    case LeaveNotify:
                    {
                        cui_window_handle_event(window, CUI_EVENT_TYPE_MOUSE_LEAVE);
                    } break;

                    case ButtonPress:
                    {
                        switch (ev.xbutton.button)
                        {
                            case Button1:
                            {
                                window->base.event.mouse.x = ev.xbutton.x;
                                window->base.event.mouse.y = ev.xbutton.y;

                                if (((int64_t) ev.xbutton.time - window->last_left_click_time) <= (int64_t) _cui_context.double_click_time)
                                {
                                    window->last_left_click_time = INT16_MIN;
                                    cui_window_handle_event(window, CUI_EVENT_TYPE_DOUBLE_CLICK);
                                }
                                else
                                {
                                    window->last_left_click_time = (int64_t) ev.xbutton.time;
                                    cui_window_handle_event(window, CUI_EVENT_TYPE_LEFT_DOWN);
                                }
                            } break;

                            case Button3:
                            {
                                window->base.event.mouse.x = ev.xbutton.x;
                                window->base.event.mouse.y = ev.xbutton.y;

                                cui_window_handle_event(window, CUI_EVENT_TYPE_RIGHT_DOWN);
                            } break;

                            case Button4:
                            case Button5:
                            {
#if 0
                                printf("WHEEL %d\n", (ev.xbutton.button == Button4) ? 1 : -1);
#endif
                                window->base.event.wheel.is_precise_scrolling = false;
                                // TODO: where should this 3 be applied?
                                window->base.event.wheel.dy = (ev.xbutton.button == Button4) ? 3.0f : -3.0f;
                                cui_window_handle_event(window, CUI_EVENT_TYPE_MOUSE_WHEEL);
                            } break;
                        }
                    } break;

                    case ButtonRelease:
                    {
                        switch (ev.xbutton.button)
                        {
                            case Button1:
                            {
                                window->base.event.mouse.x = ev.xbutton.x;
                                window->base.event.mouse.y = ev.xbutton.y;

                                cui_window_handle_event(window, CUI_EVENT_TYPE_LEFT_UP);
                            } break;

                            case Button3:
                            {
                                window->base.event.mouse.x = ev.xbutton.x;
                                window->base.event.mouse.y = ev.xbutton.y;

                                cui_window_handle_event(window, CUI_EVENT_TYPE_RIGHT_UP);
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

#define _CUI_KEY_DOWN_EVENT(key_id)                                                     \
    window->base.event.key.codepoint       = (key_id);                                  \
    window->base.event.key.alt_is_down     = 1 & (ev.xkey.state >> Mod1MapIndex);       \
    window->base.event.key.ctrl_is_down    = 1 & (ev.xkey.state >> ControlMapIndex);    \
    window->base.event.key.shift_is_down   = 1 & (ev.xkey.state >> ShiftMapIndex);      \
    window->base.event.key.command_is_down = false;                                     \
    cui_window_handle_event(window, CUI_EVENT_TYPE_KEY_DOWN);

                        if ((status == XLookupKeySym) || (status == XLookupBoth))
                        {
                            switch (key)
                            {
                                case XK_BackSpace: { _CUI_KEY_DOWN_EVENT(CUI_KEY_BACKSPACE); } break;
                                case XK_Tab:       { _CUI_KEY_DOWN_EVENT(CUI_KEY_TAB);       } break;
                                case XK_Linefeed:  { _CUI_KEY_DOWN_EVENT(CUI_KEY_LINEFEED);  } break;
                                case XK_Return:    { _CUI_KEY_DOWN_EVENT(CUI_KEY_ENTER);     } break;
                                case XK_Escape:    { _CUI_KEY_DOWN_EVENT(CUI_KEY_ESCAPE);    } break;
                                case XK_Delete:    { _CUI_KEY_DOWN_EVENT(CUI_KEY_DELETE);    } break;
                                case XK_Up:        { _CUI_KEY_DOWN_EVENT(CUI_KEY_UP);        } break;
                                case XK_Down:      { _CUI_KEY_DOWN_EVENT(CUI_KEY_DOWN);      } break;
                                case XK_Left:      { _CUI_KEY_DOWN_EVENT(CUI_KEY_LEFT);      } break;
                                case XK_Right:     { _CUI_KEY_DOWN_EVENT(CUI_KEY_RIGHT);     } break;

                                case XK_F1:  case XK_F2:  case XK_F3:  case XK_F4:
                                case XK_F5:  case XK_F6:  case XK_F7:  case XK_F8:
                                case XK_F9:  case XK_F10: case XK_F11: case XK_F12:
                                {
#if CUI_FRAMEBUFFER_SCREENSHOT_ENABLED
                                    if (key == XK_F2)
                                    {
                                        window->take_screenshot = true;
                                        window->base.needs_redraw = true;
                                    }
                                    else
#endif
                                    {
                                        _CUI_KEY_DOWN_EVENT(CUI_KEY_F1 + (key - XK_F1));
                                    }
                                } break;

                                default:
                                {
                                    if ((key >= 32) && (key < 127))
                                    {
                                        _CUI_KEY_DOWN_EVENT(key);
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
                                    _CUI_KEY_DOWN_EVENT(utf8.codepoint);
                                }

                                i += utf8.byte_count;
                            }
                        }

#undef _CUI_KEY_DOWN_EVENT

                    } break;

                    case KeyRelease:
                    {
                    } break;
                }
            }

            for (uint32_t window_index = 0;
                 window_index < _cui_context.common.window_count; window_index += 1)
            {
                CuiWindow *window = _cui_context.common.windows[window_index];

                if (window->is_mapped && window->base.needs_redraw)
                {
                    _cui_window_draw(window);

                    switch (window->base.renderer_type)
                    {
                        case CUI_RENDERER_TYPE_SOFTWARE:
                        {
#  if CUI_RENDERER_SOFTWARE_ENABLED
                            CuiFramebuffer *framebuffer = window->renderer.software.current_framebuffer;

                            XImage backbuffer;
                            backbuffer.width = framebuffer->bitmap.width;
                            backbuffer.height = framebuffer->bitmap.height;
                            backbuffer.xoffset = 0;
                            backbuffer.format = ZPixmap;
                            backbuffer.data = (char *) framebuffer->bitmap.pixels;
                            backbuffer.byte_order = LSBFirst;
                            backbuffer.bitmap_unit = 32;
                            backbuffer.bitmap_bit_order = LSBFirst;
                            backbuffer.bitmap_pad = 32;
                            backbuffer.depth = 24;
                            backbuffer.bytes_per_line = framebuffer->bitmap.stride;
                            backbuffer.bits_per_pixel = 32;
                            backbuffer.red_mask = 0xFF0000;
                            backbuffer.green_mask = 0x00FF00;
                            backbuffer.blue_mask = 0x0000FF;

                            if (_cui_context.has_shared_memory_extension)
                            {
                                backbuffer.width = CuiAlign(backbuffer.width, 16);
                                backbuffer.obdata = (char *) &framebuffer->backend.x11.shared_memory_info;

                                XShmPutImage(_cui_context.x11_display, window->x11_window, _cui_context.x11_default_gc, &backbuffer,
                                             0, 0, 0, 0, backbuffer.width, backbuffer.height, True);
                            }
                            else
                            {
                                XPutImage(_cui_context.x11_display, window->x11_window, _cui_context.x11_default_gc, &backbuffer,
                                          0, 0, 0, 0, backbuffer.width, backbuffer.height);
                                XFlush(_cui_context.x11_display);

                                framebuffer->is_busy = false;
                            }

                            window->renderer.software.current_framebuffer = 0;
#  else
                            CuiAssert(!"CUI_RENDERER_TYPE_SOFTWARE not enabled.");
#  endif
                        } break;

                        case CUI_RENDERER_TYPE_OPENGLES2:
                        {
#  if CUI_RENDERER_OPENGLES2_ENABLED
                            eglSwapBuffers(_cui_context.egl_display, window->renderer.opengles2.egl_surface);
#  else
                            CuiAssert(!"CUI_RENDERER_TYPE_OPENGLES2 not enabled.");
#  endif
                        } break;

                        case CUI_RENDERER_TYPE_METAL:
                        {
                            CuiAssert(!"CUI_RENDERER_TYPE_METAL not supported.");
                        } break;

                        case CUI_RENDERER_TYPE_DIRECT3D11:
                        {
                            CuiAssert(!"CUI_RENDERER_TYPE_DIRECT3D11 not supported.");
                        } break;
                    }

                    if (window->x11_configure_serial)
                    {
                        XSyncValue counter_value;

                        XSyncIntsToValue(&counter_value, window->x11_configure_serial & 0xFFFFFFFF, window->x11_configure_serial >> 32);
                        XSyncSetCounter(_cui_context.x11_display, window->basic_frame_counter, counter_value);

                        window->x11_configure_serial = 0;
                    }

                    // On some egl implementations after you created the window and than changed the size of the window
                    // the size of the egl backbuffer seems not to be correct or at least not probably placed in the window.
                    // The result is a black or shifted first frame. To work around that we render a second frame right after.
                    if (window->first_frame)
                    {
                        window->first_frame = false;
                        window->base.needs_redraw = true;
                    }
                    else
                    {
                        window->base.needs_redraw = false;
                    }
                }
            }
        } break;

#endif

    }
}

#endif

uint32_t
cui_platform_get_core_count(void)
{
    return (uint32_t) get_nprocs();
}

uint32_t
cui_platform_get_performance_core_count(void)
{
    return (uint32_t) get_nprocs();
}

uint32_t
cui_platform_get_efficiency_core_count(void)
{
    return 0;
}

void
cui_platform_get_font_directories(CuiArena *temporary_memory, CuiArena *arena, CuiString **font_dirs)
{
    CuiString data_dir = cui_platform_get_data_directory(temporary_memory, temporary_memory);

    *cui_array_append(*font_dirs) = cui_path_concat(arena, data_dir, CuiStringLiteral("fonts"));
    *cui_array_append(*font_dirs) = CuiStringLiteral("/usr/share/fonts");
}

CuiString
cui_platform_get_data_directory(CuiArena *temporary_memory, CuiArena *arena)
{
    CuiString path = { 0 };
    CuiString append = { 0 };

    CuiString data_dir = cui_platform_get_environment_variable(temporary_memory, temporary_memory, CuiStringLiteral("XDG_DATA_HOME"));

    if (data_dir.count)
    {
        path = data_dir;
        append = CuiStringLiteral("/");
    }
    else
    {
        data_dir = cui_platform_get_environment_variable(temporary_memory, temporary_memory, CuiStringLiteral("HOME"));

        if (data_dir.count)
        {
            path = data_dir;
            append = CuiStringLiteral("/.local/share/");
        }
        else
        {
            path = CuiStringLiteral("./");
        }
    }

    if (path.count && (path.data[path.count - 1] == '/'))
    {
        cui_string_advance(&append, 1);
    }

    CuiString result = cui_string_concat(arena, path, append);

    return result;
}
