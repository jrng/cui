#include <AppKit/AppKit.h>

@interface AppKitApplicationDelegate: NSObject<NSApplicationDelegate>
@end

@interface AppKitWindow: NSWindow
@end

@interface AppKitWindowDelegate: NSObject<NSWindowDelegate>
{
    CuiWindow *cui_window;
}
@end

@interface AppKitView: NSView<NSTextInputClient>
{
    CuiWindow *cui_window;
}

- (void) clearCuiWindow;

@end

struct CuiWindow
{
    CuiWindowBase base;

    CuiArena arena;

    int32_t width;
    int32_t height;

    int32_t backbuffer_scale;

    CuiFontId font_id;

#if CUI_FRAMEBUFFER_SCREENSHOT_ENABLED
    bool take_screenshot;
#endif

    AppKitView *appkit_view;
    AppKitWindow *appkit_window;

    NSTrackingArea *tracking_area;

    CuiWidget *titlebar;

    union
    {

#if CUI_RENDERER_SOFTWARE_ENABLED

        struct
        {
            CuiRendererSoftware *renderer_software;

            CuiBitmap backbuffer;
            int64_t backbuffer_memory_size;

            CGContextRef backbuffer_context;
        } software;

#endif

#if CUI_RENDERER_METAL_ENABLED

        struct
        {
            CuiRendererMetal *renderer_metal;
        } metal;

#endif

    } renderer;
};

typedef struct CuiContext
{
    CuiContextCommon common;

    NSEvent *signal_event;

    NSScreen *main_screen;
    int32_t desktop_scale;

    CuiArena files_to_open_arena;
    CuiString *files_to_open;
} CuiContext;
