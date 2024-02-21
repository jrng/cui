#include <jni.h>

#include <errno.h>
#include <sys/sysinfo.h>

static CuiString __external_data_path;

static inline void
_cui_send_message(uint8_t message_type)
{
    write(_cui_context.write_fd, &message_type, sizeof(message_type));
}

static inline uint8_t
_cui_receive_message()
{
    uint8_t message_type = 0;
    read(_cui_context.read_fd, &message_type, sizeof(message_type));
    return message_type;
}

static inline void
_cui_set_input_queue(AInputQueue *input_queue)
{
    pthread_mutex_lock(&_cui_context.platform_mutex);
    _cui_context.new_input_queue = input_queue;
    _cui_send_message(CUI_ANDROID_MESSAGE_INPUT_QUEUE_CHANGED);
    while (_cui_context.input_queue != _cui_context.new_input_queue)
    {
        pthread_cond_wait(&_cui_context.platform_cond, &_cui_context.platform_mutex);
    }
    pthread_mutex_unlock(&_cui_context.platform_mutex);
}

static inline void
_cui_set_native_window(ANativeWindow *window)
{
    pthread_mutex_lock(&_cui_context.platform_mutex);
    _cui_context.new_native_window = window;
    _cui_send_message(CUI_ANDROID_MESSAGE_WINDOW_CHANGED);
    while (_cui_context.native_window != _cui_context.new_native_window)
    {
        pthread_cond_wait(&_cui_context.platform_cond, &_cui_context.platform_mutex);
    }
    pthread_mutex_unlock(&_cui_context.platform_mutex);
}

