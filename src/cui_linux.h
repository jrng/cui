#ifndef CUI_NO_BACKEND

typedef int  gint;
typedef gint gboolean;

typedef struct GSList GSList;

struct GSList
{
    void *data;
    GSList *next;
};

typedef struct GMainContext GMainContext;

typedef struct GtkDialog GtkDialog;
typedef struct GtkWidget GtkWidget;
typedef struct GtkWindow GtkWindow;
typedef struct GtkFileChooser GtkFileChooser;

typedef enum
{
    GTK_FILE_CHOOSER_ACTION_OPEN          = 0,
    GTK_FILE_CHOOSER_ACTION_SAVE          = 1,
    GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER = 2,
    GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER = 3,
} GtkFileChooserAction;

typedef enum
{
    GTK_RESPONSE_NONE         = -1,
    GTK_RESPONSE_REJECT       = -2,
    GTK_RESPONSE_ACCEPT       = -3,
    GTK_RESPONSE_DELETE_EVENT = -4,
    GTK_RESPONSE_OK           = -5,
    GTK_RESPONSE_CANCEL       = -6,
    GTK_RESPONSE_CLOSE        = -7,
    GTK_RESPONSE_YES          = -8,
    GTK_RESPONSE_NO           = -9,
    GTK_RESPONSE_APPLY        = -10,
    GTK_RESPONSE_HELP         = -11,
} GtkResponseType;

typedef void (*PFN_g_free)(void *mem);
typedef void (*PFN_g_slist_free)(GSList *list);
typedef gboolean (*PFN_g_main_context_iteration)(GMainContext *context, gboolean may_block);

typedef void (*PFN_gtk_init)(int *argc, char ***argv);
typedef void (*PFN_gtk_widget_destroy)(GtkWidget *widget);
typedef gint (*PFN_gtk_dialog_run)(GtkDialog *dialog);
typedef GtkWidget *(*PFN_gtk_file_chooser_dialog_new)(const char *title, GtkWindow *parent, GtkFileChooserAction action, const char *first_button_text, ...);
typedef void (*PFN_gtk_file_chooser_set_select_multiple)(GtkFileChooser *chooser, gboolean select_multiple);
typedef GSList *(*PFN_gtk_file_chooser_get_filenames)(GtkFileChooser *chooser);

#if CUI_BACKEND_X11_ENABLED

typedef struct CuiX11DesktopSettings
{
    int32_t ui_scale;
    int32_t double_click_time; // ms
} CuiX11DesktopSettings;

#include <X11/Xlib.h>
#include <X11/extensions/sync.h>
#  if CUI_RENDERER_SOFTWARE_ENABLED
#include <X11/extensions/XShm.h>
#  endif

typedef struct CuiX11FrameCompletionData
{
    int event_type;
    Window window;
} CuiX11FrameCompletionData;

#endif

#if CUI_BACKEND_WAYLAND_ENABLED

#  if  CUI_RENDERER_OPENGLES2_ENABLED
#include <wayland-egl.h>
#  endif

#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-compose.h>

#include "xdg-shell.h"
#include "viewporter.h"
#include "xdg-decoration-unstable-v1.h"

typedef struct CuiWaylandMonitor
{
    uint32_t id;
    int32_t scale;
    int32_t pending_scale;
    struct wl_output *output;
} CuiWaylandMonitor;

typedef struct CuiWaylandCursorTheme
{
    int32_t ref_count;

    struct wl_cursor_theme *cursor_theme;

    struct wl_cursor *wayland_cursor_none;
    struct wl_cursor *wayland_cursor_arrow;
    struct wl_cursor *wayland_cursor_hand;
    struct wl_cursor *wayland_cursor_text;
    struct wl_cursor *wayland_cursor_move_all;
    struct wl_cursor *wayland_cursor_move_left_right;
    struct wl_cursor *wayland_cursor_move_up_down;
    struct wl_cursor *wayland_cursor_resize_top;
    struct wl_cursor *wayland_cursor_resize_bottom;
    struct wl_cursor *wayland_cursor_resize_left;
    struct wl_cursor *wayland_cursor_resize_right;
    struct wl_cursor *wayland_cursor_resize_top_left;
    struct wl_cursor *wayland_cursor_resize_top_right;
    struct wl_cursor *wayland_cursor_resize_bottom_left;
    struct wl_cursor *wayland_cursor_resize_bottom_right;
} CuiWaylandCursorTheme;

