static const uint8_t _cui_jpeg_zigzag[64] = {
     0,  1,  8, 16,  9,  2,  3, 10,
    17, 24, 32, 25, 18, 11,  4,  5,
    12, 19, 26, 33, 40, 48, 41, 34,
    27, 20, 13,  6,  7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36,
    29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46,
    53, 60, 61, 54, 47, 55, 62, 63
};

static inline CuiJpegBitReader
_cui_jpeg_begin_bit_reader(CuiString *stream)
{
    CuiJpegBitReader result;

    result.stream = stream;
    result.bit_buffer = 0;
    result.bit_count = 0;

    return result;
}

static inline uint32_t
_cui_jpeg_peek_bits(CuiJpegBitReader *reader, uint32_t bit_count)
{
    CuiAssert(bit_count <= 32);

    while (reader->bit_count < bit_count)
    {
        reader->bit_buffer <<= 8;
        reader->bit_count += 8;

        if (reader->stream->count)
        {
            uint8_t byte = reader->stream->data[0];
            cui_string_advance(reader->stream, 1);

            reader->bit_buffer |= byte;

            if (byte == 0xFF)
            {
                cui_string_advance(reader->stream, 1);
            }
        }
    }

    return (reader->bit_buffer >> (reader->bit_count - bit_count)) & ((1 << bit_count) - 1);
}

static inline void
_cui_jpeg_advance_bits(CuiJpegBitReader *reader, uint32_t bit_count)
{
    CuiAssert(bit_count <= reader->bit_count);
    reader->bit_count -= bit_count;
}

static inline uint32_t
_cui_jpeg_consume_bits(CuiJpegBitReader *reader, uint32_t bit_count)
{
    uint32_t result = _cui_jpeg_peek_bits(reader, bit_count);
    _cui_jpeg_advance_bits(reader, bit_count);
    return result;
}

static inline uint8_t
_cui_jpeg_huffman_decode(CuiJpegHuffmanTable *table, CuiJpegBitReader *reader)
{
    uint32_t index = _cui_jpeg_peek_bits(reader, table->max_code_length);
    CuiAssert(index < table->entry_count);

    CuiJpegHuffmanTableEntry entry = table->entries[index];

    CuiAssert(entry.length);

    _cui_jpeg_advance_bits(reader, entry.length);

    return entry.symbol;
}

static inline void
_cui_jpeg_inverse_cosinus_transform(float *result, float *block)
{
    for (uint32_t m = 0; m < 8; m++)
    {
        float *row = block + (8 * m);

        float a1 = (row[0] + row[4]) * 0.707107f;
        float a2 = (row[0] - row[4]) * 0.707107f;

        float b1 = (row[2] * 0.923880f) + (row[6] * 0.382683f);
        float b2 = (row[2] * 0.382683f) - (row[6] * 0.923880f);

        float c1 = (row[1] * 0.980785f) + (row[5] * 0.555570f);
        float c2 = (row[1] * 0.831470f) - (row[5] * 0.980785f);
        float c3 = (row[1] * 0.555570f) + (row[5] * 0.195090f);
        float c4 = (row[1] * 0.195090f) + (row[5] * 0.831470f);

        float d1 = (row[3] * 0.831470f) + (row[7] * 0.195090f);
        float d2 = (row[3] * 0.195090f) + (row[7] * 0.555570f);
        float d3 = (row[3] * 0.980785f) - (row[7] * 0.831470f);
        float d4 = (row[3] * 0.555570f) + (row[7] * 0.980785f);

        float e1 = a1 + b1;
        float e2 = a1 - b1;
        float e3 = a2 + b2;
        float e4 = a2 - b2;

        float f1 = c1 + d1;
        float f2 = c2 - d2;
        float f3 = c3 - d3;
        float f4 = c4 - d4;

        row[0] = e1 + f1;
        row[1] = e3 + f2;
        row[2] = e4 + f3;
        row[3] = e2 + f4;
        row[4] = e2 - f4;
        row[5] = e4 - f3;
        row[6] = e3 - f2;
        row[7] = e1 - f1;
    }

    for (uint32_t x = 0; x < 8; x++)
    {
        float a1 = (block[0 * 8 + x] + block[4 * 8 + x]) * 0.707107f;
        float a2 = (block[0 * 8 + x] - block[4 * 8 + x]) * 0.707107f;

        float b1 = (block[2 * 8 + x] * 0.923880f) + (block[6 * 8 + x] * 0.382683f);
        float b2 = (block[2 * 8 + x] * 0.382683f) - (block[6 * 8 + x] * 0.923880f);

        float c1 = (block[1 * 8 + x] * 0.980785f) + (block[5 * 8 + x] * 0.555570f);
        float c2 = (block[1 * 8 + x] * 0.831470f) - (block[5 * 8 + x] * 0.980785f);
        float c3 = (block[1 * 8 + x] * 0.555570f) + (block[5 * 8 + x] * 0.195090f);
        float c4 = (block[1 * 8 + x] * 0.195090f) + (block[5 * 8 + x] * 0.831470f);

        float d1 = (block[3 * 8 + x] * 0.831470f) + (block[7 * 8 + x] * 0.195090f);
        float d2 = (block[3 * 8 + x] * 0.195090f) + (block[7 * 8 + x] * 0.555570f);
        float d3 = (block[3 * 8 + x] * 0.980785f) - (block[7 * 8 + x] * 0.831470f);
        float d4 = (block[3 * 8 + x] * 0.555570f) + (block[7 * 8 + x] * 0.980785f);

        float e1 = a1 + b1;
        float e2 = a1 - b1;
        float e3 = a2 + b2;
        float e4 = a2 - b2;

        float f1 = c1 + d1;
        float f2 = c2 - d2;
        float f3 = c3 - d3;
        float f4 = c4 - d4;

        float out0 = ((e1 + f1) * 0.25f) + 128.0f;
        float out1 = ((e3 + f2) * 0.25f) + 128.0f;
        float out2 = ((e4 + f3) * 0.25f) + 128.0f;
        float out3 = ((e2 + f4) * 0.25f) + 128.0f;
        float out4 = ((e2 - f4) * 0.25f) + 128.0f;
        float out5 = ((e4 - f3) * 0.25f) + 128.0f;
        float out6 = ((e3 - f2) * 0.25f) + 128.0f;
        float out7 = ((e1 - f1) * 0.25f) + 128.0f;

        result[0 * 8 + x] = (out0 < 0.0f) ? 0.0f : ((out0 > 255.0f) ? 255.0f : out0);
        result[1 * 8 + x] = (out1 < 0.0f) ? 0.0f : ((out1 > 255.0f) ? 255.0f : out1);
        result[2 * 8 + x] = (out2 < 0.0f) ? 0.0f : ((out2 > 255.0f) ? 255.0f : out2);
        result[3 * 8 + x] = (out3 < 0.0f) ? 0.0f : ((out3 > 255.0f) ? 255.0f : out3);
        result[4 * 8 + x] = (out4 < 0.0f) ? 0.0f : ((out4 > 255.0f) ? 255.0f : out4);
        result[5 * 8 + x] = (out5 < 0.0f) ? 0.0f : ((out5 > 255.0f) ? 255.0f : out5);
        result[6 * 8 + x] = (out6 < 0.0f) ? 0.0f : ((out6 > 255.0f) ? 255.0f : out6);
        result[7 * 8 + x] = (out7 < 0.0f) ? 0.0f : ((out7 > 255.0f) ? 255.0f : out7);
    }
}

