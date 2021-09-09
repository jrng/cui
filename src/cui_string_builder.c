void
cui_string_builder_init(CuiStringBuilder *builder)
{
    builder->base_buffer.next = 0;
    builder->base_buffer.occupied = 0;
    builder->write_buffer = &builder->base_buffer;
}

static void
_cui_string_builder_expand(CuiStringBuilder *builder)
{
    CuiTextBuffer *new_buffer = cui_alloc_type(builder->arena, CuiTextBuffer, CuiDefaultAllocationParams());
    new_buffer->next = 0;
    new_buffer->occupied = 0;

    builder->write_buffer->next = new_buffer;
    builder->write_buffer = new_buffer;
}

static uint8_t *
_cui_string_builder_reserve_space(CuiStringBuilder *builder, int64_t _size)
{
    CuiAssert(_size <= CuiArrayCount(builder->base_buffer.data));

    int16_t size = (int16_t) _size;

    if ((builder->write_buffer->occupied + size) > CuiArrayCount(builder->base_buffer.data))
    {
        _cui_string_builder_expand(builder);
    }

    uint8_t *result = builder->write_buffer->data + builder->write_buffer->occupied;

    builder->write_buffer->occupied += size;

    return result;
}

void
cui_string_builder_append_character(CuiStringBuilder *builder, uint8_t c)
{
    if (builder->write_buffer->occupied >= CuiArrayCount(builder->base_buffer.data))
    {
        _cui_string_builder_expand(builder);
    }

    CuiTextBuffer *buffer = builder->write_buffer;
    buffer->data[buffer->occupied++] = c;
}

void
cui_string_builder_append_string(CuiStringBuilder *builder, CuiString str)
{
    uint8_t *bytes = str.data;
    int64_t bytes_to_write = str.count;

    int64_t count = CuiArrayCount(builder->base_buffer.data) - builder->write_buffer->occupied;

    if (count > bytes_to_write)
    {
        count = bytes_to_write;
    }

    bytes_to_write -= count;
    uint8_t *dst = builder->write_buffer->data + builder->write_buffer->occupied;
    builder->write_buffer->occupied += count;

    while (count--)
    {
        *dst++ = *bytes++;
    }

    while (bytes_to_write)
    {
        _cui_string_builder_expand(builder);
        count = CuiArrayCount(builder->base_buffer.data);

        if (count > bytes_to_write)
        {
            count = bytes_to_write;
        }

        bytes_to_write -= count;
        dst = builder->write_buffer->data;
        builder->write_buffer->occupied = count;

        while (count--)
        {
            *dst++ = *bytes++;
        }
    }
}

void
cui_string_builder_append_number(CuiStringBuilder *builder, int64_t value, int64_t leading_zeros)
{
    if (value < 0)
    {
        value = -value;
        cui_string_builder_append_character(builder, '-');
    }

    uint8_t buf[20];
    int64_t index = CuiArrayCount(buf) - 1;

    if (!value)
    {
        buf[index--] = '0';
    }
    else
    {
        while (value)
        {
            buf[index--] = '0' + (value % 10);
            value /= 10;
        }
    }

    int64_t count = (CuiArrayCount(buf) - 1) - index;

    for (; count < leading_zeros; count++)
    {
        cui_string_builder_append_character(builder, '0');
    }

    cui_string_builder_append_string(builder, cui_make_string(buf + index + 1, CuiArrayCount(buf) - index - 1));
}

void
cui_string_builder_append_unsigned_number(CuiStringBuilder *builder, uint64_t value, int64_t leading_zeros, int64_t base)
{
    uint8_t buf[64];
    int64_t index = CuiArrayCount(buf) - 1;

    // const char *digits = "0123456789abcdefghijklmnopqrstuvwxyz";
    const char *digits = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    if (!value)
    {
        buf[index--] = '0';
    }
    else
    {
        while (value)
        {
            buf[index--] = digits[value % base];
            value /= base;
        }
    }

    int64_t count = (CuiArrayCount(buf) - 1) - index;

    for (; count < leading_zeros; count++)
    {
        cui_string_builder_append_character(builder, '0');
    }

    cui_string_builder_append_string(builder, cui_make_string(buf + index + 1, CuiArrayCount(buf) - index - 1));
}

void
cui_string_builder_append_float(CuiStringBuilder *builder, double value)
{
    if (value < 0.0)
    {
        cui_string_builder_append_character(builder, '-');
        value = -value;
    }

    int64_t a = (int64_t) value;
    int64_t b = (int64_t) ((value - a) * 10000000.0f);

    cui_string_builder_append_number(builder, a, 0);
    cui_string_builder_append_character(builder, '.');
    cui_string_builder_append_number(builder, b, 7);
}

