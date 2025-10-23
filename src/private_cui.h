#if !defined(CUI_BACKEND_X11_ENABLED)
#  define CUI_BACKEND_X11_ENABLED 0
#endif

#if !defined(CUI_BACKEND_WAYLAND_ENABLED)
#  define CUI_BACKEND_WAYLAND_ENABLED 0
#endif

#if !defined(CUI_RENDERER_SOFTWARE_ENABLED)
#  define CUI_RENDERER_SOFTWARE_ENABLED 0
#endif

#if !defined(CUI_RENDERER_OPENGLES2_ENABLED)
#  define CUI_RENDERER_OPENGLES2_ENABLED 0
#endif

#if !defined(CUI_RENDERER_METAL_ENABLED)
#  define CUI_RENDERER_METAL_ENABLED 0
#endif

#if !defined(CUI_RENDERER_DIRECT3D11_ENABLED)
#  define CUI_RENDERER_DIRECT3D11_ENABLED 0
#endif

#if !defined(CUI_FRAMEBUFFER_SCREENSHOT_ENABLED)
#  define CUI_FRAMEBUFFER_SCREENSHOT_ENABLED 0
#endif

#if !defined(CUI_RENDERER_OPENGLES2_RENDER_TIMES_ENABLED)
#  define CUI_RENDERER_OPENGLES2_RENDER_TIMES_ENABLED 0
#endif

#if !CUI_PLATFORM_WINDOWS
#include <pthread.h>
#endif

typedef enum CuiJpegMode
{
    CUI_JPEG_MODE_UNKNOWN     = 0,
    CUI_JPEG_MODE_BASELINE    = 1,
    CUI_JPEG_MODE_PROGRESSIVE = 2,
} CuiJpegMode;

typedef struct CuiJpegComponent
{
    uint8_t factor_x;
    uint8_t factor_y;
    uint8_t quant_table_id;
    uint8_t ac_table_id;
    uint8_t dc_table_id;
    int32_t dc_value;
} CuiJpegComponent;

typedef struct CuiJpegHuffmanTableEntry
{
    uint8_t symbol;
    uint8_t length;
} CuiJpegHuffmanTableEntry;

typedef struct CuiJpegHuffmanTable
{
    uint32_t entry_count;
    uint32_t max_code_length;
    CuiJpegHuffmanTableEntry *entries;
} CuiJpegHuffmanTable;

typedef struct CuiJpegQuantizationTable
{
    uint8_t values[64];
} CuiJpegQuantizationTable;

typedef struct CuiJpegBitReader
{
    CuiString *stream;
    uint32_t bit_buffer;
    uint32_t bit_count;
} CuiJpegBitReader;

typedef struct CuiTransform
{
    float m[6];
} CuiTransform;

typedef struct CuiContourPoint
{
    uint8_t flags;
    float x;
    float y;
} CuiContourPoint;

typedef struct CuiEdge
{
    bool positive;
    float x0, y0;
    float x1, y1;
} CuiEdge;

typedef struct CuiColoredGlyphLayer
{
    uint32_t glyph_index;
    CuiColor color;
    CuiRect bounding_box;
} CuiColoredGlyphLayer;

typedef enum CuiPathCommandType
{
    CUI_PATH_COMMAND_MOVE_TO            = 0,
    CUI_PATH_COMMAND_LINE_TO            = 1,
    CUI_PATH_COMMAND_QUADRATIC_CURVE_TO = 2,
    CUI_PATH_COMMAND_CUBIC_CURVE_TO     = 3,
    CUI_PATH_COMMAND_ARC_TO             = 4,
    CUI_PATH_COMMAND_CLOSE_PATH         = 5,
} CuiPathCommandType;

typedef struct CuiPathCommand
{
    CuiPathCommandType type;

    float x, y;

    float cx1, cy1;
    float cx2, cy2;

    bool large_arc_flag, sweep_flag;
} CuiPathCommand;