static inline void
_cui_jpeg_convert_color(float *mcu, uint8_t *pixels, uint32_t block_size_x, uint32_t block_size_y, uint32_t stride)
{
    uint32_t block_size = block_size_x * block_size_y;

    for (uint32_t y = 0; y < block_size_y; y++)
    {
        uint32_t *pixel = (uint32_t *) pixels;
        for (uint32_t x = 0; x < block_size_x; x++)
        {
            float Y  = mcu[(0 * block_size) + (y * block_size_x) + x];
            float Cb = mcu[(1 * block_size) + (y * block_size_x) + x] - 128.0f;
            float Cr = mcu[(2 * block_size) + (y * block_size_x) + x] - 128.0f;

            int32_t final_r = (int32_t) (Y                 + 1.402f   * Cr);
            int32_t final_g = (int32_t) (Y - 0.34414f * Cb - 0.71414f * Cr);
            int32_t final_b = (int32_t) (Y + 1.772f   * Cb                );

            uint8_t r = (final_r < 0) ? 0 : ((final_r > 255) ? 255 : (uint8_t) final_r);
            uint8_t g = (final_g < 0) ? 0 : ((final_g > 255) ? 255 : (uint8_t) final_g);
            uint8_t b = (final_b < 0) ? 0 : ((final_b > 255) ? 255 : (uint8_t) final_b);

            *pixel++ = 0xFF000000 | (r << 16) | (g << 8) | b;
        }
        pixels += stride;
    }
}

CuiString
cui_image_encode_bmp(CuiBitmap bitmap, bool top_to_bottom, bool bgra, CuiArena *arena)
{
    CuiString result;

    uint32_t file_header_size = 14;
    uint32_t info_header_size = 108;
    uint32_t pixel_data_offset = file_header_size + info_header_size;

    result.count = file_header_size + info_header_size + (bitmap.width * bitmap.height * 4);
    result.data = cui_alloc(arena, result.count, CuiDefaultAllocationParams());

    result.data[0] = 'B';
    result.data[1] = 'M';
    // file size
    cui_write_u32_le(result.data, 2, (uint32_t) result.count);
    // reserved
    cui_write_u32_le(result.data, 6, 0);
    // pixel data offset
    cui_write_u32_le(result.data, 10, pixel_data_offset);

    // BITMAPV4HEADER
    // header size
    cui_write_u32_le(result.data, 14, info_header_size);
    // bitmap width
    cui_write_u32_le(result.data, 18, (uint32_t) bitmap.width);

    if (top_to_bottom)
    {
        // bitmap height
        cui_write_u32_le(result.data, 22, (uint32_t) -bitmap.height);
    }
    else
    {
        // bitmap height
        cui_write_u32_le(result.data, 22, (uint32_t) bitmap.height);
    }

    // number of color planes
    cui_write_u16_le(result.data, 26, 1);
    // bits per pixel
    cui_write_u16_le(result.data, 28, 32);
    // compression
    cui_write_u32_le(result.data, 30, 3);
    // pixel data size
    cui_write_u32_le(result.data, 34, 0);
    // horizontal resolution
    cui_write_u32_le(result.data, 38, 0);
    // vertical resolution
    cui_write_u32_le(result.data, 42, 0);
    // number of colors
    cui_write_u32_le(result.data, 46, 0);
    // number of important colors
    cui_write_u32_le(result.data, 50, 0);

    if (bgra)
    {
        // red mask
        cui_write_u32_le(result.data, 54, 0x00FF0000);
        // green mask
        cui_write_u32_le(result.data, 58, 0x0000FF00);
        // blue mask
        cui_write_u32_le(result.data, 62, 0x000000FF);
    }
    else
    {
        // red mask
        cui_write_u32_le(result.data, 54, 0x000000FF);
        // green mask
        cui_write_u32_le(result.data, 58, 0x0000FF00);
        // blue mask
        cui_write_u32_le(result.data, 62, 0x00FF0000);
    }

    // alpha mask
    cui_write_u32_le(result.data, 66, 0xFF000000);
    // cs type
    cui_write_u32_le(result.data, 70, 0);

    // red gamma
    cui_write_u32_le(result.data, 96, 0);
    // green gamma
    cui_write_u32_le(result.data, 100, 0);
    // blue gamma
    cui_write_u32_le(result.data, 104, 0);

    if (bitmap.stride == (bitmap.width * 4))
    {
        uint64_t pixel_size = bitmap.width * bitmap.height * 4;
        cui_copy_memory(result.data + pixel_data_offset, bitmap.pixels, pixel_size);
    }
    else
    {
        uint8_t *dst_row = result.data + pixel_data_offset;
        uint8_t *src_row = bitmap.pixels;

        int32_t dst_stride = bitmap.width * 4;
        int32_t src_stride = bitmap.stride;

        for (int32_t y = 0; y < bitmap.height; y += 1)
        {
            cui_copy_memory(dst_row, src_row, dst_stride);

            dst_row += dst_stride;
            src_row += src_stride;
        }
    }

    return result;
}

