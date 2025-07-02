static CuiRenderer *
_cui_renderer_metal_create(CuiArena *temporary_memory, id<MTLDevice> metal_device)
{
    CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(temporary_memory);

    uint64_t renderer_size          = CuiAlign(sizeof(CuiRendererMetal), 16);
    uint64_t texture_operation_size = CuiAlign(_CUI_MAX_TEXTURE_OPERATION_COUNT * sizeof(CuiTextureOperation), 16);

    uint64_t allocation_size = renderer_size + texture_operation_size;

    uint8_t *allocation = (uint8_t *) cui_platform_allocate(allocation_size);

    CuiRendererMetal *renderer = (CuiRendererMetal *) allocation;
    allocation += renderer_size;

    CuiClearStruct(*renderer);

    renderer->base.type = CUI_RENDERER_TYPE_METAL;
    renderer->allocation_size = allocation_size;
    renderer->device = metal_device;

    CuiCommandBuffer *command_buffer = &renderer->command_buffer;

    // These numbers are taken from 'Metal Feature Set Tables' for the Mac1 and
    // Apple7 GPU family. This should be the lowest spec'd mac.
    //
    // https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf
    command_buffer->max_texture_width  = 16384;
    command_buffer->max_texture_height = 16384;

    command_buffer->max_texture_operation_count = _CUI_MAX_TEXTURE_OPERATION_COUNT;
    command_buffer->texture_operations = (CuiTextureOperation *) allocation;
    allocation += texture_operation_size;

    renderer->command_queue = [renderer->device newCommandQueue];

    bool success = false;

    if (renderer->command_queue)
    {
        renderer->render_pass = [MTLRenderPassDescriptor renderPassDescriptor];

        renderer->render_pass.colorAttachments[0].clearColor  = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);
        renderer->render_pass.colorAttachments[0].loadAction  = MTLLoadActionClear;
        renderer->render_pass.colorAttachments[0].storeAction = MTLStoreActionStore;

        CuiString shader_src = CuiStringLiteral(
        "#include <metal_stdlib>\n"
        "\n"
        "using namespace metal;\n"
        "\n"
        "struct ClipRect\n"
        "{\n"
        "    short2 min;\n"
        "    short2 max;\n"
        "    float2 _padding;\n"
        "};\n"
        "\n"
        "struct TexturedRect\n"
        "{\n"
        "    short2 position_min;\n"
        "    short2 position_max;\n"
        "    short2 uv_min;\n"
        "    short2 uv_max;\n"
        "    float4 color;\n"
        "    uint texture_index;\n"
        "    uint clip_rect;\n"
        "};\n"
        "\n"
        "struct VertexOutput\n"
        "{\n"
        "    float4 position [[ position ]];\n"
        "    float4 color;\n"
        "    float2 uv;\n"
        "    uint texture_index [[ flat ]];\n"
        "};\n"
        "\n"
        "vertex VertexOutput vertex_main(constant float4x4 &vertex_transform  [[ buffer(0) ]],\n"
        "                                constant uint8_t  *push_buffer       [[ buffer(1) ]],\n"
        "                                constant uint32_t *index_buffer      [[ buffer(2) ]],\n"
        "                                uint               vertex_index      [[ vertex_id ]])\n"
        "{\n"
        "    uint index = vertex_index / 6;\n"
        "    uint id    = vertex_index %% 6;\n"
        "\n"
        "    uint rect_offset = index_buffer[index];\n"
        "\n"
        "    constant TexturedRect *rect = (constant TexturedRect *) (push_buffer + rect_offset);\n"
        "\n"
        "    short x0 = rect->position_min.x;\n"
        "    short y0 = rect->position_min.y;\n"
        "    short x1 = rect->position_max.x;\n"
        "    short y1 = rect->position_max.y;\n"
        "\n"
        "    float2 uv1 = float2((float) rect->uv_min.x, (float) rect->uv_min.y);\n"
        "    float2 uv2 = float2((float) rect->uv_max.x, (float) rect->uv_min.y);\n"
        "    float2 uv3 = float2((float) rect->uv_max.x, (float) rect->uv_max.y);\n"
        "    float2 uv4 = float2((float) rect->uv_min.x, (float) rect->uv_max.y);\n"
        "\n"
        "    if (rect->clip_rect)\n"
        "    {\n"
        "        constant ClipRect *clip_rect = (constant ClipRect *) (push_buffer + (rect->clip_rect - 1));\n"
        "\n"
        "        if (x0 < clip_rect->min.x)\n"
        "        {\n"
        "            float t = min((float) (clip_rect->min.x - x0) / (float) (x1 - x0), 1.0f);\n"
        "            x0 = min(clip_rect->min.x, x1);\n"
        "            uv1 = mix(uv1, uv2, t);\n"
        "            uv4 = mix(uv4, uv3, t);\n"
        "        }\n"
        "\n"
        "        if (y0 < clip_rect->min.y)\n"
        "        {\n"
        "            float t = min((float) (clip_rect->min.y - y0) / (float) (y1 - y0), 1.0f);\n"
        "            y0 = min(clip_rect->min.y, y1);\n"
        "            uv1 = mix(uv1, uv4, t);\n"
        "            uv2 = mix(uv2, uv3, t);\n"
        "        }\n"
        "\n"
        "        if (x1 > clip_rect->max.x)\n"
        "        {\n"
        "            float t = max((float) (clip_rect->max.x - x0) / (float) (x1 - x0), 0.0f);\n"
        "            x1 = max(clip_rect->max.x, x0);\n"
        "            uv2 = mix(uv1, uv2, t);\n"
        "            uv3 = mix(uv4, uv3, t);\n"
        "        }\n"
        "\n"
        "        if (y1 > clip_rect->max.y)\n"
        "        {\n"
        "            float t = max((float) (clip_rect->max.y - y0) / (float) (y1 - y0), 0.0f);\n"
        "            y1 = max(clip_rect->max.y, y0);\n"
        "            uv4 = mix(uv1, uv4, t);\n"
        "            uv3 = mix(uv2, uv3, t);\n"
        "        }\n"
        "    }\n"
        "\n"
        "    float2 position;\n"
        "    float2 uv;\n"
        "\n"
        "    switch (id)\n"
        "    {\n"
        "        case 0:\n"
        "        {\n"
        "            position = float2((float) x0, (float) y1);\n"
        "            uv = uv4;\n"
        "        } break;\n"
        "\n"
        "        case 1:\n"
        "        {\n"
        "            position = float2((float) x1, (float) y0);\n"
        "            uv = uv2;\n"
        "        } break;\n"
        "\n"
        "        case 2:\n"
        "        {\n"
        "            position = float2((float) x0, (float) y0);\n"
        "            uv = uv1;\n"
        "        } break;\n"
        "\n"
        "        case 3:\n"
        "        {\n"
        "            position = float2((float) x0, (float) y1);\n"
        "            uv = uv4;\n"
        "        } break;\n"
        "\n"
        "        case 4:\n"
        "        {\n"
        "            position = float2((float) x1, (float) y1);\n"
        "            uv = uv3;\n"
        "        } break;\n"
        "\n"
        "        case 5:\n"
        "        {\n"
        "            position = float2((float) x1, (float) y0);\n"
        "            uv = uv2;\n"
        "        } break;\n"
        "    }\n"
        "\n"
        "    VertexOutput result;\n"
        "\n"
        "    result.position      = vertex_transform * float4(position, 0.0f, 1.0f);\n"
        "    result.color         = rect->color;\n"
        "    result.uv            = uv;\n"
        "    result.texture_index = rect->texture_index;\n"
        "\n"
        "    return result;\n"
        "}\n"
        "\n"
        "fragment half4 fragment_main(VertexOutput vertex_input [[ stage_in ]],\n"
        "                             array<texture2d<float>, %u> textures [[ texture(0) ]])\n"
        "{\n"
        "    constexpr sampler tex_sampler(coord::pixel);\n"
        "\n"
        "    float4 tex_sample = textures[vertex_input.texture_index].sample(tex_sampler, vertex_input.uv);\n"
        "    float4 color = vertex_input.color;\n"
        "\n"
        "    return half4(color * tex_sample);\n"
        "}\n");

        shader_src = cui_sprint(temporary_memory, shader_src, CUI_MAX_TEXTURE_COUNT);

        NSString *shader_source = [[NSString alloc] initWithBytes: shader_src.data
                                                           length: shader_src.count
                                                         encoding: NSUTF8StringEncoding];

        NSError *error = nil;

        id<MTLLibrary> shader = [renderer->device newLibraryWithSource: shader_source
                                                               options: nil
                                                                 error: &error];

        if (shader)
        {
            id<MTLFunction> vertex_program = [shader newFunctionWithName: @"vertex_main"];
            id<MTLFunction> fragment_program = [shader newFunctionWithName: @"fragment_main"];

            MTLRenderPipelineDescriptor *pipeline_descriptor = [MTLRenderPipelineDescriptor new];

            pipeline_descriptor.label = @"my_pipeline";
            pipeline_descriptor.vertexFunction = vertex_program;
            pipeline_descriptor.fragmentFunction = fragment_program;
            pipeline_descriptor.colorAttachments[0].blendingEnabled = YES;
            pipeline_descriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorOne;
            pipeline_descriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
            pipeline_descriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
            pipeline_descriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
            pipeline_descriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;

            renderer->pipeline = [renderer->device newRenderPipelineStateWithDescriptor: pipeline_descriptor
                                                                                  error: nil];

            [vertex_program release];
            [fragment_program release];

            renderer->buffer_transform = [renderer->device newBufferWithLength: 16 * sizeof(float)
                                                                       options: MTLResourceStorageModeManaged];
            renderer->push_buffer      = [renderer->device newBufferWithLength: _CUI_MAX_PUSH_BUFFER_SIZE
                                                                       options: MTLResourceStorageModeShared];
            renderer->index_buffer     = [renderer->device newBufferWithLength: _CUI_MAX_INDEX_BUFFER_COUNT * sizeof(uint32_t)
                                                                       options: MTLResourceStorageModeShared];

            CuiAssert(renderer->buffer_transform.contents);
            CuiAssert(renderer->push_buffer.contents);
            CuiAssert(renderer->index_buffer.contents);

            command_buffer->max_push_buffer_size = _CUI_MAX_PUSH_BUFFER_SIZE;
            command_buffer->push_buffer = (uint8_t *) renderer->push_buffer.contents;

            command_buffer->max_index_buffer_count = _CUI_MAX_INDEX_BUFFER_COUNT;
            command_buffer->index_buffer = (uint32_t *) renderer->index_buffer.contents;

            success = true;
        }
        else
        {
            printf("[Metal] shader error: %s\n", [[error localizedDescription] UTF8String]);
        }

        [shader_source release];
    }

    if (!success)
    {
        cui_platform_deallocate(renderer, renderer->allocation_size);
        renderer = 0;
    }

    cui_end_temporary_memory(temp_memory);

    return &renderer->base;
}

