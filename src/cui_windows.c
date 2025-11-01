#include <versionhelpers.h>
#include <windowsx.h>
#include <uxtheme.h>
#include <vssym32.h>

// #define CUI_WINDOWS_CALLBACK_TRACING

static DWORD
_cui_worker_thread_proc(void *data)
{
    (void) data;

    CuiWorkerThreadQueue *queue = &_cui_context.common.worker_thread_queue;

    for (;;)
    {
        if (_cui_do_next_worker_thread_queue_entry(queue))
        {
            WaitForSingleObject(queue->semaphore, INFINITE);
        }
    }

    return 0;
}

static DWORD
_cui_background_thread_proc(void *data)
{
    CuiBackgroundThreadQueue *queue = (CuiBackgroundThreadQueue *) data;

    for (;;)
    {
        if (_cui_do_next_background_thread_queue_entry(queue))
        {
            WaitForSingleObject(queue->semaphore, INFINITE);
        }
    }

    return 0;
}

#if CUI_RENDERER_SOFTWARE_ENABLED

static void
_cui_window_resize_backbuffer(CuiWindow *window, int32_t width, int32_t height)
{
    CuiAssert(window->base.renderer->type == CUI_RENDERER_TYPE_SOFTWARE);

    CuiAssert((window->renderer.software.backbuffer.width != width) || (window->renderer.software.backbuffer.height != height));

    window->renderer.software.backbuffer.width  = width;
    window->renderer.software.backbuffer.height = height;
    window->renderer.software.backbuffer.stride = CuiAlign(window->renderer.software.backbuffer.width * 4, 64);

    int64_t needed_size = (int64_t) window->renderer.software.backbuffer.stride *
                          (int64_t) window->renderer.software.backbuffer.height;

    if (needed_size > window->renderer.software.backbuffer_memory_size)
    {
        if (window->renderer.software.backbuffer.pixels)
        {
            cui_platform_deallocate(window->renderer.software.backbuffer.pixels, window->renderer.software.backbuffer_memory_size);
        }

        window->renderer.software.backbuffer_memory_size = CuiAlign(needed_size, CuiMiB(4));
        window->renderer.software.backbuffer.pixels = cui_platform_allocate(window->renderer.software.backbuffer_memory_size);
    }
}

#endif

#if CUI_RENDERER_DIRECT3D11_ENABLED

static bool
_cui_initialize_direct3d11(CuiWindow *window)
{
    ID3D11Device *d3d11_base_device;
    ID3D11DeviceContext *d3d11_base_context;

    UINT flags = D3D11_CREATE_DEVICE_SINGLETHREADED | D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    D3D_FEATURE_LEVEL feature_levels[] = { D3D_FEATURE_LEVEL_11_0 };

#if 0
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    HRESULT res = D3D11CreateDevice(0, D3D_DRIVER_TYPE_HARDWARE, 0, flags, feature_levels, CuiArrayCount(feature_levels),
                                    D3D11_SDK_VERSION, &d3d11_base_device, 0, &d3d11_base_context);

    if (FAILED(res))
    {
        res = D3D11CreateDevice(0, D3D_DRIVER_TYPE_WARP, 0, flags, feature_levels, CuiArrayCount(feature_levels),
                                D3D11_SDK_VERSION, &d3d11_base_device, 0, &d3d11_base_context);
    }

    if (SUCCEEDED(res))
    {
        IDXGIFactory2 *dxgi_factory = 0;

        IDXGIDevice *dxgi_device;

        if (SUCCEEDED(ID3D11Device_QueryInterface(d3d11_base_device, &IID_IDXGIDevice, (void **) &dxgi_device)))
        {
            IDXGIAdapter *dxgi_adapter;

            if (SUCCEEDED(IDXGIDevice_GetAdapter(dxgi_device, &dxgi_adapter)))
            {
                IDXGIAdapter_GetParent(dxgi_adapter, &IID_IDXGIFactory2, (void **) &dxgi_factory);
                IDXGIAdapter_Release(dxgi_adapter);
            }

            IDXGIDevice_Release(dxgi_device);
        }

        if (dxgi_factory)
        {
            IDXGISwapChain1 *dxgi_swapchain;

            DXGI_SWAP_CHAIN_DESC1 swapchain_description;
            swapchain_description.Width              = 0;
            swapchain_description.Height             = 0;
            swapchain_description.Format             = DXGI_FORMAT_B8G8R8A8_UNORM;
            swapchain_description.Stereo             = FALSE;
            swapchain_description.SampleDesc.Count   = 1;
            swapchain_description.SampleDesc.Quality = 0;
            swapchain_description.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            swapchain_description.BufferCount        = 2;
            swapchain_description.Scaling            = DXGI_SCALING_NONE;

            if (IsWindows10OrGreater())
            {
                swapchain_description.SwapEffect     = DXGI_SWAP_EFFECT_FLIP_DISCARD;
            }
            else
            {
                swapchain_description.SwapEffect     = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
            }

            swapchain_description.AlphaMode          = DXGI_ALPHA_MODE_IGNORE;
            swapchain_description.Flags              = 0;

            if (SUCCEEDED(IDXGIFactory2_CreateSwapChainForHwnd(dxgi_factory, (IUnknown *) d3d11_base_device, window->window_handle,
                                                               &swapchain_description, 0, 0, &dxgi_swapchain)))
            {
                IDXGIFactory2_MakeWindowAssociation(dxgi_factory, window->window_handle, DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_WINDOW_CHANGES);

                window->base.renderer = _cui_renderer_direct3d11_create(d3d11_base_device, d3d11_base_context, dxgi_swapchain);

                if (window->base.renderer)
                {
                    window->renderer.direct3d11.d3d11_device = d3d11_base_device;
                    window->renderer.direct3d11.d3d11_device_context = d3d11_base_context;
                    window->renderer.direct3d11.dxgi_swapchain = dxgi_swapchain;
                    return true;
                }
            }

            IDXGIFactory2_Release(dxgi_factory);
        }
    }

    if (window->renderer.direct3d11.dxgi_swapchain)
    {
        IDXGISwapChain1_Release(window->renderer.direct3d11.dxgi_swapchain);
    }

    if (window->renderer.direct3d11.d3d11_device_context)
    {
        ID3D11DeviceContext_Release(window->renderer.direct3d11.d3d11_device_context);
    }

    if (window->renderer.direct3d11.d3d11_device)
    {
        ID3D11Device_Release(window->renderer.direct3d11.d3d11_device);
    }

    return false;
}

#endif

static void
_cui_window_destroy(CuiWindow *window)
{
    switch (window->base.renderer->type)
    {
        case CUI_RENDERER_TYPE_SOFTWARE:
        {
#if CUI_RENDERER_SOFTWARE_ENABLED
            if (window->renderer.software.backbuffer.pixels)
            {
                cui_platform_deallocate(window->renderer.software.backbuffer.pixels, window->renderer.software.backbuffer_memory_size);
            }

            CuiRendererSoftware *renderer_software = CuiContainerOf(window->base.renderer, CuiRendererSoftware, base);
            _cui_renderer_software_destroy(renderer_software);
#else
            CuiAssert(!"CUI_RENDERER_TYPE_SOFTWARE not enabled.");
#endif
        } break;

        case CUI_RENDERER_TYPE_OPENGLES2:
        {
            CuiAssert(!"CUI_RENDERER_TYPE_OPENGLES2 not supported.");
        } break;

        case CUI_RENDERER_TYPE_METAL:
        {
            CuiAssert(!"CUI_RENDERER_TYPE_METAL not supported.");
        } break;

        case CUI_RENDERER_TYPE_DIRECT3D11:
        {
#if CUI_RENDERER_DIRECT3D11_ENABLED
            CuiRendererDirect3D11 *renderer_direct3d11 = CuiContainerOf(window->base.renderer, CuiRendererDirect3D11, base);
            _cui_renderer_direct3d11_destroy(renderer_direct3d11);

            CuiAssert(window->renderer.direct3d11.dxgi_swapchain);
            IDXGISwapChain1_Release(window->renderer.direct3d11.dxgi_swapchain);

            CuiAssert(window->renderer.direct3d11.d3d11_device_context);
            ID3D11DeviceContext_Release(window->renderer.direct3d11.d3d11_device_context);

            CuiAssert(window->renderer.direct3d11.d3d11_device);
            ID3D11Device_Release(window->renderer.direct3d11.d3d11_device);
#else
            CuiAssert(!"CUI_RENDERER_TYPE_DIRECT3D11 not enabled.");
#endif
        } break;
    }

    cui_arena_deallocate(&window->arena);
    _cui_remove_window(window);
}

static void
_cui_window_close(CuiWindow *window)
{
    DestroyWindow(window->window_handle);
}

