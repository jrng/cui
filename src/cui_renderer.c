static inline CuiCommandBuffer *
_cui_renderer_begin_command_buffer(CuiRenderer *renderer)
{
    CuiCommandBuffer *command_buffer = 0;

    switch (renderer->type)
    {
        case CUI_RENDERER_TYPE_SOFTWARE:
        {
#if CUI_RENDERER_SOFTWARE_ENABLED
            CuiRendererSoftware *renderer_software = CuiContainerOf(renderer, CuiRendererSoftware, base);
            command_buffer = _cui_renderer_software_begin_command_buffer(renderer_software);
#else
            CuiAssert(!"CUI_RENDERER_TYPE_SOFTWARE not enabled.");
#endif
        } break;

        case CUI_RENDERER_TYPE_OPENGLES2:
        {
#if CUI_RENDERER_OPENGLES2_ENABLED
            CuiRendererOpengles2 *renderer_opengles2 = CuiContainerOf(renderer, CuiRendererOpengles2, base);
            command_buffer = _cui_renderer_opengles2_begin_command_buffer(renderer_opengles2);
#else
            CuiAssert(!"CUI_RENDERER_TYPE_OPENGLES2 not enabled.");
#endif
        } break;

        case CUI_RENDERER_TYPE_METAL:
        {
#if CUI_RENDERER_METAL_ENABLED
            CuiRendererMetal *renderer_metal = CuiContainerOf(renderer, CuiRendererMetal, base);
            command_buffer = _cui_renderer_metal_begin_command_buffer(renderer_metal);
#else
            CuiAssert(!"CUI_RENDERER_TYPE_METAL not enabled.");
#endif
        } break;

        case CUI_RENDERER_TYPE_DIRECT3D11:
        {
#if CUI_RENDERER_DIRECT3D11_ENABLED
            CuiRendererDirect3D11 *renderer_direct3d11 = CuiContainerOf(renderer, CuiRendererDirect3D11, base);
            command_buffer = _cui_renderer_direct3d11_begin_command_buffer(renderer_direct3d11);
#else
            CuiAssert(!"CUI_RENDERER_TYPE_DIRECT3D11 not enabled.");
#endif
        } break;
    }

    return command_buffer;
}

static inline void
_cui_renderer_render(CuiRenderer *renderer, CuiBitmap *framebuffer, int32_t framebuffer_width, int32_t framebuffer_height,
                     CuiCommandBuffer *command_buffer, CuiColor clear_color)
{
    switch (renderer->type)
    {
        case CUI_RENDERER_TYPE_SOFTWARE:
        {
#if CUI_RENDERER_SOFTWARE_ENABLED
            CuiRendererSoftware *renderer_software = CuiContainerOf(renderer, CuiRendererSoftware, base);
            _cui_renderer_software_render(renderer_software, framebuffer, command_buffer, clear_color);
#else
            (void) framebuffer;
            CuiAssert(!"CUI_RENDERER_TYPE_SOFTWARE not enabled.");
#endif
        } break;

        case CUI_RENDERER_TYPE_OPENGLES2:
        {
#if CUI_RENDERER_OPENGLES2_ENABLED
            CuiRendererOpengles2 *renderer_opengles2 = CuiContainerOf(renderer, CuiRendererOpengles2, base);
            _cui_renderer_opengles2_render(renderer_opengles2, command_buffer, framebuffer_width, framebuffer_height, clear_color);
#else
            CuiAssert(!"CUI_RENDERER_TYPE_OPENGLES2 not enabled.");
#endif
        } break;

        case CUI_RENDERER_TYPE_METAL:
        {
#if CUI_RENDERER_METAL_ENABLED
            id<CAMetalDrawable> drawable = nil;
            CuiAssert(!"not implemented");

            CuiRendererMetal *renderer_metal = CuiContainerOf(renderer, CuiRendererMetal, base);
            _cui_renderer_metal_render(renderer_metal, drawable, framebuffer_width,
                                       framebuffer_height, command_buffer, clear_color);
#else
            CuiAssert(!"CUI_RENDERER_TYPE_METAL not enabled.");
#endif
        } break;

        case CUI_RENDERER_TYPE_DIRECT3D11:
        {
#if CUI_RENDERER_DIRECT3D11_ENABLED
            CuiRendererDirect3D11 *renderer_direct3d11 = CuiContainerOf(renderer, CuiRendererDirect3D11, base);
            _cui_renderer_direct3d11_render(renderer_direct3d11, command_buffer, framebuffer_width, framebuffer_height, clear_color);
#else
            CuiAssert(!"CUI_RENDERER_TYPE_DIRECT3D11 not enabled.");
#endif
        } break;
    }
}

static inline void
_cui_renderer_destroy(CuiRenderer *renderer)
{
    switch (renderer->type)
    {
        case CUI_RENDERER_TYPE_SOFTWARE:
        {
#if CUI_RENDERER_SOFTWARE_ENABLED
            CuiRendererSoftware *renderer_software = CuiContainerOf(renderer, CuiRendererSoftware, base);
            _cui_renderer_software_destroy(renderer_software);
#else
            CuiAssert(!"CUI_RENDERER_TYPE_SOFTWARE not enabled.");
#endif
        } break;

        case CUI_RENDERER_TYPE_OPENGLES2:
        {
#if CUI_RENDERER_OPENGLES2_ENABLED
            CuiRendererOpengles2 *renderer_opengles2 = CuiContainerOf(renderer, CuiRendererOpengles2, base);
            _cui_renderer_opengles2_destroy(renderer_opengles2);
#else
            CuiAssert(!"CUI_RENDERER_TYPE_OPENGLES2 not enabled.");
#endif
        } break;

        case CUI_RENDERER_TYPE_METAL:
        {
#if CUI_RENDERER_METAL_ENABLED
            CuiRendererMetal *renderer_metal = CuiContainerOf(renderer, CuiRendererMetal, base);
            _cui_renderer_metal_destroy(renderer_metal);
#else
            CuiAssert(!"CUI_RENDERER_TYPE_METAL not enabled.");
#endif
        } break;

        case CUI_RENDERER_TYPE_DIRECT3D11:
        {
#if CUI_RENDERER_DIRECT3D11_ENABLED
            CuiRendererDirect3D11 *renderer_direct3d11 = CuiContainerOf(renderer, CuiRendererDirect3D11, base);
            _cui_renderer_direct3d11_destroy(renderer_direct3d11);
#else
            CuiAssert(!"CUI_RENDERER_TYPE_DIRECT3D11 not enabled.");
#endif
        } break;
    }
}