typedef struct CuiFontFileId { uint16_t value; } CuiFontFileId;

typedef struct CuiFontFile
{
    CuiString name;
    CuiString contents;

    int16_t ascent;
    int16_t descent;
    int16_t line_gap;

    uint16_t glyph_count;
    uint16_t hmetrics_count;
    uint16_t loca_index_format;

    uint8_t *cmap;
    uint8_t *COLR;
    uint8_t *CPAL;
    uint8_t *mapping_table;
    uint8_t *glyf;
    uint8_t *hmtx;
    uint8_t *loca;
} CuiFontFile;

typedef struct CuiFont
{
    float font_scale;
    float baseline_offset;

    int32_t line_height;
    int32_t cursor_offset;
    int32_t cursor_height;

    CuiFontFileId file_id;
    CuiFontId fallback_id;
} CuiFont;

typedef struct CuiSizedFont
{
    float size;
    float line_height;

    CuiFont font;
} CuiSizedFont;

typedef struct CuiFontRef
{
    CuiString name;
    CuiString path;
} CuiFontRef;

typedef struct CuiFontFileManager
{
    CuiArena arena;

    CuiFontFile *font_files;
    CuiFontRef  *font_refs;
} CuiFontFileManager;

typedef struct CuiFontManager
{
    CuiArena arena;

    CuiSizedFont *sized_fonts;
    CuiFontFileManager *font_file_manager;
} CuiFontManager;

#ifndef CUI_NO_BACKEND

#define CUI_MAX_WINDOW_COUNT 16
#define CUI_MAX_TEXTURE_COUNT 16
#define CUI_DEFAULT_WINDOW_WIDTH 800
#define CUI_DEFAULT_WINDOW_HEIGHT 600

typedef struct CuiWorkerThreadTaskGroup
{
    volatile uint32_t completion_goal;
    volatile uint32_t completion_count;

    void (*task_func)(void *);
} CuiWorkerThreadTaskGroup;

typedef struct CuiWorkerThreadQueueEntry
{
    CuiWorkerThreadTaskGroup *task_group;
    void *data;
} CuiWorkerThreadQueueEntry;

typedef struct CuiWorkerThreadQueue
{
    volatile uint32_t write_index;
    volatile uint32_t read_index;

#if CUI_PLATFORM_WINDOWS
    HANDLE semaphore;
#else
    pthread_mutex_t semaphore_mutex;
    pthread_cond_t semaphore_cond;
#endif

    CuiWorkerThreadQueueEntry entries[64];
} CuiWorkerThreadQueue;

typedef enum CuiBackgroundTaskState
{
    CUI_BACKGROUND_TASK_STATE_PENDING   = 0,
    CUI_BACKGROUND_TASK_STATE_RUNNING   = 1,
    CUI_BACKGROUND_TASK_STATE_CANCELING = 2,
    CUI_BACKGROUND_TASK_STATE_FINISHED  = 3,
} CuiBackgroundTaskState;

typedef struct CuiBackgroundThreadQueueEntry
{
    CuiBackgroundTask *task;
    void (*task_func)(CuiBackgroundTask *, void *);
    void *data;
} CuiBackgroundThreadQueueEntry;

typedef struct CuiBackgroundThreadQueue
{
    volatile uint32_t write_index;
    volatile uint32_t read_index;

#if CUI_PLATFORM_WINDOWS
    HANDLE semaphore;
#else
    pthread_mutex_t semaphore_mutex;
    pthread_cond_t semaphore_cond;
#endif

    CuiBackgroundThreadQueueEntry entries[8];
} CuiBackgroundThreadQueue;

typedef struct CuiGlyphKey
{
    // NOTE: id 0 is for shapes, aller others are font ids
    uint32_t id;

    uint32_t codepoint;

    float scale;
    float offset_x;
    float offset_y;
} CuiGlyphKey;