static inline void
_cui_window_create_renderer(CuiWindow *window)
{
    if (_cui_context.egl_display == EGL_NO_DISPLAY)
    {
        return;
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
        return;
    }

    EGLSurface egl_surface = eglCreateWindowSurface(_cui_context.egl_display, config, _cui_context.native_window, 0);

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
#if 1
                android_print("OpenGL ES Version: %s\n", glGetString(GL_VERSION));
                android_print("OpenGL ES Extensions: %s\n", glGetString(GL_EXTENSIONS));
#endif

                window->opengles2.egl_surface = egl_surface;
                window->opengles2.egl_context = egl_context;
                window->base.renderer = _cui_renderer_opengles2_create();

                if (window->base.renderer)
                {
                    CuiRendererOpengles2 *renderer_opengles2 = CuiContainerOf(window->base.renderer, CuiRendererOpengles2, base);
                    _cui_renderer_opengles2_restore_textures(renderer_opengles2, window->texture_state_count, window->texture_states);
                }
                else
                {
                    eglMakeCurrent(_cui_context.egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
                    eglDestroyContext(_cui_context.egl_display, egl_context);
                }

                return;
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
}

static inline void
_cui_window_destroy_renderer(CuiWindow *window)
{
    CuiRendererOpengles2 *renderer_opengles2 = CuiContainerOf(window->base.renderer, CuiRendererOpengles2, base);
    window->texture_state_count = _cui_renderer_opengles2_store_textures(renderer_opengles2, window->max_texture_state_count, window->texture_states);

    _cui_renderer_opengles2_destroy(renderer_opengles2);
    window->base.renderer = 0;

    if (_cui_context.egl_display != EGL_NO_DISPLAY)
    {
        eglMakeCurrent(_cui_context.egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

        if (window->opengles2.egl_context != EGL_NO_CONTEXT)
        {
            eglDestroyContext(_cui_context.egl_display, window->opengles2.egl_context);
            window->opengles2.egl_context = EGL_NO_CONTEXT;
        }

        if (window->opengles2.egl_surface != EGL_NO_SURFACE)
        {
            eglDestroySurface(_cui_context.egl_display, window->opengles2.egl_surface);
            window->opengles2.egl_surface = EGL_NO_SURFACE;
        }
    }
}

int main(int argc, char **argv);

static void *
_cui_android_main(void *data)
{
    (void) data;

    char *arguments[] = { "cui_android" };

    main(CuiArrayCount(arguments), arguments);

    return 0;
}

static void
_cui_update_configuration(CuiWindow *window)
{
    AConfiguration_fromAssetManager(_cui_context.config, _cui_context.native_activity->assetManager);

    // density  dpi  icon size  ui scale
    // ldpi     120  36x36      0.75
    // mdpi     160  48x48      1.00
    // hdpi     240  72x72      1.50
    // xhdpi    320  96x96      2.00
    // xxhdpi   480  144x144    3.00
    // xxxhdpi  640  192x192    4.00

    int32_t dpi = AConfiguration_getDensity(_cui_context.config);
    float ui_scale = (float) dpi / 160.0f;

    int32_t orientation = AConfiguration_getOrientation(_cui_context.config);

    android_print("config changed, orientation = 0x%03x, ui_scale = %f\n", orientation, ui_scale);

    if (window->orientation != orientation)
    {
        window->orientation = orientation;

        int32_t tmp = window->width;
        window->width = window->height;
        window->height = tmp;

        if (window->base.platform_root_widget)
        {
            CuiRect rect = cui_make_rect(0, 0, window->width, window->height);
            cui_widget_layout(window->base.platform_root_widget, rect);
        }

        window->base.needs_redraw = true;
    }

    if (window->base.ui_scale != ui_scale)
    {
        _cui_window_set_ui_scale(window, ui_scale);
        window->base.needs_redraw = true;
    }
}

static void
_cui_on_configuration_changed(ANativeActivity *activity)
{
    android_print("onConfigurationChanged: %p\n", (void *) activity);
    _cui_send_message(CUI_ANDROID_MESSAGE_CONFIG_CHANGED);
}

static void
_cui_on_content_rect_changed(ANativeActivity *activity, const ARect *rect)
{
    android_print("onContentRectChanged: %p (min_x = %d, min_y = %d, max_x = %d, max_y = %d)\n",
                  (void *) activity, rect->left, rect->top, rect->right, rect->bottom);
    pthread_mutex_lock(&_cui_context.platform_mutex);
    _cui_context.new_content_rect = cui_make_rect(rect->left, rect->top, rect->right, rect->bottom);
    _cui_send_message(CUI_ANDROID_MESSAGE_CONTENT_RECT_CHANGED);
    while (!cui_rect_equals(_cui_context.content_rect, _cui_context.new_content_rect))
    {
        pthread_cond_wait(&_cui_context.platform_cond, &_cui_context.platform_mutex);
    }
    pthread_mutex_unlock(&_cui_context.platform_mutex);
}

static void
_cui_on_destroy(ANativeActivity *activity)
{
    android_print("onDestroy: %p\n", (void *) activity);
#if 0
    pthread_mutex_lock(&android.platform_mutex);
    send_message(ANDROID_MESSAGE_DESTROY_ACTIVITY);
    while (android.is_running)
    {
        pthread_cond_wait(&android.platform_cond, &android.platform_mutex);
    }
    pthread_mutex_unlock(&android.platform_mutex);

    __external_data_path = make_string(0, 0);
#endif

    __external_data_path = cui_make_string(0, 0);
}

static void
_cui_on_input_queue_created(ANativeActivity *activity, AInputQueue *input_queue)
{
    android_print("onInputQueueCreated: %p\n", (void *) activity);
    _cui_set_input_queue(input_queue);
}

static void
_cui_on_input_queue_destroyed(ANativeActivity *activity, AInputQueue *input_queue)
{
    (void) input_queue;

    android_print("onInputQueueDestroyed: %p\n", (void *) activity);
    _cui_set_input_queue(0);
}

static void
_cui_on_low_memory(ANativeActivity *activity)
{
    android_print("onLowMemory: %p\n", (void *) activity);
    _cui_send_message(CUI_ANDROID_MESSAGE_LOW_MEMORY);
}

static void
_cui_on_native_window_created(ANativeActivity *activity, ANativeWindow *window)
{
    android_print("onNativeWindowCreated: %p, window = %p\n", (void *) activity, (void *) window);
    _cui_set_native_window(window);
}

static void
_cui_on_native_window_destroyed(ANativeActivity *activity, ANativeWindow *window)
{
    android_print("onNativeWindowDestroyed: %p, window = %p\n", (void *) activity, (void *) window);
    _cui_set_native_window(0);
}

static void
_cui_on_native_window_redraw_needed(ANativeActivity *activity, ANativeWindow *window)
{
    android_print("onNativeWindowRedrawNeeded: %p, window = %p\n", (void *) activity, (void *) window);
    pthread_mutex_lock(&_cui_context.platform_mutex);
    _cui_context.redraw_done = false;
    _cui_send_message(CUI_ANDROID_MESSAGE_REDRAW_NEEDED);
    while (!_cui_context.redraw_done)
    {
        pthread_cond_wait(&_cui_context.platform_cond, &_cui_context.platform_mutex);
    }
    pthread_mutex_unlock(&_cui_context.platform_mutex);
}

static void
_cui_on_native_window_resized(ANativeActivity *activity, ANativeWindow *window)
{
    android_print("onNativeWindowResized: %p, window = %p\n", (void *) activity, (void *) window);
    pthread_mutex_lock(&_cui_context.platform_mutex);
    _cui_context.redraw_done = false;
    _cui_send_message(CUI_ANDROID_MESSAGE_WINDOW_SIZE_CHANGED);
    while (!_cui_context.redraw_done)
    {
        pthread_cond_wait(&_cui_context.platform_cond, &_cui_context.platform_mutex);
    }
    pthread_mutex_unlock(&_cui_context.platform_mutex);
}

static void
_cui_on_pause(ANativeActivity *activity)
{
    android_print("onPause: %p\n", (void *) activity);
}

static void
_cui_on_resume(ANativeActivity *activity)
{
    android_print("onResume: %p\n", (void *) activity);
}

static void *
_cui_on_save_instance_state(ANativeActivity *activity, size_t *output_size)
{
    android_print("onSaveInstanceState: %p\n", (void *) activity);
    *output_size = 0;
    return 0;
}

static void
_cui_on_start(ANativeActivity *activity)
{
    android_print("onStart: %p\n", (void *) activity);
}

static void
_cui_on_stop(ANativeActivity *activity)
{
    android_print("onStop: %p\n", (void *) activity);
}

static void
_cui_on_window_focus_changed(ANativeActivity *activity, int has_focus)
{
    android_print("onWindowFocusChanged: %p, focus = %d\n", (void *) activity, has_focus);
}

static CuiFramebuffer *
_cui_acquire_framebuffer(CuiWindow *window, int32_t width, int32_t height)
{
    (void) width;
    (void) height;

    CuiAssert(window->base.renderer);
    CuiAssert(window->base.renderer->type == CUI_RENDERER_TYPE_OPENGLES2);

    eglQuerySurface(_cui_context.egl_display, window->opengles2.egl_surface, EGL_WIDTH, &window->framebuffer.width);
    eglQuerySurface(_cui_context.egl_display, window->opengles2.egl_surface, EGL_HEIGHT, &window->framebuffer.height);

#if 0
    android_print("redraw w = %d, h = %d, sw = %d, sh = %d\n", window->width, window->height, surface_width, surface_height);
#endif

    return &window->framebuffer;
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
    (void) text;
}

CuiString
cui_platform_get_clipboard_text(CuiArena *arena)
{
    (void) arena;

    CuiString result = { 0, 0 };

    return result;
}

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
    (void) temporary_memory;
    (void) arena;

    *cui_array_append(*font_dirs) = CuiStringLiteral("/system/fonts");
    // TODO: maybe starting with android 12
    // *cui_array_append(*font_dirs) = CuiStringLiteral("/data/fonts/files");
}

CuiString
cui_platform_get_data_directory(CuiArena *temporary_memory, CuiArena *arena)
{
    (void) temporary_memory;

    return cui_copy_string(arena, __external_data_path);
}

CuiString *
cui_get_files_to_open(void)
{
    return 0;
}

bool
cui_init(int argument_count, char **arguments)
{
    if (!_cui_common_init(argument_count, arguments))
    {
        return false;
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

    CuiAssert(!_cui_context.config);

    _cui_context.config = AConfiguration_new();

    _cui_context.looper = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
    ALooper_addFd(_cui_context.looper, _cui_context.read_fd, CUI_LOOPER_EVENT_TYPE_MESSAGE, ALOOPER_EVENT_INPUT, 0, 0);
    ALooper_addFd(_cui_context.looper, _cui_context.signal_fd[0], CUI_LOOPER_EVENT_TYPE_SIGNAL, ALOOPER_EVENT_INPUT, 0, 0);

    _cui_context.egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

    if (_cui_context.egl_display == EGL_NO_DISPLAY)
    {
        return false;
    }

    EGLint major_version;
    EGLint minor_version;

    if (eglInitialize(_cui_context.egl_display, &major_version, &minor_version))
    {
#if 1
        char *apis = (char *) eglQueryString(_cui_context.egl_display, EGL_CLIENT_APIS);
        char *vendor = (char *) eglQueryString(_cui_context.egl_display, EGL_VENDOR);
        char *extensions = (char *) eglQueryString(_cui_context.egl_display, EGL_EXTENSIONS);

        android_print("EGL version %d.%d\n", major_version, minor_version);
        android_print("EGL APIs: %s\n", apis);
        android_print("EGL Vendor: %s\n", vendor);
        android_print("EGL Extensions: %s\n", extensions);
#endif
    }
    else
    {
        _cui_context.egl_display = EGL_NO_DISPLAY;
    }

    pthread_mutex_lock(&_cui_context.platform_mutex);

    _cui_context.startup_finished = true;

    pthread_cond_broadcast(&_cui_context.platform_cond);
    pthread_mutex_unlock(&_cui_context.platform_mutex);

    return true;
}

CuiWindow *
cui_window_create(uint32_t creation_flags)
{
    CuiWindow *window = 0;

    if (!_cui_context.window)
    {
        window = _cui_add_window(creation_flags);

        window->base.ui_scale = 1.0f;
        window->orientation = ACONFIGURATION_ORIENTATION_ANY;

        _cui_update_configuration(window);

        window->font_id = _cui_font_manager_find_font(&window->base.temporary_memory, &window->base.font_manager, window->base.ui_scale,
                                                      cui_make_sized_font_spec(CuiStringLiteral("Roboto-Regular"), 18.0f, 1.0f),
                                                      cui_make_sized_font_spec(CuiStringLiteral("SourceSansPro-Regular"), 20.0f, 1.0f));

        cui_arena_allocate(&window->arena, CuiKiB(8));

        window->texture_state_count = 0;
        window->max_texture_state_count = _CUI_MAX_TEXTURE_OPERATION_COUNT;
        window->texture_states = cui_alloc_array(&window->arena, CuiTextureState, window->max_texture_state_count, CuiDefaultAllocationParams());

        {
            window->north_split = cui_alloc_type(&window->arena, CuiWidget, CuiDefaultAllocationParams());

            cui_widget_init(window->north_split, CUI_WIDGET_TYPE_BOX);
            cui_widget_set_main_axis(window->north_split, CUI_AXIS_Y);
            cui_widget_set_y_axis_gravity(window->north_split, CUI_GRAVITY_START);

            window->north_box = cui_alloc_type(&window->arena, CuiWidget, CuiDefaultAllocationParams());

            cui_widget_init(window->north_box, CUI_WIDGET_TYPE_BOX);
            cui_widget_set_main_axis(window->north_box, CUI_AXIS_Y);
            cui_widget_set_y_axis_gravity(window->north_box, CUI_GRAVITY_START);
            cui_widget_add_flags(window->north_box, CUI_WIDGET_FLAG_DRAW_BACKGROUND | CUI_WIDGET_FLAG_FIXED_HEIGHT);
            cui_widget_set_preferred_size(window->north_box, 0.0f, 0.0f);

            window->north_box->color_normal_background = CUI_COLOR_WINDOW_TITLEBAR_BACKGROUND;

            cui_widget_append_child(window->north_split, window->north_box);
        }

        {
            window->east_split = cui_alloc_type(&window->arena, CuiWidget, CuiDefaultAllocationParams());

            cui_widget_init(window->east_split, CUI_WIDGET_TYPE_BOX);
            cui_widget_set_main_axis(window->east_split, CUI_AXIS_X);
            cui_widget_set_x_axis_gravity(window->east_split, CUI_GRAVITY_END);

            window->east_box = cui_alloc_type(&window->arena, CuiWidget, CuiDefaultAllocationParams());

            cui_widget_init(window->east_box, CUI_WIDGET_TYPE_BOX);
            cui_widget_set_main_axis(window->east_box, CUI_AXIS_Y);
            cui_widget_set_y_axis_gravity(window->east_box, CUI_GRAVITY_START);
            cui_widget_add_flags(window->east_box, CUI_WIDGET_FLAG_DRAW_BACKGROUND | CUI_WIDGET_FLAG_FIXED_WIDTH);
            cui_widget_set_preferred_size(window->east_box, 0.0f, 0.0f);

            window->east_box->color_normal_background = CUI_COLOR_WINDOW_TITLEBAR_BACKGROUND;

            cui_widget_append_child(window->east_split, window->east_box);
            cui_widget_append_child(window->north_split, window->east_split);
        }

        {
            window->south_split = cui_alloc_type(&window->arena, CuiWidget, CuiDefaultAllocationParams());

            cui_widget_init(window->south_split, CUI_WIDGET_TYPE_BOX);
            cui_widget_set_main_axis(window->south_split, CUI_AXIS_Y);
            cui_widget_set_y_axis_gravity(window->south_split, CUI_GRAVITY_END);

            window->south_box = cui_alloc_type(&window->arena, CuiWidget, CuiDefaultAllocationParams());

            cui_widget_init(window->south_box, CUI_WIDGET_TYPE_BOX);
            cui_widget_set_main_axis(window->south_box, CUI_AXIS_Y);
            cui_widget_set_y_axis_gravity(window->south_box, CUI_GRAVITY_START);
            cui_widget_add_flags(window->south_box, CUI_WIDGET_FLAG_DRAW_BACKGROUND | CUI_WIDGET_FLAG_FIXED_HEIGHT);
            cui_widget_set_preferred_size(window->south_box, 0.0f, 0.0f);

            window->south_box->color_normal_background = CUI_COLOR_WINDOW_TITLEBAR_BACKGROUND;

            cui_widget_append_child(window->south_split, window->south_box);
            cui_widget_append_child(window->east_split, window->south_split);
        }

        {
            window->west_split = cui_alloc_type(&window->arena, CuiWidget, CuiDefaultAllocationParams());

            cui_widget_init(window->west_split, CUI_WIDGET_TYPE_BOX);
            cui_widget_set_main_axis(window->west_split, CUI_AXIS_X);
            cui_widget_set_x_axis_gravity(window->west_split, CUI_GRAVITY_START);

            window->west_box = cui_alloc_type(&window->arena, CuiWidget, CuiDefaultAllocationParams());

            cui_widget_init(window->west_box, CUI_WIDGET_TYPE_BOX);
            cui_widget_set_main_axis(window->west_box, CUI_AXIS_Y);
            cui_widget_set_y_axis_gravity(window->west_box, CUI_GRAVITY_START);
            cui_widget_add_flags(window->west_box, CUI_WIDGET_FLAG_DRAW_BACKGROUND | CUI_WIDGET_FLAG_FIXED_WIDTH);
            cui_widget_set_preferred_size(window->west_box, 0.0f, 0.0f);

            window->west_box->color_normal_background = CUI_COLOR_WINDOW_TITLEBAR_BACKGROUND;

            cui_widget_append_child(window->west_split, window->west_box);
            cui_widget_append_child(window->south_split, window->west_split);
        }

        {
            CuiWidget *dummy_user_root_widget = cui_alloc_type(&window->arena, CuiWidget, CuiDefaultAllocationParams());
            cui_widget_init(dummy_user_root_widget, CUI_WIDGET_TYPE_BOX);

            cui_widget_append_child(window->west_split, dummy_user_root_widget);

            window->base.user_root_widget = dummy_user_root_widget;
        }

        {
            cui_widget_set_window(window->north_split, window);
            cui_widget_set_ui_scale(window->north_split, window->base.ui_scale);

            window->base.platform_root_widget = window->north_split;
        }

        _cui_context.window = window;
    }

    return window;
}

void
cui_window_set_title(CuiWindow *window, CuiString title)
{
    (void) window;
    (void) title;
}

void
cui_window_resize(CuiWindow *window, int32_t width, int32_t height)
{
    (void) window;
    (void) width;
    (void) height;
}

void
cui_window_set_fullscreen(CuiWindow *window, bool fullscreen)
{
    (void) window;
    (void) fullscreen;
}

void
cui_window_show(CuiWindow *window)
{
    (void) window;
}

float
cui_window_get_titlebar_height(CuiWindow *window)
{
    (void) window;

    return 0.0f;
}

void
cui_window_close(CuiWindow *window)
{
    (void) window;
}

void
cui_window_destroy(CuiWindow *window)
{
    _cui_context.window = 0;
    cui_arena_deallocate(&window->arena);
    _cui_remove_window(window);
}

void
cui_window_set_root_widget(CuiWindow *window, CuiWidget *widget)
{
    CuiAssert(widget);
    CuiAssert(window->base.user_root_widget);

    cui_widget_replace_child(window->west_split, window->base.user_root_widget, widget);

    window->base.user_root_widget = widget;

    CuiRect rect = cui_make_rect(0, 0, window->width, window->height);

    cui_widget_set_window(window->base.user_root_widget, window);
    cui_widget_set_ui_scale(window->base.user_root_widget, window->base.ui_scale);
    cui_widget_layout(window->base.platform_root_widget, rect);
}

void
cui_window_set_cursor(CuiWindow *window, CuiCursorType cursor_type)
{
    if (window->base.cursor == cursor_type)
    {
        return;
    }

    window->base.cursor = cursor_type;

    // TODO:
}

void
cui_step(void)
{
    CuiAssert(_cui_context.window);

    CuiWindow *window = _cui_context.window;

    int event_type, events;
    int next_timeout = -1;

    while ((event_type = ALooper_pollAll(next_timeout, 0, &events, 0)) >= 0)
    {
        next_timeout = 0;

        switch (event_type)
        {
            case CUI_LOOPER_EVENT_TYPE_MESSAGE:
            {
                uint8_t message_type = _cui_receive_message();

                switch (message_type)
                {
                    case CUI_ANDROID_MESSAGE_WINDOW_CHANGED:
                    {
                        pthread_mutex_lock(&_cui_context.platform_mutex);

                        if (_cui_context.native_window)
                        {
                            _cui_window_destroy_renderer(window);
                        }

                        _cui_context.native_window = _cui_context.new_native_window;

                        if (_cui_context.native_window)
                        {
                            window->width  = ANativeWindow_getWidth(_cui_context.native_window);
                            window->height = ANativeWindow_getHeight(_cui_context.native_window);

                            _cui_window_create_renderer(window);
                        }

                        pthread_cond_broadcast(&_cui_context.platform_cond);
                        pthread_mutex_unlock(&_cui_context.platform_mutex);
                    } break;

                    case CUI_ANDROID_MESSAGE_WINDOW_SIZE_CHANGED:
                    {
                        if (_cui_context.native_window)
                        {
                            pthread_mutex_lock(&_cui_context.platform_mutex);

                            window->width  = ANativeWindow_getWidth(_cui_context.native_window);
                            window->height = ANativeWindow_getHeight(_cui_context.native_window);

                            android_print("window resized (w = %d, h = %d)\n", window->width, window->height);

                            if (window->base.platform_root_widget)
                            {
                                CuiRect rect = cui_make_rect(0, 0, window->width, window->height);
                                cui_widget_layout(window->base.platform_root_widget, rect);
                            }

                            // The framebuffer size is queried in _cui_acquire_framebuffer()
                            _cui_window_draw(window, 0, 0);

                            switch (window->base.renderer->type)
                            {
                                case CUI_RENDERER_TYPE_SOFTWARE:
                                {
                                    CuiAssert(!"CUI_RENDERER_TYPE_SOFTWARE not supported.");
                                } break;

                                case CUI_RENDERER_TYPE_OPENGLES2:
                                {
#  if CUI_RENDERER_OPENGLES2_ENABLED
                                    eglSwapBuffers(_cui_context.egl_display, window->opengles2.egl_surface);
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

                            window->base.needs_redraw = false;

                            _cui_context.redraw_done = true;

                            pthread_cond_broadcast(&_cui_context.platform_cond);
                            pthread_mutex_unlock(&_cui_context.platform_mutex);
                        }
                    } break;

                    case CUI_ANDROID_MESSAGE_INPUT_QUEUE_CHANGED:
                    {
                        pthread_mutex_lock(&_cui_context.platform_mutex);

                        if (_cui_context.input_queue)
                        {
                            AInputQueue_detachLooper(_cui_context.input_queue);
                        }

                        _cui_context.input_queue = _cui_context.new_input_queue;

                        if (_cui_context.input_queue)
                        {
                            AInputQueue_attachLooper(_cui_context.input_queue, _cui_context.looper,
                                                     CUI_LOOPER_EVENT_TYPE_INPUT_QUEUE, 0, 0);
                        }

                        pthread_cond_broadcast(&_cui_context.platform_cond);
                        pthread_mutex_unlock(&_cui_context.platform_mutex);
                    } break;

                    case CUI_ANDROID_MESSAGE_CONTENT_RECT_CHANGED:
                    {
                        pthread_mutex_lock(&_cui_context.platform_mutex);
                        _cui_context.content_rect = _cui_context.new_content_rect;
                        pthread_cond_broadcast(&_cui_context.platform_cond);
                        pthread_mutex_unlock(&_cui_context.platform_mutex);

                        android_print("content_rect changed: (min_x = %d, min_y = %d, max_x = %d, max_y = %d)\n",
                                      _cui_context.content_rect.min.x, _cui_context.content_rect.min.y,
                                      _cui_context.content_rect.max.x, _cui_context.content_rect.max.y);

                        float north_height = ceilf((float) _cui_context.content_rect.min.y / window->base.ui_scale);
                        float east_width   = ceilf((float) (window->width - _cui_context.content_rect.max.x) / window->base.ui_scale);
                        float south_height = ceilf((float) (window->height - _cui_context.content_rect.max.y) / window->base.ui_scale);
                        float west_width   = ceilf((float) _cui_context.content_rect.min.x / window->base.ui_scale);

                        cui_widget_set_preferred_size(window->north_box, 0.0f, north_height);
                        cui_widget_set_preferred_size(window->east_box, east_width, 0.0f);
                        cui_widget_set_preferred_size(window->south_box, 0.0f, south_height);
                        cui_widget_set_preferred_size(window->west_box, west_width, 0.0f);

                        CuiRect rect = cui_make_rect(0, 0, window->width, window->height);
                        cui_widget_layout(window->base.platform_root_widget, rect);

                        window->base.needs_redraw = true;

#if 0
                        android.input.event_flags |= EVENT_FLAG_SIZE_CHANGED;
                        android.input.window_width = get_width(android.content_rect);
                        android.input.window_height = get_height(android.content_rect);
#endif
                    } break;

                    case CUI_ANDROID_MESSAGE_LOW_MEMORY:
                    {
                    } break;

                    case CUI_ANDROID_MESSAGE_REDRAW_NEEDED:
                    {
                        pthread_mutex_lock(&_cui_context.platform_mutex);

                        // The framebuffer size is queried in _cui_acquire_framebuffer()
                        _cui_window_draw(window, 0, 0);

                        switch (window->base.renderer->type)
                        {
                            case CUI_RENDERER_TYPE_SOFTWARE:
                            {
                                CuiAssert(!"CUI_RENDERER_TYPE_SOFTWARE not supported.");
                            } break;

                            case CUI_RENDERER_TYPE_OPENGLES2:
                            {
#  if CUI_RENDERER_OPENGLES2_ENABLED
                                eglSwapBuffers(_cui_context.egl_display, window->opengles2.egl_surface);
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

                        window->base.needs_redraw = false;

                        _cui_context.redraw_done = true;

                        pthread_cond_broadcast(&_cui_context.platform_cond);
                        pthread_mutex_unlock(&_cui_context.platform_mutex);
                    } break;

                    case CUI_ANDROID_MESSAGE_CONFIG_CHANGED:
                    {
                        _cui_update_configuration(window);
                    } break;

                    case CUI_ANDROID_MESSAGE_DESTROY_ACTIVITY:
                    {
                    } break;
                }
            } break;

            case CUI_LOOPER_EVENT_TYPE_SIGNAL:
            {
                uint8_t buffer[8];
                while (read(_cui_context.signal_fd[0], buffer, sizeof(buffer)) == sizeof(buffer));

                if (_cui_context.common.signal_callback)
                {
                    _cui_context.common.signal_callback();
                }
            } break;

            case CUI_LOOPER_EVENT_TYPE_INPUT_QUEUE:
            {
                AInputEvent* ev;

                if ((AInputQueue_getEvent(_cui_context.input_queue, &ev) >= 0) &&
                    !AInputQueue_preDispatchEvent(_cui_context.input_queue, ev))
                {
                    switch (AInputEvent_getType(ev))
                    {
                        case AINPUT_EVENT_TYPE_KEY:
                        {
                            int32_t action = AKeyEvent_getAction(ev);
                            int32_t keycode = AKeyEvent_getKeyCode(ev);

                            switch (action)
                            {
                                case AKEY_EVENT_ACTION_DOWN:
                                {

#define _CUI_KEY_DOWN_EVENT(key_id)                             \
    window->base.event.key.codepoint       = (key_id);          \
    window->base.event.key.alt_is_down     = false;             \
    window->base.event.key.ctrl_is_down    = false;             \
    window->base.event.key.shift_is_down   = false;             \
    window->base.event.key.command_is_down = false;             \
    cui_window_handle_event(window, CUI_EVENT_TYPE_KEY_DOWN);

                                    switch (keycode)
                                    {
                                        case AKEYCODE_F1:   case AKEYCODE_F2:   case AKEYCODE_F3:   case AKEYCODE_F4:
                                        case AKEYCODE_F5:   case AKEYCODE_F6:   case AKEYCODE_F7:   case AKEYCODE_F8:
                                        case AKEYCODE_F9:   case AKEYCODE_F10:  case AKEYCODE_F11:  case AKEYCODE_F12:
                                        {
                                            _CUI_KEY_DOWN_EVENT(CUI_KEY_F1 + (keycode - AKEYCODE_F1));
                                        } break;
                                    }

#undef _CUI_KEY_DOWN_EVENT
                                } break;

#if 0
                                case AKEY_EVENT_ACTION_MULTIPLE:
                                {
                                    android_print("multiple 0 = %d (0x%08x)\n", keycode, keycode);

                                    int32_t repeat_count = AKeyEvent_getRepeatCount(ev);

                                    for (int32_t i = 0; i < repeat_count; i += 1)
                                    {
                                        android_print("multiple %d = %d (0x%08x)\n", i + 1, keycode, keycode);
                                    }
                                } break;
#endif

                                case AKEY_EVENT_ACTION_UP:
                                {
                                } break;
                            }
                        } break;

                        case AINPUT_EVENT_TYPE_MOTION:
                        {
                            int32_t action = AMotionEvent_getAction(ev) & AMOTION_EVENT_ACTION_MASK;
                            int32_t pointer_index = (AMotionEvent_getAction(ev) & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >>
                                                    AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;

                            switch (action)
                            {
                                case AMOTION_EVENT_ACTION_DOWN:
                                {
                                    window->base.event.pointer.index = pointer_index;
                                    window->base.event.pointer.position.x = (int32_t) AMotionEvent_getX(ev, pointer_index);
                                    window->base.event.pointer.position.y = (int32_t) AMotionEvent_getY(ev, pointer_index);
                                    cui_window_handle_event(window, CUI_EVENT_TYPE_POINTER_DOWN);
                                } break;

                                case AMOTION_EVENT_ACTION_UP:
                                {
                                    window->base.event.pointer.index = pointer_index;
                                    window->base.event.pointer.position.x = (int32_t) AMotionEvent_getX(ev, pointer_index);
                                    window->base.event.pointer.position.y = (int32_t) AMotionEvent_getY(ev, pointer_index);
                                    cui_window_handle_event(window, CUI_EVENT_TYPE_POINTER_UP);
                                } break;

                                case AMOTION_EVENT_ACTION_MOVE:
                                {
                                    window->base.event.pointer.index = pointer_index;
                                    window->base.event.pointer.position.x = (int32_t) AMotionEvent_getX(ev, pointer_index);
                                    window->base.event.pointer.position.y = (int32_t) AMotionEvent_getY(ev, pointer_index);
                                    cui_window_handle_event(window, CUI_EVENT_TYPE_POINTER_MOVE);
                                } break;
                            }
                        } break;
                    }

                    AInputQueue_finishEvent(_cui_context.input_queue, ev, false);
                }
            } break;
        }
    }

    for (uint32_t window_index = 0;
         window_index < _cui_context.common.window_count; window_index += 1)
    {
        CuiWindow *window = _cui_context.common.windows[window_index];

        if (_cui_context.native_window && window->base.needs_redraw)
        {
            // The framebuffer size is queried in _cui_acquire_framebuffer()
            _cui_window_draw(window, 0, 0);

            switch (window->base.renderer->type)
            {
                case CUI_RENDERER_TYPE_SOFTWARE:
                {
                    CuiAssert(!"CUI_RENDERER_TYPE_SOFTWARE not supported.");
                } break;

                case CUI_RENDERER_TYPE_OPENGLES2:
                {
#  if CUI_RENDERER_OPENGLES2_ENABLED
                    eglSwapBuffers(_cui_context.egl_display, window->opengles2.egl_surface);
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

            window->base.needs_redraw = false;
        }
    }
}

JNIEXPORT void
ANativeActivity_onCreate(ANativeActivity *activity, void *saved_state, size_t saved_state_size)
{
    (void) saved_state;
    (void) saved_state_size;

    android_print("onCreate: %p\n", (void *) activity);

    _cui_context.native_activity = activity;

    android_print("externalDataPath: '%s'\n", activity->externalDataPath);
    android_print("internalDataPath: '%s'\n", activity->internalDataPath);

    __external_data_path = CuiCString(activity->externalDataPath);

    activity->callbacks->onConfigurationChanged     = _cui_on_configuration_changed;
    activity->callbacks->onContentRectChanged       = _cui_on_content_rect_changed;
    activity->callbacks->onDestroy                  = _cui_on_destroy;
    activity->callbacks->onInputQueueCreated        = _cui_on_input_queue_created;
    activity->callbacks->onInputQueueDestroyed      = _cui_on_input_queue_destroyed;
    activity->callbacks->onLowMemory                = _cui_on_low_memory;
    activity->callbacks->onNativeWindowCreated      = _cui_on_native_window_created;
    activity->callbacks->onNativeWindowDestroyed    = _cui_on_native_window_destroyed;
    activity->callbacks->onNativeWindowRedrawNeeded = _cui_on_native_window_redraw_needed;
    activity->callbacks->onNativeWindowResized      = _cui_on_native_window_resized;
    activity->callbacks->onPause                    = _cui_on_pause;
    activity->callbacks->onResume                   = _cui_on_resume;
    activity->callbacks->onSaveInstanceState        = _cui_on_save_instance_state;
    activity->callbacks->onStart                    = _cui_on_start;
    activity->callbacks->onStop                     = _cui_on_stop;
    activity->callbacks->onWindowFocusChanged       = _cui_on_window_focus_changed;

    if (_cui_context.was_created)
    {
    }
    else
    {
        pthread_cond_init(&_cui_context.platform_cond, 0);
        pthread_mutex_init(&_cui_context.platform_mutex, 0);

        int message_pipe[2];

        if (pipe(message_pipe))
        {
            return;
        }

        _cui_context.read_fd = message_pipe[0];
        _cui_context.write_fd = message_pipe[1];

        pthread_t platform_thread;
        pthread_attr_t thread_attr;

        pthread_attr_init(&thread_attr);
        pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);

        pthread_create(&platform_thread, &thread_attr, _cui_android_main, 0);

        pthread_mutex_lock(&_cui_context.platform_mutex);
        while (!_cui_context.startup_finished)
        {
            pthread_cond_wait(&_cui_context.platform_cond, &_cui_context.platform_mutex);
        }
        pthread_mutex_unlock(&_cui_context.platform_mutex);

        _cui_context.was_created = true;
    }
}
