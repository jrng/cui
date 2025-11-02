#include <cui.h>

static const uint32_t WIDGET_TYPE_IMAGE_VIEW = CUI_WIDGET_TYPE_CUSTOM + 0;

typedef struct ImageView
{
    CuiWidget base;

    bool has_new_bitmap;

    CuiBitmap bitmap;
    int32_t texture_id;
    CuiRect image_rect;
} ImageView;

typedef struct ImageState
{
    CuiArena memory;
    CuiArena temporary_memory;

    CuiString filename;
    CuiBitmap bitmap;
    CuiImageMetaData *meta_data;
} ImageState;

typedef struct ImageViewer
{
    CuiArena temporary_memory;

    CuiString directory;
    CuiString filename;

    CuiArena files_list_memory;
    CuiString *files_list;

    CuiBackgroundTask background_task;

    bool image_is_loading;

    ImageState image_states[2];
    ImageState *loaded_state;
    ImageState *loading_state;

    CuiWindow *window;

    CuiWidget root_widget;

    CuiWidget info_panel;

    CuiWidget label_column;
    CuiWidget content_column;

    CuiWidget info_label_width;
    CuiWidget info_label_width_content;

    CuiWidget info_label_height;
    CuiWidget info_label_height_content;

    CuiWidget ui_stack;

    ImageView image_view;

    CuiWidget ui_overlay;

    CuiWidget ui_overlay_top;
    CuiWidget ui_overlay_middle;
    CuiWidget ui_overlay_bottom;

    CuiWidget nav_box_right;
    CuiWidget nav_box_center;

    CuiWidget nav_left;
    CuiWidget nav_right;

    CuiWidget control_bar_wrapper;
    CuiWidget control_bar_wrapper_left;
    CuiWidget control_bar_wrapper_right;
    CuiWidget control_bar;
    CuiWidget ui_overlay_bottom_rest;

    CuiWidget fullscreen_button;
    CuiWidget info_button;
} ImageViewer;

static ImageViewer app;

static void
image_view_set_ui_scale(CuiWidget *widget, float ui_scale)
{
    (void) widget;
    (void) ui_scale;
}

static CuiPoint
image_view_get_preferred_size(CuiWidget *widget)
{
    (void) widget;
    return cui_make_point(0, 0);
}

static void
image_view_layout(CuiWidget *widget, CuiRect rect)
{
    ImageView *image_view = CuiContainerOf(widget, ImageView, base);

    if (image_view->bitmap.pixels)
    {
        float bitmap_width = (float) image_view->bitmap.width;
        float bitmap_height = (float) image_view->bitmap.height;

        float aspect_ratio = bitmap_width / bitmap_height;
        float max_width = (float) cui_rect_get_width(rect);
        float max_height = (float) cui_rect_get_height(rect);

        image_view->image_rect = rect;

        if ((bitmap_width < max_width) && (bitmap_height < max_height))
        {
            image_view->image_rect.min.x += lroundf(0.5f * (max_width - bitmap_width));
            image_view->image_rect.max.x = image_view->image_rect.min.x + (int32_t) bitmap_width;
            image_view->image_rect.min.y += lroundf(0.5f * (max_height - bitmap_height));
            image_view->image_rect.max.y = image_view->image_rect.min.y + (int32_t) bitmap_height;
        }
        else
        {
            float width = roundf(max_height * aspect_ratio);

            if (width < max_width)
            {
                float offset = roundf(0.5f * (max_width - width));
                image_view->image_rect.min.x += (int32_t) offset;
                image_view->image_rect.max.x = image_view->image_rect.min.x + (int32_t) width;
            }
            else
            {
                float height = roundf(max_width / aspect_ratio);
                float offset = roundf(0.5f * (max_height - height));
                image_view->image_rect.min.y += (int32_t) offset;
                image_view->image_rect.max.y = image_view->image_rect.min.y + (int32_t) height;
            }
        }
    }
}

