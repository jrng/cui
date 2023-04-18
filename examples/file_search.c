#include <cui.h>

typedef struct FileEntry
{
    int32_t parent_index;
    bool is_directory;
    CuiString name;
} FileEntry;

typedef struct FileSearch
{
    CuiArena temporary_memory;
    CuiArena widget_arena;
    CuiArena files_arena;
    CuiArena file_names_arena;

    CuiString directory;

    FileEntry *files;

    CuiWindow *window;

    CuiWidget *root_widget;

    CuiWidget *search_input;
    CuiWidget *search_results;
    CuiWidget *last_search_result;

    CuiWidget *free_list;
} FileSearch;

static FileSearch app;

static void
scan_directory(CuiString directory)
{
#if 0
    printf("directory = '%.*s'\n", (int) directory.count, directory.data);
#endif

    app.files = 0;
    cui_array_init(app.files, 100, &app.files_arena);

    int32_t current_parent_index = -1;

    while (current_parent_index < cui_array_count(app.files))
    {
        if ((current_parent_index < 0) || app.files[current_parent_index].is_directory)
        {
            CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(&app.temporary_memory);

            CuiFileInfo *file_infos = 0;
            cui_array_init(file_infos, 32, &app.temporary_memory);

            CuiString path = { 0 };
            int32_t parent_index = current_parent_index;

            while (parent_index > 0)
            {
                FileEntry file_entry = app.files[parent_index];
                path = cui_path_concat(&app.temporary_memory, file_entry.name, path);
                parent_index = file_entry.parent_index;
            }

            path = cui_path_concat(&app.temporary_memory, directory, path);

#if 0
            printf("--- read dir '%.*s'\n", (int) path.count, path.data);
#endif

            cui_platform_get_files(&app.temporary_memory, &app.temporary_memory, path, &file_infos);

            for (int32_t i = 0; i < cui_array_count(file_infos); i += 1)
            {
                CuiFileInfo info = file_infos[i];

                if (!cui_string_starts_with(info.name, CuiStringLiteral(".")))
                {
#if 0
                    printf("%d entry '%.*s'\n", i, (int) info.name.count, info.name.data);
#endif

                    FileEntry *file_entry = cui_array_append(app.files);

                    file_entry->parent_index = current_parent_index;
                    file_entry->is_directory = (info.attr.flags & CUI_FILE_ATTRIBUTE_IS_DIRECTORY) ? true : false;
                    file_entry->name = cui_copy_string(&app.file_names_arena, info.name);
                }
            }

            cui_end_temporary_memory(temp_memory);
        }

        current_parent_index += 1;
    }
}

static void
read_index_file(CuiString index_filename)
{
    app.files = 0;
    cui_array_init(app.files, 100, &app.files_arena);

    CuiFile *file = cui_platform_file_open(&app.temporary_memory, index_filename, CUI_FILE_MODE_READ);

    if (file)
    {
        uint64_t file_size = cui_platform_file_get_size(file);
        char *buffer = (char *) cui_platform_allocate(file_size);

        cui_platform_file_read(file, buffer, 0, file_size);

        CuiString content = cui_make_string(buffer, file_size);
        int64_t index = 0;

        while (index < content.count)
        {
            FileEntry *file_entry = cui_array_append(app.files);

            char first_char = content.data[index];
            index += 9;

            CuiString name;
            name.data = content.data + index;

            while ((index < content.count) && (content.data[index] != '\n'))
            {
                index += 1;
            }

            name.count = (content.data + index) - name.data;

            // file_entry->parent_index = current_parent_index;
            file_entry->is_directory = (first_char == '1') ? true : false;
            file_entry->name = cui_copy_string(&app.file_names_arena, name);

            index += 1;
        }

        cui_platform_deallocate(buffer, file_size);
        cui_platform_file_close(file);
    }
}

