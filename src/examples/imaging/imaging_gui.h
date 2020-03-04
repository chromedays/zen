#ifndef IMAGING_GUI_H
#define IMAGING_GUI_H
#include "imaging_model.h"
#include "imaging_operation.h"

typedef struct OpState_
{
    OpInput input;
    Image result_image;
    uint result_gl_texture;
} OpState;

typedef struct GUIState_
{
    // Immutable states
    const char* op_names[OpType_Count];
    OnFinishOpFn* on_finish_op;

    // Mutable states
    const ImageKeyValue* target_image_kv;

    struct
    {
        OpState states[100];
        int count;
    } selected_op;

    bool should_execute_operations;
} GUIState;

void gui_init(GUIState* gui);
void gui_render(GUIState* gui,
                const ImageKeyValue* image_keyvalues,
                int images_count);

#endif // IMAGING_GUI_H
