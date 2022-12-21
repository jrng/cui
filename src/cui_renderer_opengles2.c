#define GL_MAJOR_VERSION                  0x821B
#define GL_UNPACK_ROW_LENGTH_EXT          0x0CF2

static void
_cui_opengles2_renderer_check_errors(GLuint id)
{
    GLint length = 0;

    if (glIsShader(id))
    {
        glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length);
    }
    else if (glIsProgram(id))
    {
        glGetProgramiv(id, GL_INFO_LOG_LENGTH, &length);
    }
    else
    {
        printf("[OpenGL ES] warning: no program or shader\n");
        return;
    }

    if (length < 2)
        return;

    char *log = (char *) cui_platform_allocate(length);

    if (glIsShader(id))
    {
        glGetShaderInfoLog(id, length, NULL, log);
        printf("[OpenGL ES] shader error: %s\n", log);
    }
    else if (glIsProgram(id))
    {
        glGetProgramInfoLog(id, length, NULL, log);
        printf("[OpenGL ES] program error: %s\n", log);
    }

    cui_platform_deallocate(log, length);
}

static GLuint
_cui_opengles2_renderer_create_program(char *header_source, char *vertex_source, char *fragment_source)
{
    GLchar const * const vertex_shader_code[] = {
        header_source,
        vertex_source,
    };

    GLuint vertex_shader_id = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader_id, CuiArrayCount(vertex_shader_code),
                   vertex_shader_code, 0);
    glCompileShader(vertex_shader_id);
    _cui_opengles2_renderer_check_errors(vertex_shader_id);

    GLchar const * const fragment_shader_code[] = {
        header_source,
        fragment_source,
    };

    GLuint fragment_shader_id = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader_id, CuiArrayCount(fragment_shader_code),
                   fragment_shader_code, 0);
    glCompileShader(fragment_shader_id);
    _cui_opengles2_renderer_check_errors(fragment_shader_id);

    GLuint program_id = glCreateProgram();
    glAttachShader(program_id, vertex_shader_id);
    glAttachShader(program_id, fragment_shader_id);

    glLinkProgram(program_id);
    _cui_opengles2_renderer_check_errors(program_id);

    glDeleteShader(vertex_shader_id);
    glDeleteShader(fragment_shader_id);

    return program_id;
}