static CuiFramebuffer *
_cui_acquire_framebuffer(CuiWindow *window, int32_t width, int32_t height)
{
    CuiFramebuffer *framebuffer = 0;

    switch (window->base.renderer->type)
    {
        case CUI_RENDERER_TYPE_SOFTWARE:
        {
#if CUI_RENDERER_SOFTWARE_ENABLED
            if ((window->renderer.software.backbuffer.width != width) || (window->renderer.software.backbuffer.height != height))
            {
                _cui_window_resize_backbuffer(window, width, height);
            }

            window->framebuffer.bitmap = window->renderer.software.backbuffer;
            framebuffer = &window->framebuffer;
#else
            CuiAssert(!"CUI_RENDERER_TYPE_SOFTWARE not enabled.");
#endif
        } break;

        case CUI_RENDERER_TYPE_OPENGLES2:
        {
            CuiAssert(!"CUI_RENDERER_TYPE_OPENGLES2 not supported.");
        } break;

        case CUI_RENDERER_TYPE_METAL:
        {
            CuiAssert(!"CUI_RENDERER_TYPE_METAL not supported.");
        } break;

        case CUI_RENDERER_TYPE_DIRECT3D11:
        {
#if CUI_RENDERER_DIRECT3D11_ENABLED
            framebuffer = &window->framebuffer;

            framebuffer->width = width;
            framebuffer->height = height;
#else
            CuiAssert(!"CUI_RENDERER_TYPE_DIRECT3D11 not enabled.");
#endif
        } break;
    }

    return framebuffer;
}

static inline bool
_cui_window_is_maximized(CuiWindow *window)
{
    bool result = false;

    WINDOWPLACEMENT window_placement = { sizeof(WINDOWPLACEMENT) };

    if (GetWindowPlacement(window->window_handle, &window_placement))
    {
        result = (window_placement.showCmd == SW_SHOWMAXIMIZED) ? true : false;
    }

    return result;
}

