#include "win32.h"
#include "app.h"
#include "primitive.h"
#include "debug.h"
#include "util.h"
#include <glad/glad_wgl.h>
#include <himath.h>
#include <stdio.h>
#include <stdlib.h>

// TODO: abstract input handling code

static HMODULE g_opengl;
static Input* g_input;
static HCURSOR g_cursor;

#define WIN32_MENU_NAME "Zen Win32 Menu"
#define WIN32_CLASS_NAME "Zen Win32 Class"

static void* win32_gl_get_proc(const char* name);
static HGLRC win32_gl_context_make(HDC dc,
                                   int color_bits,
                                   int depth_bits,
                                   int stencil_bits,
                                   int major_version,
                                   int minor_version);

LRESULT CALLBACK win32_message_callback(HWND window,
                                        UINT msg,
                                        WPARAM wp,
                                        LPARAM lp)
{
    LRESULT result = 0;

    // TODO: Handle cursor visibility in a better way

    switch (msg)
    {
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK: g_input->mouse_down[0] = true; break;
    case WM_LBUTTONUP: g_input->mouse_down[0] = false; break;
    case WM_RBUTTONDOWN: SetCursor(NULL);
    case WM_RBUTTONDBLCLK: g_input->mouse_down[1] = true; break;
    case WM_RBUTTONUP:
        g_input->mouse_down[1] = false;
        SetCursor(g_cursor);
        break;
    case WM_MBUTTONDOWN:
    case WM_MBUTTONDBLCLK: g_input->mouse_down[2] = true; break;
    case WM_MBUTTONUP: g_input->mouse_down[2] = false; break;

    case WM_SYSKEYDOWN:
    case WM_KEYDOWN:
        if (g_input && wp < ARRAY_LENGTH(g_input->key_down))
            g_input->key_down[wp] = true;
        break;
    case WM_SYSKEYUP:
    case WM_KEYUP:
        if (g_input && wp < ARRAY_LENGTH(g_input->key_down))
            g_input->key_down[wp] = false;
        break;
    case WM_SETCURSOR:
        PRINTLN("SETCURSOR");
        if (g_input)
        {
            if (g_input->mouse_down[1])
                SetCursor(NULL);
            else
                SetCursor(g_cursor);
            result = TRUE;
        }
        break;
    case WM_CLOSE: PostQuitMessage(0); break;
    default: result = DefWindowProcA(window, msg, wp, lp);
    }
    return result;
}

bool win32_app_init(Win32App* app,
                    HINSTANCE instance,
                    const char* title,
                    int width,
                    int height,
                    int color_bits,
                    int depth_bits,
                    int stencil_bits,
                    int gl_major_version,
                    int gl_minor_version)
{
    bool result = false;

    WNDCLASSEXA wc = {.cbSize = sizeof(WNDCLASSEXA),
                      .style = CS_OWNDC,
                      .lpfnWndProc = &win32_message_callback,
                      .hInstance = instance,
                      .hCursor = LoadCursorA(NULL, IDC_ARROW),
                      .lpszMenuName = WIN32_MENU_NAME,
                      .lpszClassName = WIN32_CLASS_NAME};
    g_cursor = wc.hCursor;
    if (RegisterClassExA(&wc))
    {
        DWORD window_style =
            ((WS_OVERLAPPEDWINDOW | WS_VISIBLE) ^ WS_THICKFRAME) ^
            WS_MAXIMIZEBOX;
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
            app->instance = instance;
            app->window = window;
            app->dc = GetDC(app->window);
            app->rc = win32_gl_context_make(app->dc, color_bits, depth_bits,
                                            stencil_bits, gl_major_version,
                                            gl_minor_version);
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

IVec2 win32_get_window_size(HWND window)
{
    RECT cr;
    GetClientRect(window, &cr);
    IVec2 result = {cr.right - cr.left, cr.bottom - cr.top};
    return result;
}

float win32_get_window_aspect_ratio(HWND window)
{
    IVec2 ws = win32_get_window_size(window);
    float result = (float)ws.x / (float)ws.y;
    return result;
}

char* win32_load_text_file(const char* filename)
{
    FILE* f = fopen(filename, "r");

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    char* buf = (char*)malloc(size + 1);
    ASSERT(size >= 0);
    size = (long)fread(buf, 1, (size_t)size, f);
    buf[size] = '\0';
    fclose(f);

    return buf;
}

void win32_print(const char* str)
{
    OutputDebugStringA(str);
}

void win32_register_input(Input* input)
{
    g_input = input;
    g_input->key_map[Key_W] = 'W';
    g_input->key_map[Key_A] = 'A';
    g_input->key_map[Key_S] = 'S';
    g_input->key_map[Key_D] = 'D';
}

void win32_update_input(const Win32App* app)
{
    if (!g_input)
        return;
    Input* input = g_input;

    POINT mp;
    GetCursorPos(&mp);
    ScreenToClient(app->window, &mp);
    if (input->mouse_pos.x != 0 || input->mouse_pos.y != 0)
    {
        input->mouse_delta = (IVec2){
            mp.x - input->mouse_pos.x,
            mp.y - input->mouse_pos.y,
        };
    }
    input->mouse_pos.x = mp.x;
    input->mouse_pos.y = mp.y;

    input->key_ctrl = GetKeyState(VK_CONTROL) & 0x8000;
    input->key_shift = GetKeyState(VK_SHIFT) & 0x8000;
    input->key_alt = GetKeyState(VK_MENU) & 0x8000;

    input->window_size = win32_get_window_size(app->window);
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

static HGLRC win32_gl_context_make(HDC dc,
                                   int color_bits,
                                   int depth_bits,
                                   int stencil_bits,
                                   int major_version,
                                   int minor_version)
{
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

    ASSERT(gladLoadWGLLoader((GLADloadproc)&win32_gl_get_proc, dc));
    ASSERT(gladLoadWGL(dc));

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
