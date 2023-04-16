#include <cui.h>

typedef struct FileSearch
{
    CuiArena widget_arena;

    CuiWindow *window;

    CuiWidget *root_widget;

    CuiWidget *search_input;
    CuiWidget *search_results;
} FileSearch;

static FileSearch app;

static CuiWidget *
create_widget(CuiArena *arena, uint32_t type)
{
    CuiWidget *widget = cui_alloc_type(arena, CuiWidget, CuiDefaultAllocationParams());
    cui_widget_init(widget, type);
    return widget;
}

static void
create_user_interface(CuiWindow *window, CuiArena *arena)
{
    app.root_widget = create_widget(arena, CUI_WIDGET_TYPE_BOX);

    cui_widget_set_main_axis(app.root_widget, CUI_AXIS_Y);
    cui_widget_set_y_axis_gravity(app.root_widget, CUI_GRAVITY_START);
    cui_widget_add_flags(app.root_widget, CUI_WIDGET_FLAG_DRAW_BACKGROUND);
    cui_widget_set_inline_padding(app.root_widget, 8.0f);
    cui_widget_set_padding(app.root_widget, 8.0f, 8.0f, 8.0f, 8.0f);

    app.search_input = create_widget(arena, CUI_WIDGET_TYPE_TEXTINPUT);

    cui_widget_set_icon(app.search_input, CUI_ICON_SEARCH_12);
    cui_widget_set_label(app.search_input, CuiStringLiteral("Search files and folders"));
    cui_widget_set_textinput_buffer(app.search_input, cui_alloc(arena, CuiKiB(1), CuiDefaultAllocationParams()), CuiKiB(1));

    cui_widget_append_child(app.root_widget, app.search_input);

    app.search_results = create_widget(arena, CUI_WIDGET_TYPE_BOX);

    cui_widget_add_flags(app.search_results, CUI_WIDGET_FLAG_DRAW_BACKGROUND);
    cui_widget_set_border_width(app.search_results, 1.0f, 1.0f, 1.0f, 1.0f);
    cui_widget_set_border_radius(app.search_results, 4.0f, 4.0f, 4.0f, 4.0f);

    cui_widget_append_child(app.root_widget, app.search_results);

    cui_window_set_root_widget(window, app.root_widget);
}

CUI_PLATFORM_MAIN
{
    if (!CUI_PLATFORM_INIT)
    {
        return -1;
    }

    cui_arena_allocate(&app.widget_arena, CuiKiB(32));

    app.window = cui_window_create(0);

    if (!app.window)
    {
        return -1;
    }

    cui_window_set_title(app.window, CuiStringLiteral("File Search"));
    cui_window_resize(app.window, lroundf(cui_window_get_ui_scale(app.window) * 400),
                                  lroundf(cui_window_get_ui_scale(app.window) * 500));

    create_user_interface(app.window, &app.widget_arena);

    cui_window_show(app.window);

    return cui_main_loop();
}
