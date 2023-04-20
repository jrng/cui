#include <libproc.h>
#include <sys/sysctl.h>
#include <Carbon/Carbon.h>

@implementation AppKitApplicationDelegate

- (void) applicationWillFinishLaunching: (NSNotification *) notification
{
    NSDictionary* bundle_info = [[NSBundle mainBundle] infoDictionary];
    NSString *app_name = bundle_info[@"CFBundleName"];

    if (!app_name)
    {
        app_name = @"App";
    }

    NSMenu *menubar = [[NSMenu alloc] init];
    [NSApp setMainMenu: menubar];

    NSMenuItem *app_menu_item = [[NSMenuItem alloc] init];
    [menubar addItem: app_menu_item];

    NSMenu *app_menu = [[NSMenu alloc] init];
    [app_menu_item setSubmenu: app_menu];
    [app_menu_item release];

    [app_menu addItemWithTitle: [NSString stringWithFormat:@"About %@", app_name]
                        action: @selector(orderFrontStandardAboutPanel:)
                 keyEquivalent: @""];

    [app_menu addItem: [NSMenuItem separatorItem]];

    NSMenu* services_menu = [[NSMenu alloc] init];
    [NSApp setServicesMenu: services_menu];

    [[app_menu addItemWithTitle: @"Services"
                         action: 0
                  keyEquivalent: @""] setSubmenu: services_menu];

    [services_menu release];

    [app_menu addItem: [NSMenuItem separatorItem]];

    [app_menu addItemWithTitle: [NSString stringWithFormat: @"Hide %@", app_name]
                        action: @selector(hide:)
                 keyEquivalent: @"h"];
    [[app_menu addItemWithTitle: @"Hide Others"
                         action: @selector(hideOtherApplications:)
                  keyEquivalent: @"h"] setKeyEquivalentModifierMask: NSEventModifierFlagOption |
                                                                     NSEventModifierFlagCommand];
    [app_menu addItemWithTitle: @"Show All"
                        action: @selector(unhideAllApplications:)
                 keyEquivalent: @""];

    [app_menu addItem: [NSMenuItem separatorItem]];

    [app_menu addItemWithTitle: [NSString stringWithFormat: @"Quit %@", app_name]
                        action: @selector(terminate:)
                 keyEquivalent: @"q"];

    [app_menu release];

    NSMenuItem* window_menu_item = [menubar addItemWithTitle: @""
                                                      action: 0
                                               keyEquivalent: @""];
    [menubar release];

    NSMenu* window_menu = [[NSMenu alloc] initWithTitle: @"Window"];
    [NSApp setWindowsMenu:window_menu];
    [window_menu_item setSubmenu: window_menu];

    [window_menu addItemWithTitle: @"Minimize"
                           action: @selector(performMiniaturize:)
                    keyEquivalent: @"m"];
    [window_menu addItemWithTitle: @"Zoom"
                           action: @selector(performZoom:)
                    keyEquivalent: @""];

    [window_menu addItem: [NSMenuItem separatorItem]];

    [window_menu addItemWithTitle: @"Bring All to Front"
                           action: @selector(arrangeInFront:)
                    keyEquivalent: @""];

    [window_menu addItem: [NSMenuItem separatorItem]];
    [[window_menu addItemWithTitle: @"Enter Full Screen"
                            action: @selector(toggleFullScreen:)
                     keyEquivalent: @"f"] setKeyEquivalentModifierMask: NSEventModifierFlagControl |
                                                                        NSEventModifierFlagCommand];
}

- (void) applicationDidFinishLaunching: (NSNotification *) notification
{
    @autoreleasepool
    {
        NSEvent* event = [NSEvent otherEventWithType: NSEventTypeApplicationDefined
                                            location: NSMakePoint(0, 0)
                                       modifierFlags: 0
                                           timestamp: 0
                                        windowNumber: 0
                                             context: nil
                                             subtype: 0
                                               data1: 0
                                               data2: 0];
        [NSApp postEvent: event atStart: YES];
        [NSApp stop:nil];
    }
}

- (NSApplicationTerminateReply) applicationShouldTerminate: (NSApplication *) sender
{
    for (uint32_t window_index = 0;
         window_index < _cui_context.common.window_count; window_index += 1)
    {
        CuiWindow *window = _cui_context.common.windows[window_index];
        cui_window_close(window);
    }

    return NSTerminateCancel;
}

- (BOOL) application: (NSApplication *) sender
            openFile: (NSString *) filename
{
    cui_arena_clear(&_cui_context.files_to_open_arena);

    _cui_context.files_to_open = 0;
    cui_array_init(_cui_context.files_to_open, 4, &_cui_context.files_to_open_arena);

    const char *filename_c = [filename fileSystemRepresentation];

    if (filename_c)
    {
        CuiString name = cui_copy_string(&_cui_context.files_to_open_arena, CuiCString(filename_c));
        *cui_array_append(_cui_context.files_to_open) = name;

        // TODO: free filename_c
    }

    return YES;
}

@end

@implementation AppKitWindow

- (BOOL) canBecomeKeyWindow
{
    return YES;
}

- (BOOL) canBecomeMainWindow
{
    return YES;
}

- (BOOL) acceptsMouseMovedEvents
{
    return NO;
}

@end

#if CUI_RENDERER_SOFTWARE_ENABLED

static void
_cui_window_resize_backbuffer(CuiWindow *window, int32_t width, int32_t height)
{
    CuiAssert(window->base.renderer_type == CUI_RENDERER_TYPE_SOFTWARE);

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

    CGContextRelease(window->renderer.software.backbuffer_context);

    CGColorSpaceRef colorspace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    window->renderer.software.backbuffer_context = CGBitmapContextCreate(window->renderer.software.backbuffer.pixels,
                                                                         window->renderer.software.backbuffer.width,
                                                                         window->renderer.software.backbuffer.height, 8,
                                                                         window->renderer.software.backbuffer.stride,
                                                                         colorspace,
                                                                         kCGImageAlphaNoneSkipFirst |
                                                                         kCGBitmapByteOrder32Little);
    CGColorSpaceRelease(colorspace);
}

#endif

