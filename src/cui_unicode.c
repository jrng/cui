bool
cui_unicode_is_digit(uint32_t c)
{
    return ((c >= '0') && (c <= '9'));
}

bool
cui_unicode_is_printable(uint32_t c)
{
    // TODO: is this complete?
    return ((c >= 32) && (c != 127));
}

CuiUnicodeResult
cui_utf8_decode(CuiString str, int64_t index)
{
    CuiUnicodeResult result = { '?', 1 };

    if (index < str.count)
    {
        uint8_t c = str.data[index];

        if ((c & 0x80) == 0x00)
        {
            result.codepoint = (c & 0x7F);
        }
        else if (((c & 0xE0) == 0xC0) && ((index + 1) < str.count) &&
                 ((str.data[index + 1] & 0xC0) == 0x80))
        {
            result.codepoint = ((uint32_t) (c & 0x1F) << 6) |
                                (uint32_t) (str.data[index + 1] & 0x3F);
            result.byte_count = 2;
        }
        else if (((c & 0xF0) == 0xE0) && ((index + 2) < str.count) &&
                 ((str.data[index + 1] & 0xC0) == 0x80) &&
                 ((str.data[index + 2] & 0xC0) == 0x80))
        {
            result.codepoint = ((uint32_t) (c & 0x0F) << 12) |
                               ((uint32_t) (str.data[index + 1] & 0x3F) << 6) |
                                (uint32_t) (str.data[index + 2] & 0x3F);
            result.byte_count = 3;
        }
        else if (((c & 0xF8) == 0xF0) && ((index + 3) < str.count) &&
                 ((str.data[index + 1] & 0xC0) == 0x80) &&
                 ((str.data[index + 2] & 0xC0) == 0x80) &&
                 ((str.data[index + 3] & 0xC0) == 0x80))
        {
            result.codepoint = ((uint32_t) (c & 0x0F) << 18) |
                               ((uint32_t) (str.data[index + 1] & 0x3F) << 12) |
                               ((uint32_t) (str.data[index + 2] & 0x3F) << 6) |
                                (uint32_t) (str.data[index + 3] & 0x3F);
            result.byte_count = 4;
        }
    }

    return result;
}

int64_t
cui_utf8_encode(CuiString str, int64_t index, uint32_t codepoint)
{
    if (codepoint < 0x80)
    {
        if (index < str.count)
        {
            str.data[index] = (uint8_t) codepoint;

            return 1;
        }
    }
    else if (codepoint < 0x800)
    {
        if ((index + 1) < str.count)
        {
            str.data[index + 0] = 0xC0 | (uint8_t) ((codepoint >>  6) & 0x1F);
            str.data[index + 1] = 0x80 | (uint8_t) ( codepoint        & 0x3F);

            return 2;
        }
    }
    else if (codepoint < 0x10000)
    {
        if ((index + 2) < str.count)
        {
            str.data[index + 0] = 0xE0 | (uint8_t) ((codepoint >> 12) & 0x0F);
            str.data[index + 1] = 0x80 | (uint8_t) ((codepoint >>  6) & 0x3F);
            str.data[index + 2] = 0x80 | (uint8_t) ( codepoint        & 0x3F);

            return 3;
        }
    }
    else if (codepoint < 0x110000)
    {
        if ((index + 3) < str.count)
        {
            str.data[index + 0] = 0xF0 | (uint8_t) ((codepoint >> 18) & 0x07);
            str.data[index + 1] = 0x80 | (uint8_t) ((codepoint >> 12) & 0x3F);
            str.data[index + 2] = 0x80 | (uint8_t) ((codepoint >>  6) & 0x3F);
            str.data[index + 3] = 0x80 | (uint8_t) ( codepoint        & 0x3F);

            return 4;
        }
    }

    return 0;
}