bool
cui_image_decode_jpeg(CuiArena *temporary_memory, CuiBitmap *bitmap, CuiString data, CuiArena *arena,
                      CuiImageMetaData **meta_data, CuiArena *meta_data_arena)
{
    if (!cui_string_starts_with(data, CuiStringLiteral("\xFF\xD8\xFF")))
    {
        return false;
    }

#if 0
    uint64_t perf_freq = cui_platform_get_performance_frequency();
    uint64_t decode_start = cui_platform_get_performance_counter();
    uint64_t transform_interval = 0;
    uint64_t color_transform_interval = 0;
#endif

    CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(temporary_memory);

    bool result = true;

    CuiString at = data;
    cui_string_advance(&at, 2);

    uint16_t orientation = 1;
    uint8_t component_count = 0;
    uint32_t width = 0, height = 0;

    CuiJpegMode mode = CUI_JPEG_MODE_UNKNOWN;
    CuiJpegComponent components[3];
    CuiJpegHuffmanTable huffman_tables[8] = { { 0 } };
    CuiJpegQuantizationTable *quantization_tables = cui_alloc_array(temporary_memory, CuiJpegQuantizationTable, 4, CuiDefaultAllocationParams());

    while ((at.count >= 2) && (at.data[0] == 0xFF))
    {
        uint8_t type = at.data[1];
        cui_string_advance(&at, 2);

#if 0
        switch (type)
        {
            case 0xC0: printf("SOF0 - start of frame\n");         break;
            case 0xC2: printf("SOF2 - start of frame\n");         break;
            case 0xC4: printf("DHT - huffman table(s)\n");        break;
            case 0xD9: printf("EOI - end of image\n");            break;
            case 0xDA: printf("SOS - start of scan\n");           break;
            case 0xDB: printf("DQT - quantization table(s)\n");   break;
            case 0xDD: printf("DRI - define restart interval\n"); break;
            case 0xFE: printf("COM - comment\n");                 break;

            default:
            {
                if ((type & 0xF0) == 0xE0) // APPn - application data
                {
                    printf("APP%u - application data\n", type & 0xF);
                }
                else
                {
                    printf("0x%02X marker\n", type);
                }
            } break;
        }
#endif

        if (type == 0xD9) // EOI - end of image
        {
            break;
        }

        uint16_t length = cui_read_u16_be(at.data, 0);

        CuiString cursor = at;

        if (length > cursor.count)
        {
            result = false;
            break;
        }

        cursor.count = length;
        cui_string_advance(&cursor, 2);

        switch (type)
        {
            case 0xC0: // SOF0 - start of frame
            case 0xC2: // SOF2 - start of frame
            {
                if (mode != CUI_JPEG_MODE_UNKNOWN)
                {
                    result = false;
                    break;
                }

                if (type == 0xC0)
                {
                    mode = CUI_JPEG_MODE_BASELINE;
                }
                else
                {
                    CuiAssert(type == 0xC2);
                    mode = CUI_JPEG_MODE_PROGRESSIVE;

                    CuiAssert(!"not supported");
                    result = false;
                    break;
                }

                if (length != 17)
                {
                    result = false;
                    break;
                }

                uint8_t precision;

                precision       = cursor.data[0];
                height          = cui_read_u16_be(cursor.data, 1);
                width           = cui_read_u16_be(cursor.data, 3);
                component_count = cursor.data[5];

                cui_string_advance(&cursor, 6);

                if (precision != 8)
                {
                    result = false;
                    break;
                }

                if ((width == 0) || (height == 0))
                {
                    result = false;
                    break;
                }

                if (component_count != 3)
                {
                    result = false;
                    break;
                }

                for (int32_t component_index = 0; component_index < component_count; component_index += 1)
                {
                    uint8_t component_id   = cursor.data[0];
                    uint8_t sample_factors = cursor.data[1];
                    uint8_t quant_table_id = cursor.data[2];

                    cui_string_advance(&cursor, 3);

                    // This is not required, but most file have the components sorted.
                    // This makes decoding easier.
                    if (component_id != (component_index + 1))
                    {
                        result = false;
                        break;
                    }

                    CuiJpegComponent *comp = components + component_index;

                    comp->factor_x = sample_factors >> 4;
                    comp->factor_y = sample_factors & 0xF;
                    comp->quant_table_id = quant_table_id;
                    comp->dc_value = 0;

                    if ((comp->factor_x == 0) || (comp->factor_y == 0))
                    {
                        result = false;
                        break;
                    }

                    if ((comp->factor_x & (comp->factor_x - 1)) ||
                        (comp->factor_y & (comp->factor_y - 1)))
                    {
                        result = false;
                        break;
                    }
                }
            } break;

            case 0xC4: // DHT - huffman table(s)
            {
                while (cursor.count >= 17)
                {
                    uint8_t huffman_info = cursor.data[0];

                    uint8_t huffman_id = huffman_info & 0xF;
                    uint8_t table_type = huffman_info >> 4;

                    if (huffman_id >= 4) // most files don't go beyond 4 tables
                    {
                        result = false;
                        break;
                    }

                    if (table_type >= 2)
                    {
                        result = false;
                        break;
                    }

                    uint8_t *code_counts = cursor.data;
                    cui_string_advance(&cursor, 17);

                    uint32_t max_code_length = 0;

                    for (int32_t code_length = 1; code_length <= 16; code_length += 1)
                    {
                        if (code_counts[code_length])
                        {
                            max_code_length = code_length;
                        }
                    }

                    uint32_t table_index = (huffman_id << 1) | table_type;

                    CuiJpegHuffmanTable *table = huffman_tables + table_index;

                    table->entry_count = (1 << max_code_length);
                    table->max_code_length = max_code_length;
                    table->entries = cui_alloc_array(temporary_memory, CuiJpegHuffmanTableEntry, table->entry_count, CuiDefaultAllocationParams());

                    int32_t remaining_entries = table->entry_count;

                    CuiJpegHuffmanTableEntry *entry = table->entries;

                    for (int32_t code_length = 1; code_length <= 16; code_length += 1)
                    {
                        uint8_t count = code_counts[code_length];
                        int32_t fill_length = (1 << (table->max_code_length - code_length));

                        remaining_entries -= count * fill_length;

                        if (remaining_entries < 0)
                        {
                            result = false;
                            break;
                        }

                        for (int32_t i = 0; i < count; i += 1)
                        {
                            uint8_t symbol = cursor.data[i];

                            for (int32_t j = 0; j < fill_length; j += 1)
                            {
                                entry->symbol = symbol;
                                entry->length = (uint8_t) code_length;
                                entry += 1;
                            }
                        }

                        cui_string_advance(&cursor, count);
                    }

                    if (!result) break;

                    while (remaining_entries)
                    {
                        CuiClearStruct(*entry);
                        entry += 1;
                        remaining_entries -= 1;
                    }
                }

                if (cursor.count > 0)
                {
                    result = false;
                }
            } break;

            case 0xDA: // SOS - start of scan
            {
                if (mode == CUI_JPEG_MODE_UNKNOWN)
                {
                    result = false;
                    break;
                }

                if (cursor.data[0] != component_count)
                {
                    result = false;
                    break;
                }

                cui_string_advance(&cursor, 1);

                if (mode == CUI_JPEG_MODE_BASELINE)
                {
                    int32_t max_factor_x = 1;
                    int32_t max_factor_y = 1;

                    CuiJpegComponent *comp = components;

                    for (int32_t component_index = 0; component_index < component_count; component_index += 1)
                    {
                        if (cursor.data[0] != (component_index + 1))
                        {
                            result = false;
                            break;
                        }

                        uint8_t component_info = cursor.data[1];
                        comp->ac_table_id = component_info & 0xF;
                        comp->dc_table_id = component_info >> 4;

                        if ((comp->ac_table_id >= 4) || (comp->dc_table_id >= 4))
                        {
                            result = false;
                            break;
                        }

                        if (comp->factor_x > max_factor_x) max_factor_x = comp->factor_x;
                        if (comp->factor_y > max_factor_y) max_factor_y = comp->factor_y;

                        comp->ac_table_id <<= 1;
                        comp->dc_table_id <<= 1;
                        comp->ac_table_id |= 1;

                        comp += 1;

                        cui_string_advance(&cursor, 2);
                    }

                    if (!result) break;

                    if (cursor.count != 3)
                    {
                        result = false;
                        break;
                    }

                    cui_string_advance(&cursor, 3);

                    cursor.count = at.count - (cursor.data - at.data);

                    int32_t max_block_size_x = 8 * max_factor_x;
                    int32_t max_block_size_y = 8 * max_factor_y;

                    int32_t block_count_x = (width + max_block_size_x - 1) / max_block_size_x;
                    int32_t block_count_y = (height + max_block_size_y - 1) / max_block_size_y;

                    CuiArena *bitmap_arena = arena;

                    if ((orientation == 6) || (orientation == 8))
                    {
                        bitmap_arena = temporary_memory;
                    }

                    bitmap->width  = width;
                    bitmap->height = height;
                    bitmap->stride = block_count_x * max_block_size_x * 4;
                    bitmap->pixels = cui_alloc(bitmap_arena, bitmap->stride * block_count_y * max_block_size_y,
                                               cui_make_allocation_params(false, 16));

                    int32_t block_size = max_block_size_x * max_block_size_y;

                    // TODO: align to 16 bytes
                    float *mcu = cui_alloc_array(temporary_memory, float, 3 * block_size, CuiDefaultAllocationParams());
                    // TODO: align to 16 bytes
                    float *input_block = cui_alloc_array(temporary_memory, float, 2 * 64, CuiDefaultAllocationParams());
                    float *output_block = input_block + 64;

                    CuiJpegBitReader reader = _cui_jpeg_begin_bit_reader(&cursor);

                    for (int32_t block_y = 0; block_y < block_count_y; block_y += 1)
                    {
                        for (int32_t block_x = 0; block_x < block_count_x; block_x += 1)
                        {
                            CuiJpegComponent *comp = components;

                            for (int32_t component_index = 0; component_index < component_count; component_index += 1)
                            {
                                CuiJpegQuantizationTable *quant_table = quantization_tables + comp->quant_table_id;
                                CuiJpegHuffmanTable *dc_table = huffman_tables + comp->dc_table_id;
                                CuiJpegHuffmanTable *ac_table = huffman_tables + comp->ac_table_id;

                                for (int32_t sub_block_y = 0; sub_block_y < comp->factor_y; sub_block_y += 1)
                                {
                                    for (int32_t sub_block_x = 0; sub_block_x < comp->factor_x; sub_block_x += 1)
                                    {
                                        // TODO: clear memory fast
                                        cui_clear_memory(input_block, sizeof(float) * 64);

                                        int32_t value = 0;
                                        uint8_t bits_to_read = _cui_jpeg_huffman_decode(dc_table, &reader) & 0xF;

                                        if (bits_to_read)
                                        {
                                            value = _cui_jpeg_consume_bits(&reader, bits_to_read);

                                            if (value < (1 << (bits_to_read - 1)))
                                            {
                                                value += (int32_t) ((uint32_t) (-1) << bits_to_read) + 1;
                                            }

                                            comp->dc_value += value;
                                        }

                                        input_block[0] = (float) (comp->dc_value * quant_table->values[0]);

                                        uint32_t index = 0;

                                        do {
                                            value = 0;
                                            uint8_t symbol = _cui_jpeg_huffman_decode(ac_table, &reader);

                                            if (!symbol) break;

                                            uint8_t bits_to_read = symbol & 0xF;
                                            index += (symbol >> 4) + 1;

                                            if (index > 63)
                                            {
                                                result = false;
                                                break;
                                            }

                                            if (bits_to_read)
                                            {
                                                value = _cui_jpeg_consume_bits(&reader, bits_to_read);

                                                if (value < (1 << (bits_to_read - 1)))
                                                {
                                                    value += (int32_t) ((uint32_t) (-1) << bits_to_read) + 1;
                                                }
                                            }

                                            input_block[_cui_jpeg_zigzag[index]] = (float) (value * quant_table->values[index]);
                                        }
                                        while (index < 63);

                                        if (!result) break;

#if 0
                                        transform_interval -= cui_platform_get_performance_counter();
#endif

                                        _cui_jpeg_inverse_cosinus_transform(output_block, input_block);

#if 0
                                        transform_interval += cui_platform_get_performance_counter();
#endif

                                        uint32_t repeat_x = max_factor_x / comp->factor_x;
                                        uint32_t repeat_y = max_factor_y / comp->factor_y;
                                        float *dst = mcu + (component_index * block_size) +
                                                           (sub_block_y * repeat_y * 8 * max_block_size_x) +
                                                           (sub_block_x * repeat_x * 8);

                                        for (uint32_t y = 0; y < 8; y++)
                                        {
                                            for (uint32_t ry = 0; ry < repeat_y; ry++)
                                            {
                                                float *row = dst;
                                                for (uint32_t x = 0; x < 8; x++)
                                                {
                                                    float value = output_block[y * 8 + x];
                                                    value = (value < 0) ? 0 : ((value > 255) ? 255 : value);
                                                    for (uint32_t rx = 0; rx < repeat_x; rx++)
                                                    {
                                                        *row++ = value;
                                                    }
                                                }
                                                dst += max_block_size_x;
                                            }
                                        }
                                    }

                                    if (!result) break;
                                }

                                if (!result) break;

                                comp += 1;
                            }

                            if (!result) break;

                            uint8_t *row = (uint8_t *) bitmap->pixels +
                                                       (block_y * max_block_size_y * bitmap->stride) +
                                                       (4 * block_x * max_block_size_x);

#if 0
                            color_transform_interval -= cui_platform_get_performance_counter();
#endif

                            _cui_jpeg_convert_color(mcu, row, max_block_size_x, max_block_size_y, bitmap->stride);

#if 0
                            color_transform_interval += cui_platform_get_performance_counter();
#endif

                            // reset marker
                        }

                        if (!result) break;
                    }

                    switch (orientation)
                    {
                        case 1: // normal
                        {
                        } break;

                        case 2: // mirrored in x
                        {
                        } break;

                        case 3: // rotated by 180°
                        {
                        } break;

                        case 4: // mirrored in y
                        {
                        } break;

                        case 5:
                        {
                        } break;

                        case 6: // rotated by 90° counterclockwise
                        {
                            CuiBitmap source_bitmap = *bitmap;

                            bitmap->width  = source_bitmap.height;
                            bitmap->height = source_bitmap.width;
                            bitmap->stride = source_bitmap.width * 4;
                            bitmap->pixels = cui_alloc(arena, bitmap->stride * bitmap->height, CuiDefaultAllocationParams());

                            uint8_t *row = (uint8_t *) bitmap->pixels;

                            for (int32_t y = 0; y < bitmap->height; y += 1)
                            {
                                uint32_t *pixel = (uint32_t *) row;

                                for (int32_t x = 0; x < bitmap->width; x += 1)
                                {
                                    *pixel = *(uint32_t *) ((uint8_t *) source_bitmap.pixels + ((bitmap->width - x) * bitmap->stride) + (y * 4));
                                    pixel += 1;
                                }

                                row += bitmap->stride;
                            }
                        } break;

                        case 7:
                        {
                        } break;

                        case 8: // rotated by 90° clockwise
                        {
                            CuiBitmap source_bitmap = *bitmap;

                            bitmap->width  = source_bitmap.height;
                            bitmap->height = source_bitmap.width;
                            bitmap->stride = source_bitmap.width * 4;
                            bitmap->pixels = cui_alloc(arena, bitmap->stride * bitmap->height, CuiDefaultAllocationParams());

                            uint8_t *row = (uint8_t *) bitmap->pixels;

                            for (int32_t y = 0; y < bitmap->height; y += 1)
                            {
                                uint32_t *pixel = (uint32_t *) row;

                                for (int32_t x = 0; x < bitmap->width; x += 1)
                                {
                                    *pixel = *(uint32_t *) ((uint8_t *) source_bitmap.pixels + (x * bitmap->stride) + ((bitmap->height - y) * 4));
                                    pixel += 1;
                                }

                                row += bitmap->stride;
                            }
                        } break;
                    }
                }
                else
                {
                    result = false;
                    break;
                }

                at.count -= (cursor.data - at.data);
                at.data = cursor.data;
            } break;

            case 0xDB: // DQT - quantization table(s)
            {
                while (cursor.count >= 65)
                {
                    uint8_t quant_info = cursor.data[0];

                    uint8_t quantization_id = quant_info & 0xF;
                    uint8_t element_size    = quant_info >> 4;

                    if (element_size != 0) // only byte sizes are supported
                    {
                        result = false;
                        break;
                    }

                    if (quantization_id >= 4) // most files don't go beyond 4 tables
                    {
                        result = false;
                        break;
                    }

                    CuiJpegQuantizationTable *table = quantization_tables + quantization_id;

                    for (int i = 0; i < 64; i += 1)
                    {
                        table->values[i] = cursor.data[i + 1];
                    }

                    cui_string_advance(&cursor, 65);
                }

                if (cursor.count > 0)
                {
                    result = false;
                }
            } break;

            case 0xDD: // DRI - define restart interval
            {
            } break;

            case 0xFE: // COM - comment
            {
            } break;

            default:
            {
                if ((type & 0xF0) == 0xE0) // APPn - application data
                {
                    if (type == 0xE1) // APP1 - Exif
                    {
                        if (cui_string_starts_with(cursor, CuiStringLiteral("Exif\x00\x00")))
                        {
                            cui_string_advance(&cursor, 6);

                            CuiString tiff = cursor;

                            uint16_t u16_le_mask = 0;
                            uint32_t u32_le_mask = 0;

                            if (cui_string_starts_with(cursor, CuiStringLiteral("II")))
                            {
                                u16_le_mask = 0xFFFF;
                                u32_le_mask = 0xFFFFFFFF;
                            }
                            else if (!cui_string_starts_with(cursor, CuiStringLiteral("MM")))
                            {
                                result = false;
                                break;
                            }

                            uint16_t magic_number = cui_read_u16(cursor.data, 2, u16_le_mask);

                            if (magic_number != 42)
                            {
                                result = false;
                                break;
                            }

                            uint32_t ifd_offset = cui_read_u32(cursor.data, 4, u32_le_mask);

                            while (ifd_offset)
                            {
                                cursor = tiff;
                                cui_string_advance(&cursor, ifd_offset);

                                uint16_t ifd_count = cui_read_u16(cursor.data, 0, u16_le_mask);

                                cui_string_advance(&cursor, 2);

                                for (uint16_t ifd_index = 0; ifd_index < ifd_count; ifd_index += 1)
                                {
                                    uint16_t tag    = cui_read_u16(cursor.data, 0, u16_le_mask);
                                    uint16_t type   = cui_read_u16(cursor.data, 2, u16_le_mask);
                                    uint32_t count  = cui_read_u32(cursor.data, 4, u32_le_mask);
                                    uint32_t offset = cui_read_u32(cursor.data, 8, u32_le_mask);
                                    uint16_t short_value = cui_read_u16(cursor.data, 8, u16_le_mask);

                                    switch (tag)
                                    {
                                        case 271: // Make - Image input equipment manufacturer
                                        {
                                            // ASCII
                                            if (meta_data && (type == 2))
                                            {
                                                CuiAssert(*meta_data);
                                                CuiAssert(meta_data_arena);

                                                char *maker_str;

                                                if (count <= 4)
                                                {
                                                    maker_str = (char *) cursor.data + 8;
                                                }
                                                else
                                                {
                                                    maker_str = (char *) tiff.data + offset;
                                                }

                                                CuiString maker = CuiCString(maker_str);

                                                CuiImageMetaData *meta = cui_array_append(*meta_data);
                                                meta->type = CUI_IMAGE_META_DATA_CAMERA_MAKER;
                                                meta->value = cui_copy_string(meta_data_arena, maker);
                                            }
                                        } break;

                                        case 272: // Model - Image input equipment model
                                        {
                                            // ASCII
                                            if (meta_data && (type == 2))
                                            {
                                                CuiAssert(*meta_data);
                                                CuiAssert(meta_data_arena);

                                                char *model_str;

                                                if (count <= 4)
                                                {
                                                    model_str = (char *) cursor.data + 8;
                                                }
                                                else
                                                {
                                                    model_str = (char *) tiff.data + offset;
                                                }

                                                CuiString model = CuiCString(model_str);

                                                CuiImageMetaData *meta = cui_array_append(*meta_data);
                                                meta->type = CUI_IMAGE_META_DATA_CAMERA_MODEL;
                                                meta->value = cui_copy_string(meta_data_arena, model);
                                            }
                                        } break;

                                        case 274: // Orientation - Orientation of Image
                                        {
                                            // SHORT
                                            orientation = short_value;
                                        } break;

                                        case 282: // XResolution - Image resolution in width direction
                                        {
                                            // RATIONAL
                                        } break;

                                        case 283: // YResolution - Image resolution in height direction
                                        {
                                            // RATIONAL
                                        } break;

                                        case 296: // ResolutionUnit - Unit of X and Y resolution
                                        {
                                            // RATIONAL
                                        } break;

                                        case 305: // Software - Software used
                                        {
                                            // ASCII
#if 0
                                            char *software_str = (char *) tiff.data + offset;
                                            CuiString software = CuiCString(software_str);

                                            printf("software = '%.*s'\n", (int) software.count, software.data);
#endif
                                        } break;

#if 0
                                        default:
                                        {
                                            printf("unknown tag %d\n", tag);
                                        } break;
#endif
                                    }

                                    cui_string_advance(&cursor, 12);
                                }

                                ifd_offset = cui_read_u32(cursor.data, 0, u32_le_mask);
                            }
                        }
                    }
                }
                else
                {
                    result = false;
                    break;
                }
            } break;
        }

        if (!result) break;

        cui_string_advance(&at, length);
    }

    cui_end_temporary_memory(temp_memory);

#if 0
    uint64_t decode_end = cui_platform_get_performance_counter();
    double decode_time = (double) (decode_end - decode_start) / (double) perf_freq;
    double transform_time = (double) transform_interval / (double) perf_freq;
    double color_transform_time = (double) color_transform_interval / (double) perf_freq;

    printf("decode = %f s\n", decode_time);
    printf("transform = %f s\n", transform_time);
    printf("color transform = %f s\n", color_transform_time);
#endif

    return result;
}

