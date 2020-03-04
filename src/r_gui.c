#include "renderer.h"
#include "app.h"
#include <cimgui/cimgui_impl.h>

void r_gui_init()
{
    igCreateContext(NULL);
    ImGui_ImplOpenGL3_Init(NULL);
}
void r_gui_cleanup()
{
    ImGui_ImplOpenGL3_Shutdown();
    igDestroyContext(NULL);
}

void r_gui_new_frame(const Input* input)
{
    ImGuiIO* io = igGetIO();
    io->DisplaySize.x = (float)input->window_size.x;
    io->DisplaySize.y = (float)input->window_size.y;
    io->DeltaTime = input->dt;
    io->KeyCtrl = input->key_ctrl;
    io->KeyShift = input->key_shift;
    io->KeyAlt = input->key_alt;
    for (int i = 0; i < input->chcount; i++)
        ImGuiIO_AddInputCharacter(io, input->chbuf[i]);
    io->MouseDown[0] = input->mouse_down[0];
    io->MouseDown[1] = input->mouse_down[1];
    io->MouseDown[2] = input->mouse_down[2];
    io->MousePos.x = (float)input->mouse_pos.x;
    io->MousePos.y = (float)input->mouse_pos.y;
    ImGui_ImplOpenGL3_NewFrame();
    igNewFrame();
}

void r_gui_render()
{
    igRender();
    ImGui_ImplOpenGL3_RenderDrawData(igGetDrawData());
}
