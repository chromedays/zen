#include "win32.h"
#include <glad/glad_wgl.h>
#include <himath.h>
#include "primitive.h"
#include "debug.h"

static HMODULE g_opengl;

#define WIN32_MENU_NAME "Zen Win32 Menu"
#define WIN32_CLASS_NAME "Zen Win32 Class"

LRESULT CALLBACK win32_message_callback(HWND window,
                                        UINT msg,
                                        WPARAM wp,
                                        LPARAM lp)
{
    LRESULT result = 0;

    switch (msg)
    {
    case WM_CLOSE: PostQuitMessage(0); break;
    default: result = DefWindowProcA(window, msg, wp, lp);
    }
    return result;
}

bool win32_app_init(Win32App* app,
                    HINSTANCE instance,
                    const char* title,
                    int width,
                    int height)
{
    bool result = false;

    WNDCLASSEXA wc = {.cbSize = sizeof(WNDCLASSEXA),
                      .style = CS_OWNDC,
                      .lpfnWndProc = &win32_message_callback,
                      .hInstance = instance,
                      .hCursor = LoadCursorA(NULL, IDC_ARROW),
                      .lpszMenuName = WIN32_MENU_NAME,
                      .lpszClassName = WIN32_CLASS_NAME};
    if (RegisterClassExA(&wc))
    {
        DWORD window_style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
        RECT window_rect = {
            .left = 0, .top = 0, .right = width, .bottom = height};
        AdjustWindowRectEx(&window_rect, window_style, 0, 0);
        IVec2 actual_window_size = {window_rect.right - window_rect.left,
                                    window_rect.bottom - window_rect.top};
        HWND window =
            CreateWindowExA(0, WIN32_CLASS_NAME, title, window_style,
                            CW_USEDEFAULT, CW_USEDEFAULT, actual_window_size.x,
                            actual_window_size.y, NULL, NULL, instance, NULL);

        if (window)
        {
            app->window = window;
            app->instance = instance;
            result = true;
        }
    }

    return result;
}

void win32_app_cleanup(Win32App* app)
{
    DestroyWindow(app->window);
    UnregisterClass(WIN32_CLASS_NAME, app->instance);
}

static void* win32_gl_get_proc(const char* name)
{
    if (!g_opengl)
        g_opengl = LoadLibrary("opengl32.dll");

    void* proc = NULL;

    if (g_opengl)
    {
        proc = (void*)wglGetProcAddress(name);
        if (!proc)
            proc = (void*)GetProcAddress(g_opengl, name);
    }

    return proc;
}

HGLRC win32_gl_context_make(HWND window,
                            int color_bits,
                            int depth_bits,
                            int stencil_bits,
                            int major_version,
                            int minor_version)
{
    HDC dc = GetDC(window);
    PIXELFORMATDESCRIPTOR pfd_dummy = {
        .nSize = sizeof(PIXELFORMATDESCRIPTOR),
        .nVersion = 1,
        .dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        .iPixelType = PFD_TYPE_RGBA,
        .cColorBits = 32,
        .cDepthBits = 24,
        .cStencilBits = 8};

    int pixelformat_dummy = ChoosePixelFormat(dc, &pfd_dummy);
    SetPixelFormat(dc, pixelformat_dummy, &pfd_dummy);

    HGLRC rc_dummy = wglCreateContext(dc);
    wglMakeCurrent(dc, rc_dummy);

    ASSERT(wglGetCurrentContext());

    int pixel_format_attribs[] = {WGL_DRAW_TO_WINDOW_ARB,
                                  GL_TRUE,
                                  WGL_SUPPORT_OPENGL_ARB,
                                  GL_TRUE,
                                  WGL_DOUBLE_BUFFER_ARB,
                                  GL_TRUE,
                                  WGL_PIXEL_TYPE_ARB,
                                  WGL_TYPE_RGBA_ARB,
                                  WGL_COLOR_BITS_ARB,
                                  color_bits,
                                  WGL_DEPTH_BITS_ARB,
                                  depth_bits,
                                  WGL_STENCIL_BITS_ARB,
                                  stencil_bits,
                                  0};
    int context_attribs[] = {WGL_CONTEXT_MAJOR_VERSION_ARB,
                             major_version,
                             WGL_CONTEXT_MINOR_VERSION_ARB,
                             minor_version,
                             WGL_CONTEXT_PROFILE_MASK_ARB,
                             WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
                             0};

    ASSERT(gladLoadWGLLoader((GLADloadproc)&win32_gl_get_proc, GetDC(window)));
    ASSERT(gladLoadWGL(GetDC(window)));

    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(rc_dummy);

    int pixelformat;
    uint format_count;
    ASSERT(wglChoosePixelFormatARB(dc, pixel_format_attribs, NULL, 1,
                                   &pixelformat, &format_count));
    ASSERT(SetPixelFormat(dc, pixelformat, NULL));

    HGLRC rc = wglCreateContextAttribsARB(dc, NULL, context_attribs);
    ASSERT(rc);

    wglMakeCurrent(dc, rc);

    ASSERT(gladLoadGLLoader((GLADloadproc)&win32_gl_get_proc));
    ASSERT(gladLoadGL());

    return rc;
}