typedef enum CuiWaylandPointerEventFlags
{
    CUI_WAYLAND_POINTER_EVENT_ENTER         = (1 << 0),
    CUI_WAYLAND_POINTER_EVENT_LEAVE         = (1 << 1),
    CUI_WAYLAND_POINTER_EVENT_MOTION        = (1 << 2),
    CUI_WAYLAND_POINTER_EVENT_BUTTON        = (1 << 3),
    CUI_WAYLAND_POINTER_EVENT_AXIS          = (1 << 4),
    CUI_WAYLAND_POINTER_EVENT_AXIS_DISCRETE = (1 << 5),
} CuiWaylandPointerEventFlags;

typedef struct CuiWaylandPointerEvent
{
    uint32_t flags;
    uint32_t serial;
    uint32_t time;

    uint32_t button;
    uint32_t state;

    uint32_t axis_source;

    wl_fixed_t x, y;
    float axis_x, axis_y;
    int32_t axis_discrete_x, axis_discrete_y;

    CuiWindow *window;
} CuiWaylandPointerEvent;

typedef enum CuiWaylandPointerButton
{
    CUI_WAYLAND_POINTER_BUTTON_LEFT   = (1 << 0),
    CUI_WAYLAND_POINTER_BUTTON_MIDDLE = (1 << 1),
    CUI_WAYLAND_POINTER_BUTTON_RIGHT  = (1 << 2),
} CuiWaylandPointerButton;

typedef struct CuiWaylandTouchPoint
{
    int32_t index;
    CuiWindow *window;
    CuiPoint position;
} CuiWaylandTouchPoint;

typedef struct CuiWaylandTouchEvent
{
    CuiEventType type;
    int32_t index;
    uint32_t serial;
    CuiWindow *window;
    CuiPoint position;
} CuiWaylandTouchEvent;

#endif

typedef enum CuiLinuxBackend
{
    CUI_LINUX_BACKEND_NONE    = 0,
#if CUI_BACKEND_WAYLAND_ENABLED
    CUI_LINUX_BACKEND_WAYLAND = 1,
#endif
#if CUI_BACKEND_X11_ENABLED
    CUI_LINUX_BACKEND_X11     = 2,
#endif
} CuiLinuxBackend;

typedef struct CuiLinuxFramebuffer
{
    CuiFramebuffer base;

#if CUI_RENDERER_SOFTWARE_ENABLED

    bool is_busy;
    int64_t shared_memory_size;

    union
    {

#  if CUI_BACKEND_X11_ENABLED

        struct
        {
            XShmSegmentInfo shared_memory_info;
        } x11;

#  endif

#  if CUI_BACKEND_WAYLAND_ENABLED

        struct
        {
            int shared_memory_fd;
            struct wl_buffer *buffer;
            struct wl_shm_pool *shared_memory_pool;
        } wayland;

#  endif

    } backend;

#endif

} CuiLinuxFramebuffer;

struct CuiWindow
{
    CuiWindowBase base;

    CuiArena arena;

    int32_t backbuffer_scale;

    CuiRect backbuffer_rect;
    CuiRect layout_rect;
    CuiRect window_rect;
    CuiRect content_rect;

    CuiFontId font_id;

    bool is_mapped;

    int32_t windowed_width;
    int32_t windowed_height;

    int64_t last_left_click_time;
    int64_t last_touch_down_time;

#if CUI_BACKEND_X11_ENABLED

    bool first_frame;

    int32_t windowed_x;
    int32_t windowed_y;

    Window x11_window;
    XIC x11_input_context;

    XSyncCounter basic_frame_counter;

    uint64_t x11_configure_serial;
    uint64_t x11_sync_request_serial;

#endif

#if CUI_BACKEND_WAYLAND_ENABLED

    CuiTemporaryMemory title_temp_memory;

    bool do_client_side_decoration;
    bool pending_do_client_side_decoration;

    uint32_t pointer_button_mask;

    CuiRect wayland_backbuffer_rect;
    CuiRect wayland_window_rect;
    CuiRect wayland_input_rect;

    CuiWidget *titlebar;
    CuiWidget *button_layer;
    CuiWidget *minimize_button;
    CuiWidget *maximize_button;
    CuiWidget *close_button;
    CuiWidget *title;

    CuiWidget *client_side_decoration_root_widget;
    CuiWidget *client_side_decoration_dummy_user_root_widget;

    struct wl_surface *wayland_surface;
    struct xdg_surface *wayland_xdg_surface;
    struct xdg_toplevel *wayland_xdg_toplevel;
    struct wp_viewport *wayland_viewport;
    struct zxdg_toplevel_decoration_v1 *wayland_xdg_decoration;

#  if CUI_RENDERER_OPENGLES2_ENABLED

    struct wl_egl_window *wayland_egl_window;

#  endif

    uint32_t *wayland_monitors;

    uint32_t wayland_configure_serial;

    int32_t pending_window_width;
    int32_t pending_window_height;