static void
_cui_renderer_metal_destroy(CuiRendererMetal *renderer)
{
    for (uint32_t texture_index = 0;
         texture_index < CuiArrayCount(renderer->textures);
         texture_index += 1)
    {
        if (renderer->textures[texture_index])
        {
            [renderer->textures[texture_index] setPurgeableState: MTLPurgeableStateEmpty];
            [renderer->textures[texture_index] release];
            renderer->textures[texture_index] = 0;
        }
    }

    [renderer->index_buffer setPurgeableState: MTLPurgeableStateEmpty];
    [renderer->index_buffer release];

    [renderer->push_buffer setPurgeableState: MTLPurgeableStateEmpty];
    [renderer->push_buffer release];

    [renderer->buffer_transform setPurgeableState: MTLPurgeableStateEmpty];
    [renderer->buffer_transform release];

    [renderer->pipeline release];
    [renderer->render_pass release];
    [renderer->command_queue release];

    cui_platform_deallocate(renderer, renderer->allocation_size);
}

static CuiCommandBuffer *
_cui_renderer_metal_begin_command_buffer(CuiRendererMetal *renderer)
{
    CuiCommandBuffer *command_buffer = &renderer->command_buffer;

    command_buffer->push_buffer_size = 0;
    command_buffer->index_buffer_count = 0;
    command_buffer->texture_operation_count = 0;

    return command_buffer;
}