static void
_cui_window_draw(CuiWindow *window)
{
    CuiCommandBuffer *command_buffer = 0;

    switch (window->base.renderer_type)
    {
        case CUI_RENDERER_TYPE_SOFTWARE:
        {
#if CUI_RENDERER_SOFTWARE_ENABLED
            command_buffer = _cui_software_renderer_begin_command_buffer(window->renderer.software.renderer_software);
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
#if CUI_RENDERER_METAL_ENABLED
            command_buffer = _cui_metal_renderer_begin_command_buffer(window->renderer.metal.renderer_metal);
#else
            CuiAssert(!"CUI_RENDERER_TYPE_METAL not enabled.");
#endif
        } break;

        case CUI_RENDERER_TYPE_DIRECT3D11:
        {
            CuiAssert(!"CUI_RENDERER_TYPE_DIRECT3D11 not supported.");
        } break;
    }

    if (window->base.platform_root_widget)
    {
        if (!window->base.glyph_cache.allocated)
        {
            _cui_glyph_cache_initialize(&window->base.glyph_cache, command_buffer,
                                        cui_window_allocate_texture_id(window));
        }
        else
        {
            _cui_glyph_cache_maybe_reset(&window->base.glyph_cache, command_buffer);
        }

        CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(&window->base.temporary_memory);

        CuiRect window_rect = cui_make_rect(0, 0, window->width, window->height);

        CuiGraphicsContext ctx;
        ctx.clip_rect_offset = 0;
        ctx.clip_rect = window_rect;
        ctx.window_rect = window_rect;
        ctx.command_buffer = command_buffer;
        ctx.glyph_cache = &window->base.glyph_cache;
        ctx.temporary_memory = &window->base.temporary_memory;
        ctx.font_manager = &window->base.font_manager;

        const CuiColorTheme *color_theme = &cui_color_theme_default_dark;

        if (window->base.color_theme)
        {
            color_theme = window->base.color_theme;
        }

        cui_widget_draw(window->base.platform_root_widget, &ctx, color_theme);

#if 0
        int32_t texture_width = ctx.glyph_cache->texture.width;
        int32_t texture_height = ctx.glyph_cache->texture.height;
        _cui_push_textured_rect(ctx.command_buffer, cui_make_rect(10, 10, 10 + texture_width, 10 + texture_height),
                                cui_make_rect(0, 0, 0, 0), cui_make_color(0.0f, 0.0f, 0.0f, 1.0f), ctx.glyph_cache->texture_id, 0);
        _cui_push_textured_rect(ctx.command_buffer, cui_make_rect(10, 10, 10 + texture_width, 10 + texture_height),
                                cui_make_rect(0, 0, texture_width, texture_height), cui_make_color(1.0f, 1.0f, 1.0f, 1.0f),
                                ctx.glyph_cache->texture_id, 0);
#endif

        cui_end_temporary_memory(temp_memory);
    }

#if CUI_FRAMEBUFFER_SCREENSHOT_ENABLED
    CuiArena screenshot_arena;
    CuiBitmap screenshot_bitmap;

    if (window->take_screenshot)
    {
        cui_arena_allocate(&screenshot_arena, CuiMiB(32));
    }
#endif

    switch (window->base.renderer_type)
    {
        case CUI_RENDERER_TYPE_SOFTWARE:
        {
#if CUI_RENDERER_SOFTWARE_ENABLED
            if ((window->renderer.software.backbuffer.width != window->width) ||
                (window->renderer.software.backbuffer.height != window->height))
            {
                _cui_window_resize_backbuffer(window, window->width, window->height);
            }

            _cui_software_renderer_render(window->renderer.software.renderer_software,
                                          &window->renderer.software.backbuffer,
                                          command_buffer, CuiHexColor(0xFF000000));

#  if CUI_FRAMEBUFFER_SCREENSHOT_ENABLED
            screenshot_bitmap = window->renderer.software.backbuffer;
#  endif
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
#if CUI_RENDERER_METAL_ENABLED
            CGSize drawable_size = CGSizeMake((CGFloat) window->width, (CGFloat) window->height);
            CAMetalLayer *metal_layer = (CAMetalLayer *) window->appkit_view.layer;

            if ((metal_layer.drawableSize.width != drawable_size.width) ||
                (metal_layer.drawableSize.height != drawable_size.height))
            {
                metal_layer.drawableSize = drawable_size;
            }

            id<CAMetalDrawable> drawable = [metal_layer nextDrawable];

            if (drawable)
            {
                _cui_metal_renderer_render(window->renderer.metal.renderer_metal, drawable,
                                           window->width, window->height, command_buffer,
                                           CuiHexColor(0xFF000000));

#  if CUI_FRAMEBUFFER_SCREENSHOT_ENABLED
                if (window->take_screenshot)
                {
                    screenshot_bitmap.width  = window->width;
                    screenshot_bitmap.height = window->height;
                    screenshot_bitmap.stride = screenshot_bitmap.width * 4;
                    screenshot_bitmap.pixels = cui_alloc(&screenshot_arena, screenshot_bitmap.stride * screenshot_bitmap.height,
                                                         CuiDefaultAllocationParams());

                    [drawable.texture getBytes: screenshot_bitmap.pixels
                                   bytesPerRow: screenshot_bitmap.stride
                                    fromRegion: MTLRegionMake2D(0, 0, window->width, window->height)
                                   mipmapLevel: 0];
                }
#  endif

                [drawable present];
            }
#else
            CuiAssert(!"CUI_RENDERER_TYPE_METAL not enabled.");
#endif
        } break;

        case CUI_RENDERER_TYPE_DIRECT3D11:
        {
            CuiAssert(!"CUI_RENDERER_TYPE_DIRECT3D11 not supported.");
        } break;
    }

#if CUI_FRAMEBUFFER_SCREENSHOT_ENABLED

    if (window->take_screenshot)
    {
        CuiString bmp_data = cui_image_encode_bmp(screenshot_bitmap, true, true, &screenshot_arena);

        CuiFile *screenshot_file = cui_platform_file_create(&screenshot_arena, CuiStringLiteral("screenshot_cui.bmp"));

        if (screenshot_file)
        {
            cui_platform_file_truncate(screenshot_file, bmp_data.count);
            cui_platform_file_write(screenshot_file, bmp_data.data, 0, bmp_data.count);
            cui_platform_file_close(screenshot_file);
        }

        cui_arena_deallocate(&screenshot_arena);

        window->take_screenshot = false;
    }

#endif

}

@implementation AppKitWindowDelegate

- (instancetype) initWithCuiWindow: (CuiWindow *) window
{
    self = [super init];

    if (self)
    {
        cui_window = window;
        [cui_window->appkit_window setDelegate: self];
    }

    return self;
}

- (BOOL) windowShouldClose: (NSWindow *) sender
{
    return YES;
}

- (void) windowWillClose: (NSNotification *) sender
{
    [cui_window->appkit_view clearCuiWindow];
    cui_window_destroy(cui_window);
    cui_window = 0;
}

- (void) windowWillEnterFullScreen: (NSNotification *) sender
{
    if (cui_window->base.creation_flags & CUI_WINDOW_CREATION_FLAG_MACOS_UNIFIED_TITLEBAR)
    {
        cui_window->appkit_window.toolbar = nil;
    }

    if (!(cui_window->base.creation_flags & CUI_WINDOW_CREATION_FLAG_PREFER_SYSTEM_DECORATION) &&
        !(cui_window->base.creation_flags & CUI_WINDOW_CREATION_FLAG_MACOS_UNIFIED_TITLEBAR))
    {
        cui_widget_set_preferred_size(cui_window->titlebar, 0.0f, 0.0f);
    }
}

- (void) windowDidEnterFullScreen: (NSNotification *) sender
{
    cui_window->base.state |= CUI_WINDOW_STATE_FULLSCREEN;
}

- (void) windowWillExitFullScreen: (NSNotification *) sender
{
    if (cui_window->base.creation_flags & CUI_WINDOW_CREATION_FLAG_MACOS_UNIFIED_TITLEBAR)
    {
        NSButton *close_button    = [cui_window->appkit_window standardWindowButton: NSWindowCloseButton];
        NSButton *minimize_button = [cui_window->appkit_window standardWindowButton: NSWindowMiniaturizeButton];
        NSButton *maximize_button = [cui_window->appkit_window standardWindowButton: NSWindowZoomButton];

        CuiAssert(close_button);
        CuiAssert(minimize_button);
        CuiAssert(maximize_button);

        close_button.hidden    = YES;
        minimize_button.hidden = YES;
        maximize_button.hidden = YES;
    }

    if (!(cui_window->base.creation_flags & CUI_WINDOW_CREATION_FLAG_PREFER_SYSTEM_DECORATION) &&
        !(cui_window->base.creation_flags & CUI_WINDOW_CREATION_FLAG_MACOS_UNIFIED_TITLEBAR))
    {
        cui_widget_set_preferred_size(cui_window->titlebar, 0.0f, cui_window->titlebar_height);
    }
}