bool
cui_image_decode_bmp(CuiBitmap *bitmap, CuiString data, CuiArena *arena)
{
    // The minimum file size is 26 bytes. That's 14 bytes for the header
    // and 12 bytes for the smallest DIB header.
    if (data.count < 26)
    {
        return false;
    }

    if (!cui_string_starts_with(data, CuiStringLiteral("BM")))
    {
        return false;
    }

    uint32_t pixel_data_offset = cui_read_u32_le(data.data, 10);
    uint32_t header_size = cui_read_u32_le(data.data, 14);

    int32_t width = 0;
    int32_t height = 0;
    uint16_t bits_per_pixel = 0;
    uint32_t compression = 0;

    uint32_t red_mask   = 0x00FF0000;
    uint32_t green_mask = 0x0000FF00;
    uint32_t blue_mask  = 0x000000FF;
    uint32_t alpha_mask = 0xFF000000;

    uint32_t red_shift   = 16;
    uint32_t green_shift = 8;
    uint32_t blue_shift  = 0;
    uint32_t alpha_shift = 24;

    switch (header_size)
    {
        case 124: // BITMAPV5HEADER
        case 108: // BITMAPV4HEADER
        case 56:  // BITMAPV3INFOHEADER
            alpha_mask = cui_read_u32_le(data.data, 66);
            /* FALLTHRU */
        case 52:  // BITMAPV2INFOHEADER
            red_mask   = cui_read_u32_le(data.data, 54);
            green_mask = cui_read_u32_le(data.data, 58);
            blue_mask  = cui_read_u32_le(data.data, 62);

            if (header_size == 52)
            {
                alpha_mask = ~(red_mask | green_mask | blue_mask);
            }
            /* FALLTHRU */
        case 40:  // BITMAPINFOHEADER
            width  = (int32_t) cui_read_u32_le(data.data, 18);
            height = (int32_t) cui_read_u32_le(data.data, 22);
            bits_per_pixel = cui_read_u16_le(data.data, 28);
            compression = cui_read_u32_le(data.data, 30);
            break;
    }

    if ((width == 0) || (height == 0))
    {
        return false;
    }

    if ((bits_per_pixel != 24) && (bits_per_pixel != 32))
    {
        return false;
    }

    if ((bits_per_pixel == 24) && (compression != 0))
    {
        return false;
    }

    if ((bits_per_pixel == 32) && (compression != 3))
    {
        return false;
    }

    if (compression == 3)
    {
        cui_count_trailing_zeros_u32(&red_shift, red_mask);
        cui_count_trailing_zeros_u32(&green_shift, green_mask);
        cui_count_trailing_zeros_u32(&blue_shift, blue_mask);
        cui_count_trailing_zeros_u32(&alpha_shift, alpha_mask);

        // we expect every mask to be 8 bits
        if ((((uint32_t) 0xFF << red_shift) != red_mask) ||
            (((uint32_t) 0xFF << green_shift) != green_mask) ||
            (((uint32_t) 0xFF << blue_shift) != blue_mask) ||
            (((uint32_t) 0xFF << alpha_shift) != alpha_mask))
        {
            return false;
        }
    }

    bool bottom_to_top = true;

    if (height < 0)
    {
        height = -height;
        bottom_to_top = false;
    }

    bitmap->width  = width;
    bitmap->height = height;
    bitmap->stride = bitmap->width * 4;
    bitmap->pixels = cui_alloc(arena, bitmap->stride * bitmap->height, CuiDefaultAllocationParams());

    int32_t dst_stride = bitmap->stride;
    uint8_t *dst_row = (uint8_t *) bitmap->pixels;

    if (bottom_to_top)
    {
        dst_stride = -bitmap->stride;
        dst_row = (uint8_t *) bitmap->pixels + (bitmap->stride * (bitmap->height - 1));
    }

    if (bits_per_pixel == 24)
    {
        int32_t src_stride = CuiAlign(width * 3, 4);
        uint8_t *src_row = (uint8_t *) data.data + pixel_data_offset;

        for (int32_t y = 0; y < bitmap->height; y += 1)
        {
            uint8_t *src = src_row;
            uint32_t *dst_pixel = (uint32_t *) dst_row;

            for (int32_t x = 0; x < bitmap->width; x += 1)
            {
                uint8_t b = src[0];
                uint8_t g = src[1];
                uint8_t r = src[2];

                *dst_pixel = 0xFF000000 | (r << 16) | (g << 8) | b;

                src += 3;
                dst_pixel += 1;
            }

            src_row += src_stride;
            dst_row += dst_stride;
        }
    }
    else
    {
        CuiAssert(bits_per_pixel == 32);

        int32_t src_stride = width * 4;
        uint8_t *src_row = (uint8_t *) data.data + pixel_data_offset;

        for (int32_t y = 0; y < bitmap->height; y += 1)
        {
            uint32_t *src_pixel = (uint32_t *) src_row;
            uint32_t *dst_pixel = (uint32_t *) dst_row;

            for (int32_t x = 0; x < bitmap->width; x += 1)
            {
                uint32_t pixel = *src_pixel;

                CuiColor color;
                color.r = (float) ((pixel >>   red_shift) & 0xFF) / 255.0f;
                color.g = (float) ((pixel >> green_shift) & 0xFF) / 255.0f;
                color.b = (float) ((pixel >>  blue_shift) & 0xFF) / 255.0f;
                color.a = (float) ((pixel >> alpha_shift) & 0xFF) / 255.0f;

                color.r *= color.a;
                color.g *= color.a;
                color.b *= color.a;

                *dst_pixel = cui_color_pack_bgra(color);

                src_pixel += 1;
                dst_pixel += 1;
            }

            src_row += src_stride;
            dst_row += dst_stride;
        }
    }

    return true;
}

