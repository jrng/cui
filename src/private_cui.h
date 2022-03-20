#define CUI_MAX_WINDOW_COUNT 16
#define CUI_DEFAULT_WINDOW_WIDTH 800
#define CUI_DEFAULT_WINDOW_HEIGHT 600
#define CUI_DEFAULT_PUSH_BUFFER_SIZE CuiMiB(1)
#define CUI_DEFAULT_INDEX_BUFFER_SIZE CuiKiB(64)

#if CUI_PLATFORM_LINUX
#include <pthread.h>
#endif

typedef struct CuiCompletionState
{
    volatile uint32_t completion_goal;
    volatile uint32_t completion_count;
} CuiCompletionState;

typedef void CuiWorkFunction(void *data);

typedef struct CuiWorkQueueEntry
{
    CuiWorkFunction *work;
    void *data;
    CuiCompletionState *completion_state;
} CuiWorkQueueEntry;

typedef struct CuiWorkQueue
{
    volatile uint32_t write_index;
    volatile uint32_t read_index;

#if CUI_PLATFORM_WINDOWS
    HANDLE semaphore;
#else
    pthread_mutex_t semaphore_mutex;
    pthread_cond_t semaphore_cond;
#endif

    CuiWorkQueueEntry entries[64];
} CuiWorkQueue;

typedef struct CuiTextBuffer
{
    struct CuiTextBuffer *next;
    int16_t occupied;
    uint8_t data[4096];
} CuiTextBuffer;

typedef struct CuiStringBuilder
{
    CuiArena *arena;
    CuiTextBuffer *write_buffer;
    CuiTextBuffer base_buffer;
} CuiStringBuilder;

typedef struct CuiFontRef
{
    CuiString name;
    CuiString path;
} CuiFontRef;

typedef struct CuiFontManager
{
    CuiArena arena;

    CuiFontFile *font_files;
    CuiFontRef  *font_refs;
} CuiFontManager;

typedef struct CuiGlyphKey
{
    uint32_t id;
    uint32_t codepoint;
    float scale;
    float offset_x;
    float offset_y;
} CuiGlyphKey;

typedef struct CuiGlyphCache
{
    uint32_t count;
    uint32_t allocated;

    uint32_t *hashes;
    CuiGlyphKey *keys;
    CuiRect *rects;

    int32_t x, y, y_max;

    CuiBitmap texture;
} CuiGlyphCache;

typedef struct CuiSolidRect
{
    uint32_t clip_rect;
    CuiRect rect;
    CuiColor color;
} CuiSolidRect;

typedef struct CuiTexturedRect
{
    uint32_t clip_rect;
    CuiRect rect;
    CuiColor color;
    CuiPoint uv;
} CuiTexturedRect;

typedef struct CuiCommandBuffer
{
    uint32_t push_buffer_size;
    uint32_t max_push_buffer_size;
    uint8_t *push_buffer;

    uint32_t index_buffer_count;
    uint32_t max_index_buffer_count;
    uint32_t *index_buffer;
} CuiCommandBuffer;

struct CuiGraphicsContext
{
    uint32_t clip_rect_offset;

    CuiRect clip_rect;
    CuiRect redraw_rect;
    CuiCommandBuffer *command_buffer;
    CuiGlyphCache *cache;
};

typedef struct CuiContextCommon
{
    bool running;
    CuiArena arena;
    CuiArena temporary_memory;

    uint32_t window_count;
    struct CuiWindow *windows[CUI_MAX_WINDOW_COUNT];

    CuiFontManager font_manager;

    CuiWorkQueue work_queue;
} CuiContextCommon;

typedef struct CuiWindowBase
{
    float ui_scale;

    CuiFont *font;

    CuiWidget *hovered_widget;
    CuiWidget *pressed_widget;
    CuiWidget *focused_widget;

    void *user_data;

    uint32_t max_push_buffer_size;
    uint32_t max_index_buffer_count;

    uint8_t *push_buffer;
    uint32_t *index_buffer;

    CuiArena temporary_memory;
    CuiGlyphCache glyph_cache;

    CuiEvent event;
    CuiWidget root_widget;
} CuiWindowBase;

#if CUI_PLATFORM_WINDOWS

struct CuiWindow
{
    CuiWindowBase base;

    HWND window_handle;

    CuiBitmap backbuffer;
    int64_t backbuffer_memory_size;

    bool needs_redraw;
    CuiRect redraw_rect;

    bool is_tracking;
};

typedef struct CuiContext
{
    CuiContextCommon common;

    LPCWSTR window_class_name;

    UINT (*GetDpiForSystem)();
    UINT (*GetDpiForWindow)(HWND window_handle);

    HCURSOR cursor_arrow;
    HCURSOR cursor_text;
    HCURSOR cursor_hand;
    HCURSOR cursor_move_all;
    HCURSOR cursor_move_left_right;
    HCURSOR cursor_move_top_down;
} CuiContext;

#elif CUI_PLATFORM_LINUX

#include <X11/Xlib.h>

typedef int gint;
typedef char gchar;
typedef gint gboolean;
typedef unsigned long gulong;
typedef gulong GType;

typedef enum GConnectFlags
{
    G_CONNECT_AFTER   = 1 << 0,
    G_CONNECT_SWAPPED = 1 << 1,
} GConnectFlags;

typedef void (*GCallback) (void);

