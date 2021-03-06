#include <cui.h>

typedef struct InterfaceBrowser
{
    CuiWindow *window;

    CuiWidget first_page;
    CuiWidget second_page;

    CuiWidget first_page_column;

    CuiWidget icon_button_row;
    CuiWidget icon_buttons[2];
    CuiWidget last_icon_button;

    CuiWidget checkbox_row;
    CuiWidget checkboxes[3];
    CuiWidget last_checkbox;

    CuiWidget textinput_row;
    CuiWidget textinputs[3];
    CuiWidget last_textinput;

    CuiWidget first_page_last_row;
} InterfaceBrowser;

static InterfaceBrowser app;


#if CUI_PLATFORM_WINDOWS
int CALLBACK wWinMain(HINSTANCE instance, HINSTANCE prev_instance, PWSTR command_line, int show_code)
#else
int main(int argc, char **argv)
#endif
{
#if CUI_PLATFORM_WINDOWS
    if (!cui_init(0, 0))
#else
    if (!cui_init(argc, argv))
#endif
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

    { // icon buttons
        cui_widget_gravity_box_init(&app.icon_button_row, CUI_DIRECTION_WEST);
        cui_widget_append_child(&app.first_page_column, &app.icon_button_row);

        cui_widget_icon_button_init(app.icon_buttons + 0, CuiStringLiteral("Load"), CUI_ICON_LOAD);
        cui_widget_icon_button_init(app.icon_buttons + 1, CuiStringLiteral("Record"), CUI_ICON_TAPE);
        cui_widget_box_init(&app.last_icon_button);

        cui_widget_append_child(&app.icon_button_row, app.icon_buttons + 0);
        cui_widget_append_child(&app.icon_button_row, app.icon_buttons + 1);
        cui_widget_append_child(&app.icon_button_row, &app.last_icon_button);
    }

    { // checkboxes
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
    }

    { // textinputs
        cui_widget_gravity_box_init(&app.textinput_row, CUI_DIRECTION_WEST);
        cui_widget_append_child(&app.first_page_column, &app.textinput_row);

        int64_t textinput_buffer_size = CuiKiB(1);
        uint8_t *textinput_buffer = (uint8_t *) cui_allocate_platform_memory(CuiArrayCount(app.textinputs) * textinput_buffer_size);

        cui_widget_textinput_init(app.textinputs + 0, textinput_buffer + (0 * textinput_buffer_size), textinput_buffer_size);
        cui_widget_textinput_init(app.textinputs + 1, textinput_buffer + (1 * textinput_buffer_size), textinput_buffer_size);
        cui_widget_textinput_init(app.textinputs + 2, textinput_buffer + (2 * textinput_buffer_size), textinput_buffer_size);
        cui_widget_box_init(&app.last_textinput);

        cui_widget_append_child(&app.textinput_row, app.textinputs + 0);
        cui_widget_append_child(&app.textinput_row, app.textinputs + 1);
        cui_widget_append_child(&app.textinput_row, app.textinputs + 2);
        cui_widget_append_child(&app.textinput_row, &app.last_textinput);
    }

    cui_widget_box_init(&app.first_page_last_row);
    cui_widget_append_child(&app.first_page_column, &app.first_page_last_row);

    cui_widget_box_init(&app.second_page);
    cui_widget_add_flags(&app.second_page, CUI_WIDGET_FLAG_FILL_BACKGROUND);
    cui_widget_set_label(&app.second_page, CuiStringLiteral("Second page"));

    cui_widget_append_child(cui_window_get_root_widget(app.window), &app.second_page);

    cui_window_show(app.window);

    return cui_main_loop();
}