bool
cui_image_decode_qoi(CuiBitmap *bitmap, CuiString data, CuiArena *arena)
{
    // The minimum file size is 22 bytes. This is 14 bytes for the header
    // and 8 bytes for the end marker.
    if (data.count < 22)
    {
        return false;
    }

    if (!cui_string_starts_with(data, CuiStringLiteral("qoif")))
    {
        return false;
    }

    uint32_t width   = cui_read_u32_be(data.data, 4);
    uint32_t height  = cui_read_u32_be(data.data, 8);

    uint8_t *at = data.data + 14;
    uint8_t *end = data.data + (data.count - 8);

    bitmap->width  = width;
    bitmap->height = height;
    bitmap->stride = bitmap->width * 4;
    bitmap->pixels = cui_alloc(arena, bitmap->stride * bitmap->height, CuiDefaultAllocationParams());

    uint32_t index[64];

    cui_clear_memory(index, sizeof(index));

    uint32_t current_pixel = 0xFF000000;
    int32_t run = 0;

    uint8_t *row = (uint8_t *) bitmap->pixels;

    for (int32_t y = 0; y < bitmap->height; y += 1)
    {
        uint32_t *pixel = (uint32_t *) row;

        for (int32_t x = 0; x < bitmap->width; x += 1)
        {
            if (run)
            {
                run -= 1;
            }
            else if (at < end)
            {
                uint8_t c = *at++;

                if (c == 0xFE) // rgb
                {
                    uint32_t value = *(uint32_t *) at;
                    current_pixel = (value & 0x00FFFFFF) | (current_pixel & 0xFF000000);
                    at += 3;
                }
                else if (c == 0xFF) // rgba
                {
                    current_pixel = *(uint32_t *) at;
                    at += 4;
                }
                else if ((c & 0xC0) == 0x00) // index
                {
                    current_pixel = index[c];
                }
                else if ((c & 0xC0) == 0x40) // diff
                {
                    ((uint8_t *) &current_pixel)[0] += ((c >> 4) & 0x03) - 2;
                    ((uint8_t *) &current_pixel)[1] += ((c >> 2) & 0x03) - 2;
                    ((uint8_t *) &current_pixel)[2] += ( c       & 0x03) - 2;
                }
                else if ((c & 0xC0) == 0x80) // luma
                {
                    int32_t b = *at++;
                    int32_t vg = (c & 0x3F) - 32;
                    ((uint8_t *) &current_pixel)[0] += vg - 8 + ((b >> 4) & 0x0F);
                    ((uint8_t *) &current_pixel)[1] += vg;
                    ((uint8_t *) &current_pixel)[2] += vg - 8 +  (b       & 0x0F);
                }
                else if ((c & 0xC0) == 0xC0) // run
                {
                    run = c & 0x3F;
                }

                uint32_t hash = ( current_pixel        & 0xFF) * 3 +
                                ((current_pixel >>  8) & 0xFF) * 5 +
                                ((current_pixel >> 16) & 0xFF) * 7 +
                                ((current_pixel >> 24) & 0xFF) * 11;

                index[hash % 64] = current_pixel;
            }

            CuiColor color = cui_color_unpack_rgba(current_pixel);

            color.r *= color.a;
            color.g *= color.a;
            color.b *= color.a;

            *pixel = cui_color_pack_bgra(color);
            pixel += 1;
        }

        row += bitmap->stride;
    }

    return true;
}