static void
write_index_file(CuiString index_filename)
{
    CuiFile *file = cui_platform_file_create(&app.temporary_memory, index_filename);

    if (file)
    {
        CuiArena arena;
        CuiStringBuilder builder;

        cui_arena_allocate(&arena, CuiMiB(100));
        cui_string_builder_init(&builder, &arena);

        int32_t count = cui_array_count(app.files);

        for (int32_t i = 0; i < count; i += 1)
        {
            FileEntry file_entry = app.files[i];

            cui_string_builder_print(&builder, CuiStringLiteral("%d%08X%S\n"),
                                     file_entry.is_directory ? 1 : 0, file_entry.parent_index, file_entry.name);
        }

        int64_t size = cui_string_builder_get_size(&builder);

        cui_platform_file_truncate(file, size);
        cui_string_builder_write_to_file(&builder, file, 0);

        cui_arena_deallocate(&arena);
        cui_platform_file_close(file);
    }
}

static CuiWidget *
create_widget(CuiArena *arena, uint32_t type)
{
    CuiWidget *widget = cui_widget_get_first_child(app.free_list);

    if (widget)
    {
        cui_widget_remove_child(app.free_list, widget);
    }
    else
    {
        widget = cui_alloc_type(arena, CuiWidget, CuiDefaultAllocationParams());
    }

    cui_widget_init(widget, type);
    return widget;
}

static void
destroy_widget(CuiWidget *widget)
{
    cui_widget_append_child(app.free_list, widget);
}

static void
clear_search_results(void)
{
    CuiWidget *child = cui_widget_get_first_child(app.search_results);

    while (child != app.last_search_result)
    {
        cui_widget_remove_child(app.search_results, child);
        destroy_widget(child);
        child = cui_widget_get_first_child(app.search_results);
    }
}

static void
insert_search_result(CuiString name)
{
    CuiWidget *widget = create_widget(&app.widget_arena, CUI_WIDGET_TYPE_LABEL);

    cui_widget_set_label(widget, name);
    cui_widget_add_flags(widget, CUI_WIDGET_FLAG_DRAW_BACKGROUND);

    cui_widget_insert_before(app.search_results, app.last_search_result, widget);
}

static bool
filename_matches(CuiString filename, CuiString search)
{
    if (filename.count >= search.count)
    {
        int64_t index = 0;
        int64_t end_index = filename.count - search.count;

        while (index <= end_index)
        {
            CuiString test_string = filename;
            cui_string_advance(&test_string, index);

            if (cui_string_starts_with(test_string, search))
            {
                return true;
            }

            index += 1;
        }
    }

    return false;
}

static void
on_input_action(CuiWidget *widget)
{
    CuiString value = cui_widget_get_textinput_value(widget);

    if (cui_utf8_get_character_count(value) >= 3)
    {
        clear_search_results();

        int32_t count = cui_array_count(app.files);

        for (int32_t i = 0; i < count; i += 1)
        {
            FileEntry file_entry = app.files[i];

            if (filename_matches(file_entry.name, value))
            {
                insert_search_result(file_entry.name);
            }
        }

        cui_widget_relayout_parent(app.search_results);
    }
}