static void
_cui_renderer_metal_render(CuiRendererMetal *renderer, CuiFramebuffer *framebuffer,
                           CuiCommandBuffer *command_buffer, CuiColor clear_color)
{
    CuiAssert(&renderer->command_buffer == command_buffer);

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
                CuiBitmap bitmap = texture_op->payload.bitmap;
                renderer->bitmaps[texture_id] = bitmap;

                MTLTextureDescriptor *texture_descriptor = [MTLTextureDescriptor new];
                texture_descriptor.pixelFormat = MTLPixelFormatBGRA8Unorm;
                texture_descriptor.width       = bitmap.width;
                texture_descriptor.height      = bitmap.height;
                texture_descriptor.storageMode = MTLStorageModeManaged;

                renderer->textures[texture_id] = [renderer->device newTextureWithDescriptor: texture_descriptor];

                [texture_descriptor release];
            } break;

            case CUI_TEXTURE_OPERATION_DEALLOCATE:
            {
                CuiClearStruct(renderer->bitmaps[texture_id]);
                [renderer->textures[texture_id] setPurgeableState: MTLPurgeableStateEmpty];
                [renderer->textures[texture_id] release];
                renderer->textures[texture_id] = 0;
            } break;

            case CUI_TEXTURE_OPERATION_UPDATE:
            {
                CuiRect rect = texture_op->payload.rect;
                CuiBitmap bitmap = renderer->bitmaps[texture_id];

                MTLRegion region = MTLRegionMake2D(rect.min.x, rect.min.y,
                                                   cui_rect_get_width(rect),
                                                   cui_rect_get_height(rect));

                uint8_t *pixels = (uint8_t *) bitmap.pixels + (rect.min.y * bitmap.stride) + (rect.min.x * 4);

                [renderer->textures[texture_id] replaceRegion: region
                                                  mipmapLevel: 0
                                                    withBytes: pixels
                                                  bytesPerRow: bitmap.stride];
            } break;
        }
    }

    id<MTLCommandBuffer> cmd_buffer = [renderer->command_queue commandBuffer];

    renderer->render_pass.colorAttachments[0].clearColor  = MTLClearColorMake(clear_color.r, clear_color.g, clear_color.b, clear_color.a);
    renderer->render_pass.colorAttachments[0].texture = framebuffer->drawable.texture;

    float a = 2.0f / (float) framebuffer->width;
    float b = 2.0f / (float) framebuffer->height;

    float vertex_transform[] = {
            a, 0.0f, 0.0f, 0.0f,
         0.0f,   -b, 0.0f, 0.0f,
         0.0f, 0.0f, 1.0f, 0.0f,
        -1.0f, 1.0f, 0.0f, 1.0f
    };

    cui_copy_memory(renderer->buffer_transform.contents, vertex_transform, sizeof(vertex_transform));

    [renderer->buffer_transform didModifyRange: NSMakeRange(0, sizeof(vertex_transform))];

    id<MTLRenderCommandEncoder> command_encoder = [cmd_buffer renderCommandEncoderWithDescriptor: renderer->render_pass];

    MTLViewport viewport;
    viewport.originX = 0.0;
    viewport.originY = 0.0;
    viewport.width = (double) framebuffer->width;
    viewport.height = (double) framebuffer->height;
    viewport.znear = 0.0;
    viewport.zfar = 1.0;

    [command_encoder setViewport: viewport];
    [command_encoder setRenderPipelineState: renderer->pipeline];

    [command_encoder setVertexBuffer: renderer->buffer_transform offset: 0 atIndex: 0];
    [command_encoder setVertexBuffer: renderer->push_buffer      offset: 0 atIndex: 1];
    [command_encoder setVertexBuffer: renderer->index_buffer     offset: 0 atIndex: 2];

    [command_encoder setFragmentTextures: renderer->textures
                               withRange: NSMakeRange(0, CUI_MAX_TEXTURE_COUNT)];

    [command_encoder drawPrimitives: MTLPrimitiveTypeTriangle
                        vertexStart: 0
                        vertexCount: command_buffer->index_buffer_count * 6];

    [command_encoder endEncoding];

    [cmd_buffer commit];
    [cmd_buffer waitUntilCompleted];
}