- (void) windowDidExitFullScreen: (NSNotification *) sender
{
    cui_window->base.state &= ~CUI_WINDOW_STATE_FULLSCREEN;

    if (cui_window->base.creation_flags & CUI_WINDOW_CREATION_FLAG_MACOS_UNIFIED_TITLEBAR)
    {
        NSButton *close_button    = [cui_window->appkit_window standardWindowButton: NSWindowCloseButton];
        NSButton *minimize_button = [cui_window->appkit_window standardWindowButton: NSWindowMiniaturizeButton];
        NSButton *maximize_button = [cui_window->appkit_window standardWindowButton: NSWindowZoomButton];

        CuiAssert(close_button);
        CuiAssert(minimize_button);
        CuiAssert(maximize_button);

        close_button.hidden    = NO;
        minimize_button.hidden = NO;
        maximize_button.hidden = NO;

        cui_window->appkit_window.toolbar = cui_window->appkit_toolbar;
    }
}

- (void) windowDidResize: (NSNotification *) notification
{
    [self updateWindowSize];
}

- (void) updateWindowSize
{
    AppKitWindow *appkit_window = cui_window->appkit_window;

    NSRect window_rect = appkit_window.contentView.frame;
    window_rect = [appkit_window convertRectToBacking: window_rect];

    int32_t new_width = lround(window_rect.size.width);
    int32_t new_height = lround(window_rect.size.height);

    if ((cui_window->width != new_width) || (cui_window->height != new_height))
    {
        cui_window->width = new_width;
        cui_window->height = new_height;

        if (cui_window->base.platform_root_widget)
        {
            CuiRect rect = cui_make_rect(0, 0, cui_window->width, cui_window->height);
            cui_widget_layout(cui_window->base.platform_root_widget, rect);
        }

        _cui_window_draw(cui_window);

        switch (cui_window->base.renderer_type)
        {
            case CUI_RENDERER_TYPE_SOFTWARE:
            {
#if CUI_RENDERER_SOFTWARE_ENABLED
                [cui_window->appkit_view setNeedsDisplay: YES];
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
#if !CUI_RENDERER_METAL_ENABLED
                CuiAssert(!"CUI_RENDERER_TYPE_METAL not enabled.");
#endif
            } break;

            case CUI_RENDERER_TYPE_DIRECT3D11:
            {
                CuiAssert(!"CUI_RENDERER_TYPE_DIRECT3D11 not supported.");
            } break;
        }
    }
}

@end

@implementation AppKitView

- (instancetype) initWithFrame: (NSRect) frame_rect
                     cuiWindow: (CuiWindow *) window
#if CUI_RENDERER_METAL_ENABLED
                   metalDevice: (id<MTLDevice>) metal_device
#endif
{
    self = [super initWithFrame: frame_rect];

    if (self)
    {
        cui_window = window;

        self.wantsLayer = YES;

#if CUI_RENDERER_METAL_ENABLED

        if (cui_window->base.renderer_type == CUI_RENDERER_TYPE_METAL)
        {
            self.layer = [CAMetalLayer layer];

            CAMetalLayer *metal_layer = (CAMetalLayer *) self.layer;
            metal_layer.device = metal_device;
            metal_layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
            metal_layer.presentsWithTransaction = YES;
        }

#endif

        cui_window->tracking_area = [[NSTrackingArea alloc] initWithRect: self.bounds
                                                                 options: NSTrackingMouseEnteredAndExited |
                                                                          NSTrackingMouseMoved |
                                                                          NSTrackingActiveAlways
                                                                   owner: self
                                                                userInfo: nil];
        [self addTrackingArea: cui_window->tracking_area];
    }

    return self;
}

- (void) clearCuiWindow
{
    cui_window = 0;
}

- (BOOL) wantsUpdateLayer
{
    return YES;
}

- (void) updateLayer
{
#if CUI_RENDERER_SOFTWARE_ENABLED
    CGImageRef image = CGBitmapContextCreateImage(cui_window->renderer.software.backbuffer_context);
    self.layer.contents = (id) image;
    CGImageRelease(image);
#endif
}

- (BOOL) acceptsFirstResponder
{
    return YES;
}

- (void) updateTrackingAreas
{
    [super updateTrackingAreas];

    [self removeTrackingArea: cui_window->tracking_area];
    [cui_window->tracking_area release];

    cui_window->tracking_area = [[NSTrackingArea alloc] initWithRect: self.bounds
                                                             options: NSTrackingMouseEnteredAndExited |
                                                                      NSTrackingMouseMoved |
                                                                      NSTrackingActiveAlways
                                                               owner: self
                                                            userInfo: nil];
    [self addTrackingArea: cui_window->tracking_area];
}

- (void) mouseEntered: (NSEvent *) ev
{
    NSPoint point_in_view = [self convertPoint: [ev locationInWindow]
                                      fromView: nil];
    NSPoint point_in_backing = [self convertPointToBacking: point_in_view];

    // printf("mouse entered (%f, %f)\n", point_in_backing.x, (double) cui_window->height - point_in_backing.y);

    cui_window->base.event.mouse.x = lroundf(point_in_backing.x);
    cui_window->base.event.mouse.y = cui_window->height - lroundf(point_in_backing.y);

    cui_window_handle_event(cui_window, CUI_EVENT_TYPE_MOUSE_MOVE);
}

- (void) mouseExited: (NSEvent *) ev
{
    // printf("mouse exited\n");
    cui_window_handle_event(cui_window, CUI_EVENT_TYPE_MOUSE_LEAVE);
}

- (void) mouseMoved: (NSEvent *) ev
{
    NSPoint point_in_view = [self convertPoint: [ev locationInWindow]
                                      fromView: nil];
    NSPoint point_in_backing = [self convertPointToBacking: point_in_view];

    // printf("mouse moved (%f, %f)\n", point_in_backing.x, (double) cui_window->height - point_in_backing.y);

    cui_window->base.event.mouse.x = lroundf(point_in_backing.x);
    cui_window->base.event.mouse.y = cui_window->height - lroundf(point_in_backing.y);

    cui_window_handle_event(cui_window, CUI_EVENT_TYPE_MOUSE_MOVE);
}

- (void) mouseDragged: (NSEvent *) ev
{
    NSPoint point_in_view = [self convertPoint: [ev locationInWindow]
                                      fromView: nil];
    NSPoint point_in_backing = [self convertPointToBacking: point_in_view];

    // printf("mouse dragged (%f, %f)\n", point_in_backing.x, (double) cui_window->height - point_in_backing.y);

    cui_window->base.event.mouse.x = lroundf(point_in_backing.x);
    cui_window->base.event.mouse.y = cui_window->height - lroundf(point_in_backing.y);

    cui_window_handle_event(cui_window, CUI_EVENT_TYPE_MOUSE_MOVE);
}