static void
create_user_interface(CuiWindow *window, CuiArena *arena)
{
    app.root_widget = create_widget(arena, CUI_WIDGET_TYPE_BOX);

    cui_widget_set_main_axis(app.root_widget, CUI_AXIS_Y);
    cui_widget_set_y_axis_gravity(app.root_widget, CUI_GRAVITY_START);
    cui_widget_add_flags(app.root_widget, CUI_WIDGET_FLAG_DRAW_BACKGROUND);
    cui_widget_set_inline_padding(app.root_widget, 8.0f);
    cui_widget_set_padding(app.root_widget, 8.0f, 8.0f, 4.0f, 8.0f);

    app.search_input = create_widget(arena, CUI_WIDGET_TYPE_TEXTINPUT);

    cui_widget_set_icon(app.search_input, CUI_ICON_SEARCH_12);
    cui_widget_set_label(app.search_input, CuiStringLiteral("Search files and folders"));
    cui_widget_set_textinput_buffer(app.search_input, cui_alloc(arena, CuiKiB(1), CuiDefaultAllocationParams()), CuiKiB(1));

    app.search_input->on_action = on_input_action;

    cui_widget_append_child(app.root_widget, app.search_input);

    CuiWidget *bottom = create_widget(arena, CUI_WIDGET_TYPE_BOX);

    cui_widget_set_main_axis(bottom, CUI_AXIS_Y);
    cui_widget_set_y_axis_gravity(bottom, CUI_GRAVITY_END);
    cui_widget_set_inline_padding(bottom, 4.0f);

    cui_widget_append_child(app.root_widget, bottom);

    CuiWidget *directory_label = create_widget(&app.widget_arena, CUI_WIDGET_TYPE_LABEL);

    cui_widget_set_label(directory_label, app.directory);

    cui_widget_append_child(bottom, directory_label);

    app.search_results = create_widget(arena, CUI_WIDGET_TYPE_BOX);

    cui_widget_set_main_axis(app.search_results, CUI_AXIS_Y);
    cui_widget_set_y_axis_gravity(app.search_results, CUI_GRAVITY_START);
    cui_widget_add_flags(app.search_results, CUI_WIDGET_FLAG_DRAW_BACKGROUND);
    cui_widget_set_border_width(app.search_results, 1.0f, 1.0f, 1.0f, 1.0f);
    cui_widget_set_border_radius(app.search_results, 4.0f, 4.0f, 4.0f, 4.0f);

    cui_widget_append_child(bottom, app.search_results);

    app.last_search_result = create_widget(&app.widget_arena, CUI_WIDGET_TYPE_BOX);

    cui_widget_append_child(app.search_results, app.last_search_result);

    cui_window_set_root_widget(window, app.root_widget);
}

CUI_PLATFORM_MAIN
{
    if (!CUI_PLATFORM_INIT)
    {
        return -1;
    }

    cui_arena_allocate(&app.temporary_memory, CuiMiB(2));
    cui_arena_allocate(&app.widget_arena, CuiMiB(32));
    cui_arena_allocate(&app.files_arena, CuiMiB(64));
    cui_arena_allocate(&app.file_names_arena, CuiMiB(32));

    app.directory = cui_platform_get_current_working_directory(&app.temporary_memory, &app.widget_arena);

    CuiString index_filename = CuiStringLiteral("file_index.txt");

    if (cui_platform_file_exists(&app.temporary_memory, index_filename))
    {
        read_index_file(index_filename);
    }
    else
    {
        scan_directory(app.directory);
        write_index_file(index_filename);
    }

    int32_t file_count = cui_array_count(app.files);

    printf("file_count = %d\n", file_count);
    printf("files_arena = %lu / %lu\n", app.files_arena.occupied, app.files_arena.capacity);
    printf("file_names_arena = %lu / %lu\n", app.file_names_arena.occupied, app.file_names_arena.capacity);

#if 0
    for (int32_t i = 0; i < file_count; i += 1)
    {
        FileEntry file_entry = app.files[i];
        printf(" %04d %s '%.*s'\n", file_entry.parent_index, file_entry.is_directory ? " dir" : "file", (int) file_entry.name.count, file_entry.name.data);
    }
#endif

    app.window = cui_window_create(0);

    if (!app.window)
    {
        return -1;
    }

    cui_window_set_title(app.window, CuiStringLiteral("File Search"));
    cui_window_resize(app.window, lroundf(cui_window_get_ui_scale(app.window) * 400),
                                  lroundf(cui_window_get_ui_scale(app.window) * 500));

    app.free_list = cui_alloc_type(&app.widget_arena, CuiWidget, CuiDefaultAllocationParams());
    cui_widget_init(app.free_list, CUI_WIDGET_TYPE_BOX);

    create_user_interface(app.window, &app.widget_arena);

    cui_window_show(app.window);

    return cui_main_loop();
}