CuiUnicodeResult
cui_utf16le_decode(CuiString str, int64_t index)
{
    CuiUnicodeResult result = { '?', 2 };

    if ((index + 1) < str.count)
    {
        uint16_t leading = (str.data[index + 1] << 8) | str.data[index + 0];

        if (((leading & 0xFC00) == 0xD800) && ((index + 3) < str.count))
        {
            uint16_t trailing = (str.data[index + 3] << 8) | str.data[index + 2];

            result.codepoint = (((uint32_t) (leading & 0x3FF) << 10) |
                                 (uint32_t) (trailing & 0x3FF)) + 0x10000;
            result.byte_count = 4;
        }
        else
        {
            result.codepoint = leading;
            result.byte_count = 2;
        }
    }

    return result;
}

int64_t
cui_utf16le_encode(CuiString str, int64_t index, uint32_t codepoint)
{
    if ((codepoint < 0xD800) || ((codepoint >= 0xE000) && (codepoint < 0x10000)))
    {
        if ((index + 1) < str.count)
        {
            str.data[index + 0] = (uint8_t) ( codepoint       & 0xFF);
            str.data[index + 1] = (uint8_t) ((codepoint >> 8) & 0xFF);
        }

        return 2;
    }
    else if ((codepoint >= 0x10000) && (codepoint < 0x110000))
    {
        if ((index + 3) < str.count)
        {
            codepoint -= 0x10000;
            uint32_t leading  = 0xD800 | ((codepoint >> 10) & 0x3FF);
            uint32_t trailing = 0xDC00 | ( codepoint        & 0x3FF);

            str.data[index + 0] = (uint8_t) ( leading       & 0xFF);
            str.data[index + 1] = (uint8_t) ((leading >> 8) & 0xFF);
            str.data[index + 2] = (uint8_t) ( trailing       & 0xFF);
            str.data[index + 3] = (uint8_t) ((trailing >> 8) & 0xFF);
        }

        return 4;
    }

    return 0;
}

CuiString
cui_utf8_to_utf16le(CuiArena *arena, CuiString utf8_str)
{
    CuiString result = { 0 };

    if (utf8_str.count)
    {
        result.count = 2 * utf8_str.count;
        result.data  = cui_alloc_array(arena, uint8_t, result.count, CuiDefaultAllocationParams());

        int64_t input_index = 0;
        int64_t output_index = 0;

        while (input_index < utf8_str.count)
        {
            CuiUnicodeResult utf8 = cui_utf8_decode(utf8_str, input_index);
            output_index += cui_utf16le_encode(result, output_index, utf8.codepoint);
            input_index += utf8.byte_count;
        }

        CuiAssert(output_index <= result.count);

        result.count = output_index;
    }

    return result;
}

CuiString
cui_utf16le_to_utf8(CuiArena *arena, CuiString utf16_str)
{
    CuiString result = { 0 };

    if (utf16_str.count)
    {
        result.count = 2 * utf16_str.count;
        result.data  = cui_alloc_array(arena, uint8_t, result.count, CuiDefaultAllocationParams());

        int64_t input_index = 0;
        int64_t output_index = 0;

        while (input_index < utf16_str.count)
        {
            CuiUnicodeResult utf16 = cui_utf16le_decode(utf16_str, input_index);
            output_index += cui_utf8_encode(result, output_index, utf16.codepoint);
            input_index += utf16.byte_count;
        }

        CuiAssert(output_index <= result.count);

        result.count = output_index;
    }

    return result;
}

int64_t
cui_utf8_get_character_count(CuiString str)
{
    int64_t index = 0;
    int64_t count = 0;

    while (index < str.count)
    {
        index += cui_utf8_decode(str, index).byte_count;
        count += 1;
    }

    return count;
}

int64_t
cui_utf8_get_character_byte_offset(CuiString str, int64_t character_index)
{
    int64_t index = 0;
    int64_t count = 0;

    while (index < str.count)
    {
        if (count == character_index)
        {
            break;
        }

        index += cui_utf8_decode(str, index).byte_count;
        count += 1;
    }

    return index;
}
