#include <glad/glad_wgl.h>
#include "primitive.h"
#include "debug.h"
#include <himath.h>
#include <stdbool.h>

#define ZEN_WIN32_MENU_NAME "Zen Win32 Menu"
#define ZEN_WIN32_CLASS_NAME "Zen Win32 Class"
#define ZEN_WINDOW_TITLE "Zen"
#define ZEN_WINDOW_WIDTH 1280
#define ZEN_WINDOW_HEIGHT 720

typedef struct _Win32GlobalState
{
    bool running;
} Win32GlobalState;

Win32GlobalState g_win32_state;

LRESULT CALLBACK win32_message_callback(HWND window,
                                        UINT msg,
                                        WPARAM wp,
                                        LPARAM lp);

void* win32_gl_get_proc(const char* name);
HGLRC win32_gl_context_make(HWND window,
                            int* pixel_format_attribs,
                            int* context_attribs);

int CALLBACK WinMain(HINSTANCE instance,
                     HINSTANCE prev_instance,
                     LPSTR cmdstr,
                     int cmd)
{
    WNDCLASSEXA wc = {.cbSize = sizeof(WNDCLASSEXA),
                      .style = CS_OWNDC,
                      .lpfnWndProc = &win32_message_callback,
                      .hInstance = instance,
                      .hCursor = LoadCursorA(NULL, IDC_ARROW),
                      .lpszMenuName = ZEN_WIN32_MENU_NAME,
                      .lpszClassName = ZEN_WIN32_CLASS_NAME};
    ASSERT(RegisterClassExA(&wc));

    DWORD window_style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;

    RECT window_rect = {.left = 0,
                        .top = 0,
                        .right = ZEN_WINDOW_WIDTH,
                        .bottom = ZEN_WINDOW_HEIGHT};
    AdjustWindowRectEx(&window_rect, window_style, 0, 0);

    IVec2 actual_window_size = {window_rect.right - window_rect.left,
                                window_rect.bottom - window_rect.top};

    HWND window =
        CreateWindowExA(0, ZEN_WIN32_CLASS_NAME, ZEN_WINDOW_TITLE, window_style,
                        CW_USEDEFAULT, CW_USEDEFAULT, actual_window_size.x,
                        actual_window_size.y, NULL, NULL, instance, NULL);
    ASSERT(window);

    int pixel_format_attribs[] = {WGL_DRAW_TO_WINDOW_ARB,
                                  GL_TRUE,
                                  WGL_SUPPORT_OPENGL_ARB,
                                  GL_TRUE,
                                  WGL_DOUBLE_BUFFER_ARB,
                                  GL_TRUE,
                                  WGL_PIXEL_TYPE_ARB,
                                  WGL_TYPE_RGBA_ARB,
                                  WGL_COLOR_BITS_ARB,
                                  32,
                                  WGL_DEPTH_BITS_ARB,
                                  24,
                                  WGL_STENCIL_BITS_ARB,
                                  8,
                                  0};
    int context_attribs[] = {WGL_CONTEXT_MAJOR_VERSION_ARB,
                             3,
                             WGL_CONTEXT_MINOR_VERSION_ARB,
                             3,
                             WGL_CONTEXT_PROFILE_MASK_ARB,
                             WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
                             0};
    HGLRC rc =
        win32_gl_context_make(window, pixel_format_attribs, context_attribs);

    g_win32_state.running = true;

    while (g_win32_state.running)
    {
        MSG msg = {0};
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                g_win32_state.running = false;
                break;
            }

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    DestroyWindow(window);
    UnregisterClass(ZEN_WIN32_CLASS_NAME, instance);

    return 0;
}

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

void* win32_gl_get_proc(const char* name)
{
    static HMODULE opengl = 0;

    if (!opengl)
        opengl = LoadLibrary("opengl32.dll");

    void* proc = NULL;

    if (opengl)
    {
        proc = (void*)wglGetProcAddress(name);
        if (!proc)
            proc = (void*)GetProcAddress(opengl, name);
    }

    return proc;
}

HGLRC win32_gl_context_make(HWND window,
                            int* pixel_format_attribs,
                            int* context_attribs)
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
