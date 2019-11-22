#include "win32.h"
#include "debug.h"
#include "himath.h"

typedef struct Win32GlobalState_
{
    bool running;
} Win32GlobalState;

static Win32GlobalState g_win32_state;

int CALLBACK WinMain(HINSTANCE instance,
                     HINSTANCE prev_instance,
                     LPSTR cmdstr,
                     int cmd)
{
    Win32App app = {0};
    ASSERT(win32_app_init(&app, instance, "Zen", 1280, 720, 32, 24, 8, 3, 3));

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

        glClearColor(0.3f, 0.3f, 0.5f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        SwapBuffers(app.dc);
    }

    win32_app_cleanup(&app);

    return 0;
}