static void
_cui_window_set_fullscreen(CuiWindow *window, bool fullscreen)
{
    if (cui_window_is_fullscreen(window))
    {
        if (!fullscreen)
        {
            DWORD window_style = GetWindowLong(window->window_handle, GWL_STYLE);

            window->base.state &= ~CUI_WINDOW_STATE_FULLSCREEN;

            if (window->use_custom_decoration)
            {
                SetWindowLong(window->window_handle, GWL_STYLE, window_style | WS_SIZEBOX | WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX);
                cui_widget_set_preferred_size(window->titlebar, 0.0f, window->titlebar_height);
            }
            else
            {
                SetWindowLong(window->window_handle, GWL_STYLE, window_style | WS_OVERLAPPEDWINDOW);
            }

            SetWindowPlacement(window->window_handle, &window->windowed_placement);
            SetWindowPos(window->window_handle, 0, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
    }
    else
    {
        if (fullscreen)
        {
            DWORD window_style = GetWindowLong(window->window_handle, GWL_STYLE);

            window->base.state |= CUI_WINDOW_STATE_FULLSCREEN;

            MONITORINFO monitor_info = { sizeof(monitor_info) };

            window->windowed_placement.length = sizeof(window->windowed_placement);

            if (GetWindowPlacement(window->window_handle, &window->windowed_placement) &&
                GetMonitorInfo(MonitorFromWindow(window->window_handle, MONITOR_DEFAULTTOPRIMARY), &monitor_info))
            {
                if (window->use_custom_decoration)
                {
                    SetWindowLong(window->window_handle, GWL_STYLE, window_style & ~(WS_SIZEBOX | WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX));
                    cui_widget_set_preferred_size(window->titlebar, 0.0f, 0.0f);
                }
                else
                {
                    SetWindowLong(window->window_handle, GWL_STYLE, window_style & ~WS_OVERLAPPEDWINDOW);
                }

                SetWindowPos(window->window_handle, HWND_TOP,
                             monitor_info.rcMonitor.left, monitor_info.rcMonitor.top,
                             monitor_info.rcMonitor.right - monitor_info.rcMonitor.left,
                             monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top,
                             SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
            }
        }
    }
}

#if defined(CUI_WINDOWS_CALLBACK_TRACING)
static int32_t window_callback_indent = 0;
#endif

LRESULT CALLBACK
_cui_window_callback(HWND window_handle, UINT message, WPARAM w_param, LPARAM l_param)
{
    CuiWindow *window = 0;

    if (message == WM_NCCREATE)
    {
#if defined(CUI_WINDOWS_CALLBACK_TRACING)
        fprintf(stderr, "%*sWM_NCCREATE\n", window_callback_indent, "");
#endif

        LPCREATESTRUCT create_struct = (LPCREATESTRUCT) l_param;
        window = (CuiWindow *) create_struct->lpCreateParams;
        SetWindowLongPtr(window_handle, GWLP_USERDATA, (LONG_PTR) window);
    }
    else
    {
        window = (CuiWindow *) GetWindowLongPtr(window_handle, GWLP_USERDATA);
    }

#if defined(CUI_WINDOWS_CALLBACK_TRACING)
    window_callback_indent += 2;
#endif

    LRESULT result = FALSE;

    switch (message)
    {
        case WM_SETCURSOR:
        {
#if defined(CUI_WINDOWS_CALLBACK_TRACING)
            fprintf(stderr, "%*sWM_SETCURSOR\n", window_callback_indent, "");
#endif

            result = DefWindowProc(window_handle, message, w_param, l_param);
        } break;

        case WM_SYSCOMMAND:
        {
#if defined(CUI_WINDOWS_CALLBACK_TRACING)
            fprintf(stderr, "%*sWM_SYSCOMMAND\n", window_callback_indent, "");
#endif

            result = DefWindowProc(window_handle, message, w_param, l_param);
        } break;

        case WM_NCCALCSIZE:
        {
#if defined(CUI_WINDOWS_CALLBACK_TRACING)
            fprintf(stderr, "%*sWM_NCCALCSIZE\n", window_callback_indent, "");
#endif

            if (w_param && window->use_custom_decoration)
            {
                if (!cui_window_is_fullscreen(window))
                {
                    int frame_x, frame_y, border_padding;

                    // TODO: cache those on dpi change
                    if (_cui_context.GetSystemMetricsForDpi)
                    {
                        frame_x = _cui_context.GetSystemMetricsForDpi(SM_CXSIZEFRAME, window->dpi);
                        frame_y = _cui_context.GetSystemMetricsForDpi(SM_CYSIZEFRAME, window->dpi);
                        border_padding = _cui_context.GetSystemMetricsForDpi(SM_CXPADDEDBORDER, window->dpi);
                    }
                    else
                    {
                        frame_x = GetSystemMetrics(SM_CXSIZEFRAME);
                        frame_y = GetSystemMetrics(SM_CYSIZEFRAME);
                        border_padding = GetSystemMetrics(SM_CXPADDEDBORDER);
                    }

                    NCCALCSIZE_PARAMS *params = (NCCALCSIZE_PARAMS *) l_param;
                    RECT *requested_client_rect = params->rgrc;

                    requested_client_rect->left   += frame_x + border_padding;
                    requested_client_rect->right  -= frame_x + border_padding;
                    requested_client_rect->bottom -= frame_y + border_padding;

                    if (_cui_window_is_maximized(window))
                    {
                        requested_client_rect->top += border_padding;
                    }
                }
            }
            else
            {
                result = DefWindowProc(window_handle, message, w_param, l_param);
            }

#if CUI_RENDERER_DIRECT3D11_ENABLED

            if (window->base.renderer && (window->base.renderer->type == CUI_RENDERER_TYPE_DIRECT3D11))
            {
                RECT *client_rect;

                if (w_param)
                {
                    client_rect = ((NCCALCSIZE_PARAMS *) l_param)->rgrc;
                }
                else
                {
                    client_rect = (RECT *) l_param;
                }

                int32_t width = client_rect->right - client_rect->left;
                int32_t height = client_rect->bottom - client_rect->top;

                if ((window->width != width) || (window->height != height))
                {
                    window->width = width;
                    window->height = height;

                    if (window->base.platform_root_widget)
                    {
                        CuiRect rect = cui_make_rect(0, 0, window->width, window->height);
                        cui_widget_layout(window->base.platform_root_widget, rect);
                    }

                    window->base.needs_redraw = true;
                    window->base.width = window->width;
                    window->base.height = window->height;

                    CuiWindowFrameResult window_frame_result = { 0 };
                    CuiFramebuffer *framebuffer = _cui_window_frame_routine(window, window->base.events, &window_frame_result);

                    if (framebuffer)
                    {
                        // TODO: check for errors
                        IDXGISwapChain1_Present(window->renderer.direct3d11.dxgi_swapchain, 0, DXGI_PRESENT_RESTART);
                        IDXGISwapChain1_Present(window->renderer.direct3d11.dxgi_swapchain, 1, DXGI_PRESENT_DO_NOT_SEQUENCE);
                    }

                    if (window_frame_result.window_frame_actions & CUI_WINDOW_FRAME_ACTION_CLOSE)
                    {
                        _cui_window_close(window);
                    }
                }
            }

#endif

        } break;

        case WM_NCHITTEST:
        {
#if defined(CUI_WINDOWS_CALLBACK_TRACING)
            fprintf(stderr, "%*sWM_NCHITTEST\n", window_callback_indent, "");
#endif

            if (window->use_custom_decoration)
            {
                LRESULT hit = DefWindowProc(window_handle, message, w_param, l_param);

                if ((hit == HTNOWHERE) || (hit == HTRIGHT) || (hit == HTLEFT) || (hit == HTTOP) || (hit == HTBOTTOM) ||
                    (hit == HTTOPLEFT) || (hit == HTTOPRIGHT) || (hit == HTBOTTOMLEFT) || (hit == HTBOTTOMRIGHT))
                {
                    result = hit;
                    break;
                }

                if (!cui_window_is_fullscreen(window))
                {
                    int frame_y, border_padding;

                    // TODO: cache those on dpi change
                    if (_cui_context.GetSystemMetricsForDpi)
                    {
                        frame_y = _cui_context.GetSystemMetricsForDpi(SM_CYSIZEFRAME, window->dpi);
                        border_padding = _cui_context.GetSystemMetricsForDpi(SM_CXPADDEDBORDER, window->dpi);
                    }
                    else
                    {
                        frame_y = GetSystemMetrics(SM_CYSIZEFRAME);
                        border_padding = GetSystemMetrics(SM_CXPADDEDBORDER);
                    }

                    if ((window->base.hovered_widget == window->minimize_button) ||
                        (window->base.hovered_widget == window->close_button))
                    {
                        result = HTCLIENT;
                        break;
                    }

                    if (window->maximize_button && (window->base.hovered_widget == window->maximize_button))
                    {
                        // This is needed to support snap layouts on windows 11
                        result = HTMAXBUTTON;
                        break;
                    }

                    POINT cursor_point = { .x = GET_X_LPARAM(l_param), .y = GET_Y_LPARAM(l_param) };
                    ScreenToClient(window->window_handle, &cursor_point);

                    if ((cursor_point.y > 0) && (cursor_point.y < (frame_y + border_padding)))
                    {
                        if (cursor_point.x < border_padding)
                        {
                            result = HTTOPLEFT;
                        }
                        else if (cursor_point.x >= (window->width - border_padding))
                        {
                            result = HTTOPRIGHT;
                        }
                        else
                        {
                            result = HTTOP;
                        }
                        break;
                    }

                    if (window->base.hovered_widget == window->title)
                    {
                        result = HTCAPTION;
                        break;
                    }
                }

                result = HTCLIENT;
            }
            else
            {
                result = DefWindowProc(window_handle, message, w_param, l_param);
            }
        } break;

        case WM_CREATE:
        {
#if defined(CUI_WINDOWS_CALLBACK_TRACING)
            fprintf(stderr, "%*sWM_CREATE\n", window_callback_indent, "");
#endif

            if (window->use_custom_decoration)
            {
                RECT window_rect;
                GetWindowRect(window_handle, &window_rect);

                SetWindowPos(window_handle, 0, window_rect.left, window_rect.top,
                             window_rect.right - window_rect.left, window_rect.bottom - window_rect.top,
                             SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE);
            }
        } break;

        case WM_DPICHANGED:
        {
#if defined(CUI_WINDOWS_CALLBACK_TRACING)
            fprintf(stderr, "%*sWM_DPICHANGED\n", window_callback_indent, "");
#endif

            window->dpi = HIWORD(w_param);
            float ui_scale = (float) window->dpi / 96.0f;

            _cui_window_set_ui_scale(window, ui_scale);

            RECT *new_window_rect = (RECT *) l_param;

            SetWindowPos(window->window_handle, 0, new_window_rect->left, new_window_rect->top,
                         new_window_rect->right - new_window_rect->left, new_window_rect->bottom - new_window_rect->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);

            result = TRUE;
        } break;

        case WM_SIZE:
        {
#if defined(CUI_WINDOWS_CALLBACK_TRACING)
            fprintf(stderr, "%*sWM_SIZE\n", window_callback_indent, "");
#endif

            RECT client_rect;
            GetClientRect(window_handle, &client_rect);

            int32_t width = client_rect.right - client_rect.left;
            int32_t height = client_rect.bottom - client_rect.top;

            if ((window->width != width) || (window->height != height))
            {
                window->width = width;
                window->height = height;

                if (window->base.platform_root_widget)
                {
                    CuiRect rect = cui_make_rect(0, 0, window->width, window->height);
                    cui_widget_layout(window->base.platform_root_widget, rect);
                }

                window->base.needs_redraw = true;
            }
        } break;

        case WM_PAINT:
        {
#if defined(CUI_WINDOWS_CALLBACK_TRACING)
            fprintf(stderr, "%*sWM_PAINT\n", window_callback_indent, "");
#endif

            switch (window->base.renderer->type)
            {
                case CUI_RENDERER_TYPE_SOFTWARE:
                {
#if CUI_RENDERER_SOFTWARE_ENABLED
                    ValidateRect(window->window_handle, 0);

                    window->base.width = window->width;
                    window->base.height = window->height;

                    CuiWindowFrameResult window_frame_result = { 0 };
                    CuiFramebuffer *framebuffer = _cui_window_frame_routine(window, window->base.events, &window_frame_result);

                    if (framebuffer)
                    {
                        HDC device_context = GetDC(window->window_handle);

                        BITMAPINFO bitmap;
                        bitmap.bmiHeader.biSize        = sizeof(bitmap.bmiHeader);
                        bitmap.bmiHeader.biWidth       = window->renderer.software.backbuffer.stride / sizeof(uint32_t);
                        bitmap.bmiHeader.biHeight      = -(LONG) window->renderer.software.backbuffer.height;
                        bitmap.bmiHeader.biPlanes      = 1;
                        bitmap.bmiHeader.biBitCount    = 32;
                        bitmap.bmiHeader.biCompression = BI_RGB;

                        StretchDIBits(device_context, 0, 0, window->renderer.software.backbuffer.width, window->renderer.software.backbuffer.height,
                                      0, 0, window->renderer.software.backbuffer.width, window->renderer.software.backbuffer.height,
                                      window->renderer.software.backbuffer.pixels, &bitmap, DIB_RGB_COLORS, SRCCOPY);

                        ReleaseDC(window->window_handle, device_context);
                    }

                    if (window_frame_result.window_frame_actions & CUI_WINDOW_FRAME_ACTION_CLOSE)
                    {
                        _cui_window_close(window);
                        break;
                    }

                    if (window_frame_result.window_frame_actions & CUI_WINDOW_FRAME_ACTION_SET_FULLSCREEN)
                    {
                        _cui_window_set_fullscreen(window, window_frame_result.should_be_fullscreen);
                    }
#else
                    CuiAssert(!"CUI_RENDERER_TYPE_SOFTWARE not enabled.");
#endif
                } break;

                case CUI_RENDERER_TYPE_OPENGLES2:
                {
                    CuiAssert(!"CUI_RENDERER_TYPE_OPENGLES2 not supported.");
                } break;

                case CUI_RENDERER_TYPE_METAL:
                {
                    CuiAssert(!"CUI_RENDERER_TYPE_METAL not supported.");
                } break;

                case CUI_RENDERER_TYPE_DIRECT3D11:
                {
#if CUI_RENDERER_DIRECT3D11_ENABLED
                    result = DefWindowProc(window_handle, message, w_param, l_param);
#else
                    CuiAssert(!"CUI_RENDERER_TYPE_DIRECT3D11 not enabled.");
#endif
                } break;
            }
        } break;

        case WM_NCMOUSEMOVE:
        {
            if (window->use_custom_decoration)
            {
                // OutputDebugString(L"WM_NCMOUSEMOVE\n");

                if (window->is_tracking_mouse)
                {
                    window->is_tracking_mouse = false;
                }

                if (!window->is_tracking_ncmouse)
                {
                    TRACKMOUSEEVENT tracking = { sizeof(TRACKMOUSEEVENT) };
                    tracking.dwFlags = TME_LEAVE | TME_NONCLIENT;
                    tracking.hwndTrack = window->window_handle;

                    TrackMouseEvent(&tracking);
                    window->is_tracking_ncmouse = true;
                }

                // TODO: check if we're outside the window bounds to generate LEAVE events

                POINT cursor_point = { .x = GET_X_LPARAM(l_param), .y = GET_Y_LPARAM(l_param) };
                ScreenToClient(window->window_handle, &cursor_point);

                CuiEvent *event = cui_array_append(window->base.events);

                event->type = CUI_EVENT_TYPE_MOUSE_MOVE;
                event->mouse.x = cursor_point.x;
                event->mouse.y = cursor_point.y;
            }
            else
            {
                result = DefWindowProc(window_handle, message, w_param, l_param);
            }
        } break;

        case WM_MOUSEMOVE:
        {
            // OutputDebugString(L"WM_MOUSEMOVE\n");

            if (window->is_tracking_ncmouse)
            {
                window->is_tracking_ncmouse = false;
            }

            if (!window->is_tracking_mouse)
            {
                TRACKMOUSEEVENT tracking = { sizeof(TRACKMOUSEEVENT) };
                tracking.dwFlags = TME_LEAVE;
                tracking.hwndTrack = window->window_handle;

                TrackMouseEvent(&tracking);
                window->is_tracking_mouse = true;
            }

            CuiEvent *event = cui_array_append(window->base.events);

            event->type = CUI_EVENT_TYPE_MOUSE_MOVE;
            event->mouse.x = GET_X_LPARAM(l_param);
            event->mouse.y = GET_Y_LPARAM(l_param);
        } break;

        case WM_NCMOUSELEAVE:
        {
            if (window->is_tracking_ncmouse)
            {
                // OutputDebugString(L"WM_NCMOUSELEAVE\n");

                window->is_tracking_ncmouse = false;
                CuiEvent *event = cui_array_append(window->base.events);
                event->type = CUI_EVENT_TYPE_MOUSE_LEAVE;
            }
        } break;

        case WM_MOUSELEAVE:
        {
            if (window->is_tracking_mouse)
            {
                // OutputDebugString(L"WM_MOUSELEAVE\n");

                window->is_tracking_mouse = false;
                CuiEvent *event = cui_array_append(window->base.events);
                event->type = CUI_EVENT_TYPE_MOUSE_LEAVE;
            }
        } break;

        case WM_NCLBUTTONDOWN:
        {
            // OutputDebugString(L"WM_NCLBUTTONDOWN\n");

            if ((!window->minimize_button || (window->base.hovered_widget != window->minimize_button)) &&
                (!window->maximize_button || (window->base.hovered_widget != window->maximize_button)) &&
                (!window->close_button || (window->base.hovered_widget != window->close_button)))
            {
                result = DefWindowProc(window_handle, message, w_param, l_param);
            }
        } break;

        case WM_LBUTTONDOWN:
        {
            // OutputDebugString(L"WM_LBUTTONDOWN\n");
            // TODO: SetCapture(window->window_handle) ?
            CuiEvent *event = cui_array_append(window->base.events);

            event->type = CUI_EVENT_TYPE_LEFT_DOWN;
            event->mouse.x = GET_X_LPARAM(l_param);
            event->mouse.y = GET_Y_LPARAM(l_param);
        } break;

        case WM_LBUTTONDBLCLK:
        {
            // OutputDebugString(L"WM_LBUTTONDBLCLK\n");
            CuiEvent *event = cui_array_append(window->base.events);

            event->type = CUI_EVENT_TYPE_DOUBLE_CLICK;
            event->mouse.x = GET_X_LPARAM(l_param);
            event->mouse.y = GET_Y_LPARAM(l_param);
        } break;

        case WM_LBUTTONUP:
        {
            // TODO: ReleaseCapture(window->window_handle) ?
            CuiEvent *event = cui_array_append(window->base.events);

            event->type = CUI_EVENT_TYPE_LEFT_UP;
            event->mouse.x = GET_X_LPARAM(l_param);
            event->mouse.y = GET_Y_LPARAM(l_param);
        } break;

        case WM_RBUTTONDOWN:
        {
            CuiEvent *event = cui_array_append(window->base.events);

            event->type = CUI_EVENT_TYPE_RIGHT_DOWN;
            event->mouse.x = GET_X_LPARAM(l_param);
            event->mouse.y = GET_Y_LPARAM(l_param);
        } break;

        case WM_RBUTTONUP:
        {
            CuiEvent *event = cui_array_append(window->base.events);

            event->type = CUI_EVENT_TYPE_RIGHT_UP;
            event->mouse.x = GET_X_LPARAM(l_param);
            event->mouse.y = GET_Y_LPARAM(l_param);
        } break;

        case WM_MOUSEHWHEEL:
        {
            POINT mouse_position = { .x = GET_X_LPARAM(l_param), .y = GET_Y_LPARAM(l_param) };
            ScreenToClient(window->window_handle, &mouse_position);

            CuiEvent *event = cui_array_append(window->base.events);

            event->type = CUI_EVENT_TYPE_MOUSE_WHEEL;
            event->mouse.x = mouse_position.x;
            event->mouse.y = mouse_position.y;
            event->wheel.is_precise_scrolling = false;
            // TODO: where should this 3 be applied?
            event->wheel.dx = (float) ((3 * GET_WHEEL_DELTA_WPARAM(w_param)) / WHEEL_DELTA);
            event->wheel.dy = 0.0f;
        } break;

        case WM_MOUSEWHEEL:
        {
            POINT mouse_position = { .x = GET_X_LPARAM(l_param), .y = GET_Y_LPARAM(l_param) };
            ScreenToClient(window->window_handle, &mouse_position);

            CuiEvent *event = cui_array_append(window->base.events);

            event->type = CUI_EVENT_TYPE_MOUSE_WHEEL;
            event->mouse.x = mouse_position.x;
            event->mouse.y = mouse_position.y;
            event->wheel.is_precise_scrolling = false;
            // TODO: where should this 3 be applied?
            event->wheel.dx = 0.0f;
            event->wheel.dy = (float) ((3 * GET_WHEEL_DELTA_WPARAM(w_param)) / WHEEL_DELTA);
        } break;

#define _CUI_KEY_DOWN_EVENT(key_id)                         \
    CuiEvent *event = cui_array_append(window->base.events);\
    event->type = CUI_EVENT_TYPE_KEY_DOWN;                  \
    event->key.codepoint       = (key_id);                  \
    event->key.alt_is_down     = window->alt_is_down;       \
    event->key.ctrl_is_down    = window->ctrl_is_down;      \
    event->key.shift_is_down   = window->shift_is_down;     \
    event->key.command_is_down = false;

        case WM_KEYDOWN:
        {
            switch (w_param)
            {
                case VK_BACK:   { _CUI_KEY_DOWN_EVENT(CUI_KEY_BACKSPACE); } break;
                case VK_TAB:    { _CUI_KEY_DOWN_EVENT(CUI_KEY_TAB);       } break;
                case VK_RETURN: { _CUI_KEY_DOWN_EVENT(CUI_KEY_ENTER);     } break;
                case VK_ESCAPE: { _CUI_KEY_DOWN_EVENT(CUI_KEY_ESCAPE);    } break;
                case VK_UP:     { _CUI_KEY_DOWN_EVENT(CUI_KEY_UP);        } break;
                case VK_DOWN:   { _CUI_KEY_DOWN_EVENT(CUI_KEY_DOWN);      } break;
                case VK_LEFT:   { _CUI_KEY_DOWN_EVENT(CUI_KEY_LEFT);      } break;
                case VK_RIGHT:  { _CUI_KEY_DOWN_EVENT(CUI_KEY_RIGHT);     } break;

                case VK_F1:  case VK_F2:  case VK_F3:  case VK_F4:
                case VK_F5:  case VK_F6:  case VK_F7:  case VK_F8:
                case VK_F9:  case VK_F10: case VK_F11: case VK_F12:
                {
#if CUI_FRAMEBUFFER_SCREENSHOT_ENABLED
                    if (w_param == VK_F2)
                    {
                        window->base.take_screenshot = true;
                        window->base.needs_redraw = true;
                    }
                    else
#endif
                    {
                        _CUI_KEY_DOWN_EVENT(CUI_KEY_F1 + (w_param - VK_F1));
                    }
                } break;

                case VK_CONTROL:
                case VK_LCONTROL:
                case VK_RCONTROL:
                case VK_MENU:
                case VK_LMENU:
                case VK_RMENU:
                case VK_SHIFT:
                case VK_LSHIFT:
                case VK_RSHIFT:
                    break;

                default:
                {
                    BYTE state[256];
                    uint8_t character;

                    UINT vk = (UINT) w_param;
                    UINT scan = (UINT) ((l_param >> 1) & 0x7F);

                    GetKeyboardState(state);

                    if (window->ctrl_is_down && !window->alt_is_down)
                    {
                        state[VK_CONTROL] = 0;
                    }

                    int ret = ToAscii(vk, scan, state, (LPWORD) &character, 0);

#if 0
                    WCHAR buffer[200];
                    wsprintf(buffer, L"character: %u\n", character);
                    OutputDebugString(buffer);
#endif

                    if ((ret == 1) && (character >= 32) && (character < 127))
                    {
                        _CUI_KEY_DOWN_EVENT(character);
                    }
                } break;
            }
        } break;

        case WM_CHAR:
        {
            if (w_param >= 128)
            {
                _CUI_KEY_DOWN_EVENT(w_param);
            }
        } break;

#undef _CUI_KEY_DOWN_EVENT

        case WM_INPUT:
        {
            uint8_t raw_input_buffer[sizeof(RAWINPUT)];
            UINT buffer_size = sizeof(raw_input_buffer);

            GetRawInputData((HRAWINPUT) l_param, RID_INPUT, raw_input_buffer, &buffer_size, sizeof(RAWINPUTHEADER));

            RAWINPUT *raw_input = (RAWINPUT *) raw_input_buffer;

            if (raw_input->header.dwType == RIM_TYPEKEYBOARD)
            {
                RAWKEYBOARD *keyboard = &raw_input->data.keyboard;

                bool is_down = (keyboard->Flags & 1) ? false : true;

                switch (keyboard->VKey)
                {
                    case VK_MENU:
                    {
                        window->alt_is_down = is_down;
                    } break;

                    case VK_CONTROL:
                    {
                        window->ctrl_is_down = is_down;
                    } break;

                    case VK_SHIFT:
                    {
                        window->shift_is_down = is_down;
                    } break;
                }
            }

            result = DefWindowProc(window_handle, message, w_param, l_param);
        } break;

        case WM_DESTROY:
        {
            _cui_window_destroy(window);
        } break;

        default:
        {
#if defined(CUI_WINDOWS_CALLBACK_TRACING)
            fprintf(stderr, "%*sMESSAGE %u\n", window_callback_indent, "", message);
#endif
            result = DefWindowProc(window_handle, message, w_param, l_param);
        } break;
    }

#if defined(CUI_WINDOWS_CALLBACK_TRACING)
    window_callback_indent -= 2;
#endif

    return result;
}

static void
_cui_window_on_maximize_button(CuiWidget *widget)
{
    // OutputDebugString(L"on_maximize_button\n");
    ShowWindow(widget->window->window_handle, _cui_window_is_maximized(widget->window) ? SW_NORMAL : SW_MAXIMIZE);
}

static void
_cui_window_on_minimize_button(CuiWidget *widget)
{
    // OutputDebugString(L"on_minimize_button\n");
    ShowWindow(widget->window->window_handle, SW_MINIMIZE);
}

void
cui_signal_main_thread(void)
{
    SetEvent(_cui_context.signal_event);
}

CuiString
cui_platform_get_environment_variable(CuiArena *temporary_memory, CuiArena *arena, CuiString name)
{
    CuiString result = { 0 };

    CuiString utf16_name = cui_utf8_to_utf16le(temporary_memory, name);
    const char *c_name = cui_to_c_string(temporary_memory, utf16_name);

    DWORD size = GetEnvironmentVariable((LPCWSTR) c_name, 0, 0);

    if (size)
    {
        int64_t string_length = 2 * size;
        int64_t buffer_size = string_length + 2;
        void *buffer = cui_alloc(temporary_memory, buffer_size, CuiDefaultAllocationParams());
        GetEnvironmentVariable((LPCWSTR) c_name, buffer, buffer_size);
        result = cui_utf16le_to_utf8(arena, cui_make_string(buffer, string_length));
    }

    return result;
}

int32_t cui_platform_get_environment_variable_int32(CuiArena *temporary_memory, CuiString name)
{
    CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(temporary_memory);

    int32_t result = cui_string_parse_int32(cui_platform_get_environment_variable(temporary_memory, temporary_memory, name));

    cui_end_temporary_memory(temp_memory);

    return result;
}

void *
cui_platform_allocate(uint64_t size)
{
    return VirtualAlloc(0, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

void
cui_platform_deallocate(void *ptr, uint64_t size)
{
    (void) size;
    VirtualFree(ptr, 0, MEM_RELEASE);
}

bool
cui_platform_open_file_dialog(CuiArena *temporary_memory, CuiArena *arena, CuiString **filenames,
                              bool can_select_multiple, bool can_select_files, bool can_select_directories)
{
    CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(temporary_memory);

    DWORD max_filename_length = 32 * 1024;
    uint16_t *filename_buffer = cui_alloc_array(temporary_memory, uint16_t, max_filename_length, CuiDefaultAllocationParams());

    filename_buffer[0] = 0;

    OPENFILENAMEW open_file = { sizeof(open_file) };

    open_file.lpstrFile = filename_buffer;
    open_file.nMaxFile  = max_filename_length;
    open_file.Flags     = OFN_FILEMUSTEXIST | OFN_EXPLORER;

    if (can_select_multiple)
    {
        open_file.Flags |= OFN_ALLOWMULTISELECT;
    }

    bool result = false;

    if (GetOpenFileNameW(&open_file))
    {
        int64_t filename_length = wcslen((wchar_t *) filename_buffer);

        CuiString utf16_string = cui_make_string(filename_buffer, 2 * filename_length);
        CuiString filename = cui_utf16le_to_utf8(temporary_memory, utf16_string);

        filename_buffer += (filename_length + 1);

        if (*filename_buffer)
        {
            CuiString directory = filename;

            while (*filename_buffer)
            {
                filename_length = wcslen((wchar_t *) filename_buffer);

                utf16_string = cui_make_string(filename_buffer, 2 * filename_length);
                filename = cui_utf16le_to_utf8(temporary_memory, utf16_string);

                filename_buffer += (filename_length + 1);

                *cui_array_append(*filenames) = cui_path_concat(arena, directory, filename);
            }
        }
        else
        {
            *cui_array_append(*filenames) = cui_copy_string(arena, filename);
        }

        result = true;
    }

    cui_end_temporary_memory(temp_memory);

    return result;
}

void
cui_platform_set_clipboard_text(CuiArena *temporary_memory, CuiString text)
{
    if (OpenClipboard(0))
    {
        EmptyClipboard();

        CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(temporary_memory);

        // TODO: convert to CRLF
        CuiString utf16_string = cui_utf8_to_utf16le(temporary_memory, text);

        HANDLE clipbuffer = GlobalAlloc(GMEM_MOVEABLE, utf16_string.count + 2);

        if (clipbuffer)
        {
            uint8_t *dst = (uint8_t *) GlobalLock(clipbuffer);

            cui_copy_memory(dst, utf16_string.data, utf16_string.count);
            dst[utf16_string.count + 0] = 0;
            dst[utf16_string.count + 1] = 0;

            GlobalUnlock(clipbuffer);

            SetClipboardData(CF_UNICODETEXT, clipbuffer);
        }

        cui_end_temporary_memory(temp_memory);

        CloseClipboard();
    }
}

CuiString
cui_platform_get_clipboard_text(CuiArena *arena)
{
    CuiString result = { 0, 0 };

    if (IsClipboardFormatAvailable(CF_UNICODETEXT) && OpenClipboard(0))
    {
        HANDLE clipboard_handle = GetClipboardData(CF_UNICODETEXT);

        if (clipboard_handle)
        {
            void *data = GlobalLock(clipboard_handle);

            if (data)
            {
                // TODO: handle CRLF to LF conversion
                CuiString utf16_string = cui_make_string(data, 2 * wcslen((wchar_t *) data));
                result = cui_utf16le_to_utf8(arena, utf16_string);

                GlobalUnlock(clipboard_handle);
            }
        }

        CloseClipboard();
    }

    return result;
}

uint64_t
cui_platform_get_performance_counter(void)
{
    LARGE_INTEGER time;
    QueryPerformanceCounter(&time);
    return time.QuadPart;
}

uint64_t
cui_platform_get_performance_frequency(void)
{
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    return frequency.QuadPart;
}

uint32_t
cui_platform_get_core_count(void)
{
    SYSTEM_INFO system_info;
    GetSystemInfo(&system_info);

    return system_info.dwNumberOfProcessors;
}

uint32_t
cui_platform_get_performance_core_count(void)
{
    SYSTEM_INFO system_info;
    GetSystemInfo(&system_info);

    return system_info.dwNumberOfProcessors;
}

uint32_t
cui_platform_get_efficiency_core_count(void)
{
    return 0;
}

bool
cui_platform_directory_exists(CuiArena *temporary_memory, CuiString directory)
{
    bool result;

    CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(temporary_memory);

    CuiString utf16_string = cui_utf8_to_utf16le(temporary_memory, directory);
    DWORD file_attr = GetFileAttributes((LPCWSTR) cui_to_c_string(temporary_memory, utf16_string));
    result = (file_attr != INVALID_FILE_ATTRIBUTES) && (file_attr & FILE_ATTRIBUTE_DIRECTORY);

    cui_end_temporary_memory(temp_memory);

    return result;
}

void
cui_platform_directory_create(CuiArena *temporary_memory, CuiString directory)
{
    CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(temporary_memory);

    CuiString utf16_string = cui_utf8_to_utf16le(temporary_memory, directory);
    CreateDirectory((LPCWSTR) cui_to_c_string(temporary_memory, utf16_string), 0);

    cui_end_temporary_memory(temp_memory);
}

bool
cui_platform_file_exists(CuiArena *temporary_memory, CuiString filename)
{
    bool result;

    CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(temporary_memory);

    CuiString utf16_string = cui_utf8_to_utf16le(temporary_memory, filename);
    DWORD file_attr = GetFileAttributes((LPCWSTR) cui_to_c_string(temporary_memory, utf16_string));
    result = (file_attr != INVALID_FILE_ATTRIBUTES) && !(file_attr & FILE_ATTRIBUTE_DIRECTORY);

    cui_end_temporary_memory(temp_memory);

    return result;
}

CuiFile *
cui_platform_file_open(CuiArena *temporary_memory, CuiString filename, uint32_t mode)
{
    CuiFile *result = 0;

    DWORD flags = FILE_ATTRIBUTE_NORMAL;
    DWORD share_mode = 0;
    DWORD access_mode = 0;

    if (mode & CUI_FILE_MODE_READ)
    {
        share_mode = FILE_SHARE_READ;
        access_mode = GENERIC_READ;
    }

    if (mode & CUI_FILE_MODE_WRITE)
    {
        flags = FILE_FLAG_WRITE_THROUGH;
        access_mode |= GENERIC_WRITE;
    }

    CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(temporary_memory);

    CuiString utf16_string = cui_utf8_to_utf16le(temporary_memory, filename);
    HANDLE file = CreateFile((LPCWSTR) cui_to_c_string(temporary_memory, utf16_string),
                             access_mode, share_mode, 0, OPEN_EXISTING, flags, 0);

    if (file != INVALID_HANDLE_VALUE)
    {
        result = (CuiFile *) file;
    }

    cui_end_temporary_memory(temp_memory);

    return result;
}

CuiFile *
cui_platform_file_create(CuiArena *temporary_memory, CuiString filename)
{
    CuiFile *result = 0;

    CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(temporary_memory);

    CuiString utf16_string = cui_utf8_to_utf16le(temporary_memory, filename);
    HANDLE file = CreateFile((LPCWSTR) cui_to_c_string(temporary_memory, utf16_string),
                             GENERIC_READ | GENERIC_WRITE, 0, 0, CREATE_ALWAYS,
                             FILE_FLAG_WRITE_THROUGH, 0);

    if (file != INVALID_HANDLE_VALUE)
    {
        result = (CuiFile *) file;
    }

    cui_end_temporary_memory(temp_memory);

    return result;
}

CuiFileAttributes
cui_platform_file_get_attributes(CuiFile *file)
{
    CuiAssert(file);

    CuiFileAttributes result = { 0 };
    BY_HANDLE_FILE_INFORMATION file_info = { 0 };

    if (GetFileInformationByHandle((HANDLE) file, &file_info))
    {
        if (file_info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            result.flags = CUI_FILE_ATTRIBUTE_IS_DIRECTORY;
        }

        result.size = ((uint64_t) file_info.nFileSizeHigh << 32) | (uint64_t) file_info.nFileSizeLow;
    }

    FILETIME last_write_time;

    if (GetFileTime((HANDLE) file, 0, 0, &last_write_time))
    {
        result.modification_time = ((uint64_t) last_write_time.dwHighDateTime << 32) | (uint64_t) last_write_time.dwLowDateTime;
    }

    return result;
}

CuiFileAttributes
cui_platform_get_file_attributes(CuiArena *temporary_memory, CuiString filename)
{
    CuiFileAttributes result = { 0 };

    CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(temporary_memory);

    CuiString utf16_string = cui_utf8_to_utf16le(temporary_memory, filename);
    HANDLE file = CreateFile((LPCWSTR) cui_to_c_string(temporary_memory, utf16_string),
                             GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING,
                             FILE_ATTRIBUTE_NORMAL, 0);

    if (file != INVALID_HANDLE_VALUE)
    {
        result = cui_platform_file_get_attributes((CuiFile *) file);
    }

    cui_end_temporary_memory(temp_memory);

    return result;
}

void
cui_platform_file_truncate(CuiFile *file, uint64_t size)
{
    CuiAssert(file);

    LARGE_INTEGER trunc_size;
    trunc_size.QuadPart = size;
    SetFilePointerEx((HANDLE) file, trunc_size, 0, FILE_BEGIN);
    SetEndOfFile((HANDLE) file);
    trunc_size.QuadPart = 0;
    SetFilePointerEx((HANDLE) file, trunc_size, 0, FILE_BEGIN);
}

void
cui_platform_file_read(CuiFile *file, void *buffer, uint64_t offset, uint64_t size)
{
    CuiAssert(file);

    DWORD bytes_read = 0;
    LARGE_INTEGER seek_offset;
    seek_offset.QuadPart = offset;
    SetFilePointerEx((HANDLE) file, seek_offset, 0, FILE_BEGIN);
    ReadFile((HANDLE) file, buffer, size, &bytes_read, 0);
}

void
cui_platform_file_write(CuiFile *file, void *buffer, uint64_t offset, uint64_t size)
{
    CuiAssert(file);

    DWORD bytes_written = 0;
    LARGE_INTEGER seek_offset;
    seek_offset.QuadPart = offset;
    SetFilePointerEx((HANDLE) file, seek_offset, 0, FILE_BEGIN);
    WriteFile((HANDLE) file, buffer, size, &bytes_written, 0);
}

void
cui_platform_file_close(CuiFile *file)
{
    CuiAssert(file);
    CloseHandle((HANDLE) file);
}

CuiString
cui_platform_get_canonical_filename(CuiArena *temporary_memory, CuiArena *arena, CuiString filename)
{
    (void) temporary_memory;
    (void) arena;

    CuiString result = { 0 };

    result = filename;

    return result;
}

void
cui_platform_get_files(CuiArena *temporary_memory, CuiArena *arena, CuiString directory, CuiFileInfo **file_list)
{
    CuiString search_path = cui_path_concat(temporary_memory, directory, CuiStringLiteral("*"));

    WIN32_FIND_DATA find_data = { 0 };
    CuiString utf16_string = cui_utf8_to_utf16le(temporary_memory, search_path);
    HANDLE search = FindFirstFile((LPCWSTR) cui_to_c_string(temporary_memory, utf16_string), &find_data);

    if (search != INVALID_HANDLE_VALUE)
    {
        for (;;)
        {
            CuiFileInfo *file_info = cui_array_append(*file_list);

            CuiString utf16_string = cui_make_string(find_data.cFileName, 2 * wcslen(find_data.cFileName));
            file_info->name = cui_utf16le_to_utf8(arena, utf16_string);

            if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                file_info->attr.flags = CUI_FILE_ATTRIBUTE_IS_DIRECTORY;
            }
            else
            {
                file_info->attr.flags = 0;
            }

            file_info->attr.size = ((uint64_t) find_data.nFileSizeHigh << 32) | (uint64_t) find_data.nFileSizeLow;

            if (!FindNextFile(search, &find_data))
            {
                FindClose(search);
                break;
            }
        }
    }
}

void
cui_platform_get_font_directories(CuiArena *temporary_memory, CuiArena *arena, CuiString **font_dirs)
{
    (void) temporary_memory;
    (void) arena;

    *cui_array_append(*font_dirs) = CuiStringLiteral("C:\\Windows\\Fonts");
}

CuiString
cui_platform_get_data_directory(CuiArena *temporary_memory, CuiArena *arena)
{
    (void) temporary_memory;
    (void) arena;

    CuiString result = { 0 };

    CuiAssert(!"unimplemented");

    return result;
}

CuiString
cui_platform_get_current_working_directory(CuiArena *temporary_memory, CuiArena *arena)
{
    CuiString result = { 0 };

    size_t buffer_size = CuiKiB(1);
    char *current_working_directory = cui_alloc_array(temporary_memory, char, buffer_size, CuiDefaultAllocationParams());

    DWORD count = GetCurrentDirectory(buffer_size, (LPWSTR) current_working_directory);

    if (count)
    {
        CuiString utf16_string = cui_make_string(current_working_directory, 2 * count);
        result = cui_utf16le_to_utf8(arena, utf16_string);
    }

    return result;
}

CuiString *
cui_get_files_to_open(void)
{
    return 0;
}

bool
cui_init(int argument_count, char **arguments)
{
    if (!_cui_common_init(argument_count, arguments))
    {
        return false;
    }

    int arg_count;
    LPWSTR *args = CommandLineToArgvW(GetCommandLine(), &arg_count);

    if (args)
    {
        args += 1;
        arg_count -= 1;

        for (int i = 0; i < arg_count; i += 1)
        {
            CuiString utf16_argument = cui_make_string(args[i], 2 * wcslen(args[i]));
            CuiString argument = cui_utf16le_to_utf8(&_cui_context.common.command_line_arguments_arena, utf16_argument);
            *cui_array_append(_cui_context.common.command_line_arguments) = argument;
        }

        LocalFree(args);
    }

    int32_t worker_thread_count = cui_platform_get_performance_core_count() - 1;

    worker_thread_count = cui_max_int32(1, cui_min_int32(worker_thread_count, 15));

    _cui_context.common.worker_thread_queue.semaphore =
        CreateSemaphore(0, 0, CuiArrayCount(_cui_context.common.worker_thread_queue.entries), 0);
    _cui_context.common.interactive_background_thread_queue.semaphore =
        CreateSemaphore(0, 0, CuiArrayCount(_cui_context.common.interactive_background_thread_queue.entries), 0);
    _cui_context.common.non_interactive_background_thread_queue.semaphore =
        CreateSemaphore(0, 0, CuiArrayCount(_cui_context.common.non_interactive_background_thread_queue.entries), 0);

    for (int32_t worker_thread_index = 0;
         worker_thread_index < worker_thread_count;
         worker_thread_index += 1)
    {
        CreateThread(0, 0, _cui_worker_thread_proc, 0, 0, 0);
    }

    CreateThread(0, 0, _cui_background_thread_proc, &_cui_context.common.interactive_background_thread_queue, 0, 0);
    CreateThread(0, 0, _cui_background_thread_proc, &_cui_context.common.non_interactive_background_thread_queue, 0, 0);

    _cui_context.signal_event = CreateEvent(0, TRUE, FALSE, 0);

    HMODULE user_32 = LoadLibrary(L"User32.dll");

    CuiAssert(user_32);

    BOOL (*SetProcessDpiAwarenessContext)(DPI_AWARENESS_CONTEXT) =
        (BOOL (*)(DPI_AWARENESS_CONTEXT)) GetProcAddress(user_32, "SetProcessDpiAwarenessContext");

    if (SetProcessDpiAwarenessContext)
    {
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    }
    else
    {
        BOOL (*SetProcessDPIAware)() = (BOOL (*)()) GetProcAddress(user_32, "SetProcessDPIAware");

        if (SetProcessDPIAware)
        {
            SetProcessDPIAware();
        }
    }

    _cui_context.GetDpiForSystem = (UINT (*)()) GetProcAddress(user_32, "GetDpiForSystem");
    _cui_context.GetDpiForWindow = (UINT (*)(HWND)) GetProcAddress(user_32, "GetDpiForWindow");
    _cui_context.GetSystemMetricsForDpi = (int (*)(int, UINT)) GetProcAddress(user_32, "GetSystemMetricsForDpi");

    _cui_context.cursor_arrow           = LoadCursor(0, IDC_ARROW);
    _cui_context.cursor_text            = LoadCursor(0, IDC_IBEAM);
    _cui_context.cursor_hand            = LoadCursor(0, IDC_HAND);
    _cui_context.cursor_move_all        = LoadCursor(0, IDC_SIZEALL);
    _cui_context.cursor_move_left_right = LoadCursor(0, IDC_SIZEWE);
    _cui_context.cursor_move_top_down   = LoadCursor(0, IDC_SIZENS);

    _cui_context.window_class_name = L"my_very_special_window_class";

    WNDCLASS window_class = { sizeof(WNDCLASS) };
    window_class.style = CS_VREDRAW | CS_HREDRAW | CS_DBLCLKS;
    window_class.lpfnWndProc = _cui_window_callback;
    window_class.hInstance = GetModuleHandle(0);
    window_class.lpszClassName = _cui_context.window_class_name;
    window_class.hCursor = _cui_context.cursor_arrow;

    if (!RegisterClass(&window_class))
    {
        return false;
    }

    RAWINPUTDEVICE input_device;
    input_device.usUsagePage = 1;
    input_device.usUsage     = 6;
    input_device.dwFlags     = 0;
    input_device.hwndTarget  = 0;

    if (!RegisterRawInputDevices(&input_device, 1, sizeof(input_device)))
    {
        return false;
    }

    return true;
}

CuiWindow *
cui_window_create(uint32_t creation_flags)
{
    CuiWindow *window = _cui_add_window(creation_flags);

    window->dpi = 96;

    if (_cui_context.GetDpiForSystem)
    {
        window->dpi = _cui_context.GetDpiForSystem();
    }
    else
    {
        HDC device_context = GetDC(0);
        window->dpi = GetDeviceCaps(device_context, LOGPIXELSX);
        ReleaseDC(0, device_context);
    }

    window->base.ui_scale = (float) window->dpi / 96.0f;

    window->width = lroundf(window->base.ui_scale * CUI_DEFAULT_WINDOW_WIDTH);
    window->height = lroundf(window->base.ui_scale * CUI_DEFAULT_WINDOW_HEIGHT);

    window->font_id = _cui_font_manager_find_font(&window->base.temporary_memory, &window->base.font_manager, window->base.ui_scale,
                                                  cui_make_sized_font_spec(CuiStringLiteral("arial"),  14.0f, 1.0f),
                                                  // fallback font for wine
                                                  cui_make_sized_font_spec(CuiStringLiteral("consola"), 14.0f, 1.0f),
                                                  cui_make_sized_font_spec(CuiStringLiteral("seguiemj"), 14.0f, 1.0f));

    cui_arena_allocate(&window->arena, CuiKiB(4));

    if (IsWindows8OrGreater() && !(window->base.creation_flags & CUI_WINDOW_CREATION_FLAG_PREFER_SYSTEM_DECORATION))
    {
        window->use_custom_decoration = true;
    }

    if (window->use_custom_decoration)
    {
        CuiWidget *root_widget = cui_alloc_type(&window->arena, CuiWidget, CuiDefaultAllocationParams());

        cui_widget_init(root_widget, CUI_WIDGET_TYPE_BOX);
        cui_widget_set_main_axis(root_widget, CUI_AXIS_Y);
        cui_widget_set_y_axis_gravity(root_widget, CUI_GRAVITY_START);

        window->titlebar = cui_alloc_type(&window->arena, CuiWidget, CuiDefaultAllocationParams());

        cui_widget_init(window->titlebar, CUI_WIDGET_TYPE_BOX);
        cui_widget_set_main_axis(window->titlebar, CUI_AXIS_X);
        cui_widget_set_x_axis_gravity(window->titlebar, CUI_GRAVITY_END);
        cui_widget_add_flags(window->titlebar, CUI_WIDGET_FLAG_DRAW_BACKGROUND | CUI_WIDGET_FLAG_FIXED_HEIGHT);
        cui_widget_set_preferred_size(window->titlebar, 0.0f, 0.0f);
        cui_widget_set_border_width(window->titlebar, 0.0f, 0.0f, 0.0f, 0.0f);

        window->titlebar->color_normal_background = CUI_COLOR_WINDOW_TITLEBAR_BACKGROUND;
        window->titlebar->color_normal_border = CUI_COLOR_WINDOW_TITLEBAR_BORDER;

        cui_widget_append_child(root_widget, window->titlebar);

        // close button
        window->close_button = cui_alloc_type(&window->arena, CuiWidget, CuiDefaultAllocationParams());

        cui_widget_init(window->close_button, CUI_WIDGET_TYPE_BUTTON);
        cui_widget_set_icon(window->close_button, CUI_ICON_WINDOWS_CLOSE);
        cui_widget_set_border_width(window->close_button, 0.0f, 0.0f, 0.0f, 0.0f);
        cui_widget_set_border_radius(window->close_button, 0.0f, 0.0f, 0.0f, 0.0f);
        cui_widget_set_padding(window->close_button, 8.0f, 17.0f, 8.0f, 17.0f);
        cui_widget_set_box_shadow(window->close_button, 0.0f, 0.0f, 0.0f);

        window->close_button->color_normal_background = CUI_COLOR_WINDOW_TITLEBAR_BACKGROUND;
        window->close_button->color_normal_icon = CUI_COLOR_WINDOW_TITLEBAR_ICON;

        cui_widget_append_child(window->titlebar, window->close_button);

        window->close_button->on_action = _cui_window_on_close_button;

        if (!(window->base.creation_flags & CUI_WINDOW_CREATION_FLAG_NOT_USER_RESIZABLE))
        {
            // maximize button
            window->maximize_button = cui_alloc_type(&window->arena, CuiWidget, CuiDefaultAllocationParams());

            cui_widget_init(window->maximize_button, CUI_WIDGET_TYPE_BUTTON);
            cui_widget_set_icon(window->maximize_button, CUI_ICON_WINDOWS_MAXIMIZE);
            cui_widget_set_border_width(window->maximize_button, 0.0f, 0.0f, 0.0f, 0.0f);
            cui_widget_set_border_radius(window->maximize_button, 0.0f, 0.0f, 0.0f, 0.0f);
            cui_widget_set_padding(window->maximize_button, 8.0f, 17.0f, 8.0f, 17.0f);
            cui_widget_set_box_shadow(window->maximize_button, 0.0f, 0.0f, 0.0f);

            window->maximize_button->color_normal_background = CUI_COLOR_WINDOW_TITLEBAR_BACKGROUND;
            window->maximize_button->color_normal_icon = CUI_COLOR_WINDOW_TITLEBAR_ICON;

            cui_widget_append_child(window->titlebar, window->maximize_button);

            window->maximize_button->on_action = _cui_window_on_maximize_button;
        }

        // minimize button
        window->minimize_button = cui_alloc_type(&window->arena, CuiWidget, CuiDefaultAllocationParams());

        cui_widget_init(window->minimize_button, CUI_WIDGET_TYPE_BUTTON);
        cui_widget_set_icon(window->minimize_button, CUI_ICON_WINDOWS_MINIMIZE);
        cui_widget_set_border_width(window->minimize_button, 0.0f, 0.0f, 0.0f, 0.0f);
        cui_widget_set_border_radius(window->minimize_button, 0.0f, 0.0f, 0.0f, 0.0f);
        cui_widget_set_padding(window->minimize_button, 8.0f, 17.0f, 8.0f, 17.0f);
        cui_widget_set_box_shadow(window->minimize_button, 0.0f, 0.0f, 0.0f);

        window->minimize_button->color_normal_background = CUI_COLOR_WINDOW_TITLEBAR_BACKGROUND;
        window->minimize_button->color_normal_icon = CUI_COLOR_WINDOW_TITLEBAR_ICON;

        cui_widget_append_child(window->titlebar, window->minimize_button);

        window->minimize_button->on_action = _cui_window_on_minimize_button;

        // window title
        window->title = cui_alloc_type(&window->arena, CuiWidget, CuiDefaultAllocationParams());

        cui_widget_init(window->title, CUI_WIDGET_TYPE_LABEL);
        cui_widget_set_label(window->title, CuiStringLiteral(""));
        cui_widget_set_x_axis_gravity(window->title, CUI_GRAVITY_START);
        cui_widget_set_y_axis_gravity(window->title, CUI_GRAVITY_CENTER);
        cui_widget_set_padding(window->title, 0.0f, 0.0f, 0.0f, 8.0f);
        cui_widget_append_child(window->titlebar, window->title);

        CuiWidget *dummy_user_root_widget = cui_alloc_type(&window->arena, CuiWidget, CuiDefaultAllocationParams());
        cui_widget_init(dummy_user_root_widget, CUI_WIDGET_TYPE_BOX);

        cui_widget_append_child(root_widget, dummy_user_root_widget);

        window->base.user_root_widget = dummy_user_root_widget;

        CuiRect rect = cui_make_rect(0, 0, window->width, window->height);

        cui_widget_set_window(root_widget, window);
        cui_widget_set_ui_scale(root_widget, window->base.ui_scale);
        cui_widget_layout(root_widget, rect);

        window->base.platform_root_widget = root_widget;

        // all widget have to be allocated at that point
        window->title_temp_memory = cui_begin_temporary_memory(&window->arena);
    }

    int window_style = 0;

    if (window->use_custom_decoration)
    {
        window_style = WS_SYSMENU | WS_MINIMIZEBOX;
    }
    else
    {
        window_style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    }

    if (!(window->base.creation_flags & CUI_WINDOW_CREATION_FLAG_NOT_USER_RESIZABLE))
    {
        window_style |= WS_THICKFRAME | WS_MAXIMIZEBOX;
    }

    // TODO: WS_EX_NOREDIRECTIONBITMAP for direct3d11 ??? This breaks the software rendering.
    //       Maybe that is because there is only one framebuffer used. Change to a swapchain like on linux.
    window->window_handle = CreateWindowEx(WS_EX_APPWINDOW, _cui_context.window_class_name, L"Window", window_style,
                                           CW_USEDEFAULT, CW_USEDEFAULT, window->width, window->height, 0, 0, GetModuleHandle(0), window);

    if (!window->window_handle)
    {
        _cui_window_destroy(window);
        return 0;
    }

    bool renderer_initialized = false;

#if CUI_RENDERER_DIRECT3D11_ENABLED

    if (!renderer_initialized)
    {
        renderer_initialized = _cui_initialize_direct3d11(window);
    }

#endif

#if CUI_RENDERER_SOFTWARE_ENABLED

    if (!renderer_initialized)
    {
        window->base.renderer = _cui_renderer_software_create();
        renderer_initialized = true;
    }

#endif

    if (!renderer_initialized)
    {
        cui_arena_deallocate(&window->arena);
        _cui_remove_window(window);
        return 0;
    }

    if (_cui_context.GetDpiForWindow)
    {
        window->dpi = _cui_context.GetDpiForWindow(window->window_handle);
        float ui_scale = (float) window->dpi / 96.0f;

        _cui_window_set_ui_scale(window, ui_scale);
    }

    if (window->use_custom_decoration)
    {
        SIZE titlebar_size = { 0 };
        HTHEME theme = OpenThemeData(window->window_handle, L"WINDOW");
        GetThemePartSize(theme, 0, WP_CAPTION, CS_ACTIVE, 0, TS_TRUE, &titlebar_size);
        CloseThemeData(theme);

        window->titlebar_height = (float) titlebar_size.cy;

        cui_widget_set_preferred_size(window->titlebar, 0.0f, window->titlebar_height);

        CuiRect rect = cui_make_rect(0, 0, window->width, window->height);
        cui_widget_layout(window->base.platform_root_widget, rect);
    }

    return window;
}

void
cui_window_set_title(CuiWindow *window, CuiString title)
{
    if (window->use_custom_decoration)
    {
        cui_end_temporary_memory(window->title_temp_memory);
        window->title_temp_memory = cui_begin_temporary_memory(&window->arena);

        cui_widget_set_label(window->title, cui_copy_string(&window->arena, title));
    }

    // We call SetWindowText even if the system title bar isn't used.
    // That's because that string is used elsewhere. For example in
    // the task manager and in the task bar preview.

    CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(&window->base.temporary_memory);

    CuiString utf16_str = cui_utf8_to_utf16le(&window->base.temporary_memory, title);

    SetWindowText(window->window_handle, (LPCWSTR) cui_to_c_string(&window->base.temporary_memory, utf16_str));

    cui_end_temporary_memory(temp_memory);
}

void
cui_window_resize(CuiWindow *window, int32_t width, int32_t height)
{
    RECT window_rect, client_rect;
    GetWindowRect(window->window_handle, &window_rect);
    GetClientRect(window->window_handle, &client_rect);

    int32_t offset_x = (window_rect.right - window_rect.left) -
                       (client_rect.right - client_rect.left);
    int32_t offset_y = (window_rect.bottom - window_rect.top) -
                       (client_rect.bottom - client_rect.top);

    if (window->use_custom_decoration)
    {
        CuiPoint titlebar_size = cui_widget_get_preferred_size(window->titlebar);
        offset_y += titlebar_size.y;
    }

    SetWindowPos(window->window_handle, 0, 0, 0, width + offset_x, height + offset_y,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOOWNERZORDER);
}

void
cui_window_show(CuiWindow *window)
{
    ShowWindow(window->window_handle, SW_SHOW);
    window->base.needs_redraw = true;
}

float
cui_window_get_titlebar_height(CuiWindow *window)
{
    return window->titlebar_height;
}

void
cui_window_set_root_widget(CuiWindow *window, CuiWidget *widget)
{
    CuiAssert(widget);

    if (window->use_custom_decoration)
    {
        CuiAssert(window->base.user_root_widget);
        cui_widget_replace_child(window->base.platform_root_widget, window->base.user_root_widget, widget);
    }
    else
    {
        window->base.platform_root_widget = widget;
    }

    window->base.user_root_widget = widget;

    CuiRect rect = cui_make_rect(0, 0, window->width, window->height);

    cui_widget_set_window(window->base.user_root_widget, window);
    cui_widget_set_ui_scale(window->base.user_root_widget, window->base.ui_scale);
    cui_widget_layout(window->base.platform_root_widget, rect);
}

void
cui_window_set_cursor(CuiWindow *window, CuiCursorType cursor_type)
{
    if (window->base.cursor == cursor_type)
    {
        return;
    }

    window->base.cursor = cursor_type;

    // TODO:
}

void
cui_step(void)
{
    DWORD ret = MsgWaitForMultipleObjects(1, &_cui_context.signal_event, FALSE, INFINITE, QS_ALLEVENTS);

    if (ret == WAIT_OBJECT_0)
    {
        // OutputDebugString(L"Signal\n");
        ResetEvent(_cui_context.signal_event);

        if (_cui_context.common.signal_callback)
        {
            _cui_context.common.signal_callback();
        }
    }

    MSG message;

    while (PeekMessage(&message, 0, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&message);
        DispatchMessage(&message);
    }

    for (uint32_t window_index = 0;
         window_index < _cui_context.common.window_count; window_index += 1)
    {
        CuiWindow *window = _cui_context.common.windows[window_index];

        window->base.width = window->width;
        window->base.height = window->height;

        CuiWindowFrameResult window_frame_result = { 0 };
        CuiFramebuffer *framebuffer = _cui_window_frame_routine(window, window->base.events, &window_frame_result);

        if (framebuffer)
        {
            switch (window->base.renderer->type)
            {
                case CUI_RENDERER_TYPE_SOFTWARE:
                {
#if CUI_RENDERER_SOFTWARE_ENABLED
                    HDC device_context = GetDC(window->window_handle);

                    BITMAPINFO bitmap;
                    bitmap.bmiHeader.biSize        = sizeof(bitmap.bmiHeader);
                    bitmap.bmiHeader.biWidth       = window->renderer.software.backbuffer.stride / sizeof(uint32_t);
                    bitmap.bmiHeader.biHeight      = -(LONG) window->renderer.software.backbuffer.height;
                    bitmap.bmiHeader.biPlanes      = 1;
                    bitmap.bmiHeader.biBitCount    = 32;
                    bitmap.bmiHeader.biCompression = BI_RGB;

                    StretchDIBits(device_context, 0, 0, window->renderer.software.backbuffer.width, window->renderer.software.backbuffer.height,
                                  0, 0, window->renderer.software.backbuffer.width, window->renderer.software.backbuffer.height,
                                  window->renderer.software.backbuffer.pixels, &bitmap, DIB_RGB_COLORS, SRCCOPY);

                    ReleaseDC(window->window_handle, device_context);
#else
                    CuiAssert(!"CUI_RENDERER_TYPE_SOFTWARE not enabled.");
#endif
                } break;

                case CUI_RENDERER_TYPE_OPENGLES2:
                {
                    CuiAssert(!"CUI_RENDERER_TYPE_OPENGLES2 not supported.");
                } break;

                case CUI_RENDERER_TYPE_METAL:
                {
                    CuiAssert(!"CUI_RENDERER_TYPE_METAL not supported.");
                } break;

                case CUI_RENDERER_TYPE_DIRECT3D11:
                {
#if CUI_RENDERER_DIRECT3D11_ENABLED
                    // TODO: check for errors
                    IDXGISwapChain1_Present(window->renderer.direct3d11.dxgi_swapchain, 0, DXGI_PRESENT_RESTART);
                    IDXGISwapChain1_Present(window->renderer.direct3d11.dxgi_swapchain, 1, DXGI_PRESENT_DO_NOT_SEQUENCE);
#else
                    CuiAssert(!"CUI_RENDERER_TYPE_DIRECT3D11 not enabled.");
#endif
                } break;
            }
        }

        if (window_frame_result.window_frame_actions & CUI_WINDOW_FRAME_ACTION_CLOSE)
        {
            _cui_window_close(window);
            continue;
        }

        if (window_frame_result.window_frame_actions & CUI_WINDOW_FRAME_ACTION_SET_FULLSCREEN)
        {
            _cui_window_set_fullscreen(window, window_frame_result.should_be_fullscreen);
        }
    }
}
