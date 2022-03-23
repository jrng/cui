#include <cui.h>

typedef struct InterfaceBrowser
{
    CuiWindow *window;

    CuiWidget first_page;
    CuiWidget second_page;

    CuiWidget first_page_column;
    CuiWidget checkbox_row;

    CuiWidget checkboxes[3];
    CuiWidget last_checkbox;

    CuiWidget first_page_last_row;
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
    cui_widget_add_flags(&app.first_page, CUI_WIDGET_FLAG_FILL_BACKGROUND);
    cui_widget_set_label(&app.first_page, CuiStringLiteral("First page"));

    cui_widget_append_child(cui_window_get_root_widget(app.window), &app.first_page);

    cui_widget_gravity_box_init(&app.first_page_column, CUI_DIRECTION_NORTH);

    cui_widget_append_child(&app.first_page, &app.first_page_column);

    cui_widget_gravity_box_init(&app.checkbox_row, CUI_DIRECTION_WEST);

    cui_widget_append_child(&app.first_page_column, &app.checkbox_row);

    cui_widget_checkbox_init(app.checkboxes + 0, CuiStringLiteral("First checkbox"), true);
    cui_widget_checkbox_init(app.checkboxes + 1, CuiStringLiteral("Second checkbox"), false);
    cui_widget_checkbox_init(app.checkboxes + 2, CuiStringLiteral("Third checkbox"), true);
    cui_widget_box_init(&app.last_checkbox);

    cui_widget_append_child(&app.checkbox_row, app.checkboxes + 0);
    cui_widget_append_child(&app.checkbox_row, app.checkboxes + 1);
    cui_widget_append_child(&app.checkbox_row, app.checkboxes + 2);
    cui_widget_append_child(&app.checkbox_row, &app.last_checkbox);

    cui_widget_box_init(&app.first_page_last_row);
    cui_widget_append_child(&app.first_page_column, &app.first_page_last_row);

    cui_widget_box_init(&app.second_page);
    cui_widget_add_flags(&app.second_page, CUI_WIDGET_FLAG_FILL_BACKGROUND);
    cui_widget_set_label(&app.second_page, CuiStringLiteral("Second page"));

    cui_widget_append_child(cui_window_get_root_widget(app.window), &app.second_page);

    cui_window_show(app.window);

    return cui_main_loop();
}