typedef struct CuiGlyphCache
{
    uint32_t count;
    uint32_t allocated;

    uint32_t insertion_failure_count;

    uint32_t *hashes;
    CuiGlyphKey *keys;
    CuiRect *rects;

    int32_t x, y, y_max;

    int32_t texture_id;
    CuiBitmap texture;

    uint64_t allocation_size;
} CuiGlyphCache;

typedef enum CuiTextureOperationType
{
    CUI_TEXTURE_OPERATION_ALLOCATE   = 0,
    CUI_TEXTURE_OPERATION_DEALLOCATE = 1,
    CUI_TEXTURE_OPERATION_UPDATE     = 2,
} CuiTextureOperationType;

typedef struct CuiTextureOperation
{
    uint16_t type;
    uint16_t texture_id;

    union
    {
        CuiBitmap bitmap;
        CuiRect rect;
    } payload;
} CuiTextureOperation;

typedef struct CuiTextureState
{
    uint32_t texture_id;
    CuiBitmap bitmap;
} CuiTextureState;

typedef struct CuiCommandBuffer
{
    int32_t max_texture_width;
    int32_t max_texture_height;

    uint32_t push_buffer_size;
    uint32_t max_push_buffer_size;
    uint8_t *push_buffer;

    uint32_t index_buffer_count;
    uint32_t max_index_buffer_count;
    uint32_t *index_buffer;

    uint32_t texture_operation_count;
    uint32_t max_texture_operation_count;
    CuiTextureOperation *texture_operations;
} CuiCommandBuffer;

typedef struct CuiKernel
{
    float factor;
    float *weights;
} CuiKernel;

// NOTE: The following two types have to satisfy "all" the alignment rules
//       for modern graphics apis like metal and opengl. That means that
//       they have to be a multiple of 16 in size and types like CuiColor
//       have to be 16 byte aligned, because they are mapped to a vec4 type.

typedef struct CuiClipRect
{
    int16_t x_min, y_min;
    int16_t x_max, y_max;
    uint8_t padding[8];
} CuiClipRect;

typedef struct CuiTexturedRect
{
    // first block of 16 byte
    int16_t x0, y0;
    int16_t x1, y1;
    int16_t u0, v0;
    int16_t u1, v1;

    // second block of 16 byte
    CuiColor color;

    // third block of 16 byte
    uint32_t texture_id;
    uint32_t clip_rect;
    uint8_t padding[8];
} CuiTexturedRect;

typedef enum CuiRendererType
{
    CUI_RENDERER_TYPE_SOFTWARE   = 0,
    CUI_RENDERER_TYPE_OPENGLES2  = 1,
    CUI_RENDERER_TYPE_METAL      = 2,
    CUI_RENDERER_TYPE_DIRECT3D11 = 3,
} CuiRendererType;

typedef struct CuiRenderer
{
    CuiRendererType type;
} CuiRenderer;

typedef struct CuiPointerCapture
{
    int32_t pointer_index;
    CuiWidget *widget;
} CuiPointerCapture;

typedef enum CuiWindowState
{
    CUI_WINDOW_STATE_MAXIMIZED    = (1 << 0),
    CUI_WINDOW_STATE_FULLSCREEN   = (1 << 1),
    CUI_WINDOW_STATE_TILED_LEFT   = (1 << 2),
    CUI_WINDOW_STATE_TILED_RIGHT  = (1 << 3),
    CUI_WINDOW_STATE_TILED_TOP    = (1 << 4),
    CUI_WINDOW_STATE_TILED_BOTTOM = (1 << 5),
} CuiWindowState;

