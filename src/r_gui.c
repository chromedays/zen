#include "renderer.h"
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

void r_gui_new_frame(IVec2 display_size)
{
    ImGuiIO* io = igGetIO();
    io->DisplaySize.x = (float)display_size.x;
    io->DisplaySize.y = (float)display_size.y;
    ImGui_ImplOpenGL3_NewFrame();
    igNewFrame();
}

void r_gui_render()
{
    igRender();
    ImGui_ImplOpenGL3_RenderDrawData(igGetDrawData());
}
