#ifndef WIN32_H
#define WIN32_H
#include "primitive.h"
#include <himath.h>
#include <glad/glad_wgl.h>

typedef struct Input_ Input;

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
IVec2 win32_get_window_size(HWND window);
float win32_get_window_aspect_ratio(HWND window);
char* win32_load_text_file(const char* filename);
void win32_print(const char* str);

void win32_register_input(Input* input);
void win32_update_input(const Win32App* app);

#endif // WIN32_H