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
    float titlebar_height;

    CuiFontId font_id;

    AppKitView *appkit_view;
    AppKitWindow *appkit_window;
    NSToolbar *appkit_toolbar;

    NSTrackingArea *tracking_area;

    CuiWidget *titlebar;

    CuiFramebuffer framebuffer;

    union
    {

#if CUI_RENDERER_SOFTWARE_ENABLED

        struct
        {
            CuiBitmap backbuffer; // TODO: use the framebuffer bitmap
            int64_t backbuffer_memory_size;

            CGContextRef backbuffer_context;
        } software;

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