static void
image_view_draw(CuiWidget *widget, CuiGraphicsContext *ctx, const CuiColorTheme *color_theme)
{
    (void) color_theme;

    ImageView *image_view = CuiContainerOf(widget, ImageView, base);

    cui_draw_fill_rect(ctx, widget->rect, cui_make_color(0.0f, 0.0f, 0.0f, 1.0f));

    if (image_view->bitmap.pixels)
    {
        CuiRect uv = cui_make_rect(0, 0, image_view->bitmap.width, image_view->bitmap.height);

        if (image_view->has_new_bitmap)
        {
            if (image_view->texture_id < 0)
            {
                image_view->texture_id = cui_window_allocate_texture_id(widget->window);
            }
            else
            {
                cui_draw_deallocate_texture(ctx, image_view->texture_id);
            }

            cui_draw_allocate_texture(ctx, image_view->texture_id, image_view->bitmap);
            cui_draw_update_texture(ctx, image_view->texture_id, uv);

            image_view->has_new_bitmap = false;
        }

        cui_draw_fill_rect(ctx, image_view->image_rect, CuiHexColor(0xFFFFFFFF));
        cui_draw_fill_textured_rect(ctx, image_view->image_rect, uv, image_view->texture_id, CuiHexColor(0xFFFFFFFF));
    }
}

static bool
image_view_handle_event(CuiWidget *widget, CuiEventType event_type)
{
    bool result = false;

    CuiAssert(widget->window);
    CuiWindow *window = widget->window;

    switch (event_type)
    {
        case CUI_EVENT_TYPE_LEFT_DOWN:
        {
            cui_window_set_pressed(window, widget);
            result = true;
        } break;

        case CUI_EVENT_TYPE_DOUBLE_CLICK:
        {
            cui_window_set_pressed(window, widget);
            result = true;

            cui_window_set_fullscreen(window, !cui_window_is_fullscreen(window));
        } break;

        default:
        {
        } break;
    }

    return result;
}

static CuiString
get_prev_image_filename(void)
{
    int32_t index = 0;
    int32_t count = cui_array_count(app.files_list);

    for (; index < count; index += 1)
    {
        if (cui_string_equals(app.files_list[index], app.filename))
        {
            break;
        }
    }

    CuiAssert(index < count);

    if (index == 0)
    {
        index = count;
    }

    int32_t prev_index = index - 1;

    return app.files_list[prev_index];
}

static CuiString
get_next_image_filename(void)
{
    int32_t index = 0;
    int32_t count = cui_array_count(app.files_list);

    for (; index < count; index += 1)
    {
        if (cui_string_equals(app.files_list[index], app.filename))
        {
            break;
        }
    }

    CuiAssert(index < count);

    int32_t next_index = index + 1;

    if (next_index == count)
    {
        next_index = 0;
    }

    return app.files_list[next_index];
}

static void
background_task_proc(CuiBackgroundTask *task, void *data)
{
    (void) task;

    ImageState *state = (ImageState *) data;

    CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(&state->temporary_memory);

    CuiString full_filename = cui_path_concat(&state->temporary_memory, app.directory, state->filename);
    CuiFile *file = cui_platform_file_open(&state->temporary_memory, full_filename, CUI_FILE_MODE_READ);

    state->meta_data = 0;
    cui_array_init(state->meta_data, 16, &state->memory);

    if (file)
    {
        uint64_t file_size = cui_platform_file_get_size(file);

        CuiString contents;
        contents.count = file_size;
        contents.data  = (uint8_t *) cui_alloc(&state->temporary_memory, contents.count, CuiDefaultAllocationParams());

        cui_platform_file_read(file, contents.data, 0, contents.count);
        cui_platform_file_close(file);

        cui_image_decode(&state->temporary_memory, &state->bitmap, contents, &state->memory, &state->meta_data, &state->memory);
    }

    cui_end_temporary_memory(temp_memory);
}

static void
load_image_file(CuiString filename)
{
    if (!app.image_is_loading && !cui_string_equals(app.filename, filename))
    {
        cui_arena_clear(&app.loading_state->memory);
        cui_arena_clear(&app.loading_state->temporary_memory);

        app.loading_state->filename = cui_copy_string(&app.loading_state->memory, filename);
        app.image_is_loading = true;

        cui_background_task_start(&app.background_task, background_task_proc, app.loading_state, true);
    }
}

static void
on_nav_left(CuiWidget *widget)
{
    (void) widget;

    if (cui_array_count(app.files_list) > 0)
    {
        CuiString prev_filename = get_prev_image_filename();
        load_image_file(prev_filename);
    }
}