void
_cui_pnm_skip_whitespace(CuiString *str)
{
    while (str->count > 0)
    {
        if (str->data[0] == '#')
        {
            while (str->count > 0)
            {
                if (str->data[0] == '\n')
                {
                    cui_string_advance(str, 1);
                    break;
                }

                cui_string_advance(str, 1);
            }
        }
        else if (cui_unicode_is_whitespace(str->data[0]))
        {
            cui_string_advance(str, 1);
        }
        else
        {
            break;
        }
    }
}

int32_t
_cui_pnm_parse_integer(CuiString *str)
{
    int32_t value = 0;
    int64_t index = 0;

    while ((index < str->count) && cui_unicode_is_digit(str->data[index]))
    {
        value = (10 * value) + (str->data[index] - '0');
        index += 1;
    }

    str->data += index;
    str->count -= index;

    return value;
}

bool
cui_image_decode_pbm(CuiBitmap *bitmap, CuiString data, CuiArena *arena)
{
    if (!cui_string_starts_with(data, CuiStringLiteral("P1")) &&
        !cui_string_starts_with(data, CuiStringLiteral("P4")))
    {
        return false;
    }

    bool is_ascii = true;

    if (cui_string_starts_with(data, CuiStringLiteral("P4")))
    {
        is_ascii = false;
    }

    cui_string_advance(&data, 2);

    _cui_pnm_skip_whitespace(&data);

    int32_t width = _cui_pnm_parse_integer(&data);

    _cui_pnm_skip_whitespace(&data);

    int32_t height = _cui_pnm_parse_integer(&data);

    if (is_ascii)
    {
        _cui_pnm_skip_whitespace(&data);
    }
    else
    {
        cui_string_advance(&data, 1);
    }

    bitmap->width  = width;
    bitmap->height = height;
    bitmap->stride = bitmap->width * 4;
    bitmap->pixels = cui_alloc(arena, bitmap->stride * bitmap->height, CuiDefaultAllocationParams());

    if (is_ascii)
    {
        uint8_t *row = (uint8_t *) bitmap->pixels;

        for (int32_t y = 0; y < bitmap->height; y += 1)
        {
            uint32_t *pixel = (uint32_t *) row;

            for (int32_t x = 0; x < bitmap->width; x += 1)
            {
                uint8_t p = data.data[0];
                cui_string_advance(&data, 1);
                _cui_pnm_skip_whitespace(&data);

                if (p == '0')
                {
                    *pixel = 0xFFFFFFFF;
                }
                else
                {
                    *pixel = 0xFF000000;
                }

                pixel += 1;
            }

            row += bitmap->stride;
        }
    }
    else
    {
        int32_t stride = (bitmap->width + 7) / 8;
        int32_t size = stride * bitmap->height;

        if (data.count >= size)
        {
            uint8_t *row = (uint8_t *) bitmap->pixels;

            for (int32_t y = 0; y < bitmap->height; y += 1)
            {
                uint32_t *pixel = (uint32_t *) row;

                for (int32_t x = 0; x < bitmap->width; x += 1)
                {
                    int32_t byte = (stride * y) + (x / 8);
                    int32_t bit = x % 8;

                    int32_t p = data.data[byte] & (0x80 >> bit);

                    if (p)
                    {
                        *pixel = 0xFF000000;
                    }
                    else
                    {
                        *pixel = 0xFFFFFFFF;
                    }

                    pixel += 1;
                }

                row += bitmap->stride;
            }
        }
    }

    return true;
}

bool
cui_image_decode(CuiArena *temporary_memory, CuiBitmap *bitmap, CuiString data, CuiArena *arena,
                 CuiImageMetaData **meta_data, CuiArena *meta_data_arena)
{
    if (cui_string_starts_with(data, CuiStringLiteral("\xFF\xD8\xFF")))
    {
        return cui_image_decode_jpeg(temporary_memory, bitmap, data, arena, meta_data, meta_data_arena);
    }
    else if (cui_string_starts_with(data, CuiStringLiteral("BM")))
    {
        return cui_image_decode_bmp(bitmap, data, arena);
    }
    else if (cui_string_starts_with(data, CuiStringLiteral("qoif")))
    {
        return cui_image_decode_qoi(bitmap, data, arena);
    }
    else if (cui_string_starts_with(data, CuiStringLiteral("P1")) ||
             cui_string_starts_with(data, CuiStringLiteral("P4")))
    {
        return cui_image_decode_pbm(bitmap, data, arena);
    }

    return false;
}
