#include <android/log.h>
#include <android/looper.h>
#include <android/configuration.h>
#include <android/native_activity.h>

#define android_print(...) ((void) __android_log_print(ANDROID_LOG_INFO, "cui_android", __VA_ARGS__))

typedef enum CuiLooperEventType
{
    CUI_LOOPER_EVENT_TYPE_MESSAGE     = 1,
    CUI_LOOPER_EVENT_TYPE_SIGNAL      = 2,
    CUI_LOOPER_EVENT_TYPE_INPUT_QUEUE = 3,
} CuiLooperEventType;

typedef enum CuiAndroidMessageType
{
    CUI_ANDROID_MESSAGE_UNKNOWN              = 0,
    CUI_ANDROID_MESSAGE_WINDOW_CHANGED       = 1,
    CUI_ANDROID_MESSAGE_WINDOW_SIZE_CHANGED  = 2,
    CUI_ANDROID_MESSAGE_INPUT_QUEUE_CHANGED  = 3,
    CUI_ANDROID_MESSAGE_CONTENT_RECT_CHANGED = 4,
    CUI_ANDROID_MESSAGE_LOW_MEMORY           = 5,
    CUI_ANDROID_MESSAGE_REDRAW_NEEDED        = 6,
    CUI_ANDROID_MESSAGE_CONFIG_CHANGED       = 7,
    CUI_ANDROID_MESSAGE_DESTROY_ACTIVITY     = 8,
} CuiAndroidMessageType;

struct CuiWindow
{
    CuiWindowBase base;

    CuiArena arena;

    int32_t orientation;

    int32_t width;
    int32_t height;

    CuiFontId font_id;

    CuiWidget *north_split;
    CuiWidget *north_box;

    CuiWidget *west_split;
    CuiWidget *west_box;

    CuiWidget *south_split;
    CuiWidget *south_box;

    CuiWidget *east_split;
    CuiWidget *east_box;

    uint32_t texture_state_count;
    uint32_t max_texture_state_count;
    CuiTextureState *texture_states;

    union
    {

#if CUI_RENDERER_OPENGLES2_ENABLED

        struct
        {
            CuiRendererOpengles2 *renderer_opengles2;

            EGLSurface egl_surface;
            EGLContext egl_context;
        } opengles2;

#endif

    } renderer;
};

typedef struct CuiContext
{
    CuiContextCommon common;

    int signal_fd[2];

    bool redraw_done;
    bool was_created;
    bool startup_finished;

    int read_fd, write_fd;

    pthread_cond_t platform_cond;
    pthread_mutex_t platform_mutex;

    ALooper *looper;
    AConfiguration *config;
    ANativeActivity *native_activity;

    AInputQueue *input_queue;
    AInputQueue *new_input_queue;

    ANativeWindow *native_window;
    ANativeWindow *new_native_window;

    CuiRect content_rect;
    CuiRect new_content_rect;

    // NOTE: There can only be one window
    CuiWindow *window;

#if CUI_RENDERER_OPENGLES2_ENABLED

    EGLDisplay egl_display;

#endif

} CuiContext;