static void
on_nav_right(CuiWidget *widget)
{
    (void) widget;

    if (cui_array_count(app.files_list) > 0)
    {
        CuiString next_filename = get_next_image_filename();
        load_image_file(next_filename);
    }
}

static void
toggle_info_panel(CuiWidget *widget)
{
    (void) widget;

    if (cui_widget_get_first_child(&app.root_widget) == &app.info_panel)
    {
        cui_widget_remove_child(&app.root_widget, &app.info_panel);
    }
    else
    {
        cui_widget_insert_before(&app.root_widget, cui_widget_get_first_child(&app.root_widget), &app.info_panel);
    }

    cui_widget_layout(&app.root_widget, app.root_widget.rect);
    cui_window_request_redraw(app.root_widget.window);
}

static void
toggle_fullscreen(CuiWidget *widget)
{
    (void) widget;

    cui_window_set_fullscreen(app.window, !cui_window_is_fullscreen(app.window));
}

static void
signal_callback(void)
{
    if (cui_background_task_has_finished(&app.background_task))
    {
        for (CuiWidget *child = CuiContainerOf(app.info_label_height.list.next, CuiWidget, list),
                       *next  = CuiContainerOf(child->list.next, CuiWidget, list);
             &child->list != &app.label_column.children;
             child = next, next = CuiContainerOf(child->list.next, CuiWidget, list))
        {
            cui_widget_remove_child(&app.label_column, child);
        }

        for (CuiWidget *child = CuiContainerOf(app.info_label_height_content.list.next, CuiWidget, list),
                       *next  = CuiContainerOf(child->list.next, CuiWidget, list);
             &child->list != &app.content_column.children;
             child = next, next = CuiContainerOf(child->list.next, CuiWidget, list))
        {
            cui_widget_remove_child(&app.content_column, child);
        }

        ImageState *state = app.loading_state;
        app.loading_state = app.loaded_state;
        app.loaded_state = state;

        app.filename = app.loaded_state->filename;

        app.image_view.bitmap = app.loaded_state->bitmap;
        app.image_view.has_new_bitmap = true;

        cui_widget_set_label(&app.info_label_width_content,
                             cui_sprint(&app.loaded_state->memory, CuiStringLiteral("%d"),
                                        app.loaded_state->bitmap.width));

        cui_widget_set_label(&app.info_label_height_content,
                             cui_sprint(&app.loaded_state->memory, CuiStringLiteral("%d"),
                                        app.loaded_state->bitmap.height));

        for (int32_t i = 0; i < cui_array_count(app.loaded_state->meta_data); i += 1)
        {
            CuiImageMetaData *meta = app.loaded_state->meta_data + i;

            CuiWidget *label = cui_alloc_type(&app.loaded_state->memory, CuiWidget, CuiDefaultAllocationParams());

            cui_widget_init(label, CUI_WIDGET_TYPE_LABEL);
            cui_widget_set_label(label, CuiStringLiteral("<unknown-meta-type>"));
            cui_widget_set_x_axis_gravity(label, CUI_GRAVITY_END);
            cui_widget_set_y_axis_gravity(label, CUI_GRAVITY_START);
            cui_widget_append_child(&app.label_column, label);

            switch (meta->type)
            {
                case CUI_IMAGE_META_DATA_CAMERA_MAKER:
                {
                    cui_widget_set_label(label, CuiStringLiteral("Manufacturer"));
                } break;

                case CUI_IMAGE_META_DATA_CAMERA_MODEL:
                {
                    cui_widget_set_label(label, CuiStringLiteral("Camera"));
                } break;
            }

            CuiWidget *content = cui_alloc_type(&app.loaded_state->memory, CuiWidget, CuiDefaultAllocationParams());

            cui_widget_init(content, CUI_WIDGET_TYPE_LABEL);
            cui_widget_set_label(content, meta->value);
            cui_widget_set_x_axis_gravity(content, CUI_GRAVITY_START);
            cui_widget_set_y_axis_gravity(content, CUI_GRAVITY_START);
            cui_widget_append_child(&app.content_column, content);
        }

        cui_widget_layout(&app.root_widget, app.root_widget.rect);

        cui_window_request_redraw(app.image_view.base.window);
        cui_window_set_title(app.image_view.base.window, app.filename);

        app.image_is_loading = false;
        app.background_task.state = 0; // TODO: HACK: this sets it as non finished
    }
}

