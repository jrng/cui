void
cui_text_input_allocate(CuiTextInput *input, int64_t capacity)
{
    input->cursor_start = 0;
    input->cursor_end = 0;
    input->count = 0;
    input->capacity = capacity;
    input->data = cui_platform_allocate(input->capacity);
}

void
cui_text_input_clear(CuiTextInput *input)
{
    input->cursor_start = 0;
    input->cursor_end = 0;
    input->count = 0;
}

void
cui_text_input_set_string_value(CuiTextInput *input, CuiString str, int64_t cursor_start, int64_t cursor_end)
{
    CuiAssert(str.count <= input->capacity);

    input->count = str.count;

    uint8_t *dst = input->data;
    uint8_t *src = str.data;

    for (int64_t index = 0; index < str.count; index += 1)
    {
        *dst++ = *src++;
    }

    input->cursor_start = cursor_start;
    input->cursor_end   = cursor_end;
}

void
cui_text_input_select_all(CuiTextInput *input)
{
    input->cursor_start = 0;
    input->cursor_end = cui_utf8_get_character_count(cui_text_input_to_string(*input));
}

void
cui_text_input_delete_range(CuiTextInput *input, int64_t start, int64_t end)
{
    CuiString str = cui_text_input_to_string(*input);

    int64_t byte_start = cui_utf8_get_character_byte_offset(str, start);
    int64_t byte_end   = cui_utf8_get_character_byte_offset(str, end);

    uint8_t *src  = input->data + byte_end;
    uint8_t *dst  = input->data + byte_start;
    int64_t count = input->count - byte_end;

    while (count-- > 0)
    {
        *dst++ = *src++;
    }

    input->count -= (byte_end - byte_start);
}

void
cui_text_input_delete_selected_range(CuiTextInput *input)
{
    int64_t a = cui_min_int64(input->cursor_start, input->cursor_end);
    int64_t b = cui_max_int64(input->cursor_start, input->cursor_end);
    cui_text_input_delete_range(input, a, b);
    input->cursor_end   = a;
    input->cursor_start = a;
}

CuiString
cui_text_input_get_selected_text(CuiTextInput *input)
{
    int64_t start = cui_min_int64(input->cursor_start, input->cursor_end);
    int64_t end   = cui_max_int64(input->cursor_start, input->cursor_end);

    CuiString str = cui_text_input_to_string(*input);

    int64_t byte_start = cui_utf8_get_character_byte_offset(str, start);
    int64_t byte_end   = cui_utf8_get_character_byte_offset(str, end);

    CuiString result;

    result.count = byte_end - byte_start;
    result.data  = input->data + byte_start;

    return result;
}

void
cui_text_input_insert_string(CuiTextInput *input, CuiString str)
{
    if (input->cursor_start != input->cursor_end)
    {
        cui_text_input_delete_selected_range(input);
    }

    int64_t index = 0;

    while (index < str.count)
    {
        CuiUnicodeResult utf8 = cui_utf8_decode(str, index);

        cui_text_input_insert_codepoint(input, utf8.codepoint);
        index += utf8.byte_count;
    }
}

void
cui_text_input_insert_codepoint(CuiTextInput *input, uint32_t codepoint)
{
    if (input->cursor_start != input->cursor_end)
    {
        cui_text_input_delete_selected_range(input);
    }

    uint8_t buf[4];
    int64_t byte_count = cui_utf8_encode(cui_make_string(buf, sizeof(buf)), 0, codepoint);

    if ((byte_count > 0) && ((input->count + byte_count) <= input->capacity))
    {
        CuiString str   = cui_text_input_to_string(*input);
        int64_t at_byte = cui_utf8_get_character_byte_offset(str, input->cursor_end);
        int64_t count   = input->count - at_byte;

        uint8_t *src = input->data + at_byte;
        uint8_t *dst = src + byte_count;

        cui_copy_memory(dst, src, count);
        cui_copy_memory(input->data + at_byte, buf, byte_count);

        input->count += byte_count;
        input->cursor_end  += 1;
        input->cursor_start = input->cursor_end;
    }
}

bool
cui_text_input_backspace(CuiTextInput *input)
{
    bool result = false;

    if (input->cursor_start == input->cursor_end)
    {
        if (input->cursor_end > 0)
        {
            input->cursor_end -= 1;
            cui_text_input_delete_selected_range(input);
            result = true;
        }
    }
    else
    {
        cui_text_input_delete_selected_range(input);
        result = true;
    }

    return result;
}

void
cui_text_input_move_left(CuiTextInput *input, bool shift_is_down)
{
    if (shift_is_down)
    {
        input->cursor_end = cui_max_int64(input->cursor_end - 1, (int64_t) 0);
    }
    else
    {
        int64_t at = cui_min_int64(input->cursor_start, input->cursor_end);
        if (input->cursor_start == input->cursor_end)
        {
            input->cursor_end = cui_max_int64(at - 1, (int64_t) 0);
        }
        else
        {
            input->cursor_end = at;
        }
        input->cursor_start = input->cursor_end;
    }
}

void
cui_text_input_move_right(CuiTextInput *input, bool shift_is_down)
{
    if (shift_is_down)
    {
        CuiString str = cui_text_input_to_string(*input);
        int64_t max_count = cui_utf8_get_character_count(str);
        input->cursor_end = cui_min_int64(input->cursor_end + 1, max_count);
    }
    else
    {
        int64_t at = cui_max_int64(input->cursor_start, input->cursor_end);
        if (input->cursor_start == input->cursor_end)
        {
            int64_t max_count = cui_utf8_get_character_count(cui_text_input_to_string(*input));
            input->cursor_end = cui_min_int64(at + 1, max_count);
        }
        else
        {
            input->cursor_end = at;
        }
        input->cursor_start = input->cursor_end;
    }
}