- (void) rightMouseDragged: (NSEvent *) ev
{
    NSPoint point_in_view = [self convertPoint: [ev locationInWindow]
                                      fromView: nil];
    NSPoint point_in_backing = [self convertPointToBacking: point_in_view];

    // printf("mouse dragged (%f, %f)\n", point_in_backing.x, (double) cui_window->height - point_in_backing.y);

    cui_window->base.event.mouse.x = lroundf(point_in_backing.x);
    cui_window->base.event.mouse.y = cui_window->height - lroundf(point_in_backing.y);

    cui_window_handle_event(cui_window, CUI_EVENT_TYPE_MOUSE_MOVE);
}

- (void) mouseDown: (NSEvent *) ev
{
    NSPoint point_in_view = [self convertPoint: [ev locationInWindow]
                                      fromView: nil];
    NSPoint point_in_backing = [self convertPointToBacking: point_in_view];

    // printf("mouse down (%f, %f)\n", point_in_backing.x, (double) cui_window->height - point_in_backing.y);

    bool mouse_is_in_titlebar = false;

    if (!(cui_window->base.creation_flags & CUI_WINDOW_CREATION_FLAG_PREFER_SYSTEM_DECORATION))
    {
        NSRect window_rect = cui_window->appkit_window.contentView.frame;
        NSRect content_rect = cui_window->appkit_window.contentLayoutRect;

        if (NSPointInRect(point_in_view, window_rect) && !NSPointInRect(point_in_view, content_rect))
        {
            mouse_is_in_titlebar = true;
        }
    }

    cui_window->base.event.mouse.x = lroundf(point_in_backing.x);
    cui_window->base.event.mouse.y = cui_window->height - lroundf(point_in_backing.y);

    if (([ev clickCount] % 2) == 0)
    {
        bool handled = cui_window_handle_event(cui_window, CUI_EVENT_TYPE_DOUBLE_CLICK);

        if (!(cui_window->base.creation_flags & CUI_WINDOW_CREATION_FLAG_NOT_USER_RESIZABLE) &&
            !(cui_window->base.creation_flags & CUI_WINDOW_CREATION_FLAG_PREFER_SYSTEM_DECORATION) &&
            mouse_is_in_titlebar && !handled)
        {
            [cui_window->appkit_window zoom: nil];
        }
    }
    else
    {
        bool handled = cui_window_handle_event(cui_window, CUI_EVENT_TYPE_LEFT_DOWN);

        if (!(cui_window->base.creation_flags & CUI_WINDOW_CREATION_FLAG_PREFER_SYSTEM_DECORATION) &&
            (cui_window->base.creation_flags & CUI_WINDOW_CREATION_FLAG_MACOS_UNIFIED_TITLEBAR) &&
            mouse_is_in_titlebar && !handled)
        {
            [cui_window->appkit_window performWindowDragWithEvent: ev];
        }
    }
}

- (void) mouseUp: (NSEvent *) ev
{
    NSPoint point_in_view = [self convertPoint: [ev locationInWindow]
                                      fromView: nil];
    NSPoint point_in_backing = [self convertPointToBacking: point_in_view];

    cui_window->base.event.mouse.x = lroundf(point_in_backing.x);
    cui_window->base.event.mouse.y = cui_window->height - lroundf(point_in_backing.y);

    cui_window_handle_event(cui_window, CUI_EVENT_TYPE_LEFT_UP);
}

- (void) rightMouseDown: (NSEvent *) ev
{
    NSPoint point_in_view = [self convertPoint: [ev locationInWindow]
                                      fromView: nil];
    NSPoint point_in_backing = [self convertPointToBacking: point_in_view];

    cui_window->base.event.mouse.x = lroundf(point_in_backing.x);
    cui_window->base.event.mouse.y = cui_window->height - lroundf(point_in_backing.y);

    cui_window_handle_event(cui_window, CUI_EVENT_TYPE_RIGHT_DOWN);
}

- (void) rightMouseUp: (NSEvent *) ev
{
    NSPoint point_in_view = [self convertPoint: [ev locationInWindow]
                                      fromView: nil];
    NSPoint point_in_backing = [self convertPointToBacking: point_in_view];

    cui_window->base.event.mouse.x = lroundf(point_in_backing.x);
    cui_window->base.event.mouse.y = cui_window->height - lroundf(point_in_backing.y);

    cui_window_handle_event(cui_window, CUI_EVENT_TYPE_RIGHT_UP);
}

- (void) scrollWheel: (NSEvent *) ev
{
    if (ev.hasPreciseScrollingDeltas)
    {
        cui_window->base.event.wheel.is_precise_scrolling = true;
        cui_window->base.event.wheel.dx = cui_window->backbuffer_scale * (float) ev.scrollingDeltaX;
        cui_window->base.event.wheel.dy = cui_window->backbuffer_scale * (float) ev.scrollingDeltaY;
    }
    else
    {
        cui_window->base.event.wheel.is_precise_scrolling = false;
        cui_window->base.event.wheel.dx = (float) ev.scrollingDeltaX;
        cui_window->base.event.wheel.dy = (float) ev.scrollingDeltaY;
    }

    cui_window_handle_event(cui_window, CUI_EVENT_TYPE_MOUSE_WHEEL);
}

