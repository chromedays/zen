#ifndef APP_H
#define APP_H
#include <himath.h>
#include <stdbool.h>

typedef enum Key_
{
    Key_W,
    Key_A,
    Key_S,
    Key_D,
    Key_Count,
} Key;

typedef struct Input_
{
    int key_map[Key_Count];
    bool key_down[256];
    bool key_ctrl;
    bool key_shift;
    bool key_alt;
    bool mouse_down[3];
    IVec2 mouse_pos;
    IVec2 window_size;
    float dt;
} Input;

#endif // APP_H