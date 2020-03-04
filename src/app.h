#ifndef APP_H
#define APP_H
#include "primitive.h"
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
    bool mouse_pressed[3];
    bool mouse_released[3];
    uint chbuf[1000];
    int chcount;
    IVec2 mouse_delta;
    IVec2 mouse_pos;
    IVec2 window_size;
    float dt;
} Input;

inline bool a_input_is_key_down(const Input* input, Key key)
{
    return input->key_down[input->key_map[key]];
}

#endif // APP_H