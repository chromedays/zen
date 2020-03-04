#include "imaging_gui.h"
#include "../../all.h"

#define IMGUI_DEFAULT_SIZE                                                     \
    (ImVec2)                                                                   \
    {                                                                          \
        0, 0                                                                   \
    }

#define IMGUI_CHILD_PANEL_SIZE                                                 \
    (ImVec2)                                                                   \
    {                                                                          \
        0, 1                                                                   \
    }

static void begin_group_panel()
{
    igBeginGroup();
}

static void select_op(GUIState* vm, OpType op_type)
{
    OpInput* input = &vm->selected_op.states[vm->selected_op.count++].input;
    input->type = op_type;
}

static void selectable_images(const GUIState* gui,
                              const ImageKeyValue* image_keyvalues,
                              int images_count,
                              const ImageKeyValue** out_selected_image_kv)
{
    for (int i = 0; i < images_count; i++)
    {
        const ImageKeyValue* kv = &image_keyvalues[i];
        if (igSelectable(kv->key, *out_selected_image_kv == kv, 0,
                         IMGUI_DEFAULT_SIZE))
        {
            *out_selected_image_kv = kv;
            break;
        }
    }
}

static ON_FINISH_OP(on_finish_op)
{
    GUIState* gui = (GUIState*)udata;
    OpState* op_state = &gui->selected_op.states[op_index];
    op_state->result_image = image_copy(result_image);
    op_state->result_gl_texture =
        image_make_gl_texture(&op_state->result_image);
}

void gui_init(GUIState* gui)
{
    gui->op_names[0] = "Undefined";
    gui->op_names[1] = "Add";
    gui->op_names[2] = "Sub";
    gui->op_names[3] = "Product";
    gui->op_names[4] = "Negate";
    gui->op_names[5] = "Log";
    gui->op_names[6] = "Power";
    gui->op_names[7] = "CCL";
    gui->op_names[8] = "EqualizeHistogram";
    gui->op_names[9] = "MatchHistogram";
    gui->op_names[10] = "GaussianSmoothen";

    gui->on_finish_op = &on_finish_op;
}

void gui_reset(GUIState* gui)
{
    for (int i = 0; i < gui->selected_op.count; i++)
    {
        OpState* op_state = &gui->selected_op.states[i];
        image_cleanup(&op_state->result_image);
        if (op_state->result_gl_texture > 0)
            glDeleteTextures(1, &op_state->result_gl_texture);
    }

    gui->selected_op.count = 0;
    gui->should_execute_operations = false;
}

void gui_render(GUIState* gui,
                const ImageKeyValue* image_keyvalues,
                int images_count)
{
    if (gui->should_execute_operations)
        gui->should_execute_operations = false;

    if (igBegin("Input", NULL, 0))
    {
        if (igBeginCombo("##SELECT_TARGET_IMAGE",
                         gui->target_image_kv ? gui->target_image_kv->key :
                                                "Select Source Image..",
                         ImGuiComboFlags_HeightLarge))
        {
            selectable_images(gui, image_keyvalues, images_count,
                              &gui->target_image_kv);
            igEndCombo();
        }

        igSeparator();

        if (igButton("Reset", IMGUI_DEFAULT_SIZE))
            gui_reset(gui);
        if (igIsItemHovered(0))
            igSetTooltip("haha");

        igSameLine(0, -1);

        if (igButton("Execute", IMGUI_DEFAULT_SIZE))
            gui->should_execute_operations = true;

        igSeparator();

        for (int i = 0; i < gui->selected_op.count; i++)
        {
            OpInput* op_input_base = &gui->selected_op.states[i].input;
            igPushIDInt(i);

            char header_id[50] = {0};
            sprintf_s(header_id, sizeof(header_id), "%d. %s", i + 1,
                      gui->op_names[op_input_base->type]);

            if (igCollapsingHeader(header_id,
                                   ImGuiTreeNodeFlags_CollapsingHeader))
            {
                switch (op_input_base->type)
                {
                case OpType_Add: {
                    OpInputAdd* input = &op_input_base->add;
                    if (igBeginCombo("##ADD",
                                     input->ref_image_kv ?
                                         input->ref_image_kv->key :
                                         "Select Image..",
                                     ImGuiComboFlags_HeightLarge))
                    {
                        selectable_images(gui, image_keyvalues, images_count,
                                          &input->ref_image_kv);
                        igEnd();
                    }
                    break;
                }
                case OpType_Sub: {
                    OpInputSub* input = &op_input_base->sub;
                    if (igBeginCombo("##SUB",
                                     input->ref_image_kv ?
                                         input->ref_image_kv->key :
                                         "Select Image..",
                                     ImGuiComboFlags_HeightLarge))
                    {
                        selectable_images(gui, image_keyvalues, images_count,
                                          &input->ref_image_kv);
                        igEnd();
                    }
                    break;
                }
                case OpType_Product: {
                    OpInputProduct* input = &op_input_base->product;
                    if (igBeginCombo("##PRODUCT",
                                     input->ref_image_kv ?
                                         input->ref_image_kv->key :
                                         "Select Image..",
                                     ImGuiComboFlags_HeightLarge))
                    {
                        selectable_images(gui, image_keyvalues, images_count,
                                          &input->ref_image_kv);
                        igEnd();
                    }
                    break;
                }
                case OpType_Negate: {
                    break;
                }
                case OpType_Log: {
                    OpInputLog* input = &op_input_base->log;
                    break;
                }
                case OpType_Power: {
                    OpInputPower* input = &op_input_base->power;
                    break;
                }
                case OpType_CCL: {
                    OpInputCCL* input = &op_input_base->ccl;
                    break;
                }
                case OpType_EqualizeHistogram: {
                    break;
                }
                case OpType_MatchHistogram: {
                    break;
                }
                case OpType_GaussianSmoothen: {
                    break;
                }
                }
            }

            igPopID();
        }

        igSeparator();

        if (igBeginCombo("##AddOp", "Select Op to Add..",
                         ImGuiComboFlags_HeightLarge))
        {
            for (OpType i = OpType_Undefined + 1;
                 i < ARRAY_LENGTH(gui->op_names); i++)
            {
                if (igSelectable(gui->op_names[i], false, 0,
                                 IMGUI_DEFAULT_SIZE))
                    select_op(gui, i);
            }
            igEnd();
        }
    }
    igEnd();

    if (igBegin("Output", NULL, 0))
    {
        // igImageButton()
    }
    igEnd();
}
