#include "win32.h"
#include "debug.h"
#include "resource.h"
#include "renderer.h"
#include "scene.h"
#include "app.h"
#include "example.h"
#include <himath.h>

typedef struct Win32GlobalState_
{
    bool running;
} Win32GlobalState;

int CALLBACK WinMain(HINSTANCE instance,
                     HINSTANCE prev_instance,
                     LPSTR cmdstr,
                     int cmd)
{
    d_set_print_callback(&win32_print);

    Win32App app = {0};
    ASSERT(win32_app_init(&app, instance, "Zen", 1280, 720, 32, 24, 8, 4, 6));

    int work_group_counts[3] = {0};
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &work_group_counts[0]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, &work_group_counts[1]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, &work_group_counts[2]);
    PRINTLN("Max global work group sizes: (%d,%d,%d)", work_group_counts[0],
            work_group_counts[1], work_group_counts[2]);
    int work_group_sizes[3] = {0};
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &work_group_sizes[0]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &work_group_sizes[1]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &work_group_sizes[2]);
    PRINTLN("Max local work group sizes: (%d,%d,%d)", work_group_sizes[0],
            work_group_sizes[1], work_group_sizes[2]);
    int max_work_group_invocations = 0;
    glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS,
                  &max_work_group_invocations);
    PRINTLN("Max local work group invocations: %d", max_work_group_invocations);

    r_gui_init();

    Input input = {0};
    win32_register_input(&input);
    win32_update_input(&app);

    // Scene scene = {0};
    // s_init(&scene, &input);
    // s_switch_scene(&scene, EXAMPLE_LITERAL(cs300));

#ifdef USER_INIT
    USER_INIT
#endif

    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    LARGE_INTEGER last_counter;
    QueryPerformanceCounter(&last_counter);

    bool running = true;
    while (running)
    {
        LARGE_INTEGER curr_counter;
        QueryPerformanceCounter(&curr_counter);
        input.dt = (float)(curr_counter.QuadPart - last_counter.QuadPart) /
                   (float)freq.QuadPart;
        last_counter = curr_counter;

        win32_pre_update_input();

        MSG msg = {0};
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                running = false;
                break;
            }

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        win32_update_input(&app);

        r_gui_new_frame(&input);

#ifdef USER_UPDATE
        USER_UPDATE
#endif
        // s_update(&scene);

        r_gui_render();

        SwapBuffers(app.dc);
    }

// s_cleanup(&scene);
#ifdef USER_CLEANUP
    USER_CLEANUP
#endif

    r_gui_cleanup();

    win32_app_cleanup(&app);

    return 0;
}