- (void) keyDown: (NSEvent *) ev
{

#define _CUI_KEY_DOWN_EVENT(key_id)                                                                                 \
    cui_window->base.event.key.codepoint       = (key_id);                                                          \
    cui_window->base.event.key.alt_is_down     = (ev.modifierFlags & NSEventModifierFlagOption) ? true : false;     \
    cui_window->base.event.key.ctrl_is_down    = (ev.modifierFlags & NSEventModifierFlagControl) ? true : false;    \
    cui_window->base.event.key.shift_is_down   = (ev.modifierFlags & NSEventModifierFlagShift) ? true : false;      \
    cui_window->base.event.key.command_is_down = (ev.modifierFlags & NSEventModifierFlagCommand) ? true : false;    \
    cui_window_handle_event(cui_window, CUI_EVENT_TYPE_KEY_DOWN);

    switch (ev.keyCode)
    {
        case kVK_Delete:     { _CUI_KEY_DOWN_EVENT(CUI_KEY_BACKSPACE); } break;
        case kVK_Tab:        { _CUI_KEY_DOWN_EVENT(CUI_KEY_TAB);       } break;
        case kVK_Return:     { _CUI_KEY_DOWN_EVENT(CUI_KEY_ENTER);     } break;
        case kVK_Escape:     { _CUI_KEY_DOWN_EVENT(CUI_KEY_ESCAPE);    } break;
        case kVK_UpArrow:    { _CUI_KEY_DOWN_EVENT(CUI_KEY_UP);        } break;
        case kVK_DownArrow:  { _CUI_KEY_DOWN_EVENT(CUI_KEY_DOWN);      } break;
        case kVK_LeftArrow:  { _CUI_KEY_DOWN_EVENT(CUI_KEY_LEFT);      } break;
        case kVK_RightArrow: { _CUI_KEY_DOWN_EVENT(CUI_KEY_RIGHT);     } break;

        // NOTE: These key codes are not defined in order, so they can not
        //       be handled by one _CUI_KEY_DOWN_EVENT(...).
        case kVK_F1:         { _CUI_KEY_DOWN_EVENT(CUI_KEY_F1);        } break;
        case kVK_F2:
        {
#if CUI_FRAMEBUFFER_SCREENSHOT_ENABLED
            cui_window->take_screenshot = true;
            cui_window->base.needs_redraw = true;
#else
            _CUI_KEY_DOWN_EVENT(CUI_KEY_F2);
#endif
        } break;
        case kVK_F3:         { _CUI_KEY_DOWN_EVENT(CUI_KEY_F3);        } break;
        case kVK_F4:         { _CUI_KEY_DOWN_EVENT(CUI_KEY_F4);        } break;
        case kVK_F5:         { _CUI_KEY_DOWN_EVENT(CUI_KEY_F5);        } break;
        case kVK_F6:         { _CUI_KEY_DOWN_EVENT(CUI_KEY_F6);        } break;
        case kVK_F7:         { _CUI_KEY_DOWN_EVENT(CUI_KEY_F7);        } break;
        case kVK_F8:         { _CUI_KEY_DOWN_EVENT(CUI_KEY_F8);        } break;
        case kVK_F9:         { _CUI_KEY_DOWN_EVENT(CUI_KEY_F9);        } break;
        case kVK_F10:        { _CUI_KEY_DOWN_EVENT(CUI_KEY_F10);       } break;
        case kVK_F11:        { _CUI_KEY_DOWN_EVENT(CUI_KEY_F11);       } break;
        case kVK_F12:        { _CUI_KEY_DOWN_EVENT(CUI_KEY_F12);       } break;

        default:
        {
            if ((ev.modifierFlags & NSEventModifierFlagCommand) ||
                ((ev.modifierFlags & NSEventModifierFlagControl) &&
                 !(ev.modifierFlags & NSEventModifierFlagOption)))
            {
                CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(&cui_window->base.temporary_memory);

                NSString *characters = ev.charactersIgnoringModifiers;

                uint64_t buffer_length = 4 * (characters.length + 1);
                char *buffer = cui_alloc_array(&cui_window->base.temporary_memory, char, buffer_length, CuiDefaultAllocationParams());

                [characters getCString: buffer
                             maxLength: buffer_length
                              encoding: NSUTF32StringEncoding];

                uint32_t *codepoints = (uint32_t *) buffer;

                while (*codepoints)
                {
                    uint32_t codepoint = *codepoints++;

                    if ((codepoint >= 32) && (codepoint != CUI_KEY_DELETE))
                    {
                        _CUI_KEY_DOWN_EVENT(codepoint);
                    }
                }

                cui_end_temporary_memory(temp_memory);
            }
            else
            {
                [self interpretKeyEvents: [NSArray arrayWithObject: ev]];
            }
        } break;
    }

#undef _CUI_KEY_DOWN_EVENT

}

- (void) keyUp: (NSEvent *) ev
{
}

- (void) insertText: (id) string replacementRange: (NSRange) replacementRange
{
    CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(&cui_window->base.temporary_memory);

    NSString *characters = string;

    uint64_t buffer_length = 4 * (characters.length + 1);
    char *buffer = cui_alloc_array(&cui_window->base.temporary_memory, char, buffer_length, CuiDefaultAllocationParams());

    [characters getCString: buffer
                 maxLength: buffer_length
                  encoding: NSUTF32StringEncoding];

    uint32_t *codepoints = (uint32_t *) buffer;

    while (*codepoints)
    {
        uint32_t codepoint = *codepoints++;

        if ((codepoint >= 32) && (codepoint != CUI_KEY_DELETE))
        {
            cui_window->base.event.key.codepoint       = codepoint;
            cui_window->base.event.key.alt_is_down     = false;
            cui_window->base.event.key.ctrl_is_down    = false;
            cui_window->base.event.key.shift_is_down   = false;
            cui_window->base.event.key.command_is_down = false;
            cui_window_handle_event(cui_window, CUI_EVENT_TYPE_KEY_DOWN);
        }
    }

    cui_end_temporary_memory(temp_memory);
}

- (void) setMarkedText: (id) string selectedRange: (NSRange) selectedRange replacementRange: (NSRange) replacementRange
{
}

- (void) unmarkText
{
}

- (NSRange) selectedRange
{
    return NSMakeRange(0, 0);
}

- (NSRange) markedRange
{
    return NSMakeRange(0, 0);
}

- (BOOL) hasMarkedText
{
    return NO;
}

- (nullable NSAttributedString *) attributedSubstringForProposedRange: (NSRange) range actualRange: (nullable NSRangePointer) actualRange
{
    return nil;
}

- (NSArray<NSAttributedStringKey> *) validAttributesForMarkedText
{
    return nil;
}

- (NSRect) firstRectForCharacterRange: (NSRange) range actualRange: (nullable NSRangePointer) actualRange
{
    return NSMakeRect(0, 0, 0, 0);
}

- (NSUInteger) characterIndexForPoint: (NSPoint) point
{
    return 0;
}

@end

void
cui_signal_main_thread(void)
{
    [NSApp postEvent: _cui_context.signal_event
             atStart: NO];
}

void
cui_platform_set_clipboard_text(CuiArena *temporary_memory, CuiString text)
{
    (void) temporary_memory;

    NSString *data = [[NSString alloc] initWithBytes: text.data
                                              length: text.count
                                            encoding: NSUTF8StringEncoding];

    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];

    [pasteboard clearContents];
    [pasteboard setString: data
                  forType: NSPasteboardTypeString];

    [data release];
}

CuiString
cui_platform_get_clipboard_text(CuiArena *arena)
{
    CuiString result = { 0 };

    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    NSString *data = [pasteboard stringForType: NSPasteboardTypeString];

    if ([data length] > 0)
    {
        const char *data_c = [data UTF8String];
        CuiAssert(data_c);
        result = cui_copy_string(arena, CuiCString(data_c));
    }

    return result;

}

uint32_t
cui_platform_get_core_count(void)
{
    uint32_t core_count;
    size_t core_count_size = sizeof(core_count);

    if (sysctlbyname("hw.logicalcpu_max", &core_count, &core_count_size, 0, 0))
    {
        core_count = 1;
    }

    return core_count;
}

uint32_t
cui_platform_get_performance_core_count(void)
{
    uint32_t core_count;
    size_t core_count_size = sizeof(core_count);

    uint32_t perf_level_count;
    size_t perf_level_count_size = sizeof(perf_level_count);

    if (!sysctlbyname("hw.nperflevels", &perf_level_count, &perf_level_count_size, 0, 0) &&
        (perf_level_count > 0))
    {
        if (sysctlbyname("hw.perflevel0.logicalcpu_max", &core_count, &core_count_size, 0, 0))
        {
            core_count = 1;
        }
    }
    else if (sysctlbyname("hw.logicalcpu_max", &core_count, &core_count_size, 0, 0))
    {
        core_count = 1;
    }

    return core_count;
}

uint32_t
cui_platform_get_efficiency_core_count(void)
{
    uint32_t core_count = 0;
    size_t core_count_size = sizeof(core_count);

    uint32_t perf_level_count;
    size_t perf_level_count_size = sizeof(perf_level_count);

    if (!sysctlbyname("hw.nperflevels", &perf_level_count, &perf_level_count_size, 0, 0) &&
        (perf_level_count > 1))
    {
        if (sysctlbyname("hw.perflevel1.logicalcpu_max", &core_count, &core_count_size, 0, 0))
        {
            core_count = 0;
        }
    }

    return core_count;
}