typedef struct CuiWindowBase
{
    uint32_t creation_flags;
    uint32_t state;

    // size and scale of the whole window with decoration and drop shadow.
    // so it's the size of the backbuffer.
    float ui_scale;
    int32_t width;
    int32_t height;

    bool needs_redraw;

#if CUI_FRAMEBUFFER_SCREENSHOT_ENABLED
    bool take_screenshot;
#endif

    // This is a bit mask where the index of the bit is the
    // texture id and a bit being 1 means this texture id is in use.
    uint32_t allocated_texture_ids;

    CuiCursorType cursor;

    CuiArena arena;
    CuiArena temporary_memory;

    CuiEvent *events;
    CuiPointerCapture *pointer_captures;

    const CuiColorTheme *color_theme;

    CuiWidget *platform_root_widget;
    CuiWidget *user_root_widget;

    CuiWidget *hovered_widget;
    CuiWidget *pressed_widget;
    CuiWidget *focused_widget;

    CuiRenderer *renderer;

    CuiEvent event;

    // TODO: remove this, this should be handled by the window_frame_routine
    CuiWindowFrameResult window_frame_result;

    CuiGlyphCache glyph_cache;
    CuiFontManager font_manager;
} CuiWindowBase;

struct CuiGraphicsContext
{
    uint32_t clip_rect_offset;
    CuiRect clip_rect;
    CuiRect window_rect;
    CuiCommandBuffer *command_buffer;
    CuiGlyphCache *glyph_cache;
    CuiArena *temporary_memory;
    CuiFontManager *font_manager;
};

typedef struct CuiContextCommon
{
    bool main_loop_is_running;

    int32_t scale_factor;

    uint64_t perf_frequency;

    void (*signal_callback)(void);

    CuiArena arena;
    CuiArena temporary_memory;

    CuiString bundle_directory;
    CuiString executable_directory;

    CuiArena command_line_arguments_arena;
    CuiString *command_line_arguments;

    CuiFontFileManager font_file_manager;

    CuiWorkerThreadQueue worker_thread_queue;
    CuiBackgroundThreadQueue interactive_background_thread_queue;
    CuiBackgroundThreadQueue non_interactive_background_thread_queue;

    uint32_t window_count;
    struct CuiWindow *windows[CUI_MAX_WINDOW_COUNT];
} CuiContextCommon;

#if CUI_RENDERER_SOFTWARE_ENABLED

typedef struct CuiRendererSoftware
{
    CuiRenderer base;

    CuiCommandBuffer command_buffer;
    CuiBitmap bitmaps[CUI_MAX_TEXTURE_COUNT];

    uint64_t allocation_size;
} CuiRendererSoftware;

#endif

#if CUI_RENDERER_OPENGLES2_ENABLED

#include <EGL/egl.h>
#include <GLES2/gl2.h>

#define EGL_PLATFORM_X11_EXT              0x31D5
#define EGL_PLATFORM_WAYLAND_EXT          0x31D8

typedef struct CuiOpengles2Vertex
{
    CuiFloatPoint position;
    CuiFloatPoint uv;
    CuiColor color;
} CuiOpengles2Vertex;

typedef struct CuiOpengles2DrawCommand
{
    uint32_t vertex_offset;
    uint32_t vertex_count;
    GLuint texture_id;
    CuiFloatPoint texture_scale;
    int32_t clip_rect_x;
    int32_t clip_rect_y;
    int32_t clip_rect_width;
    int32_t clip_rect_height;
} CuiOpengles2DrawCommand;

typedef struct CuiRendererOpengles2
{
    CuiRenderer base;

    CuiCommandBuffer command_buffer;

#if CUI_RENDERER_OPENGLES2_RENDER_TIMES_ENABLED
    uint64_t platform_performance_frequency;

    float min_render_time;
    float max_render_time;
    double sum_render_time;
    int32_t frame_count;
#endif

    GLuint program;
    GLuint vertex_buffer;
    GLuint vertex_scale_location;
    GLuint texture_scale_location;
    GLuint texture_location;
    GLuint position_location;
    GLuint color_location;
    GLuint uv_location;

    CuiBitmap bitmaps[CUI_MAX_TEXTURE_COUNT];
    GLuint textures[CUI_MAX_TEXTURE_COUNT];

    CuiOpengles2Vertex *vertices;
    CuiOpengles2DrawCommand *draw_list;

    uint64_t allocation_size;
} CuiRendererOpengles2;