typedef struct StringArraySlice
{
    int32_t length;
    CuiString *elements;
} StringArraySlice;

static inline bool
string_is_smaller(CuiString a, CuiString b)
{
    int64_t len = a.count;

    if (b.count < len)
    {
        len = b.count;
    }

    for (int64_t i = 0; i < len; i += 1)
    {
        if (a.data[i] < b.data[i])
        {
            return true;
        }
        else if (a.data[i] > b.data[i])
        {
            return false;
        }
    }

    return a.count < b.count;
}

static void
merge_sort(StringArraySlice output, StringArraySlice input)
{
    CuiAssert(output.length == input.length);

    if (input.length < 2)
    {
        return;
    }

    StringArraySlice lower_output;
    lower_output.length = output.length / 2;
    lower_output.elements = output.elements;

    StringArraySlice upper_output;
    upper_output.length = output.length - lower_output.length;
    upper_output.elements = output.elements + lower_output.length;

    StringArraySlice lower_input;
    lower_input.length = input.length / 2;
    lower_input.elements = input.elements;

    StringArraySlice upper_input;
    upper_input.length = input.length - lower_input.length;
    upper_input.elements = input.elements + lower_input.length;

    merge_sort(lower_input, lower_output);
    merge_sort(upper_input, upper_output);

    int32_t i = 0, j = 0, k = 0;

    while ((i < lower_input.length) && (j < upper_input.length))
    {
        if (string_is_smaller(lower_input.elements[i], upper_input.elements[j]))
        {
            output.elements[k] = lower_input.elements[i];
            i += 1;
        }
        else
        {
            output.elements[k] = upper_input.elements[j];
            j += 1;
        }

        k += 1;
    }

    while (i < lower_input.length)
    {
        output.elements[k] = lower_input.elements[i];
        i += 1;
        k += 1;
    }

    while (j < upper_input.length)
    {
        output.elements[k] = upper_input.elements[j];
        j += 1;
        k += 1;
    }
}

static void
sort_filenames(CuiArena *temporary_memory, CuiString *filenames)
{
    int32_t filename_count = cui_array_count(filenames);

    CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(temporary_memory);

    CuiString *temp_array = 0;
    cui_array_init(temp_array, filename_count, temporary_memory);

    for (int32_t i = 0; i < filename_count; i += 1)
    {
        *cui_array_append(temp_array) = filenames[i];
    }

    StringArraySlice output;
    output.length = filename_count;
    output.elements = filenames;

    StringArraySlice input;
    input.length = filename_count;
    input.elements = temp_array;

    merge_sort(output, input);

    cui_end_temporary_memory(temp_memory);
}