void
cui_platform_get_font_directories(CuiArena *temporary_memory, CuiArena *arena, CuiString **font_dirs)
{
    // TODO: works for now, but is this the best way?
    CuiString home_dir = cui_platform_get_environment_variable(temporary_memory, temporary_memory, CuiStringLiteral("HOME"));

    if (home_dir.count)
    {
        *cui_array_append(*font_dirs) = cui_path_concat(arena, home_dir, CuiStringLiteral("/Library/Fonts"));
    }

    *cui_array_append(*font_dirs) = CuiStringLiteral("/Library/Fonts");
    *cui_array_append(*font_dirs) = CuiStringLiteral("/System/Library/Fonts");
}

CuiString
cui_platform_get_data_directory(CuiArena *temporary_memory, CuiArena *arena)
{
    (void) temporary_memory;

    CuiString result = CuiStringLiteral("./");

    NSArray *array = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);

    if ([array count] > 0)
    {
        NSString *data_dir = [array objectAtIndex: 0];
        const char *c_data_dir = [data_dir fileSystemRepresentation];

        if (c_data_dir)
        {
            result = cui_copy_string(arena, CuiCString(c_data_dir));
            // TODO: deallocate c_data_dir
        }

        [data_dir release];
    }

    [array release];

    return result;
}

CuiString *
cui_get_files_to_open(void)
{
    return _cui_context.files_to_open;
}

bool
cui_init(int argument_count, char **arguments)
{
    if (!_cui_common_init(argument_count, arguments))
    {
        return false;
    }

    if (argument_count > 0)
    {
        arguments += 1;
        argument_count -= 1;

        for (int i = 0; i < argument_count; i += 1)
        {
            *cui_array_append(_cui_context.common.command_line_arguments) = CuiCString(arguments[i]);
        }
    }

    cui_arena_allocate(&_cui_context.files_to_open_arena, CuiKiB(32));

    pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);

    int32_t worker_thread_count = cui_platform_get_performance_core_count() - 1;

    worker_thread_count = cui_max_int32(1, cui_min_int32(worker_thread_count, 15));

    pthread_cond_init(&_cui_context.common.worker_thread_queue.semaphore_cond, 0);
    pthread_mutex_init(&_cui_context.common.worker_thread_queue.semaphore_mutex, 0);

    pthread_cond_init(&_cui_context.common.interactive_background_thread_queue.semaphore_cond, 0);
    pthread_mutex_init(&_cui_context.common.interactive_background_thread_queue.semaphore_mutex, 0);

    pthread_cond_init(&_cui_context.common.non_interactive_background_thread_queue.semaphore_cond, 0);
    pthread_mutex_init(&_cui_context.common.non_interactive_background_thread_queue.semaphore_mutex, 0);

    pthread_attr_t worker_thread_attr;
    pthread_attr_init(&worker_thread_attr);
    pthread_attr_set_qos_class_np(&worker_thread_attr, QOS_CLASS_USER_INTERACTIVE, 0);

    pthread_attr_t interactive_background_thread_attr;
    pthread_attr_init(&interactive_background_thread_attr);
    pthread_attr_set_qos_class_np(&interactive_background_thread_attr, QOS_CLASS_USER_INITIATED, 0);

    pthread_attr_t non_interactive_background_thread_attr;
    pthread_attr_init(&non_interactive_background_thread_attr);
    pthread_attr_set_qos_class_np(&non_interactive_background_thread_attr, QOS_CLASS_BACKGROUND, 0);

    for (int32_t worker_thread_index = 0;
         worker_thread_index < worker_thread_count;
         worker_thread_index += 1)
    {
        pthread_t worker_thread;
        pthread_create(&worker_thread, &worker_thread_attr, _cui_worker_thread_proc, 0);
    }

    pthread_t interactive_background_thread;
    pthread_create(&interactive_background_thread, &interactive_background_thread_attr,
                   _cui_background_thread_proc, &_cui_context.common.interactive_background_thread_queue);

    pthread_t non_interactive_background_thread;
    pthread_create(&non_interactive_background_thread, &non_interactive_background_thread_attr,
                   _cui_background_thread_proc, &_cui_context.common.non_interactive_background_thread_queue);

    pthread_attr_destroy(&worker_thread_attr);
    pthread_attr_destroy(&interactive_background_thread_attr);
    pthread_attr_destroy(&non_interactive_background_thread_attr);

    {
        CuiTemporaryMemory temp_memory = cui_begin_temporary_memory(&_cui_context.common.temporary_memory);

        size_t buffer_size = CuiKiB(1);
        char *buffer = cui_alloc_array(&_cui_context.common.temporary_memory, char, buffer_size, CuiDefaultAllocationParams());

        int len = proc_pidpath(getpid(), buffer, buffer_size);
        CuiString executable_path = cui_make_string(buffer, len);
        cui_path_remove_last_directory(&executable_path);
        _cui_context.common.executable_directory = cui_copy_string(&_cui_context.common.arena, executable_path);

        CFBundleRef bundle = CFBundleGetMainBundle();
        CFURLRef bundle_url = CFBundleCopyBundleURL(bundle);

        CFStringRef uti;

        if (CFURLCopyResourcePropertyForKey(bundle_url, kCFURLTypeIdentifierKey, &uti, 0) &&
            uti && UTTypeConformsTo(uti, kUTTypeApplicationBundle))
        {
            if (CFURLGetFileSystemRepresentation(bundle_url, TRUE, (UInt8 *) buffer, buffer_size))
            {
                _cui_context.common.bundle_directory = cui_copy_string(&_cui_context.common.arena, CuiCString(buffer));
            }
        }

        cui_end_temporary_memory(temp_memory);
    }

    _cui_context.signal_event = [NSEvent otherEventWithType: NSEventTypeApplicationDefined
                                                   location: NSMakePoint(0.0, 0.0)
                                              modifierFlags: 0
                                                  timestamp: 0.0
                                               windowNumber: 0
                                                    context: nil
                                                    subtype: NSEventSubtypeApplicationActivated
                                                      data1: 0
                                                      data2: 0];

    _cui_context.main_screen = [NSScreen mainScreen];

    // From Apple's documentation and testing on different macs it seems that
    // the window and screen scale is always an integer. It's 1.0 for non-retina
    // displays and 2.0 for retina displays. No matter what the desktop scaling is
    // rendering is always done in integer scales. You can observe that by taking
    // a screenshot. It might be bigger than your screens resolution because of that.
    //
    // https://developer.apple.com/documentation/appkit/nswindow/1419459-backingscalefactor?language=objc

    double screen_scale = [_cui_context.main_screen backingScaleFactor];
    CuiAssert(trunc(screen_scale) == screen_scale);
    _cui_context.desktop_scale = (int32_t) screen_scale;

    [NSApplication sharedApplication];

    [NSApp setActivationPolicy: NSApplicationActivationPolicyRegular];

    AppKitApplicationDelegate *app_delegate = [[AppKitApplicationDelegate alloc] init];
    [NSApp setDelegate: app_delegate];

    [NSApp run];

    return true;
}