typedef struct GMainContext GMainContext;

typedef struct GSList
{
    void *data;
    struct GSList *next;
} GSList;

typedef enum GtkResponseType
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

typedef enum GtkFileChooserAction
{
    GTK_FILE_CHOOSER_ACTION_OPEN          = 0,
    GTK_FILE_CHOOSER_ACTION_SAVE          = 1,
    GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER = 2,
    GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER = 3,
} GtkFileChooserAction;

typedef struct GdkDisplay GdkDisplay;
typedef struct GdkWindow GdkWindow;

typedef struct GtkWidget GtkWidget;
typedef struct GtkDialog GtkDialog;
typedef struct GtkWindow GtkWindow;
typedef struct GtkFileChooser GtkFileChooser;

typedef void (*PFN_g_free) (void *mem);
typedef void (*PFN_g_slist_free) (GSList *list);
typedef gboolean (*PFN_g_main_context_iteration) (GMainContext *context, gboolean may_block);
typedef gulong (*PFN_g_signal_connect_data) (void *instance, const gchar *detailed_signal, GCallback c_handler, void *data, void *destroy_data, GConnectFlags connect_flags);

typedef GdkDisplay *(*PFN_gdk_display_get_default) (void);
typedef GdkWindow *(*PFN_gdk_x11_window_foreign_new_for_display) (GdkDisplay *display, Window window);

typedef GType (*PFN_gtk_window_get_type) (void);
typedef GtkWidget *(*PFN_gtk_widget_new) (GType type, const gchar *first_property_name, ...);
typedef gboolean (*PFN_gtk_init_check) (int *argc, char ***argv);
typedef void (*PFN_gtk_widget_destroy) (GtkWidget *widget);
typedef void (*PFN_gtk_widget_realize) (GtkWidget *widget);
typedef void (*PFN_gtk_widget_set_window) (GtkWidget *widget, GdkWindow *window);
typedef void (*PFN_gtk_widget_set_has_window) (GtkWidget *widget, gboolean has_window);
typedef gint (*PFN_gtk_dialog_run) (GtkDialog *dialog);
typedef gchar *(*PFN_gtk_file_chooser_get_filename) (GtkFileChooser *chooser);
typedef GSList *(*PFN_gtk_file_chooser_get_filenames) (GtkFileChooser *chooser);
typedef GtkWidget *(*PFN_gtk_file_chooser_dialog_new) (const gchar *title, GtkWindow *parent, GtkFileChooserAction action, const gchar *first_button_text, ...);
typedef void (*PFN_gtk_file_chooser_set_select_multiple) (GtkFileChooser *chooser, gboolean select_multiple);
typedef void (*PFN_gtk_file_chooser_set_do_overwrite_confirmation) (GtkFileChooser *chooser, gboolean do_overwrite_confirmation);
typedef void (*PFN_gtk_file_chooser_set_current_name) (GtkFileChooser *chooser, const gchar *name);

struct CuiWindow
{
    CuiWindowBase base;

    Window x11_window;

    GdkWindow *gdk_window;
    GtkWindow *gtk_window;

    CuiBitmap backbuffer;
    int64_t backbuffer_memory_size;

    bool needs_redraw;
    CuiRect redraw_rect;
};

typedef struct CuiContext
{
    CuiContextCommon common;

    float default_ui_scale;

    int x11_default_screen;

    Display *x11_display;
    Window x11_root_window;
    GC x11_default_gc;

    Atom atom_utf8_string;
    Atom atom_wm_protocols;
    Atom atom_wm_delete_window;
    Atom atom_xsettings_screen;
    Atom atom_xsettings_settings;

    bool gtk3_initialized;

    void *gtk3_handle;
    GdkDisplay *gdk_display;

    PFN_g_free                                          g_free;
    PFN_g_slist_free                                    g_slist_free;
    PFN_g_main_context_iteration                        g_main_context_iteration;
    PFN_g_signal_connect_data                           g_signal_connect_data;

    PFN_gdk_display_get_default                         gdk_display_get_default;
    PFN_gdk_x11_window_foreign_new_for_display          gdk_x11_window_foreign_new_for_display;

    PFN_gtk_window_get_type                             gtk_window_get_type;
    PFN_gtk_widget_new                                  gtk_widget_new;
    PFN_gtk_init_check                                  gtk_init_check;
    PFN_gtk_widget_destroy                              gtk_widget_destroy;
    PFN_gtk_widget_realize                              gtk_widget_realize;
    PFN_gtk_widget_set_window                           gtk_widget_set_window;
    PFN_gtk_widget_set_has_window                       gtk_widget_set_has_window;
    PFN_gtk_dialog_run                                  gtk_dialog_run;
    PFN_gtk_file_chooser_get_filename                   gtk_file_chooser_get_filename;
    PFN_gtk_file_chooser_get_filenames                  gtk_file_chooser_get_filenames;
    PFN_gtk_file_chooser_dialog_new                     gtk_file_chooser_dialog_new;
    PFN_gtk_file_chooser_set_select_multiple            gtk_file_chooser_set_select_multiple;
    PFN_gtk_file_chooser_set_do_overwrite_confirmation  gtk_file_chooser_set_do_overwrite_confirmation;
    PFN_gtk_file_chooser_set_current_name               gtk_file_chooser_set_current_name;
} CuiContext;

#else
#error unsupported platform
#endif
