static CuiRenderer *
_cui_renderer_direct3d11_create(ID3D11Device *d3d11_device, ID3D11DeviceContext *d3d11_device_context, IDXGISwapChain1 *dxgi_swapchain)
{
    uint64_t renderer_size          = CuiAlign(sizeof(CuiRendererDirect3D11), 16);
    uint64_t texture_operation_size = CuiAlign(_CUI_MAX_TEXTURE_OPERATION_COUNT * sizeof(CuiTextureOperation), 16);
    uint64_t index_buffer_size      = CuiAlign(_CUI_MAX_INDEX_BUFFER_COUNT * sizeof(uint32_t), 16);
    uint64_t push_buffer_size       = CuiAlign(_CUI_MAX_PUSH_BUFFER_SIZE, 16);

    uint64_t allocation_size = renderer_size + texture_operation_size + index_buffer_size + push_buffer_size;

    uint8_t *allocation = (uint8_t *) cui_platform_allocate(allocation_size);

    CuiRendererDirect3D11 *renderer = (CuiRendererDirect3D11 *) allocation;
    allocation += renderer_size;

    CuiClearStruct(*renderer);

    renderer->base.type = CUI_RENDERER_TYPE_DIRECT3D11;
    renderer->allocation_size = allocation_size;

    CuiCommandBuffer *command_buffer = &renderer->command_buffer;

    command_buffer->max_texture_width  = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
    command_buffer->max_texture_height = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;

    command_buffer->max_texture_operation_count = _CUI_MAX_TEXTURE_OPERATION_COUNT;
    command_buffer->texture_operations = (CuiTextureOperation *) allocation;
    allocation += texture_operation_size;

    command_buffer->max_index_buffer_count = _CUI_MAX_INDEX_BUFFER_COUNT;
    command_buffer->index_buffer = (uint32_t *) allocation;
    allocation += index_buffer_size;

    command_buffer->max_push_buffer_size = _CUI_MAX_PUSH_BUFFER_SIZE;
    command_buffer->push_buffer = (uint8_t *) allocation;

    renderer->device         = d3d11_device;
    renderer->device_context = d3d11_device_context;
    renderer->swapchain      = dxgi_swapchain;

    CuiString shader_source = CuiStringLiteral(
    "cbuffer constants : register(b0)\n"
    "{\n"
    "    row_major float4x4 vertex_transform;\n"
    "    float2 texture_scale;\n"
    "}\n"
    "\n"
    "struct VertexInput\n"
    "{\n"
    "    float2 position : POS;\n"
    "    float4 color    : COL;\n"
    "    float2 uv       : TEX;\n"
    "};\n"
    "\n"
    "struct VertexOutput\n"
    "{\n"
    "    float4 position : SV_POSITION;\n"
    "    float4 color    : COL;\n"
    "    float2 uv       : TEX;\n"
    "};\n"
    "\n"
    "VertexOutput vertex_main(VertexInput input)\n"
    "{\n"
    "    VertexOutput output;\n"
    "\n"
    "    output.position = mul(float4(input.position, 0.0, 1.0f), vertex_transform);\n"
    "    output.color    = input.color;\n"
    "    output.uv       = texture_scale * input.uv;\n"
    "\n"
    "    return output;\n"
    "}\n"
    "\n"
    "Texture2D my_texture : register(t0);\n"
    "SamplerState tex_sampler : register(s0);\n"
    "\n"
    "float4 pixel_main(VertexOutput input) : SV_TARGET\n"
    "{\n"
    "    float4 tex_sample = my_texture.Sample(tex_sampler, input.uv);\n"
    "    float4 color = input.color;\n"
    "\n"
    "    return color * tex_sample;\n"
    "}\n");

    ID3DBlob *vertex_binary;
    ID3DBlob *pixel_binary;

    ID3DBlob *errors;

    if (FAILED(D3DCompile(shader_source.data, shader_source.count, "cui_shader.hlsl", 0, 0, "vertex_main", "vs_5_0", 0, 0, &vertex_binary, &errors)))
    {
        CuiString error_string = cui_make_string(ID3D10Blob_GetBufferPointer(errors), ID3D10Blob_GetBufferSize(errors));
        (void) error_string;
        __debugbreak();
        cui_platform_deallocate(renderer, renderer->allocation_size);
        return 0;
    }

    if (FAILED(D3DCompile(shader_source.data, shader_source.count, "cui_shader.hlsl", 0, 0, "pixel_main", "ps_5_0", 0, 0, &pixel_binary, &errors)))
    {
        CuiString error_string = cui_make_string(ID3D10Blob_GetBufferPointer(errors), ID3D10Blob_GetBufferSize(errors));
        (void) error_string;
        __debugbreak();
        cui_platform_deallocate(renderer, renderer->allocation_size);
        return 0;
    }

    ID3D11Device_CreateVertexShader(renderer->device, ID3D10Blob_GetBufferPointer(vertex_binary), ID3D10Blob_GetBufferSize(vertex_binary),
                                    0, &renderer->vertex_shader);
    ID3D11Device_CreatePixelShader(renderer->device, ID3D10Blob_GetBufferPointer(pixel_binary), ID3D10Blob_GetBufferSize(pixel_binary),
                                   0, &renderer->pixel_shader);

    ID3D11DeviceContext_VSSetShader(renderer->device_context, renderer->vertex_shader, 0, 0);
    ID3D11DeviceContext_PSSetShader(renderer->device_context, renderer->pixel_shader, 0, 0);

    const D3D11_INPUT_ELEMENT_DESC vertex_layout_description[] = {
        { "POS", 0, DXGI_FORMAT_R32G32_FLOAT,       0, CuiOffsetOf(CuiDirect3D11Vertex, position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COL", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, CuiOffsetOf(CuiDirect3D11Vertex, color)   , D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEX", 0, DXGI_FORMAT_R32G32_FLOAT,       0, CuiOffsetOf(CuiDirect3D11Vertex, uv)      , D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    // TODO: check for errors
    ID3D11Device_CreateInputLayout(renderer->device, vertex_layout_description, CuiArrayCount(vertex_layout_description),
                                   ID3D10Blob_GetBufferPointer(vertex_binary), ID3D10Blob_GetBufferSize(vertex_binary), &renderer->vertex_layout);
    ID3D11DeviceContext_IASetInputLayout(renderer->device_context, renderer->vertex_layout);

    ID3D10Blob_Release(vertex_binary);
    ID3D10Blob_Release(pixel_binary);

    D3D11_BUFFER_DESC constant_buffer_description;
    constant_buffer_description.ByteWidth           = CuiAlign(sizeof(CuiDirect3D11Constants), 16);
    constant_buffer_description.Usage               = D3D11_USAGE_DYNAMIC;
    constant_buffer_description.BindFlags           = D3D11_BIND_CONSTANT_BUFFER;
    constant_buffer_description.CPUAccessFlags      = D3D11_CPU_ACCESS_WRITE;
    constant_buffer_description.MiscFlags           = 0;
    constant_buffer_description.StructureByteStride = 0;

    // TODO: check for errors
    ID3D11Device_CreateBuffer(renderer->device, &constant_buffer_description, 0, &renderer->constant_buffer);

    int32_t vertex_buffer_size = _CUI_MAX_INDEX_BUFFER_COUNT * 6 * sizeof(CuiDirect3D11Vertex);

    D3D11_BUFFER_DESC vertex_buffer_description;
    vertex_buffer_description.ByteWidth           = vertex_buffer_size;
    vertex_buffer_description.Usage               = D3D11_USAGE_DYNAMIC;
    vertex_buffer_description.BindFlags           = D3D11_BIND_VERTEX_BUFFER;
    vertex_buffer_description.CPUAccessFlags      = D3D11_CPU_ACCESS_WRITE;
    vertex_buffer_description.MiscFlags           = 0;
    vertex_buffer_description.StructureByteStride = sizeof(CuiDirect3D11Vertex);

    // TODO: check for errors
    ID3D11Device_CreateBuffer(renderer->device, &vertex_buffer_description, 0, &renderer->vertex_buffer);

    D3D11_RASTERIZER_DESC rasterizer_description;
    rasterizer_description.FillMode               = D3D11_FILL_SOLID;
    rasterizer_description.CullMode               = D3D11_CULL_BACK;
    rasterizer_description.FrontCounterClockwise  = TRUE;
    rasterizer_description.DepthBias              = 0;
    rasterizer_description.DepthBiasClamp         = 0.0f;
    rasterizer_description.SlopeScaledDepthBias   = 0.0f;
    rasterizer_description.DepthClipEnable        = TRUE;
    rasterizer_description.ScissorEnable          = TRUE;
    rasterizer_description.MultisampleEnable      = FALSE;
    rasterizer_description.AntialiasedLineEnable  = FALSE;

    // TODO: check for errors
    ID3D11Device_CreateRasterizerState(renderer->device, &rasterizer_description, &renderer->rasterizer_state);
    ID3D11DeviceContext_RSSetState(renderer->device_context, renderer->rasterizer_state);

    D3D11_BLEND_DESC blend_state_description = { 0 };
    blend_state_description.RenderTarget[0].BlendEnable           = TRUE;
    blend_state_description.RenderTarget[0].SrcBlend              = D3D11_BLEND_ONE;
    blend_state_description.RenderTarget[0].DestBlend             = D3D11_BLEND_INV_SRC_ALPHA;
    blend_state_description.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
    blend_state_description.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_ONE;
    blend_state_description.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_INV_SRC_ALPHA;
    blend_state_description.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
    blend_state_description.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    // TODO: check for errors
    ID3D11Device_CreateBlendState(renderer->device, &blend_state_description, &renderer->blend_state);
    ID3D11DeviceContext_OMSetBlendState(renderer->device_context, renderer->blend_state, 0, 0xFFFFFFFF);

    ID3D11Texture2D *framebuffer;

    IDXGISwapChain_GetBuffer(renderer->swapchain, 0, &IID_ID3D11Texture2D, (void **) &framebuffer);
    ID3D11Device_CreateRenderTargetView(renderer->device, (ID3D11Resource *) framebuffer, 0, &renderer->framebuffer_view);

    D3D11_TEXTURE2D_DESC framebuffer_description = { 0 };
    ID3D11Texture2D_GetDesc(framebuffer, &framebuffer_description);

    renderer->framebuffer_width  = framebuffer_description.Width;
    renderer->framebuffer_height = framebuffer_description.Height;

    ID3D11Texture2D_Release(framebuffer);

    D3D11_VIEWPORT viewport;
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width    = (float) renderer->framebuffer_width;
    viewport.Height   = (float) renderer->framebuffer_height;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 0.0f;

    ID3D11DeviceContext_RSSetViewports(renderer->device_context, 1, &viewport);
    ID3D11DeviceContext_IASetPrimitiveTopology(renderer->device_context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    return &renderer->base;
}

static void
_cui_renderer_direct3d11_destroy(CuiRendererDirect3D11 *renderer)
{
    ID3D11DeviceContext_ClearState(renderer->device_context);

    ID3D11RenderTargetView_Release(renderer->framebuffer_view);
    ID3D11BlendState_Release(renderer->blend_state);
    ID3D11RasterizerState_Release(renderer->rasterizer_state);
    ID3D11Buffer_Release(renderer->vertex_buffer);
    ID3D11Buffer_Release(renderer->constant_buffer);
    ID3D11InputLayout_Release(renderer->vertex_layout);
    ID3D11PixelShader_Release(renderer->pixel_shader);
    ID3D11VertexShader_Release(renderer->vertex_shader);

    cui_platform_deallocate(renderer, renderer->allocation_size);
}

static CuiCommandBuffer *
_cui_renderer_direct3d11_begin_command_buffer(CuiRendererDirect3D11 *renderer)
{
    CuiCommandBuffer *command_buffer = &renderer->command_buffer;

    command_buffer->push_buffer_size = 0;
    command_buffer->index_buffer_count = 0;
    command_buffer->texture_operation_count = 0;

    return command_buffer;
}

static void
_cui_renderer_direct3d11_render(CuiRendererDirect3D11 *renderer, CuiFramebuffer *framebuffer, CuiCommandBuffer *command_buffer, CuiColor clear_color)
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

                D3D11_TEXTURE2D_DESC texture_description;
                texture_description.Width              = bitmap.width;
                texture_description.Height             = bitmap.height;
                texture_description.MipLevels          = 1;
                texture_description.ArraySize          = 1;
                texture_description.Format             = DXGI_FORMAT_B8G8R8A8_UNORM;
                texture_description.SampleDesc.Count   = 1;
                texture_description.SampleDesc.Quality = 0;
                texture_description.Usage              = D3D11_USAGE_DEFAULT;
                texture_description.BindFlags          = D3D11_BIND_SHADER_RESOURCE;
                texture_description.CPUAccessFlags     = D3D11_CPU_ACCESS_WRITE;
                texture_description.MiscFlags          = 0;

                // TODO: check for errors
                ID3D11Device_CreateTexture2D(renderer->device, &texture_description, 0, renderer->textures + texture_id);

                D3D11_SHADER_RESOURCE_VIEW_DESC texture_view_description;
                texture_view_description.Format                    = texture_description.Format;
                texture_view_description.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
                texture_view_description.Texture2D.MostDetailedMip = 0;
                texture_view_description.Texture2D.MipLevels       = 1;

                // TODO: check for errors
                ID3D11Device_CreateShaderResourceView(renderer->device, (ID3D11Resource *) renderer->textures[texture_id],
                                                      &texture_view_description, renderer->texture_views + texture_id);

                D3D11_SAMPLER_DESC sampler_description;
                sampler_description.Filter         = D3D11_FILTER_MIN_MAG_MIP_POINT;
                sampler_description.AddressU       = D3D11_TEXTURE_ADDRESS_CLAMP;
                sampler_description.AddressV       = D3D11_TEXTURE_ADDRESS_CLAMP;
                sampler_description.AddressW       = D3D11_TEXTURE_ADDRESS_CLAMP;
                sampler_description.MipLODBias     = 0.0f;
                sampler_description.MaxAnisotropy  = 0;
                sampler_description.ComparisonFunc = 0;
                sampler_description.BorderColor[0] = 0.0f;
                sampler_description.BorderColor[1] = 0.0f;
                sampler_description.BorderColor[2] = 0.0f;
                sampler_description.BorderColor[3] = 0.0f;
                sampler_description.MinLOD         = 0.0f;
                sampler_description.MaxLOD         = 0.0f;

                // TODO: check for errors
                ID3D11Device_CreateSamplerState(renderer->device, &sampler_description, renderer->samplers + texture_id);
            } break;

            case CUI_TEXTURE_OPERATION_DEALLOCATE:
            {
                CuiClearStruct(renderer->bitmaps[texture_id]);

                ID3D11SamplerState_Release(renderer->samplers[texture_id]);
                ID3D11ShaderResourceView_Release(renderer->texture_views[texture_id]);
                ID3D11Texture2D_Release(renderer->textures[texture_id]);

                renderer->samplers[texture_id] = 0;
                renderer->texture_views[texture_id] = 0;
                renderer->textures[texture_id] = 0;
            } break;

            case CUI_TEXTURE_OPERATION_UPDATE:
            {
                CuiRect rect = texture_op->payload.rect;
                CuiBitmap bitmap = renderer->bitmaps[texture_id];

                D3D11_BOX region = { rect.min.x, rect.min.y, 0, rect.max.x, rect.max.y, 1 };

                ID3D11DeviceContext_UpdateSubresource(renderer->device_context, (ID3D11Resource *) renderer->textures[texture_id], 0,
                                                      &region, (uint8_t *) bitmap.pixels + (rect.min.y * bitmap.stride) + (rect.min.x * 4),
                                                      bitmap.stride, 0);
            } break;
        }
    }

    if ((renderer->framebuffer_width != framebuffer->width) || (renderer->framebuffer_height != framebuffer->height))
    {
        ID3D11RenderTargetView_Release(renderer->framebuffer_view);

        IDXGISwapChain_ResizeBuffers(renderer->swapchain, 0, framebuffer->width, framebuffer->height, DXGI_FORMAT_UNKNOWN, 0);

        ID3D11Texture2D *framebuffer_texture;

        IDXGISwapChain_GetBuffer(renderer->swapchain, 0, &IID_ID3D11Texture2D, (void **) &framebuffer_texture);
        ID3D11Device_CreateRenderTargetView(renderer->device, (ID3D11Resource *) framebuffer_texture, 0, &renderer->framebuffer_view);

        ID3D11Texture2D_Release(framebuffer_texture);

        D3D11_VIEWPORT viewport;
        viewport.TopLeftX = 0.0f;
        viewport.TopLeftY = 0.0f;
        viewport.Width    = (float) framebuffer->width;
        viewport.Height   = (float) framebuffer->height;
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 0.0f;

        ID3D11DeviceContext_RSSetViewports(renderer->device_context, 1, &viewport);

        renderer->framebuffer_width = framebuffer->width;
        renderer->framebuffer_height = framebuffer->height;
    }

    ID3D11DeviceContext_OMSetRenderTargets(renderer->device_context, 1, &renderer->framebuffer_view, 0);

    D3D11_RECT scissor_rect;
    scissor_rect.left   = 0;
    scissor_rect.top    = 0;
    scissor_rect.right  = framebuffer->width;
    scissor_rect.bottom = framebuffer->height;

    ID3D11DeviceContext_RSSetScissorRects(renderer->device_context, 1, &scissor_rect);

    FLOAT clear[4] = { clear_color.r, clear_color.g, clear_color.b, clear_color.a };

    ID3D11DeviceContext_ClearRenderTargetView(renderer->device_context, renderer->framebuffer_view, clear);

    float a = 2.0f / framebuffer->width;
    float b = 2.0f / framebuffer->height;

    float vertex_transform[] = {
            a, 0.0f, 0.0f, 0.0f,
         0.0f,   -b, 0.0f, 0.0f,
         0.0f, 0.0f, 1.0f, 0.0f,
        -1.0f, 1.0f, 0.0f, 1.0f
    };

    D3D11_MAPPED_SUBRESOURCE mapping;

    // TODO: check for errors
    ID3D11DeviceContext_Map(renderer->device_context, (ID3D11Resource *) renderer->constant_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapping);
    CuiDirect3D11Constants *constants = (CuiDirect3D11Constants *) mapping.pData;
    cui_copy_memory(constants->vertex_transform, vertex_transform, sizeof(vertex_transform));
    ID3D11DeviceContext_Unmap(renderer->device_context, (ID3D11Resource*) renderer->constant_buffer, 0);

    ID3D11DeviceContext_VSSetConstantBuffers(renderer->device_context, 0, 1, &renderer->constant_buffer);

    uint32_t current_clip_rect = 0;
    uint32_t current_texture_id = (uint32_t) -1;

    // TODO: check for errors
    ID3D11DeviceContext_Map(renderer->device_context, (ID3D11Resource *) renderer->vertex_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapping);

    CuiDirect3D11Vertex *vertex_start = (CuiDirect3D11Vertex *) mapping.pData;
    CuiDirect3D11Vertex *vertices = vertex_start;

    for (uint32_t i = 0; i < command_buffer->index_buffer_count; i += 1)
    {
        uint32_t rect_offset = command_buffer->index_buffer[i];

        CuiTexturedRect *textured_rect = (CuiTexturedRect *) (command_buffer->push_buffer + rect_offset);

        if ((textured_rect->texture_id != current_texture_id) || (textured_rect->clip_rect != current_clip_rect))
        {
            int64_t vertex_count = vertices - vertex_start;

            if (vertex_count)
            {
                ID3D11DeviceContext_Unmap(renderer->device_context, (ID3D11Resource*) renderer->vertex_buffer, 0);

                const UINT stride = sizeof(CuiDirect3D11Vertex);
                const UINT offset = 0;

                ID3D11DeviceContext_IASetVertexBuffers(renderer->device_context, 0, 1, &renderer->vertex_buffer, &stride, &offset);
                ID3D11DeviceContext_Draw(renderer->device_context, vertex_count, 0);

                ID3D11DeviceContext_Map(renderer->device_context, (ID3D11Resource *) renderer->vertex_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapping);

                vertex_start = (CuiDirect3D11Vertex *) mapping.pData;
                vertices = vertex_start;
            }

            if (textured_rect->texture_id != current_texture_id)
            {
                CuiBitmap bitmap = renderer->bitmaps[textured_rect->texture_id];

                // TODO: check for errors
                ID3D11DeviceContext_Map(renderer->device_context, (ID3D11Resource *) renderer->constant_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapping);

                CuiDirect3D11Constants *constants = (CuiDirect3D11Constants *) mapping.pData;
                cui_copy_memory(constants->vertex_transform, vertex_transform, sizeof(vertex_transform));
                constants->texture_scale = cui_make_float_point(1.0f / (float) bitmap.width, 1.0f / (float) bitmap.height);

                ID3D11DeviceContext_Unmap(renderer->device_context, (ID3D11Resource*) renderer->constant_buffer, 0);

                ID3D11DeviceContext_PSSetShaderResources(renderer->device_context, 0, 1, renderer->texture_views + textured_rect->texture_id);
                ID3D11DeviceContext_PSSetSamplers(renderer->device_context, 0, 1, renderer->samplers + textured_rect->texture_id);

                current_texture_id = textured_rect->texture_id;
            }

            if (textured_rect->clip_rect != current_clip_rect)
            {
                if (textured_rect->clip_rect)
                {
                    CuiClipRect *rect = (CuiClipRect *) (command_buffer->push_buffer + textured_rect->clip_rect - 1);
                    scissor_rect.left   = rect->x_min;
                    scissor_rect.top    = rect->y_min;
                    scissor_rect.right  = rect->x_max;
                    scissor_rect.bottom = rect->y_max;
                }
                else
                {
                    scissor_rect.left   = 0;
                    scissor_rect.top    = 0;
                    scissor_rect.right  = framebuffer->width;
                    scissor_rect.bottom = framebuffer->height;
                }

                ID3D11DeviceContext_RSSetScissorRects(renderer->device_context, 1, &scissor_rect);

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

    int64_t vertex_count = vertices - vertex_start;

    ID3D11DeviceContext_Unmap(renderer->device_context, (ID3D11Resource*) renderer->vertex_buffer, 0);

    if (vertex_count)
    {
        const UINT stride = sizeof(CuiDirect3D11Vertex);
        const UINT offset = 0;

        ID3D11DeviceContext_IASetVertexBuffers(renderer->device_context, 0, 1, &renderer->vertex_buffer, &stride, &offset);
        ID3D11DeviceContext_Draw(renderer->device_context, vertex_count, 0);
    }
}
