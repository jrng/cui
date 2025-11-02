CuiString
cui_string_parse_identifier_advance(CuiString *str)
{
    CuiString result = cui_make_string(str->data, 0);

    while ((result.count < str->count) && !cui_unicode_is_whitespace(str->data[result.count]))
    {
        result.count += 1;
    }

    cui_string_advance(str, result.count);

    return result;
}

int32_t
cui_string_parse_int32(CuiString str)
{
    bool sign = false;
    int32_t value = 0;

    if (str.count && (str.data[0] == '-'))
    {
        sign = true;
        cui_string_advance(&str, 1);
    }

    int64_t index = 0;

    while ((index < str.count) && cui_unicode_is_digit(str.data[index]))
    {
        // TODO: check for overflow
        value = (10 * value) + (str.data[index] - '0');
        index += 1;
    }

    if (sign) value = -value;

    return value;
}

int32_t
cui_string_parse_int32_advance(CuiString *str)
{
    bool sign = false;
    int32_t value = 0;

    if (str->count && (str->data[0] == '-'))
    {
        sign = true;
        cui_string_advance(str, 1);
    }

    int64_t index = 0;

    while ((index < str->count) && cui_unicode_is_digit(str->data[index]))
    {
        // TODO: check for overflow
        value = (10 * value) + (str->data[index] - '0');
        index += 1;
    }

    cui_string_advance(str, index);

    if (sign) value = -value;

    return value;
}

void
cui_string_skip_whitespace(CuiString *str)
{
    while ((str->count > 0) && cui_unicode_is_whitespace(str->data[0]))
    {
        cui_string_advance(str, 1);
    }
}

bool
cui_string_contains(CuiString str, CuiString sub_string)
{
    if (str.count < sub_string.count) return false;

    int64_t end = str.count - sub_string.count;

    for (int64_t i = 0; i <= end; i += 1)
    {
        if (cui_string_equals(cui_make_string(str.data + i, sub_string.count), sub_string))
        {
            return true;
        }
    }

    return false;
}

CuiString
cui_string_trim(CuiString str)
{
    while ((str.count > 0) && cui_unicode_is_whitespace(str.data[str.count - 1]))
    {
        str.count -= 1;
    }

    while ((str.count > 0) && cui_unicode_is_whitespace(str.data[0]))
    {
        cui_string_advance(&str, 1);
    }

    return str;
}

CuiString
cui_string_get_next_line(CuiString *str)
{
    CuiString result = cui_make_string(str->data, 0);

    while (str->count > 0)
    {
        if ((str->data[0] == '\r') && (str->count > 1) && (str->data[1] == '\n'))
        {
            cui_string_advance(str, 2);
            break;
        }
        else if (str->data[0] == '\n')
        {
            cui_string_advance(str, 1);
            break;
        }

        cui_string_advance(str, 1);
        result.count += 1;
    }

    return result;
}

CuiString
cui_copy_string(CuiArena *arena, CuiString str)
{
    CuiString result = { 0 };

    if (str.count)
    {
        result.count = str.count;
        result.data  = cui_alloc_array(arena, uint8_t, str.count, CuiDefaultAllocationParams());

        uint8_t *src = str.data;
        uint8_t *dst = result.data;

        while (str.count--)
        {
            *dst++ = *src++;
        }
    }

    return result;
}

CuiString
cui_string_concat_n(CuiArena *arena, int32_t n, ...)
{
    CuiAssert(n <= 10);

    CuiString strings[10];
    CuiString result = { 0 };

    va_list args;
    va_start(args, n);

    for (int32_t i = 0; i < n; i += 1)
    {
        strings[i] = va_arg(args, CuiString);
        result.count += strings[i].count;
    }

    va_end(args);

    result.data = cui_alloc_array(arena, uint8_t, result.count, CuiDefaultAllocationParams());

    CuiAssert(result.data);

    uint8_t *at = result.data;

    for (int32_t i = 0; i < n; i += 1)
    {
        CuiString str = strings[i];

        for (int64_t j = 0; j < str.count; j += 1)
        {
            *at++ = str.data[j];
        }
    }

    return result;
}

CuiString
cui_path_concat(CuiArena *arena, CuiString a, CuiString b)
{
#if CUI_PLATFORM_WINDOWS
    if (a.count && (a.data[a.count - 1] == '\\'))
#else
    if (a.count && (a.data[a.count - 1] == '/'))
#endif
    {
        a.count -= 1;
    }

#if CUI_PLATFORM_WINDOWS
    if (b.count && (b.data[0] == '\\'))
#else
    if (b.count && (b.data[0] == '/'))
#endif
    {
        cui_string_advance(&b, 1);
    }

#if CUI_PLATFORM_WINDOWS
    return cui_string_concat(arena, a, CuiStringLiteral("\\"), b);
#else
    return cui_string_concat(arena, a, CuiStringLiteral("/"), b);
#endif
}

void
cui_path_remove_last_directory(CuiString *dir)
{
#if CUI_PLATFORM_WINDOWS
    if ((dir->count > 1) && (dir->data[dir->count - 1] == '\\'))
#else
    if ((dir->count > 1) && (dir->data[dir->count - 1] == '/'))
#endif
    {
        dir->count -= 1;
    }

#if CUI_PLATFORM_WINDOWS
    while (dir->count && (dir->data[dir->count - 1] != '\\'))
#else
    while (dir->count && (dir->data[dir->count - 1] != '/'))
#endif
    {
        dir->count -= 1;
    }

#if CUI_PLATFORM_WINDOWS
    if ((dir->count > 1) && (dir->data[dir->count - 1] == '\\'))
#else
    if ((dir->count > 1) && (dir->data[dir->count - 1] == '/'))
#endif
    {
        dir->count -= 1;
    }
}

char *
cui_to_c_string(CuiArena *arena, CuiString str)
{
    // NOTE: we do two zero bytes at the end
    // to handle wide character encodings.
    char *result = cui_alloc_array(arena, char, str.count + 2, CuiDefaultAllocationParams());

    uint8_t *src = str.data;
    uint8_t *dst = (uint8_t *) result;

    while (str.count--)
    {
        *dst++ = *src++;
    }

    *dst++ = 0;
    *dst = 0;

    return result;
}