static CuiRendererOpengles2 *
_cui_create_opengles2_renderer()
{
    GLint major_version = 2;
    glGetIntegerv(GL_MAJOR_VERSION, &major_version);

    if (major_version != 3)
    {
        bool has_EXT_unpack_subimage = false;

        char *exts = (char *) glGetString(GL_EXTENSIONS);
        char *start = exts;

        while (*start)
        {
            while (*start == ' ') start += 1;
            char *end = start;
            while (*end && (*end != ' ')) end += 1;

            CuiString ext_name = cui_make_string(start, end - start);

            if (cui_string_equals(ext_name, CuiStringLiteral("GL_EXT_unpack_subimage")))
            {
                has_EXT_unpack_subimage = true;
            }

            start = end;
        }

        if (!has_EXT_unpack_subimage)
        {
            return 0;
        }
    }

    uint64_t renderer_size          = CuiAlign(sizeof(CuiRendererOpengles2), 16);
    uint64_t texture_operation_size = CuiAlign(_CUI_MAX_TEXTURE_OPERATION_COUNT * sizeof(CuiTextureOperation), 16);
    uint64_t index_buffer_size      = CuiAlign(_CUI_MAX_INDEX_BUFFER_COUNT * sizeof(uint32_t), 16);
    uint64_t push_buffer_size       = CuiAlign(_CUI_MAX_PUSH_BUFFER_SIZE, 16);
    uint64_t vertex_buffer_size     = CuiAlign(_CUI_MAX_INDEX_BUFFER_COUNT * 6 * sizeof(CuiOpengles2Vertex), 16);
    uint64_t draw_list_size         = CuiAlign(64 * sizeof(CuiOpengles2DrawCommand), 16);

    uint64_t allocation_size = renderer_size + texture_operation_size + index_buffer_size +
                               push_buffer_size + vertex_buffer_size + draw_list_size;

    uint8_t *allocation = (uint8_t *) cui_platform_allocate(allocation_size);

    CuiRendererOpengles2 *renderer = (CuiRendererOpengles2 *) allocation;
    allocation += renderer_size;

    CuiClearStruct(*renderer);

    renderer->allocation_size = allocation_size;

#if 0
    renderer->platform_performance_frequency = cui_platform_get_performance_frequency();
    renderer->min_render_time = 1000.0f;
    renderer->max_render_time = 0.0f;
#endif

    CuiCommandBuffer *command_buffer = &renderer->command_buffer;

    GLint max_texture_size;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);

    command_buffer->max_texture_width  = max_texture_size;
    command_buffer->max_texture_height = max_texture_size;

    command_buffer->max_texture_operation_count = _CUI_MAX_TEXTURE_OPERATION_COUNT;
    command_buffer->texture_operations = (CuiTextureOperation *) allocation;
    allocation += texture_operation_size;

    command_buffer->max_index_buffer_count = _CUI_MAX_INDEX_BUFFER_COUNT;
    command_buffer->index_buffer = (uint32_t *) allocation;
    allocation += index_buffer_size;

    command_buffer->max_push_buffer_size = _CUI_MAX_PUSH_BUFFER_SIZE;
    command_buffer->push_buffer = (uint8_t *) allocation;
    allocation += push_buffer_size;

    renderer->vertices = (CuiOpengles2Vertex *) allocation;
    allocation += vertex_buffer_size;

    renderer->draw_list = (CuiOpengles2DrawCommand *) allocation;
    allocation += draw_list_size;

    glGenBuffers(1, &renderer->vertex_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, vertex_buffer_size, 0, GL_STREAM_DRAW);

    char *header = (char *) "#version 100\n";

    char *vertex_source = (char *)
    "uniform vec2 u_vertex_scale;\n"
    "uniform vec2 u_texture_scale;\n"
    "\n"
    "attribute vec2 a_uv;\n"
    "attribute vec4 a_color;\n"
    "attribute vec2 a_position;\n"
    "\n"
    "varying vec4 v_color;\n"
    "varying vec2 v_uv;\n"
    "\n"
    "void main()\n"
    "{\n"
    "    v_color = a_color;\n"
    "    v_uv = u_texture_scale * a_uv;\n"
    "    gl_Position = vec4(u_vertex_scale * (a_position - vec2(1.0 / 256.0)) + vec2(-1.0, 1.0), 0.0, 1.0);\n"
    "}\n";

    char *fragment_source = (char *)
    "precision mediump float;\n"
    "\n"
    "uniform sampler2D u_texture;\n"
    "\n"
    "varying vec4 v_color;\n"
    "varying vec2 v_uv;\n"
    "\n"
    "void main()\n"
    "{\n"
    "    gl_FragColor = v_color * texture2D(u_texture, v_uv).bgra;\n"
    "}\n";

    renderer->program = _cui_opengles2_renderer_create_program(header, vertex_source, fragment_source);
    renderer->texture_scale_location = glGetUniformLocation(renderer->program, "u_texture_scale");
    renderer->vertex_scale_location = glGetUniformLocation(renderer->program, "u_vertex_scale");
    renderer->texture_location = glGetUniformLocation(renderer->program, "u_texture");
    renderer->position_location = glGetAttribLocation(renderer->program, "a_position");
    renderer->color_location = glGetAttribLocation(renderer->program, "a_color");
    renderer->uv_location = glGetAttribLocation(renderer->program, "a_uv");

    glReleaseShaderCompiler();

    return renderer;
}

static void
_cui_destroy_opengles2_renderer(CuiRendererOpengles2 *renderer)
{
    glDeleteTextures(CuiArrayCount(renderer->textures), renderer->textures);
    glDeleteProgram(renderer->program);
    glDeleteBuffers(1, &renderer->vertex_buffer);

    cui_platform_deallocate(renderer, renderer->allocation_size);
}

#if CUI_PLATFORM_ANDROID