#endif

#if CUI_RENDERER_METAL_ENABLED

#include <Metal/Metal.h>
#include <QuartzCore/CAMetalLayer.h>

typedef struct CuiRendererMetal
{
    CuiRenderer base;

    CuiCommandBuffer command_buffer;

    id<MTLDevice> device;
    id<MTLCommandQueue> command_queue;
    id<MTLRenderPipelineState> pipeline;

    MTLRenderPassDescriptor *render_pass;

    id<MTLBuffer> buffer_transform;
    id<MTLBuffer> push_buffer;
    id<MTLBuffer> index_buffer;

    CuiBitmap bitmaps[CUI_MAX_TEXTURE_COUNT];
    id<MTLTexture> textures[CUI_MAX_TEXTURE_COUNT];

    uint64_t allocation_size;
} CuiRendererMetal;

#endif

#if CUI_RENDERER_DIRECT3D11_ENABLED

#define COBJMACROS

#include <d3d11.h>
#include <dxgi1_2.h>
#include <d3dcompiler.h>

#undef COBJMACROS

typedef struct CuiDirect3D11Constants
{
    float vertex_transform[16];
    CuiFloatPoint texture_scale;
} CuiDirect3D11Constants;

typedef struct CuiDirect3D11Vertex
{
    CuiFloatPoint position;
    CuiFloatPoint uv;
    CuiColor color;
} CuiDirect3D11Vertex;

typedef struct CuiRendererDirect3D11
{
    CuiRenderer base;

    CuiCommandBuffer command_buffer;

    int32_t framebuffer_width;
    int32_t framebuffer_height;

    ID3D11Device *device;
    ID3D11DeviceContext *device_context;
    IDXGISwapChain1 *swapchain;
    ID3D11RenderTargetView *framebuffer_view;

    ID3D11VertexShader *vertex_shader;
    ID3D11PixelShader *pixel_shader;

    ID3D11Buffer *vertex_buffer;
    ID3D11InputLayout *vertex_layout;

    ID3D11Buffer *constant_buffer;
    ID3D11RasterizerState *rasterizer_state;
    ID3D11BlendState *blend_state;

    CuiBitmap bitmaps[CUI_MAX_TEXTURE_COUNT];
    ID3D11Texture2D *textures[CUI_MAX_TEXTURE_COUNT];
    ID3D11ShaderResourceView *texture_views[CUI_MAX_TEXTURE_COUNT];
    ID3D11SamplerState *samplers[CUI_MAX_TEXTURE_COUNT];

    uint64_t allocation_size;
} CuiRendererDirect3D11;

#endif

typedef struct CuiFramebuffer
{
    int32_t width;
    int32_t height;

#if CUI_RENDERER_SOFTWARE_ENABLED
    CuiBitmap bitmap;
#endif

#if CUI_RENDERER_METAL_ENABLED
    id<CAMetalDrawable> drawable;
#endif
} CuiFramebuffer;

static CuiFramebuffer *_cui_acquire_framebuffer(CuiWindow *window, int32_t width, int32_t height);

#endif

#if CUI_ARCH_ARM64
#include <arm_neon.h>
#endif

#if CUI_ARCH_X86_64
#include <emmintrin.h>
#endif

#if CUI_PLATFORM_ANDROID
#include "cui_android.h"
#elif CUI_PLATFORM_WINDOWS
#include "cui_windows.h"
#elif CUI_PLATFORM_LINUX
#include "cui_linux.h"
#elif CUI_PLATFORM_MACOS
#include "cui_macos.h"
#else
#  error unsupported platform
#endif
