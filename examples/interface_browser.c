#include <cui.h>

typedef struct InterfaceBrowser
{
    CuiWindow *window;

    CuiWidget first_page;
    CuiWidget second_page;
} InterfaceBrowser;

static InterfaceBrowser app;

int main(int argc, char **argv)
{
    if (!cui_init(argc, argv))
    {
        return -1;
    }

    app.window = cui_window_create();

    cui_window_set_title(app.window, "Interface Browser");
    cui_window_resize(app.window, lroundf(cui_window_get_ui_scale(app.window) * 800),
                                  lroundf(cui_window_get_ui_scale(app.window) * 600));

    cui_widget_tabs_init(cui_window_get_root_widget(app.window));

    cui_widget_box_init(&app.first_page);
    cui_widget_set_label(&app.first_page, CuiStringLiteral("First page"));
    cui_widget_box_init(&app.second_page);
    cui_widget_set_label(&app.second_page, CuiStringLiteral("Second page"));

    cui_widget_append_child(cui_window_get_root_widget(app.window), &app.first_page);
    cui_widget_append_child(cui_window_get_root_widget(app.window), &app.second_page);

    cui_window_show(app.window);

    return cui_main_loop();
}