static uint32_t
_cui_opengles2_renderer_store_textures(CuiRendererOpengles2 *renderer, uint32_t max_texture_state_count, CuiTextureState *texture_states)
{
    uint32_t count = 0;

    for (uint32_t texture_id = 0; texture_id < CuiArrayCount(renderer->textures); texture_id += 1)
    {
        if (renderer->textures[texture_id])
        {
            if (count < max_texture_state_count)
            {
                CuiTextureState *state = texture_states + count;
                count += 1;

                state->texture_id = texture_id;
                state->bitmap = renderer->bitmaps[texture_id];
            }
        }
    }

    return count;
}

static void
_cui_opengles2_renderer_restore_textures(CuiRendererOpengles2 *renderer, uint32_t texture_state_count, CuiTextureState *texture_states)
{
    for (uint32_t i = 0; i < texture_state_count; i += 1)
    {
        CuiTextureState *state = texture_states + i;

        int32_t texture_id = state->texture_id;
        CuiBitmap bitmap = state->bitmap;

        glGenTextures(1, renderer->textures + texture_id);
        glBindTexture(GL_TEXTURE_2D, renderer->textures[texture_id]);

        renderer->bitmaps[texture_id] = bitmap;

        if (bitmap.stride == (bitmap.width * 4))
        {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bitmap.width, bitmap.height,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, bitmap.pixels);
        }
        else
        {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bitmap.width, bitmap.height,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

            int32_t row_length = bitmap.stride >> 2;

            glPixelStorei(GL_UNPACK_ROW_LENGTH_EXT, row_length);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, bitmap.width, bitmap.height,
                            GL_RGBA, GL_UNSIGNED_BYTE, bitmap.pixels);
            glPixelStorei(GL_UNPACK_ROW_LENGTH_EXT, 0);
        }

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
}

#endif

static CuiCommandBuffer *
_cui_opengles2_renderer_begin_command_buffer(CuiRendererOpengles2 *renderer)
{
    CuiCommandBuffer *command_buffer = &renderer->command_buffer;

    command_buffer->push_buffer_size = 0;
    command_buffer->index_buffer_count = 0;
    command_buffer->texture_operation_count = 0;

    return command_buffer;
}