CuiWindow *
cui_window_create(uint32_t creation_flags)
{
    CuiWindow *window = _cui_add_window(creation_flags);

    window->backbuffer_scale = _cui_context.desktop_scale;

    if (_cui_context.common.scale_factor == 0)
    {
        window->base.ui_scale = (float) window->backbuffer_scale;
        window->width  = window->backbuffer_scale * CUI_DEFAULT_WINDOW_WIDTH;
        window->height = window->backbuffer_scale * CUI_DEFAULT_WINDOW_HEIGHT;
    }
    else
    {
        window->base.ui_scale = (float) _cui_context.common.scale_factor;
        window->width  = _cui_context.common.scale_factor * CUI_DEFAULT_WINDOW_WIDTH;
        window->height = _cui_context.common.scale_factor * CUI_DEFAULT_WINDOW_HEIGHT;
    }

    window->font_id = _cui_font_manager_find_font(&window->base.temporary_memory, &window->base.font_manager, window->base.ui_scale,
                                                  cui_make_sized_font_spec(CuiStringLiteral("Arial"),  14.0f, 1.0f),
                                                  cui_make_sized_font_spec(CuiStringLiteral("TwemojiMozilla"), 14.0f, 1.0f));

    cui_arena_allocate(&window->arena, CuiKiB(4));

    if (!(window->base.creation_flags & CUI_WINDOW_CREATION_FLAG_PREFER_SYSTEM_DECORATION) &&
        !(window->base.creation_flags & CUI_WINDOW_CREATION_FLAG_MACOS_UNIFIED_TITLEBAR))
    {
        CuiWidget *root_widget = cui_alloc_type(&window->arena, CuiWidget, CuiDefaultAllocationParams());

        cui_widget_init(root_widget, CUI_WIDGET_TYPE_BOX);
        cui_widget_set_main_axis(root_widget, CUI_AXIS_Y);
        cui_widget_set_y_axis_gravity(root_widget, CUI_GRAVITY_START);

        window->titlebar = cui_alloc_type(&window->arena, CuiWidget, CuiDefaultAllocationParams());

        cui_widget_init(window->titlebar, CUI_WIDGET_TYPE_BOX);
        cui_widget_set_main_axis(window->titlebar, CUI_AXIS_Y);
        cui_widget_set_y_axis_gravity(window->titlebar, CUI_GRAVITY_START);
        cui_widget_add_flags(window->titlebar, CUI_WIDGET_FLAG_DRAW_BACKGROUND | CUI_WIDGET_FLAG_FIXED_HEIGHT);
        cui_widget_set_preferred_size(window->titlebar, 0.0f, 0.0f);

        window->titlebar->color_normal_background = CUI_COLOR_WINDOW_TITLEBAR_BACKGROUND;

        cui_widget_append_child(root_widget, window->titlebar);

        CuiWidget *dummy_user_root_widget = cui_alloc_type(&window->arena, CuiWidget, CuiDefaultAllocationParams());
        cui_widget_init(dummy_user_root_widget, CUI_WIDGET_TYPE_BOX);

        cui_widget_append_child(root_widget, dummy_user_root_widget);

        window->base.user_root_widget = dummy_user_root_widget;

        cui_widget_set_window(root_widget, window);
        cui_widget_set_ui_scale(root_widget, window->base.ui_scale);

        window->base.platform_root_widget = root_widget;
    }

    bool renderer_initialized = false;

#if CUI_RENDERER_METAL_ENABLED
    id<MTLDevice> metal_device = nil;

    if (!renderer_initialized)
    {
        metal_device = MTLCreateSystemDefaultDevice();

        if (metal_device)
        {
#if 0
            printf("[Metal Device] name = '%s'\n", [[metal_device name] UTF8String]);
            printf("[Metal Device] is low power = %s\n", [metal_device isLowPower] ? "true" : "false");
#endif

            CuiRendererMetal *metal_renderer = _cui_create_metal_renderer(&window->base.temporary_memory, metal_device);

            if (metal_renderer)
            {
                window->base.renderer_type = CUI_RENDERER_TYPE_METAL;
                window->renderer.metal.renderer_metal = metal_renderer;
                renderer_initialized = true;
            }
        }
    }
#endif

#if CUI_RENDERER_SOFTWARE_ENABLED
    if (!renderer_initialized)
    {
        window->base.renderer_type = CUI_RENDERER_TYPE_SOFTWARE;
        window->renderer.software.renderer_software = _cui_create_software_renderer();
        renderer_initialized = true;
    }
#endif

    if (!renderer_initialized)
    {
        cui_arena_deallocate(&window->arena);
        _cui_remove_window(window);
        return 0;
    }

    NSRect backing_rect = NSMakeRect(0.0, 0.0, (double) window->width, (double) window->height);
    NSRect content_rect = [_cui_context.main_screen convertRectFromBacking: backing_rect];
    NSRect screen_rect  = [_cui_context.main_screen frame];
    NSRect window_rect  = NSMakeRect(0.5 * (screen_rect.size.width - content_rect.size.width),
                                     0.5 * (screen_rect.size.height - content_rect.size.height),
                                     content_rect.size.width, content_rect.size.height);

    NSWindowStyleMask window_style = NSWindowStyleMaskTitled |
                                     NSWindowStyleMaskClosable |
                                     NSWindowStyleMaskMiniaturizable;

    if (!(window->base.creation_flags & CUI_WINDOW_CREATION_FLAG_NOT_USER_RESIZABLE))
    {
        window_style |= NSWindowStyleMaskResizable;
    }

    if (!(window->base.creation_flags & CUI_WINDOW_CREATION_FLAG_PREFER_SYSTEM_DECORATION))
    {
        window_style |= NSWindowStyleMaskFullSizeContentView;
    }

    window->appkit_window = [[AppKitWindow alloc] initWithContentRect: window_rect
                                                            styleMask: window_style
                                                              backing: NSBackingStoreBuffered
                                                                defer: NO];

    if (window->base.creation_flags & CUI_WINDOW_CREATION_FLAG_MACOS_UNIFIED_TITLEBAR)
    {
        window->appkit_toolbar = [NSToolbar new];
        window->appkit_window.toolbar = window->appkit_toolbar;
        window->appkit_window.toolbar.showsBaselineSeparator = NO;

#if MAC_OS_X_VERSION_MIN_REQUIRED >= 110000
        // window->appkit_window.toolbarStyle = NSWindowToolbarStyleUnified;
        window->appkit_window.toolbarStyle = NSWindowToolbarStyleUnifiedCompact;
#endif

        if (!(window->base.creation_flags & CUI_WINDOW_CREATION_FLAG_PREFER_SYSTEM_DECORATION))
        {
            window->appkit_window.movable = NO;
            window->appkit_window.titleVisibility = NSWindowTitleHidden;
        }
    }

    if (!(window->base.creation_flags & CUI_WINDOW_CREATION_FLAG_PREFER_SYSTEM_DECORATION))
    {
        window->appkit_window.titlebarAppearsTransparent = YES;

        NSRect window_rect = window->appkit_window.contentView.frame;
        NSRect content_rect = window->appkit_window.contentLayoutRect;

        if (_cui_context.common.scale_factor == 0)
        {
            window->titlebar_height = ceilf((float) (window_rect.size.height - content_rect.size.height));
        }
        else
        {
            float factor = (float) window->backbuffer_scale / (float) _cui_context.common.scale_factor;
            window->titlebar_height = ceilf((float) (window_rect.size.height - content_rect.size.height) * factor);
        }

        window_rect.size.height = 2 * window_rect.size.height - content_rect.size.height;

        [window->appkit_window setContentSize: window_rect.size];

        window_rect = [window->appkit_window convertRectToBacking: window_rect];

        window->width = lround(window_rect.size.width);
        window->height = lround(window_rect.size.height);

        if (!(window->base.creation_flags & CUI_WINDOW_CREATION_FLAG_MACOS_UNIFIED_TITLEBAR))
        {
            cui_widget_set_preferred_size(window->titlebar, 0.0f, window->titlebar_height);

            CuiRect rect = cui_make_rect(0, 0, window->width, window->height);
            cui_widget_layout(window->base.platform_root_widget, rect);
        }
    }

    NSAppearance *appearance = [NSAppearance appearanceNamed: NSAppearanceNameDarkAqua];
    [window->appkit_window setAppearance: appearance];

    AppKitWindowDelegate *appkit_window_delegate = [[AppKitWindowDelegate alloc] initWithCuiWindow: window];
    (void) appkit_window_delegate; // NOTE: delegate is assigned to a window in its constructor

#if CUI_RENDERER_METAL_ENABLED
    window->appkit_view = [[AppKitView alloc] initWithFrame: content_rect
                                                  cuiWindow: window
                                                metalDevice: metal_device];
#else
    window->appkit_view = [[AppKitView alloc] initWithFrame: content_rect
                                                  cuiWindow: window];
#endif
    [window->appkit_window setContentView: window->appkit_view];

    return window;
}

