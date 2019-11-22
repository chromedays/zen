#ifndef WIN32_H
#define WIN32_H
#include "primitive.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

typedef struct Win32App_
{
    HINSTANCE instance;
    HWND window;
} Win32App;

LRESULT CALLBACK win32_message_callback(HWND window,
                                        UINT msg,
                                        WPARAM wp,
                                        LPARAM lp);

bool win32_app_init(Win32App* app,
                    HINSTANCE instance,
                    const char* title,
                    int width,
                    int height);
void win32_app_cleanup(Win32App* app);
HGLRC win32_gl_context_make(HWND window,
                            int color_bits,
                            int depth_bits,
                            int stencil_bits,
                            int major_version,
                            int minor_version);

#endif // WIN32_H