static void
_cui_opengles2_renderer_render(CuiRendererOpengles2 *renderer, CuiCommandBuffer *command_buffer,
                               int32_t window_width, int32_t window_height, CuiColor clear_color)
{
    CuiAssert(&renderer->command_buffer == command_buffer);

#if 0
    uint64_t render_start = cui_platform_get_performance_counter();
#endif

    for (uint32_t index = 0; index < command_buffer->texture_operation_count; index += 1)
    {
        CuiTextureOperation *texture_op = command_buffer->texture_operations + index;

        int32_t texture_id = texture_op->texture_id;
        CuiAssert(texture_id >= 0);
        CuiAssert(texture_id < CUI_MAX_TEXTURE_COUNT);

        switch (texture_op->type)
        {
            case CUI_TEXTURE_OPERATION_ALLOCATE:
            {
                glGenTextures(1, renderer->textures + texture_id);
                glBindTexture(GL_TEXTURE_2D, renderer->textures[texture_id]);

                CuiBitmap bitmap = texture_op->payload.bitmap;
                renderer->bitmaps[texture_id] = bitmap;

                CuiAssert(!(bitmap.stride & 3));

                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bitmap.width, bitmap.height,
                             0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            } break;

            case CUI_TEXTURE_OPERATION_DEALLOCATE:
            {
                CuiClearStruct(renderer->bitmaps[texture_id]);
                glDeleteTextures(1, renderer->textures + texture_id);
                renderer->textures[texture_id] = 0;
            } break;

            case CUI_TEXTURE_OPERATION_UPDATE:
            {
                CuiRect rect = texture_op->payload.rect;
                CuiBitmap bitmap = renderer->bitmaps[texture_id];

                glBindTexture(GL_TEXTURE_2D, renderer->textures[texture_id]);

                int32_t row_length = bitmap.stride >> 2;

                glPixelStorei(GL_UNPACK_ROW_LENGTH_EXT, row_length);
                glTexSubImage2D(GL_TEXTURE_2D, 0, rect.min.x, rect.min.y, cui_rect_get_width(rect),
                                cui_rect_get_height(rect), GL_RGBA, GL_UNSIGNED_BYTE,
                                (uint8_t *) bitmap.pixels + (rect.min.y * bitmap.stride) + (rect.min.x * 4));
                glPixelStorei(GL_UNPACK_ROW_LENGTH_EXT, 0);
            } break;
        }
    }

    uint32_t draw_list_count = 0;
    CuiOpengles2DrawCommand *draw_command = renderer->draw_list;

    int64_t vertex_buffer_count = 0;

    CuiOpengles2Vertex *vertex_start = renderer->vertices;
    CuiOpengles2Vertex *vertices = vertex_start;

    if (command_buffer->index_buffer_count)
    {
        uint32_t current_clip_rect = 0;
        uint32_t current_texture_id = (uint32_t) -1;

        draw_command->vertex_offset = 0;
        draw_command->vertex_count = 0;
        draw_command->texture_id = -1;
        draw_command->texture_scale = cui_make_float_point(1.0f, 1.0f);
        draw_command->clip_rect_x = 0;
        draw_command->clip_rect_y = 0;
        draw_command->clip_rect_width = window_width;
        draw_command->clip_rect_height = window_height;

        for (uint32_t i = 0; i < command_buffer->index_buffer_count; i += 1)
        {
            uint32_t rect_offset = command_buffer->index_buffer[i];

            CuiTexturedRect *textured_rect = (CuiTexturedRect *) (command_buffer->push_buffer + rect_offset);

            if ((textured_rect->texture_id != current_texture_id) || (textured_rect->clip_rect != current_clip_rect))
            {
                CuiOpengles2DrawCommand *last_draw_command = draw_command;

                draw_command = renderer->draw_list + draw_list_count;
                draw_list_count += 1;

                // TODO: only do the needed fields
                *draw_command = *last_draw_command;

                int64_t vertex_offset = vertices - vertex_start;

                draw_command->vertex_offset = vertex_offset;
                last_draw_command->vertex_count = vertex_offset - last_draw_command->vertex_offset;


                if (textured_rect->texture_id != current_texture_id)
                {
                    CuiBitmap bitmap = renderer->bitmaps[textured_rect->texture_id];

                    float a = 1.0f / bitmap.width;
                    float b = 1.0f / bitmap.height;

                    draw_command->texture_id = renderer->textures[textured_rect->texture_id];
                    draw_command->texture_scale = cui_make_float_point(a, b);

                    current_texture_id = textured_rect->texture_id;
                }

                if (textured_rect->clip_rect != current_clip_rect)
                {
                    if (textured_rect->clip_rect)
                    {
                        CuiClipRect *rect = (CuiClipRect *) (command_buffer->push_buffer + textured_rect->clip_rect - 1);

                        draw_command->clip_rect_x = rect->x_min;
                        draw_command->clip_rect_y = window_height - rect->y_max;
                        draw_command->clip_rect_width = rect->x_max - rect->x_min;
                        draw_command->clip_rect_height = rect->y_max - rect->y_min;
                    }
                    else
                    {
                        draw_command->clip_rect_x = 0;
                        draw_command->clip_rect_y = 0;
                        draw_command->clip_rect_width = window_width;
                        draw_command->clip_rect_height = window_height;
                    }

                    current_clip_rect = textured_rect->clip_rect;
                }
            }

            vertices[0].color = textured_rect->color;
            vertices[0].uv = cui_make_float_point(textured_rect->u0, textured_rect->v1);
            vertices[0].position = cui_make_float_point(textured_rect->x0, textured_rect->y1);

            vertices[1].color = textured_rect->color;
            vertices[1].uv = cui_make_float_point(textured_rect->u1, textured_rect->v0);
            vertices[1].position = cui_make_float_point(textured_rect->x1, textured_rect->y0);

            vertices[2].color = textured_rect->color;
            vertices[2].uv = cui_make_float_point(textured_rect->u0, textured_rect->v0);
            vertices[2].position = cui_make_float_point(textured_rect->x0, textured_rect->y0);

            vertices[3].color = textured_rect->color;
            vertices[3].uv = cui_make_float_point(textured_rect->u0, textured_rect->v1);
            vertices[3].position = cui_make_float_point(textured_rect->x0, textured_rect->y1);

            vertices[4].color = textured_rect->color;
            vertices[4].uv = cui_make_float_point(textured_rect->u1, textured_rect->v1);
            vertices[4].position = cui_make_float_point(textured_rect->x1, textured_rect->y1);

            vertices[5].color = textured_rect->color;
            vertices[5].uv = cui_make_float_point(textured_rect->u1, textured_rect->v0);
            vertices[5].position = cui_make_float_point(textured_rect->x1, textured_rect->y0);

            vertices += 6;
        }

        vertex_buffer_count = vertices - vertex_start;

        CuiAssert(vertex_buffer_count);
        draw_command->vertex_count = vertex_buffer_count - draw_command->vertex_offset;
    }

    glViewport(0, 0, window_width, window_height);
    glScissor(0, 0, window_width, window_height);

    glClearColor(clear_color.r, clear_color.g, clear_color.b, clear_color.a);
    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_SCISSOR_TEST);
    glEnable(GL_BLEND);

    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(renderer->program);

    float a = 2.0f / window_width;
    float b = 2.0f / window_height;

    glUniform1i(renderer->texture_location, 0);
    glUniform2f(renderer->vertex_scale_location, a, -b);

    glBindBuffer(GL_ARRAY_BUFFER, renderer->vertex_buffer);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertex_buffer_count * sizeof(CuiOpengles2Vertex), vertex_start);

    glEnableVertexAttribArray(renderer->position_location);
    glEnableVertexAttribArray(renderer->color_location);
    glEnableVertexAttribArray(renderer->uv_location);

    glVertexAttribPointer(renderer->position_location, 2, GL_FLOAT, GL_FALSE, sizeof(CuiOpengles2Vertex), &((CuiOpengles2Vertex *) 0)->position);
    glVertexAttribPointer(renderer->color_location, 4, GL_FLOAT, GL_FALSE, sizeof(CuiOpengles2Vertex), &((CuiOpengles2Vertex *) 0)->color);
    glVertexAttribPointer(renderer->uv_location, 2, GL_FLOAT, GL_FALSE, sizeof(CuiOpengles2Vertex), &((CuiOpengles2Vertex *) 0)->uv);

    glActiveTexture(GL_TEXTURE0);

    for (uint32_t i = 0; i < draw_list_count; i += 1)
    {
        CuiOpengles2DrawCommand *draw_command = renderer->draw_list + i;

        glBindTexture(GL_TEXTURE_2D, draw_command->texture_id);
        glUniform2f(renderer->texture_scale_location, draw_command->texture_scale.x, draw_command->texture_scale.y);
        glScissor(draw_command->clip_rect_x, draw_command->clip_rect_y, draw_command->clip_rect_width, draw_command->clip_rect_height);

        glDrawArrays(GL_TRIANGLES, draw_command->vertex_offset, draw_command->vertex_count);
    }

    glDisableVertexAttribArray(renderer->position_location);
    glDisableVertexAttribArray(renderer->color_location);
    glDisableVertexAttribArray(renderer->uv_location);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);

#if 0
    uint64_t render_end = cui_platform_get_performance_counter();
    double render_time = (1000.0 * (double) (render_end - render_start)) / (double) renderer->platform_performance_frequency;
    float render_time_ms = render_time;

    if (render_time_ms < renderer->min_render_time)
    {
        renderer->min_render_time = render_time_ms;
    }

    if (render_time_ms > renderer->max_render_time)
    {
        renderer->max_render_time = render_time_ms;
    }

    renderer->sum_render_time += render_time;
    renderer->frame_count += 1;

    if (renderer->frame_count == 300)
    {
        printf("render time:  min=%f  max=%f  avg=%f\n", renderer->min_render_time, renderer->max_render_time,
               renderer->sum_render_time * (1.0 / 300.0));

        renderer->min_render_time = 1000.0f;
        renderer->max_render_time = 0.0f;
        renderer->sum_render_time = 0.0;
        renderer->frame_count = 0;
    }
#endif
}
