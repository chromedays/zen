#include "win32.h"
#include "debug.h"
#include "resource.h"
#include "himath.h"
#include <stdlib.h>
#include <stdint.h>

typedef struct Win32GlobalState_
{
    bool running;
} Win32GlobalState;

#define UNIFORM_PADXX(x) uint32_t __pad__##x
#define UNIFORM_PADX(x) UNIFORM_PADXX(x)
#define UNIFORM_PAD UNIFORM_PADX(__LINE__)

typedef struct LightUniform_
{
    FVec3 ka;
    UNIFORM_PAD;
    FVec3 kd;
    UNIFORM_PAD;
    FVec3 ks;
    UNIFORM_PAD;
    FVec3 pos;
    UNIFORM_PAD;
    FVec3 dir;
    float inner_angle;
    float outer_angle;
    float falloff;
    float ns;
    float linear;
    float quadratic;
    UNIFORM_PAD;
    UNIFORM_PAD;
    UNIFORM_PAD;
} LightUniform;

typedef struct PerFrameUniform_
{
    Mat4 view;
    Mat4 proj;
    FVec3 global_ambient;
    UNIFORM_PAD;
    FVec3 fog;
    float z_near;
    float z_far;
    int directional_lights_count;
    int point_lights_count;
    int spotlights_count;
    LightUniform directional_lights[16];
    LightUniform point_lights[16];
    LightUniform spotlights[16];
    int use_gpu_uv;      // 0 = cpu, 1 = gpu
    int uv_mapping_type; // 0 = cylindrical, 1 = spherical, 2 = 6-side planar
    int uv_entity_type;  // 0 = position, 1 = normal
} PerFrameUniform;

typedef struct PerObjectUniform_
{
    Mat4 model;
    FVec3 ambient;
    float ns;
} PerObjectUniform;

static Win32GlobalState g_win32_state;

int CALLBACK WinMain(HINSTANCE instance,
                     HINSTANCE prev_instance,
                     LPSTR cmdstr,
                     int cmd)
{
    d_set_print_callback(&win32_print);

    Win32App app = {0};
    ASSERT(win32_app_init(&app, instance, "Zen", 1280, 720, 32, 24, 8, 4, 6));

    Mesh sp_mesh = rc_mesh_make_sphere(2, 64, 64);
    VertexBuffer sp_vb = {0};
    vb_init(&sp_vb, &sp_mesh, GL_TRIANGLES);

    char* shared_src = win32_load_text_file("shaders/shared.glsl");
    char* vs_src = win32_load_text_file("shaders/unlit.vert");
    char* fs_src = win32_load_text_file("shaders/unlit.frag");

    GLuint unlit =
        rc_shader_load_from_source(vs_src, fs_src, NULL, &shared_src, 1);

    free(vs_src);
    free(fs_src);

    PerFrameUniform per_frame = {0};
    per_frame.view = mat4_identity();

    GLuint per_frame_ubo;
    glGenBuffers(1, &per_frame_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, per_frame_ubo);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(per_frame), NULL, GL_DYNAMIC_DRAW);

    PerObjectUniform per_object = {0};
    per_object.model = mat4_translation((FVec3){0, 0, -5});
    per_object.ambient = (FVec3){1, 0, 0};
    GLuint per_object_ubo;
    glGenBuffers(1, &per_object_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, per_object_ubo);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(per_object), NULL, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    glUseProgram(unlit);
    glUniformBlockBinding(unlit, glGetUniformBlockIndex(unlit, "PerFrame"), 0);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, per_frame_ubo);
    glUniformBlockBinding(unlit, glGetUniformBlockIndex(unlit, "PerObject"), 1);
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, per_object_ubo);

    GLuint fsq_shader = 0;
    GLuint path_tracer = 0;
    IVec2 tex_size = win32_get_window_size(app.window);
    GLuint tex;
    {
        glGenTextures(1, &tex);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, tex_size.x, tex_size.y, 0,
                     GL_RGBA, GL_FLOAT, NULL);
        glBindImageTexture(0, tex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

        int work_group_counts[3] = {0};
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0,
                        &work_group_counts[0]);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1,
                        &work_group_counts[1]);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2,
                        &work_group_counts[2]);
        PRINTLN("Max global work group sizes: (%d,%d,%d)", work_group_counts[0],
                work_group_counts[1], work_group_counts[2]);
        int work_group_sizes[3] = {0};
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0,
                        &work_group_sizes[0]);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1,
                        &work_group_sizes[1]);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2,
                        &work_group_sizes[2]);
        PRINTLN("Max local work group sizes: (%d,%d,%d)", work_group_sizes[0],
                work_group_sizes[1], work_group_sizes[2]);
        int max_work_group_invocations = 0;
        glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS,
                      &max_work_group_invocations);
        PRINTLN("Max local work group invocations: %d",
                max_work_group_invocations);

        char* path_tracer_src =
            win32_load_text_file("shaders/path_tracer.comp");
        path_tracer =
            rc_shader_load_from_source(NULL, NULL, path_tracer_src, NULL, 0);
        free(path_tracer_src);

        {
            char* fsq_src[2] = {
                win32_load_text_file("shaders/fsq.vert"),
                win32_load_text_file("shaders/fsq.frag"),
            };

            fsq_shader = rc_shader_load_from_source(fsq_src[0], fsq_src[1],
                                                    NULL, &shared_src, 1);

            for (int i = 0; i < 2; ++i)
                free(fsq_src[i]);
        }
    }
    free(shared_src);

    Mesh fsq_mesh = rc_mesh_make_raw(4, 6);
    {
        Vertex vertices[] = {
            {{1, 1}, {1, 1}, {0}},
            {{-1, 1}, {0, 1}, {0}},
            {{-1, -1}, {0, 0}, {0}},
            {{1, -1}, {1, 0}, {0}},
        };
        uint indices[] = {
            2, 3, 0, 0, 1, 2,
        };
        memcpy(fsq_mesh.vertices, vertices, sizeof(vertices));
        memcpy(fsq_mesh.indices, indices, sizeof(indices));
    }
    VertexBuffer fsq_vb = {0};
    vb_init(&fsq_vb, &fsq_mesh, GL_TRIANGLES);

    g_win32_state.running = true;

    while (g_win32_state.running)
    {
        MSG msg = {0};
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                g_win32_state.running = false;
                break;
            }

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        per_frame.proj = mat4_persp(
            60, win32_get_window_aspect_ratio(app.window), 0.1f, 100);

        {
            IVec2 ws = win32_get_window_size(app.window);
            glViewport(0, 0, ws.x, ws.y);
        }

        glUseProgram(path_tracer);
        glDispatchCompute((GLuint)tex_size.x, (GLuint)tex_size.y, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(0.3f, 0.3f, 0.5f, 1.0f);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);

        glBindBuffer(GL_UNIFORM_BUFFER, per_frame_ubo);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(per_frame), &per_frame);
        glBindBuffer(GL_UNIFORM_BUFFER, per_object_ubo);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(per_object), &per_object);

        glDisable(GL_CULL_FACE);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glUseProgram(unlit);
        vb_draw(&sp_vb);

        glEnable(GL_CULL_FACE);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glUseProgram(fsq_shader);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex);
        vb_draw(&fsq_vb);

        SwapBuffers(app.dc);
    }

    rc_mesh_cleanup(&sp_mesh);
    vb_cleanup(&sp_vb);

    win32_app_cleanup(&app);

    return 0;
}
