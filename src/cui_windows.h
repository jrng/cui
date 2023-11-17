struct CuiWindow
{
    CuiWindowBase base;

    CuiArena arena;
    CuiTemporaryMemory title_temp_memory;

    int32_t width;
    int32_t height;

    float titlebar_height;

    UINT dpi;

    HWND window_handle;

    WINDOWPLACEMENT windowed_placement;

    bool alt_is_down;
    bool ctrl_is_down;
    bool shift_is_down;

    bool is_tracking_mouse;
    bool is_tracking_ncmouse;

    bool use_custom_decoration;

#if CUI_FRAMEBUFFER_SCREENSHOT_ENABLED
    bool take_screenshot;
#endif

    CuiFontId font_id;

    CuiWidget *titlebar;
    CuiWidget *minimize_button;
    CuiWidget *maximize_button;
    CuiWidget *close_button;
    CuiWidget *title;

    union
    {

#if CUI_RENDERER_SOFTWARE_ENABLED

        struct
        {
            CuiRendererSoftware *renderer_software;

            CuiBitmap backbuffer;
            int64_t backbuffer_memory_size;
        } software;

#endif

#if CUI_RENDERER_DIRECT3D11_ENABLED

        struct
        {
            CuiRendererDirect3D11 *renderer_direct3d11;

            ID3D11Device *d3d11_device;
            ID3D11DeviceContext *d3d11_device_context;
            IDXGISwapChain1 *dxgi_swapchain;
        } direct3d11;

#endif

    } renderer;
};

typedef struct CuiContext
{
    CuiContextCommon common;

    HANDLE signal_event;

    LPCWSTR window_class_name;

    UINT (*GetDpiForSystem)();
    UINT (*GetDpiForWindow)(HWND window_handle);
    int (*GetSystemMetricsForDpi)(int index, UINT dpi);

    HCURSOR cursor_arrow;
    HCURSOR cursor_text;
    HCURSOR cursor_hand;
    HCURSOR cursor_move_all;
    HCURSOR cursor_move_left_right;
    HCURSOR cursor_move_top_down;
} CuiContext;