CUI_PLATFORM_MAIN
{
    if (!CUI_PLATFORM_INIT)
    {
        return -1;
    }

    cui_set_signal_callback(signal_callback);

    cui_arena_allocate(&app.temporary_memory, CuiMiB(2));
    cui_arena_allocate(&app.files_list_memory, CuiMiB(1));

    cui_arena_allocate(&app.image_states[0].memory, CuiMiB(100));
    cui_arena_allocate(&app.image_states[1].memory, CuiMiB(100));

    cui_arena_allocate(&app.image_states[0].temporary_memory, CuiMiB(100));
    cui_arena_allocate(&app.image_states[1].temporary_memory, CuiMiB(100));

    app.loaded_state  = &app.image_states[0];
    app.loading_state = &app.image_states[1];

    CuiString filename = { 0 };

    CuiString *files_to_open = cui_get_files_to_open();
    CuiString *arguments = cui_get_command_line_arguments();

    if (cui_array_count(files_to_open) > 0)
    {
        filename = files_to_open[0];
    }
    else if (cui_array_count(arguments) > 0)
    {
        filename = arguments[0];
    }

    if (filename.count)
    {
        CuiString canonical_filename = cui_platform_get_canonical_filename(&app.temporary_memory, &app.temporary_memory, filename);

        printf("canonical filename = '%" CuiStringFmt "'\n", CuiStringArg(canonical_filename));

        int64_t index = canonical_filename.count;

        while (index > 0)
        {
#if CUI_PLATFORM_WINDOWS
            if (canonical_filename.data[index - 1] == '\\')
#else
            if (canonical_filename.data[index - 1] == '/')
#endif
            {
                app.directory.data  = canonical_filename.data;
                app.directory.count = index;

                filename.data   = canonical_filename.data + index;
                filename.count  = canonical_filename.count - index;
                break;
            }

            index -= 1;
        }

        printf("directory = '%" CuiStringFmt "'\n", CuiStringArg(app.directory));
        printf("filename  = '%" CuiStringFmt "'\n", CuiStringArg(filename));

        CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(&app.temporary_memory);

        CuiFileInfo *file_infos = 0;
        cui_array_init(file_infos, 32, &app.temporary_memory);

        cui_platform_get_files(&app.temporary_memory, &app.temporary_memory, app.directory, &file_infos);

        cui_array_init(app.files_list, 32, &app.files_list_memory);

        for (int32_t i = 0; i < cui_array_count(file_infos); i += 1)
        {
            CuiFileInfo info = file_infos[i];

            // TODO: case insensitive
            if (!cui_string_starts_with(info.name, CuiStringLiteral(".")) &&
                (cui_string_ends_with(info.name, CuiStringLiteral(".pbm")) ||
                 cui_string_ends_with(info.name, CuiStringLiteral(".qoi")) ||
                 cui_string_ends_with(info.name, CuiStringLiteral(".bmp")) ||
                 cui_string_ends_with(info.name, CuiStringLiteral(".jpg")) ||
                 cui_string_ends_with(info.name, CuiStringLiteral(".jpeg")) ||
                 cui_string_ends_with(info.name, CuiStringLiteral(".JPG")) ||
                 cui_string_ends_with(info.name, CuiStringLiteral(".JPEG"))))
            {
                *cui_array_append(app.files_list) = cui_copy_string(&app.files_list_memory, info.name);
            }
        }

        cui_end_temporary_memory(temp_memory);

        sort_filenames(&app.temporary_memory, app.files_list);

        load_image_file(filename);
    }

    app.window = cui_window_create(CUI_WINDOW_CREATION_FLAG_PREFER_SYSTEM_DECORATION);

    cui_window_set_title(app.window, CuiStringLiteral("Image Viewer"));
    cui_window_resize(app.window, lroundf(cui_window_get_ui_scale(app.window) * 800),
                                  lroundf(cui_window_get_ui_scale(app.window) * 600));

    // root widget
    cui_widget_init(&app.root_widget, CUI_WIDGET_TYPE_BOX);
    cui_widget_set_main_axis(&app.root_widget, CUI_AXIS_X);
    cui_widget_set_x_axis_gravity(&app.root_widget, CUI_GRAVITY_END);

    // into panel
    cui_widget_init(&app.info_panel, CUI_WIDGET_TYPE_BOX);
    cui_widget_add_flags(&app.info_panel, CUI_WIDGET_FLAG_DRAW_BACKGROUND);
    cui_widget_set_main_axis(&app.info_panel, CUI_AXIS_X);
    cui_widget_set_x_axis_gravity(&app.info_panel, CUI_GRAVITY_START);
    cui_widget_set_padding(&app.info_panel, 8.0f, 16.0f, 8.0f, 16.0f);
    cui_widget_set_inline_padding(&app.info_panel, 4.0f);

    // label column
    cui_widget_init(&app.label_column, CUI_WIDGET_TYPE_BOX);
    cui_widget_set_main_axis(&app.label_column, CUI_AXIS_Y);
    cui_widget_set_y_axis_gravity(&app.label_column, CUI_GRAVITY_START);
    cui_widget_set_inline_padding(&app.label_column, 4.0f);
    cui_widget_append_child(&app.info_panel, &app.label_column);

    // content column
    cui_widget_init(&app.content_column, CUI_WIDGET_TYPE_BOX);
    cui_widget_set_main_axis(&app.content_column, CUI_AXIS_Y);
    cui_widget_set_y_axis_gravity(&app.content_column, CUI_GRAVITY_START);
    cui_widget_set_inline_padding(&app.content_column, 4.0f);
    cui_widget_append_child(&app.info_panel, &app.content_column);

    // info label width
    cui_widget_init(&app.info_label_width, CUI_WIDGET_TYPE_LABEL);
    cui_widget_set_label(&app.info_label_width, CuiStringLiteral("Width"));
    cui_widget_set_x_axis_gravity(&app.info_label_width, CUI_GRAVITY_END);
    cui_widget_set_y_axis_gravity(&app.info_label_width, CUI_GRAVITY_START);
    cui_widget_append_child(&app.label_column, &app.info_label_width);

    // info label width content
    cui_widget_init(&app.info_label_width_content, CUI_WIDGET_TYPE_LABEL);
    cui_widget_set_label(&app.info_label_width_content, CuiStringLiteral("0"));
    cui_widget_set_x_axis_gravity(&app.info_label_width_content, CUI_GRAVITY_START);
    cui_widget_set_y_axis_gravity(&app.info_label_width_content, CUI_GRAVITY_START);
    cui_widget_append_child(&app.content_column, &app.info_label_width_content);

    // info label height
    cui_widget_init(&app.info_label_height, CUI_WIDGET_TYPE_LABEL);
    cui_widget_set_label(&app.info_label_height, CuiStringLiteral("Height"));
    cui_widget_set_x_axis_gravity(&app.info_label_height, CUI_GRAVITY_END);
    cui_widget_set_y_axis_gravity(&app.info_label_height, CUI_GRAVITY_START);
    cui_widget_append_child(&app.label_column, &app.info_label_height);

    // info label height content
    cui_widget_init(&app.info_label_height_content, CUI_WIDGET_TYPE_LABEL);
    cui_widget_set_label(&app.info_label_height_content, CuiStringLiteral("0"));
    cui_widget_set_x_axis_gravity(&app.info_label_height_content, CUI_GRAVITY_START);
    cui_widget_set_y_axis_gravity(&app.info_label_height_content, CUI_GRAVITY_START);
    cui_widget_append_child(&app.content_column, &app.info_label_height_content);

    // ui stack
    cui_widget_init(&app.ui_stack, CUI_WIDGET_TYPE_STACK);
    cui_widget_append_child(&app.root_widget, &app.ui_stack);

    // image view
    cui_widget_init(&app.image_view.base, WIDGET_TYPE_IMAGE_VIEW);
    CuiWidgetInitCustomFunctions(&app.image_view.base, image_view_);
    cui_widget_append_child(&app.ui_stack, &app.image_view.base);

    app.image_view.texture_id = -1;

#if CUI_PLATFORM_ANDROID
    // ui overlay
    cui_widget_init(&app.ui_overlay, CUI_WIDGET_TYPE_BOX);
    cui_widget_set_main_axis(&app.ui_overlay, CUI_AXIS_Y);
    cui_widget_set_y_axis_gravity(&app.ui_overlay, CUI_GRAVITY_CENTER);
    cui_widget_append_child(&app.ui_stack, &app.ui_overlay);

    // ui overlay top
    cui_widget_init(&app.ui_overlay_top, CUI_WIDGET_TYPE_BOX);
    cui_widget_append_child(&app.ui_overlay, &app.ui_overlay_top);

    // ui overlay middle
    cui_widget_init(&app.ui_overlay_middle, CUI_WIDGET_TYPE_BOX);
    cui_widget_set_main_axis(&app.ui_overlay_middle, CUI_AXIS_X);
    cui_widget_set_x_axis_gravity(&app.ui_overlay_middle, CUI_GRAVITY_START);
    cui_widget_set_padding(&app.ui_overlay_middle, 0.0f, 16.0f, 0.0f, 16.0f);
    cui_widget_append_child(&app.ui_overlay, &app.ui_overlay_middle);

    // ui overlay bottom
    cui_widget_init(&app.ui_overlay_bottom, CUI_WIDGET_TYPE_BOX);
    cui_widget_set_main_axis(&app.ui_overlay_bottom, CUI_AXIS_Y);
    cui_widget_set_y_axis_gravity(&app.ui_overlay_bottom, CUI_GRAVITY_END);
    cui_widget_append_child(&app.ui_overlay, &app.ui_overlay_bottom);

    // navigation left
    cui_widget_init(&app.nav_left, CUI_WIDGET_TYPE_BUTTON);
    cui_widget_set_icon(&app.nav_left, CUI_ICON_ANGLE_LEFT_12);
    cui_widget_set_padding(&app.nav_left, 12.0f, 12.0f, 12.0f, 12.0f);
    cui_widget_set_border_radius(&app.nav_left, 4.0f, 4.0f, 4.0f, 4.0f);
    cui_widget_set_border_width(&app.nav_left, 0.0f, 0.0f, 0.0f, 0.0f);
    cui_widget_append_child(&app.ui_overlay_middle, &app.nav_left);

    app.nav_left.on_action = on_nav_left;

    // navigation box right
    cui_widget_init(&app.nav_box_right, CUI_WIDGET_TYPE_BOX);
    cui_widget_set_main_axis(&app.nav_box_right, CUI_AXIS_X);
    cui_widget_set_x_axis_gravity(&app.nav_box_right, CUI_GRAVITY_END);
    cui_widget_append_child(&app.ui_overlay_middle, &app.nav_box_right);

    // navigation right
    cui_widget_init(&app.nav_right, CUI_WIDGET_TYPE_BUTTON);
    cui_widget_set_icon(&app.nav_right, CUI_ICON_ANGLE_RIGHT_12);
    cui_widget_set_padding(&app.nav_right, 12.0f, 12.0f, 12.0f, 12.0f);
    cui_widget_set_border_radius(&app.nav_right, 4.0f, 4.0f, 4.0f, 4.0f);
    cui_widget_set_border_width(&app.nav_right, 0.0f, 0.0f, 0.0f, 0.0f);
    cui_widget_append_child(&app.nav_box_right, &app.nav_right);

    app.nav_right.on_action = on_nav_right;

    // navigation box center
    cui_widget_init(&app.nav_box_center, CUI_WIDGET_TYPE_BOX);
    cui_widget_append_child(&app.nav_box_right, &app.nav_box_center);

    // control bar wrapper
    cui_widget_init(&app.control_bar_wrapper, CUI_WIDGET_TYPE_BOX);
    cui_widget_set_main_axis(&app.control_bar_wrapper, CUI_AXIS_X);
    cui_widget_set_x_axis_gravity(&app.control_bar_wrapper, CUI_GRAVITY_CENTER);
    cui_widget_set_padding(&app.control_bar_wrapper, 0.0f, 0.0f, 48.0f, 0.0f);
    cui_widget_append_child(&app.ui_overlay_bottom, &app.control_bar_wrapper);
#else
    // ui overlay
    cui_widget_init(&app.ui_overlay, CUI_WIDGET_TYPE_BOX);
    cui_widget_set_main_axis(&app.ui_overlay, CUI_AXIS_Y);
    cui_widget_set_y_axis_gravity(&app.ui_overlay, CUI_GRAVITY_END);
    cui_widget_append_child(&app.ui_stack, &app.ui_overlay);

    // control bar wrapper
    cui_widget_init(&app.control_bar_wrapper, CUI_WIDGET_TYPE_BOX);
    cui_widget_set_main_axis(&app.control_bar_wrapper, CUI_AXIS_X);
    cui_widget_set_x_axis_gravity(&app.control_bar_wrapper, CUI_GRAVITY_CENTER);
    cui_widget_set_padding(&app.control_bar_wrapper, 0.0f, 0.0f, 48.0f, 0.0f);
    cui_widget_append_child(&app.ui_overlay, &app.control_bar_wrapper);
#endif

    // control bar wrapper left
    cui_widget_init(&app.control_bar_wrapper_left, CUI_WIDGET_TYPE_BOX);
    cui_widget_append_child(&app.control_bar_wrapper, &app.control_bar_wrapper_left);

    // control bar
    cui_widget_init(&app.control_bar, CUI_WIDGET_TYPE_BOX);
    cui_widget_add_flags(&app.control_bar, CUI_WIDGET_FLAG_DRAW_BACKGROUND);
    cui_widget_set_main_axis(&app.control_bar, CUI_AXIS_X);
    cui_widget_set_x_axis_gravity(&app.control_bar, CUI_GRAVITY_START);
    cui_widget_set_border_radius(&app.control_bar, 4.0f, 4.0f, 4.0f, 4.0f);
    cui_widget_set_padding(&app.control_bar, 2.0f, 2.0f, 2.0f, 2.0f);
    cui_widget_set_inline_padding(&app.control_bar, 2.0f);
    cui_widget_append_child(&app.control_bar_wrapper, &app.control_bar);

    // fullscreen button
    cui_widget_init(&app.fullscreen_button, CUI_WIDGET_TYPE_BUTTON);
    cui_widget_set_icon(&app.fullscreen_button, CUI_ICON_EXPAND_12);
    cui_widget_set_padding(&app.fullscreen_button, 8.0f, 8.0f, 8.0f, 8.0f);
    cui_widget_set_border_radius(&app.fullscreen_button, 2.0f, 2.0f, 2.0f, 2.0f);
    cui_widget_set_border_width(&app.fullscreen_button, 0.0f, 0.0f, 0.0f, 0.0f);
    cui_widget_append_child(&app.control_bar, &app.fullscreen_button);

    app.fullscreen_button.on_action = toggle_fullscreen;

#if !CUI_PLATFORM_ANDROID
    // navigation left
    cui_widget_init(&app.nav_left, CUI_WIDGET_TYPE_BUTTON);
    cui_widget_set_icon(&app.nav_left, CUI_ICON_ANGLE_LEFT_12);
    cui_widget_set_padding(&app.nav_left, 8.0f, 8.0f, 8.0f, 8.0f);
    cui_widget_set_border_radius(&app.nav_left, 2.0f, 0.0f, 0.0f, 2.0f);
    cui_widget_set_border_width(&app.nav_left, 0.0f, 0.0f, 0.0f, 0.0f);
    cui_widget_append_child(&app.control_bar, &app.nav_left);

    app.nav_left.on_action = on_nav_left;

    // navigation right
    cui_widget_init(&app.nav_right, CUI_WIDGET_TYPE_BUTTON);
    cui_widget_set_icon(&app.nav_right, CUI_ICON_ANGLE_RIGHT_12);
    cui_widget_set_padding(&app.nav_right, 8.0f, 8.0f, 8.0f, 8.0f);
    cui_widget_set_border_radius(&app.nav_right, 0.0f, 2.0f, 2.0f, 0.0f);
    cui_widget_set_border_width(&app.nav_right, 0.0f, 0.0f, 0.0f, 0.0f);
    cui_widget_append_child(&app.control_bar, &app.nav_right);

    app.nav_right.on_action = on_nav_right;
#endif

    // info button
    cui_widget_init(&app.info_button, CUI_WIDGET_TYPE_BUTTON);
    cui_widget_set_icon(&app.info_button, CUI_ICON_INFO_12);
    cui_widget_set_padding(&app.info_button, 8.0f, 8.0f, 8.0f, 8.0f);
    cui_widget_set_border_radius(&app.info_button, 2.0f, 2.0f, 2.0f, 2.0f);
    cui_widget_set_border_width(&app.info_button, 0.0f, 0.0f, 0.0f, 0.0f);
    cui_widget_append_child(&app.control_bar, &app.info_button);

    app.info_button.on_action = toggle_info_panel;

    // control bar wrapper right
    cui_widget_init(&app.control_bar_wrapper_right, CUI_WIDGET_TYPE_BOX);
    cui_widget_append_child(&app.control_bar_wrapper, &app.control_bar_wrapper_right);

#if CUI_PLATFORM_ANDROID
    // ui overlay bottom rest
    cui_widget_init(&app.ui_overlay_bottom_rest, CUI_WIDGET_TYPE_BOX);
    cui_widget_append_child(&app.ui_overlay_bottom, &app.ui_overlay_bottom_rest);
#else
    // ui overlay bottom rest
    cui_widget_init(&app.ui_overlay_bottom_rest, CUI_WIDGET_TYPE_BOX);
    cui_widget_append_child(&app.ui_overlay, &app.ui_overlay_bottom_rest);
#endif

    cui_window_set_root_widget(app.window, &app.root_widget);

    cui_window_show(app.window);

    return cui_main_loop();
}