void
cui_window_set_title(CuiWindow *window, CuiString title)
{
    NSString *ns_title = [[NSString alloc] initWithBytes: title.data
                                                  length: title.count
                                                encoding: NSUTF8StringEncoding];

    [window->appkit_window setTitle: ns_title];
    [ns_title release];
}

void
cui_window_resize(CuiWindow *window, int32_t width, int32_t height)
{
    NSRect backing_rect = NSMakeRect(0.0, 0.0, (CGFloat) width, (CGFloat) height);

    if (!(window->base.creation_flags & CUI_WINDOW_CREATION_FLAG_PREFER_SYSTEM_DECORATION) &&
        !(window->base.creation_flags & CUI_WINDOW_CREATION_FLAG_MACOS_UNIFIED_TITLEBAR))
    {
        CuiPoint titlebar_size = cui_widget_get_preferred_size(window->titlebar);
        backing_rect.size.height += (CGFloat) titlebar_size.y;
    }

    NSRect content_rect = [_cui_context.main_screen convertRectFromBacking: backing_rect];
    NSRect screen_rect  = [_cui_context.main_screen frame];
    NSRect frame_rect   = [window->appkit_window frameRectForContentRect: content_rect];
    NSRect window_rect  = NSMakeRect(0.5 * (screen_rect.size.width - frame_rect.size.width),
                                     0.5 * (screen_rect.size.height - frame_rect.size.height),
                                     frame_rect.size.width, frame_rect.size.height);

    [window->appkit_window setFrame: window_rect display: YES];
}

void
cui_window_show(CuiWindow *window)
{
    [window->appkit_window makeKeyAndOrderFront: nil];
    window->base.needs_redraw = true;
}

void
cui_window_set_fullscreen(CuiWindow *window, bool fullscreen)
{
    if ((window->base.state & CUI_WINDOW_STATE_FULLSCREEN) != (fullscreen ? CUI_WINDOW_STATE_FULLSCREEN : 0))
    {
        [window->appkit_window toggleFullScreen: nil];
    }
}

float
cui_window_get_titlebar_height(CuiWindow *window)
{
    return window->titlebar_height;
}

void
cui_window_close(CuiWindow *window)
{
    [window->appkit_window close];
}

void
cui_window_destroy(CuiWindow *window)
{
    switch (window->base.renderer_type)
    {
        case CUI_RENDERER_TYPE_SOFTWARE:
        {
#if CUI_RENDERER_SOFTWARE_ENABLED
            if (window->renderer.software.backbuffer.pixels)
            {
                cui_platform_deallocate(window->renderer.software.backbuffer.pixels,
                                        window->renderer.software.backbuffer_memory_size);
            }

            _cui_destroy_software_renderer(window->renderer.software.renderer_software);
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
#if CUI_RENDERER_METAL_ENABLED
            _cui_destroy_metal_renderer(window->renderer.metal.renderer_metal);
#else
            CuiAssert(!"CUI_RENDERER_TYPE_METAL not enabled.");
#endif
        } break;

        case CUI_RENDERER_TYPE_DIRECT3D11:
        {
            CuiAssert(!"CUI_RENDERER_TYPE_DIRECT3D11 not supported.");
        } break;
    }

    cui_arena_deallocate(&window->arena);
    _cui_remove_window(window);
}

void
cui_window_set_root_widget(CuiWindow *window, CuiWidget *widget)
{
    CuiAssert(widget);

    if ((window->base.creation_flags & CUI_WINDOW_CREATION_FLAG_PREFER_SYSTEM_DECORATION) ||
        (window->base.creation_flags & CUI_WINDOW_CREATION_FLAG_MACOS_UNIFIED_TITLEBAR))
    {
        window->base.platform_root_widget = widget;
    }
    else
    {
        CuiAssert(window->base.user_root_widget);
        cui_widget_replace_child(window->base.platform_root_widget, window->base.user_root_widget, widget);
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
    @autoreleasepool
    {
        NSDate *event_timeout = [NSDate distantFuture];

        NSEvent *ev;

        for (;;)
        {
            ev = [NSApp nextEventMatchingMask: NSEventMaskAny
                                    untilDate: event_timeout
                                       inMode: NSDefaultRunLoopMode
                                      dequeue: YES];

            if (ev == nil) break;

            if (([ev type] == NSEventTypeApplicationDefined) &&
                ([ev subtype] == NSEventSubtypeApplicationActivated))
            {
                if (_cui_context.common.signal_callback)
                {
                    _cui_context.common.signal_callback();
                }
                break;
            }

            [NSApp sendEvent: ev];

            if (event_timeout)
            {
                [event_timeout release];
                event_timeout = nil;
            }
        }

        for (uint32_t window_index = 0;
             window_index < _cui_context.common.window_count; window_index += 1)
        {
            CuiWindow *window = _cui_context.common.windows[window_index];

            if (window->base.needs_redraw)
            {
                _cui_window_draw(window);

                switch (window->base.renderer_type)
                {
                    case CUI_RENDERER_TYPE_SOFTWARE:
                    {
#if CUI_RENDERER_SOFTWARE_ENABLED
                        [window->appkit_view setNeedsDisplay: YES];
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
#if !CUI_RENDERER_METAL_ENABLED
                        CuiAssert(!"CUI_RENDERER_TYPE_METAL not enabled.");
#endif
                    } break;

                    case CUI_RENDERER_TYPE_DIRECT3D11:
                    {
                        CuiAssert(!"CUI_RENDERER_TYPE_DIRECT3D11 not supported.");
                    } break;
                }

                window->base.needs_redraw = false;
            }
        }
    }
}
