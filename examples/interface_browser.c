#include <cui.h>

typedef struct InterfaceBrowser
{
    CuiWindow *window1;
    CuiWindow *window2;

    CuiWidget root_widget;

    CuiWidget action_label;
    CuiWidget action_row;
    CuiWidget actions[3];
    CuiWidget last_action;

    CuiWidget checkbox_label;

    CuiWidget checkbox_row;
    CuiWidget checkboxes[3];
    CuiWidget last_checkbox;

    CuiWidget icon_button_label;

    CuiWidget icon_button_row;
    CuiWidget icon_buttons[4];
    CuiWidget last_icon_button;

    CuiWidget label_button_label;

    CuiWidget label_button_row;
    CuiWidget label_buttons[4];
    CuiWidget last_label_button;

    CuiWidget icon_label_button_label;

    CuiWidget icon_label_button_row;
    CuiWidget icon_label_buttons[4];
    CuiWidget last_icon_label_button;

    CuiWidget emoji_label;
    CuiWidget emoji;

    CuiWidget label_wrapper;
    CuiWidget label_pos;

    CuiWidget text_input_label;

    CuiWidget text_input_row;
    CuiWidget text_inputs[3];
    CuiWidget last_text_input;

    CuiWidget last_row;
} InterfaceBrowser;

static InterfaceBrowser app;

static void
on_resize_button(CuiWidget *widget)
{
    CuiWindow *window = widget->window;
    CuiAssert(window);

    cui_window_resize(window, lroundf(cui_window_get_ui_scale(window) * 400),
                              lroundf(cui_window_get_ui_scale(window) * 200));
}

static void
on_fullscreen_button(CuiWidget *widget)
{
    CuiWindow *window = widget->window;
    CuiAssert(window);

    cui_window_set_fullscreen(window, !cui_window_is_fullscreen(window));
}

static void
on_open_file(CuiWidget *widget)
{
    (void) widget;

    CuiArena arena;
    cui_arena_allocate(&arena, CuiKiB(4));

    CuiArena temporary_memory;
    cui_arena_allocate(&temporary_memory, CuiMiB(1));

    CuiString *filenames = 0;
    cui_array_init(filenames, 4, &arena);

    if (cui_platform_open_file_dialog(&temporary_memory, &arena, &filenames, true, true, false))
    {
        for (int32_t i = 0; i < cui_array_count(filenames); i += 1)
        {
            CuiString filename = filenames[i];
            printf("filename[%d] = '%" CuiStringFmt "'\n", i, CuiStringArg(filename));
        }
    }

    cui_arena_deallocate(&temporary_memory);
    cui_arena_deallocate(&arena);
}