    uint32_t pending_window_state;

    int32_t wayland_cursor_buffer_scale;

#endif

#if CUI_RENDERER_SOFTWARE_ENABLED
    CuiLinuxFramebuffer *current_framebuffer;
    CuiLinuxFramebuffer framebuffers[4];
#else
    CuiLinuxFramebuffer framebuffers[1];
#endif

#if CUI_RENDERER_OPENGLES2_ENABLED

    struct
    {
        EGLSurface egl_surface;
        EGLContext egl_context;
    } opengles2;

#endif
};

typedef struct CuiContext
{
    CuiContextCommon common;

    CuiLinuxBackend backend;

    int signal_fd[2];

    int32_t double_click_time;

    CuiCursorType current_cursor;

    void *gtk_lib;
    bool gtk_initialized;

#if CUI_BACKEND_X11_ENABLED

    Display *x11_display;

    int32_t x11_desktop_scale;
    int x11_default_screen;

    Window x11_root_window;
    Window x11_settings_window;
    Window x11_clipboard_window;

    CuiArena clipboard_arena;
    CuiString clipboard_data;

    GC x11_default_gc;
    XIM x11_input_method;

    Atom x11_atom_atom;
    Atom x11_atom_manager;
    Atom x11_atom_targets;
    Atom x11_atom_cardinal;
    Atom x11_atom_xsel_data;
    Atom x11_atom_clipboard;
    Atom x11_atom_utf8_string;
    Atom x11_atom_wm_protocols;
    Atom x11_atom_wm_delete_window;
    Atom x11_atom_wm_sync_request;
    Atom x11_atom_wm_sync_request_counter;
    Atom x11_atom_wm_state;
    Atom x11_atom_wm_state_add;
    Atom x11_atom_wm_state_remove;
    Atom x11_atom_wm_state_fullscreen;
    Atom x11_atom_wm_fullscreen_monitors;
    Atom x11_atom_xsettings_screen;
    Atom x11_atom_xsettings_settings;

    Cursor x11_cursor_none;
    Cursor x11_cursor_arrow;
    Cursor x11_cursor_hand;
    Cursor x11_cursor_text;
    Cursor x11_cursor_move_all;
    Cursor x11_cursor_move_left_right;
    Cursor x11_cursor_move_up_down;

    bool has_xsync_extension;
#  if CUI_RENDERER_SOFTWARE_ENABLED
    bool has_shared_memory_extension;

    int x11_frame_completion_event;
#  endif

#endif

#if CUI_BACKEND_WAYLAND_ENABLED

    CuiWaylandPointerEvent wayland_pointer_event;

    int32_t wayland_keyboard_repeat_period;
    int32_t wayland_keyboard_repeat_delay;

    uint32_t wayland_pointer_serial;
    CuiCursorType wayland_platform_cursor;

    CuiPoint wayland_platform_mouse_position;
    CuiPoint wayland_application_mouse_position;

    CuiWaylandTouchPoint *wayland_touch_points;
    CuiWaylandTouchEvent *wayland_touch_events;

    uint32_t wayland_seat_version;

    struct wl_display *wayland_display;
    struct wl_compositor *wayland_compositor;
    struct wl_shm *wayland_shared_memory;
    struct wl_data_device_manager *wayland_data_device_manager;
    struct wl_seat *wayland_seat;
    struct wl_touch *wayland_touch;
    struct wl_pointer *wayland_pointer;
    struct wl_keyboard *wayland_keyboard;
    struct xdg_wm_base *wayland_xdg_wm_base;
    struct wp_viewporter *wayland_viewporter;
    struct zxdg_decoration_manager_v1 *wayland_xdg_decoration_manager;

    struct wl_data_device *wayland_data_device;
    struct wl_data_source *wayland_data_source;
    struct wl_data_offer *wayland_data_offer;

    struct wl_surface *wayland_cursor_surface;

    CuiWaylandMonitor *wayland_monitors;
    // TODO: rename to wayland_mouse_focused_window
    CuiWindow *wayland_window_under_cursor;
    CuiWindow *wayland_keyboard_focused_window;

    struct xkb_context *xkb_context;
    struct xkb_state *xkb_state;
    struct xkb_keymap *xkb_keymap;
    struct xkb_compose_table *xkb_compose_table;
    struct xkb_compose_state *xkb_compose_state;

    CuiWaylandCursorTheme wayland_cursor_themes[4];

#endif

#if CUI_RENDERER_OPENGLES2_ENABLED

    EGLDisplay egl_display;

    bool has_EGL_EXT_platform_base;
    bool has_EGL_EXT_platform_x11;
    bool has_EGL_EXT_platform_wayland;

#endif

} CuiContext;

#endif