void
cui_string_builder_append_builder(CuiStringBuilder *builder, CuiStringBuilder *append)
{
    if (append->write_buffer)
    {
        builder->write_buffer->next = &append->base_buffer;
        builder->write_buffer = append->write_buffer;
    }
}

int64_t
cui_string_builder_get_size(CuiStringBuilder *builder)
{
    int64_t result = 0;

    if (builder->write_buffer)
    {
        CuiTextBuffer *buffer = &builder->base_buffer;
        while (buffer)
        {
            result += buffer->occupied;
            buffer = buffer->next;
        }
    }

    return result;
}

CuiString
cui_string_builder_to_string(CuiStringBuilder *builder, CuiArena *arena)
{
    CuiString result;
    result.data = 0;
    result.count = cui_string_builder_get_size(builder);

    if (result.count)
    {
        result.data = cui_alloc_array(arena, uint8_t, result.count, CuiDefaultAllocationParams());

        uint8_t *dst = result.data;

        CuiTextBuffer *buffer = &builder->base_buffer;
        while (buffer)
        {
            uint8_t *src = buffer->data;
            int16_t count = buffer->occupied;

            while (count--)
            {
                *dst++ = *src++;
            }

            buffer = buffer->next;
        }
    }

    return result;
}

void
cui_string_builder_print(CuiStringBuilder *builder, CuiString format, va_list args)
{
    for (int64_t index = 0; index < format.count; index++)
    {
        uint8_t c = format.data[index];

        switch (c)
        {
            case '%':
            {
                index += 1;
                if (index < format.count)
                {
                    c = format.data[index];

                    switch (c)
                    {
                        case 'c':
                        {
                            cui_string_builder_append_character(builder, (uint8_t) va_arg(args, uint32_t));
                        } break;

                        case 'u':
                        {
                            cui_string_builder_append_unsigned_number(builder, va_arg(args, uint32_t), 0, 10);
                        } break;

                        case 'd':
                        {
                            cui_string_builder_append_number(builder, va_arg(args, int32_t), 0);
                        } break;

                        case 'l':
                        {
                            index += 1;
                            if (index < format.count)
                            {
                                c = format.data[index];

                                switch (c)
                                {
                                    case 'u':
                                    {
                                        cui_string_builder_append_unsigned_number(builder, va_arg(args, uint64_t), 0, 10);
                                    } break;

                                    case 'd':
                                    {
                                        cui_string_builder_append_number(builder, va_arg(args, int64_t), 0);
                                    } break;
                                }
                            }
                        } break;

                        case 's':
                        {
                            char *c_str = va_arg(args, char *);
                            cui_string_builder_append_string(builder, CuiCString(c_str));
                        } break;

                        case 'S':
                        {
                            cui_string_builder_append_string(builder, va_arg(args, CuiString));
                        } break;

                        case 'f':
                        {
                            cui_string_builder_append_float(builder, va_arg(args, double));
                        } break;

                        case '0':
                        {
                            index += 1;

                            int32_t width = 0;

                            while ((index < format.count) && (format.data[index] >= '0') && (format.data[index] <= '9'))
                            {
                                width = (10 * width) + (format.data[index] - '0');
                                index += 1;
                            }

                            if (index < format.count)
                            {
                                if (format.data[index] == 'u')
                                {
                                    cui_string_builder_append_unsigned_number(builder, va_arg(args, uint32_t), width, 10);
                                }
                                else if (format.data[index] == 'x')
                                {
                                    cui_string_builder_append_unsigned_number(builder, va_arg(args, uint32_t), width, 16);
                                }
                            }
                        } break;

                        case '%':
                        {
                            cui_string_builder_append_character(builder, '%');
                        } break;
                    }
                }
                else
                {
                    cui_string_builder_append_character(builder, '%');
                }
            } break;

            default:
            {
                cui_string_builder_append_character(builder, c);
            } break;
        }
    }
}

CuiString
cui_sprint(CuiArena *arena, CuiString format, ...)
{
    CuiStringBuilder builder;
    builder.arena = arena;

    cui_string_builder_init(&builder);

    va_list args;
    va_start(args, format);

    cui_string_builder_print(&builder, format, args);

    va_end(args);

    return cui_string_builder_to_string(&builder, arena);
}
