uint32_t
cui_color_pack_rgba(CuiColor color)
{
    uint8_t r = (uint8_t) ((color.r * 255.0f) + 0.5f);
    uint8_t g = (uint8_t) ((color.g * 255.0f) + 0.5f);
    uint8_t b = (uint8_t) ((color.b * 255.0f) + 0.5f);
    uint8_t a = (uint8_t) ((color.a * 255.0f) + 0.5f);

    return (a << 24) | (b << 16) | (g << 8) | r;
}

CuiColor
cui_color_unpack_rgba(uint32_t color)
{
    CuiColor result;
    result.r = (float) ((color >>  0) & 0xFF) / 255.0f;
    result.g = (float) ((color >>  8) & 0xFF) / 255.0f;
    result.b = (float) ((color >> 16) & 0xFF) / 255.0f;
    result.a = (float) ((color >> 24) & 0xFF) / 255.0f;
    return result;
}

uint32_t
cui_color_pack_bgra(CuiColor color)
{
    uint8_t r = (uint8_t) ((color.r * 255.0f) + 0.5f);
    uint8_t g = (uint8_t) ((color.g * 255.0f) + 0.5f);
    uint8_t b = (uint8_t) ((color.b * 255.0f) + 0.5f);
    uint8_t a = (uint8_t) ((color.a * 255.0f) + 0.5f);

    return (a << 24) | (r << 16) | (g << 8) | b;
}

CuiColor
cui_color_unpack_bgra(uint32_t color)
{
    CuiColor result;
    result.r = (float) ((color >> 16) & 0xFF) / 255.0f;
    result.g = (float) ((color >>  8) & 0xFF) / 255.0f;
    result.b = (float) ((color >>  0) & 0xFF) / 255.0f;
    result.a = (float) ((color >> 24) & 0xFF) / 255.0f;
    return result;
}

CuiColor
cui_color_rgb_to_hsv(CuiColor rgb_color)
{
    CuiColor hsv_color;

    hsv_color.a = rgb_color.a;

    float min_value = cui_min_float(rgb_color.r, cui_min_float(rgb_color.g, rgb_color.b));
    float max_value = cui_max_float(rgb_color.r, cui_max_float(rgb_color.g, rgb_color.b));

    hsv_color.b = max_value; // v

    float delta = max_value - min_value;

    if ((max_value < (1.0f / 4096.0f)) || (delta < (1.0f / 4096.0f)))
    {
        hsv_color.r = 0; // h
        hsv_color.g = 0; // s
    }
    else
    {
        hsv_color.g = delta / max_value; // s

        if (rgb_color.r == max_value)
        {
            hsv_color.r = (rgb_color.g - rgb_color.b) / delta; // h
        }
        else if (rgb_color.g == max_value)
        {
            hsv_color.r = 2.0f + (rgb_color.b - rgb_color.r) / delta; // h
        }
        else
        {
            hsv_color.r = 4.0f + (rgb_color.r - rgb_color.g) / delta; // h
        }

        hsv_color.r *= 60.0f;

        if (hsv_color.r < 0.0f) hsv_color.r += 360.0f;
    }

    return hsv_color;
}

CuiColor
cui_color_hsv_to_rgb(CuiColor hsv_color)
{
    CuiColor rgb_color;

    rgb_color.a = hsv_color.a;

    float h = hsv_color.r;
    float s = hsv_color.g;
    float v = hsv_color.b;

    CuiAssert(h <= 360.0f);

    h /= 60.0f;
    int32_t index = (int32_t) h;
    float frac = h - (float) index;

    float a = v * (1.0f - s);
    float b = v * (1.0f - (s * frac));
    float c = v * (1.0f - (s * (1.0f - frac)));

    switch (index)
    {
        case 0:
        {
            rgb_color.r = v;
            rgb_color.g = c;
            rgb_color.b = a;
        } break;

        case 1:
        {
            rgb_color.r = b;
            rgb_color.g = v;
            rgb_color.b = a;
        } break;

        case 2:
        {
            rgb_color.r = a;
            rgb_color.g = v;
            rgb_color.b = c;
        } break;

        case 3:
        {
            rgb_color.r = a;
            rgb_color.g = b;
            rgb_color.b = v;
        } break;

        case 4:
        {
            rgb_color.r = c;
            rgb_color.g = a;
            rgb_color.b = v;
        } break;

        case 5:
        {
            rgb_color.r = v;
            rgb_color.g = a;
            rgb_color.b = b;
        } break;

        case 6:
        {
            rgb_color.r = v;
            rgb_color.g = c;
            rgb_color.b = a;
        } break;


        default:
        {
            CuiAssert(!"invalid default");
        } break;
    }

    return rgb_color;
}

CuiColor
cui_color_rgb_to_hsl(CuiColor rgb_color)
{
    CuiColor hsl_color;

    hsl_color.a = rgb_color.a;

    float min_value = cui_min_float(rgb_color.r, cui_min_float(rgb_color.g, rgb_color.b));
    float max_value = cui_max_float(rgb_color.r, cui_max_float(rgb_color.g, rgb_color.b));

    hsl_color.b = 0.5f * (max_value + min_value); // l

    float delta = max_value - min_value;

    if (delta < (1.0f / 4096.0f))
    {
        hsl_color.r = 0; // h
        hsl_color.g = 0; // s
    }
    else
    {
        hsl_color.g = delta / (1.0f - fabsf(max_value + min_value - 1.0f)); // s

        if (rgb_color.r == max_value)
        {
            hsl_color.r = (rgb_color.g - rgb_color.b) / delta; // h
        }
        else if (rgb_color.g == max_value)
        {
            hsl_color.r = 2.0f + (rgb_color.b - rgb_color.r) / delta; // h
        }
        else
        {
            hsl_color.r = 4.0f + (rgb_color.r - rgb_color.g) / delta; // h
        }

        hsl_color.r *= 60.0f;

        if (hsl_color.r < 0.0f) hsl_color.r += 360.0f;
    }

    return hsl_color;
}

CuiColor
cui_color_hsl_to_rgb(CuiColor hsl_color)
{
    CuiColor rgb_color;

    // TODO: implement conversion
    rgb_color = hsl_color;

    return rgb_color;
}
