#include "debug.h"
#include <himath.h>
#include <stdbool.h>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#define ZEN_WIN32_MENU_NAME "Zen Win32 Menu"
#define ZEN_WIN32_CLASS_NAME "Zen Win32 Class"
#define ZEN_WINDOW_TITLE "Zen"
#define ZEN_WINDOW_WIDTH 1280
#define ZEN_WINDOW_HEIGHT 720

LRESULT CALLBACK win32_message_callback(HWND window,
                                        UINT msg,
                                        WPARAM wp,
                                        LPARAM lp);

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

    char exe_path_raw[MAX_PATH];
    DWORD exe_path_length =
        GetModuleFileNameA(instance, exe_path_raw, sizeof(exe_path_raw));
    for (int i = (int)exe_path_length; i >= 0; --i)
    {
        if (exe_path_raw[i] == '\\')
        {
            exe_path_raw[i + 1] = '\0';
            break;
        }
    }
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
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_QUIT: ASSERT(false); break;
    case WM_CLOSE: PostQuitMessage(0); break;
    default: result = DefWindowProcA(window, msg, wp, lp);
    }
    return result;
}
