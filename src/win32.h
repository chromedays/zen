#ifndef WIN32_H
#define WIN32_H
#include "primitive.h"
#include <glad/glad_wgl.h>

typedef struct Win32App_
{
    HINSTANCE instance;
    HWND window;

    HDC dc;
    HGLRC rc;
} Win32App;

LRESULT CALLBACK win32_message_callback(HWND window,
                                        UINT msg,
                                        WPARAM wp,
                                        LPARAM lp);

bool win32_app_init(Win32App* app,
                    HINSTANCE instance,
                    const char* title,
                    int width,
                    int height,
                    int color_bits,
                    int depth_bits,
                    int stencil_bits,
                    int gl_major_version,
                    int gl_minor_version);
void win32_app_cleanup(Win32App* app);

#endif // WIN32_H