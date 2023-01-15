int32_t
cui_parse_int32(CuiString str)
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