CUI_PLATFORM_MAIN
{
    if (!CUI_PLATFORM_INIT)
    {
        return -1;
    }

    app.window1 = cui_window_create(0);
    app.window2 = cui_window_create(0);

    cui_window_set_title(app.window1, CuiStringLiteral("🔥🔥🔥 Interface Browser 🔥🔥🔥"));
    cui_window_resize(app.window1, lroundf(cui_window_get_ui_scale(app.window1) * 640),
                                   lroundf(cui_window_get_ui_scale(app.window1) * 480));

    cui_window_set_title(app.window2, CuiStringLiteral("Window 2"));

    cui_widget_init(&app.root_widget, CUI_WIDGET_TYPE_BOX);
    cui_widget_add_flags(&app.root_widget, CUI_WIDGET_FLAG_DRAW_BACKGROUND);
    cui_widget_set_main_axis(&app.root_widget, CUI_AXIS_Y);
    cui_widget_set_y_axis_gravity(&app.root_widget, CUI_GRAVITY_START);
    cui_widget_set_inline_padding(&app.root_widget, 8.0f);
    cui_widget_set_padding(&app.root_widget, 8.0f, 8.0f, 8.0f, 8.0f);

    { // action label
        cui_widget_init(&app.action_label, CUI_WIDGET_TYPE_LABEL);
        cui_widget_set_label(&app.action_label, CuiStringLiteral("Actions"));
        cui_widget_set_x_axis_gravity(&app.action_label, CUI_GRAVITY_START);
        cui_widget_set_y_axis_gravity(&app.action_label, CUI_GRAVITY_START);

        cui_widget_append_child(&app.root_widget, &app.action_label);
    }

    { // action row
        cui_widget_init(&app.action_row, CUI_WIDGET_TYPE_BOX);
        cui_widget_set_main_axis(&app.action_row, CUI_AXIS_X);
        cui_widget_set_x_axis_gravity(&app.action_row, CUI_GRAVITY_START);
        cui_widget_set_inline_padding(&app.action_row, 8.0f);

        cui_widget_init(app.actions + 0, CUI_WIDGET_TYPE_BUTTON);
        cui_widget_init(app.actions + 1, CUI_WIDGET_TYPE_BUTTON);
        cui_widget_init(app.actions + 2, CUI_WIDGET_TYPE_BUTTON);
        cui_widget_init(&app.last_action, CUI_WIDGET_TYPE_BOX);

        app.actions[0].on_action = on_resize_button;
        app.actions[1].on_action = on_fullscreen_button;
        app.actions[2].on_action = on_open_file;

        cui_widget_set_label(app.actions + 0, CuiStringLiteral("Resize"));
        cui_widget_set_label(app.actions + 1, CuiStringLiteral("Toggle Fullscreen"));
        cui_widget_set_label(app.actions + 2, CuiStringLiteral("Open File"));

        cui_widget_append_child(&app.action_row, app.actions + 0);
        cui_widget_append_child(&app.action_row, app.actions + 1);
        cui_widget_append_child(&app.action_row, app.actions + 2);
        cui_widget_append_child(&app.action_row, &app.last_action);

        cui_widget_append_child(&app.root_widget, &app.action_row);
    }

    { // checkbox label
        cui_widget_init(&app.checkbox_label, CUI_WIDGET_TYPE_LABEL);
        cui_widget_set_label(&app.checkbox_label, CuiStringLiteral("Checkboxes"));
        cui_widget_set_x_axis_gravity(&app.checkbox_label, CUI_GRAVITY_START);
        cui_widget_set_y_axis_gravity(&app.checkbox_label, CUI_GRAVITY_START);

        cui_widget_append_child(&app.root_widget, &app.checkbox_label);
    }

    { // checkbox row
        cui_widget_init(&app.checkbox_row, CUI_WIDGET_TYPE_BOX);
        cui_widget_set_main_axis(&app.checkbox_row, CUI_AXIS_X);
        cui_widget_set_x_axis_gravity(&app.checkbox_row, CUI_GRAVITY_START);
        cui_widget_set_inline_padding(&app.checkbox_row, 8.0f);

        cui_widget_init(app.checkboxes + 0, CUI_WIDGET_TYPE_CHECKBOX);
        cui_widget_init(app.checkboxes + 1, CUI_WIDGET_TYPE_CHECKBOX);
        cui_widget_init(app.checkboxes + 2, CUI_WIDGET_TYPE_CHECKBOX);
        cui_widget_init(&app.last_checkbox, CUI_WIDGET_TYPE_BOX);

        cui_widget_set_value(app.checkboxes + 0, true);
        cui_widget_set_value(app.checkboxes + 1, false);
        cui_widget_set_value(app.checkboxes + 2, true);

        cui_widget_set_label(app.checkboxes + 0, CuiStringLiteral("First checkbox"));
        cui_widget_set_label(app.checkboxes + 1, CuiStringLiteral("Second checkbox"));
        cui_widget_set_label(app.checkboxes + 2, CuiStringLiteral("Third checkbox"));

        cui_widget_append_child(&app.checkbox_row, app.checkboxes + 0);
        cui_widget_append_child(&app.checkbox_row, app.checkboxes + 1);
        cui_widget_append_child(&app.checkbox_row, app.checkboxes + 2);
        cui_widget_append_child(&app.checkbox_row, &app.last_checkbox);

        cui_widget_append_child(&app.root_widget, &app.checkbox_row);
    }

    { // icon button label
        cui_widget_init(&app.icon_button_label, CUI_WIDGET_TYPE_LABEL);
        cui_widget_set_label(&app.icon_button_label, CuiStringLiteral("Icon buttons"));
        cui_widget_set_x_axis_gravity(&app.icon_button_label, CUI_GRAVITY_START);
        cui_widget_set_y_axis_gravity(&app.icon_button_label, CUI_GRAVITY_START);

        cui_widget_append_child(&app.root_widget, &app.icon_button_label);
    }

    { // icon button row
        cui_widget_init(&app.icon_button_row, CUI_WIDGET_TYPE_BOX);
        cui_widget_set_main_axis(&app.icon_button_row, CUI_AXIS_X);
        cui_widget_set_x_axis_gravity(&app.icon_button_row, CUI_GRAVITY_START);
        cui_widget_set_inline_padding(&app.icon_button_row, 8.0f);

        cui_widget_init(app.icon_buttons + 0, CUI_WIDGET_TYPE_BUTTON);
        cui_widget_init(app.icon_buttons + 1, CUI_WIDGET_TYPE_BUTTON);
        cui_widget_init(app.icon_buttons + 2, CUI_WIDGET_TYPE_BUTTON);
        cui_widget_init(app.icon_buttons + 3, CUI_WIDGET_TYPE_BUTTON);
        cui_widget_init(&app.last_icon_button, CUI_WIDGET_TYPE_BOX);

        cui_widget_set_icon(app.icon_buttons + 0, CUI_ICON_ANGLE_UP_12);
        cui_widget_set_icon(app.icon_buttons + 1, CUI_ICON_ANGLE_RIGHT_12);
        cui_widget_set_icon(app.icon_buttons + 2, CUI_ICON_ANGLE_DOWN_12);
        cui_widget_set_icon(app.icon_buttons + 3, CUI_ICON_ANGLE_LEFT_12);

        cui_widget_append_child(&app.icon_button_row, app.icon_buttons + 0);
        cui_widget_append_child(&app.icon_button_row, app.icon_buttons + 1);
        cui_widget_append_child(&app.icon_button_row, app.icon_buttons + 2);
        cui_widget_append_child(&app.icon_button_row, app.icon_buttons + 3);
        cui_widget_append_child(&app.icon_button_row, &app.last_icon_button);

        cui_widget_append_child(&app.root_widget, &app.icon_button_row);
    }

    { // label button label
        cui_widget_init(&app.label_button_label, CUI_WIDGET_TYPE_LABEL);
        cui_widget_set_label(&app.label_button_label, CuiStringLiteral("Label buttons"));
        cui_widget_set_x_axis_gravity(&app.label_button_label, CUI_GRAVITY_START);
        cui_widget_set_y_axis_gravity(&app.label_button_label, CUI_GRAVITY_START);

        cui_widget_append_child(&app.root_widget, &app.label_button_label);
    }

    { // label button row
        cui_widget_init(&app.label_button_row, CUI_WIDGET_TYPE_BOX);
        cui_widget_set_main_axis(&app.label_button_row, CUI_AXIS_X);
        cui_widget_set_x_axis_gravity(&app.label_button_row, CUI_GRAVITY_START);
        cui_widget_set_inline_padding(&app.label_button_row, 8.0f);

        cui_widget_init(app.label_buttons + 0, CUI_WIDGET_TYPE_BUTTON);
        cui_widget_init(app.label_buttons + 1, CUI_WIDGET_TYPE_BUTTON);
        cui_widget_init(app.label_buttons + 2, CUI_WIDGET_TYPE_BUTTON);
        cui_widget_init(app.label_buttons + 3, CUI_WIDGET_TYPE_BUTTON);
        cui_widget_init(&app.last_label_button, CUI_WIDGET_TYPE_BOX);

        cui_widget_set_label(app.label_buttons + 0, CuiStringLiteral("Up"));
        cui_widget_set_label(app.label_buttons + 1, CuiStringLiteral("Right"));
        cui_widget_set_label(app.label_buttons + 2, CuiStringLiteral("Down"));
        cui_widget_set_label(app.label_buttons + 3, CuiStringLiteral("Left"));

        cui_widget_append_child(&app.label_button_row, app.label_buttons + 0);
        cui_widget_append_child(&app.label_button_row, app.label_buttons + 1);
        cui_widget_append_child(&app.label_button_row, app.label_buttons + 2);
        cui_widget_append_child(&app.label_button_row, app.label_buttons + 3);
        cui_widget_append_child(&app.label_button_row, &app.last_label_button);

        cui_widget_append_child(&app.root_widget, &app.label_button_row);
    }

    { // icon label button label
        cui_widget_init(&app.icon_label_button_label, CUI_WIDGET_TYPE_LABEL);
        cui_widget_set_label(&app.icon_label_button_label, CuiStringLiteral("Label buttons"));
        cui_widget_set_x_axis_gravity(&app.icon_label_button_label, CUI_GRAVITY_START);
        cui_widget_set_y_axis_gravity(&app.icon_label_button_label, CUI_GRAVITY_START);

        cui_widget_append_child(&app.root_widget, &app.icon_label_button_label);
    }

    { // icon label button row
        cui_widget_init(&app.icon_label_button_row, CUI_WIDGET_TYPE_BOX);
        cui_widget_set_main_axis(&app.icon_label_button_row, CUI_AXIS_X);
        cui_widget_set_x_axis_gravity(&app.icon_label_button_row, CUI_GRAVITY_START);
        cui_widget_set_inline_padding(&app.icon_label_button_row, 8.0f);

        cui_widget_init(app.icon_label_buttons + 0, CUI_WIDGET_TYPE_BUTTON);
        cui_widget_init(app.icon_label_buttons + 1, CUI_WIDGET_TYPE_BUTTON);
        cui_widget_init(app.icon_label_buttons + 2, CUI_WIDGET_TYPE_BUTTON);
        cui_widget_init(app.icon_label_buttons + 3, CUI_WIDGET_TYPE_BUTTON);
        cui_widget_init(&app.last_icon_label_button, CUI_WIDGET_TYPE_BOX);

        cui_widget_set_icon(app.icon_label_buttons + 0, CUI_ICON_ANGLE_UP_12);
        cui_widget_set_icon(app.icon_label_buttons + 1, CUI_ICON_ANGLE_RIGHT_12);
        cui_widget_set_icon(app.icon_label_buttons + 2, CUI_ICON_ANGLE_DOWN_12);
        cui_widget_set_icon(app.icon_label_buttons + 3, CUI_ICON_ANGLE_LEFT_12);

        cui_widget_set_label(app.icon_label_buttons + 0, CuiStringLiteral("Up"));
        cui_widget_set_label(app.icon_label_buttons + 1, CuiStringLiteral("Right"));
        cui_widget_set_label(app.icon_label_buttons + 2, CuiStringLiteral("Down"));
        cui_widget_set_label(app.icon_label_buttons + 3, CuiStringLiteral("Left"));

        cui_widget_append_child(&app.icon_label_button_row, app.icon_label_buttons + 0);
        cui_widget_append_child(&app.icon_label_button_row, app.icon_label_buttons + 1);
        cui_widget_append_child(&app.icon_label_button_row, app.icon_label_buttons + 2);
        cui_widget_append_child(&app.icon_label_button_row, app.icon_label_buttons + 3);
        cui_widget_append_child(&app.icon_label_button_row, &app.last_icon_label_button);

        cui_widget_append_child(&app.root_widget, &app.icon_label_button_row);
    }

    { // emoji label
        cui_widget_init(&app.emoji_label, CUI_WIDGET_TYPE_LABEL);
        cui_widget_set_label(&app.emoji_label, CuiStringLiteral("Emojis"));
        cui_widget_set_x_axis_gravity(&app.emoji_label, CUI_GRAVITY_START);
        cui_widget_set_y_axis_gravity(&app.emoji_label, CUI_GRAVITY_START);

        cui_widget_append_child(&app.root_widget, &app.emoji_label);
    }

    { // emoji
        cui_widget_init(&app.emoji, CUI_WIDGET_TYPE_LABEL);
        cui_widget_set_label(&app.emoji, CuiStringLiteral("💩 🔥 ✅"));
        cui_widget_set_x_axis_gravity(&app.emoji, CUI_GRAVITY_START);
        cui_widget_set_y_axis_gravity(&app.emoji, CUI_GRAVITY_START);

        cui_widget_append_child(&app.root_widget, &app.emoji);
    }

    { // label positioning
        cui_widget_init(&app.label_wrapper, CUI_WIDGET_TYPE_BOX);
        cui_widget_add_flags(&app.label_wrapper, CUI_WIDGET_FLAG_DRAW_BACKGROUND | CUI_WIDGET_FLAG_FIXED_HEIGHT);
        cui_widget_set_preferred_size(&app.label_wrapper, 0.0f, 32.0f);
        cui_widget_append_child(&app.root_widget, &app.label_wrapper);

        app.label_wrapper.color_normal_background = CUI_COLOR_WINDOW_TITLEBAR_BACKGROUND;

        cui_widget_init(&app.label_pos, CUI_WIDGET_TYPE_LABEL);
        cui_widget_set_label(&app.label_pos, CuiStringLiteral("Hello World!"));
        cui_widget_set_x_axis_gravity(&app.label_pos, CUI_GRAVITY_END);
        cui_widget_set_y_axis_gravity(&app.label_pos, CUI_GRAVITY_END);
        cui_widget_append_child(&app.label_wrapper, &app.label_pos);
    }

    { // text input label
        cui_widget_init(&app.text_input_label, CUI_WIDGET_TYPE_LABEL);
        cui_widget_set_label(&app.text_input_label, CuiStringLiteral("Text Inputs"));
        cui_widget_set_x_axis_gravity(&app.text_input_label, CUI_GRAVITY_START);
        cui_widget_set_y_axis_gravity(&app.text_input_label, CUI_GRAVITY_START);

        cui_widget_append_child(&app.root_widget, &app.text_input_label);
    }

    { // text input row
        cui_widget_init(&app.text_input_row, CUI_WIDGET_TYPE_BOX);
        cui_widget_set_main_axis(&app.text_input_row, CUI_AXIS_X);
        cui_widget_set_x_axis_gravity(&app.text_input_row, CUI_GRAVITY_START);
        cui_widget_set_inline_padding(&app.text_input_row, 8.0f);

        cui_widget_init(app.text_inputs + 0, CUI_WIDGET_TYPE_TEXTINPUT);
        cui_widget_init(app.text_inputs + 1, CUI_WIDGET_TYPE_TEXTINPUT);
        cui_widget_init(app.text_inputs + 2, CUI_WIDGET_TYPE_TEXTINPUT);
        cui_widget_init(&app.last_text_input, CUI_WIDGET_TYPE_BOX);

        int64_t text_input_buffer_size = CuiKiB(1);
        uint8_t *text_input_buffer = (uint8_t *) cui_platform_allocate(CuiArrayCount(app.text_inputs) * text_input_buffer_size);

        cui_widget_set_textinput_buffer(app.text_inputs + 0, text_input_buffer + (0 * text_input_buffer_size), text_input_buffer_size);
        cui_widget_set_textinput_buffer(app.text_inputs + 1, text_input_buffer + (1 * text_input_buffer_size), text_input_buffer_size);
        cui_widget_set_textinput_buffer(app.text_inputs + 2, text_input_buffer + (2 * text_input_buffer_size), text_input_buffer_size);

        cui_widget_set_icon(app.text_inputs + 0, CUI_ICON_NONE);
        cui_widget_set_icon(app.text_inputs + 1, CUI_ICON_ANGLE_RIGHT_12);
        cui_widget_set_icon(app.text_inputs + 2, CUI_ICON_SEARCH_12);

        cui_widget_set_label(app.text_inputs + 0, CuiStringLiteral("Enter..."));
        cui_widget_set_label(app.text_inputs + 1, CuiStringLiteral(""));
        cui_widget_set_label(app.text_inputs + 2, CuiStringLiteral("Search"));

        cui_widget_set_preferred_size(app.text_inputs + 0, 128.0f, 0.0f);
        cui_widget_set_preferred_size(app.text_inputs + 1, 128.0f, 0.0f);
        cui_widget_set_preferred_size(app.text_inputs + 2, 128.0f, 0.0f);

        cui_widget_append_child(&app.text_input_row, app.text_inputs + 0);
        cui_widget_append_child(&app.text_input_row, app.text_inputs + 1);
        cui_widget_append_child(&app.text_input_row, app.text_inputs + 2);
        cui_widget_append_child(&app.text_input_row, &app.last_text_input);

        cui_widget_append_child(&app.root_widget, &app.text_input_row);
    }

    cui_widget_init(&app.last_row, CUI_WIDGET_TYPE_BOX);
    cui_widget_append_child(&app.root_widget, &app.last_row);

    cui_window_set_root_widget(app.window1, &app.root_widget);

    cui_window_show(app.window1);
    cui_window_show(app.window2);

    return cui_main_loop();
}
