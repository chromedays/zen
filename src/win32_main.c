#include "win32.h"
#include "debug.h"
#include "resource.h"
#include "renderer.h"
#include "scene.h"
#include "app.h"
#include "example.h"
#include <himath.h>
#include <math.h>
#include <stdlib.h>

typedef struct Win32GlobalState_
{
    bool running;
} Win32GlobalState;

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

typedef struct UnlitScene_
{
    Mesh sp_mesh;
    VertexBuffer sp_vb;

    // Shaders
    GLuint unlit;

    // Uniform Buffer Objects
    PerFrameUniform per_frame;
    GLuint per_frame_ubo;
    PerObjectUniform per_object;
    GLuint per_object_ubo;
} UnlitScene;

SCENE_INIT_FN_SIG(unlit_scene_init)
{
    UnlitScene* scene = (UnlitScene*)calloc(1, sizeof(*scene));
    scene->sp_mesh = rc_mesh_make_sphere(2, 64, 64);
    r_vb_init(&scene->sp_vb, &scene->sp_mesh, GL_TRIANGLES);

    char* shared_src = win32_load_text_file("shaders/shared.glsl");
    char* vs_src = win32_load_text_file("shaders/unlit.vert");
    char* fs_src = win32_load_text_file("shaders/unlit.frag");

    scene->unlit =
        rc_shader_load_from_source(vs_src, fs_src, NULL, &shared_src, 1);

    free(shared_src);
    free(vs_src);
    free(fs_src);

    scene->per_frame.view = mat4_identity();
    glGenBuffers(1, &scene->per_frame_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, scene->per_frame_ubo);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(scene->per_frame), NULL,
                 GL_DYNAMIC_DRAW);

    scene->per_object.model = mat4_translation((FVec3){0, 0, -5});
    scene->per_object.ambient = (FVec3){1, 0, 0};

    glGenBuffers(1, &scene->per_object_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, scene->per_object_ubo);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(scene->per_object), NULL,
                 GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    glUseProgram(scene->unlit);
    glUniformBlockBinding(scene->unlit,
                          glGetUniformBlockIndex(scene->unlit, "PerFrame"), 0);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, scene->per_frame_ubo);
    glUniformBlockBinding(scene->unlit,
                          glGetUniformBlockIndex(scene->unlit, "PerObject"), 1);
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, scene->per_object_ubo);

    return scene;
}

SCENE_CLEANUP_FN_SIG(unlit_scene_cleanup)
{
    UnlitScene* scene = (UnlitScene*)udata;
    rc_mesh_cleanup(&scene->sp_mesh);
    r_vb_cleanup(&scene->sp_vb);
    glDeleteProgram(scene->unlit);
    glDeleteBuffers(1, &scene->per_frame_ubo);
    glDeleteBuffers(1, &scene->per_object_ubo);
    free(scene);
}

SCENE_UPDATE_FN_SIG(unlit_scene_update)
{
    UnlitScene* scene = (UnlitScene*)udata;
    scene->per_frame.proj = mat4_persp(
        60, (float)input->window_size.x / (float)input->window_size.y, 0.1f,
        100);
    glViewport(0, 0, input->window_size.x, input->window_size.y);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glClearColor(0.3f, 0.3f, 0.5f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    glBindBuffer(GL_UNIFORM_BUFFER, scene->per_frame_ubo);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(scene->per_frame),
                    &scene->per_frame);
    glBindBuffer(GL_UNIFORM_BUFFER, scene->per_object_ubo);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(scene->per_object),
                    &scene->per_object);

    glDisable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glUseProgram(scene->unlit);
    r_vb_draw(&scene->sp_vb);
}

typedef struct RayTracerScene_
{
    Mesh fsq_mesh;
    VertexBuffer fsq_vb;
    GLuint fsq;
    GLuint fsq_tex;

    GLuint ray_tracer;
    RayTracerGlobalUniform ray_tracer_global;
    GLuint ray_tracer_global_ubo;

    float t;
    float orbit_deg_per_sec;
} RayTracerScene;

SCENE_INIT_FN_SIG(raytracer_scene_init)
{
    RayTracerScene* scene = (RayTracerScene*)calloc(1, sizeof(*scene));

    scene->fsq_mesh = rc_mesh_make_raw(4, 6);
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
        memcpy(scene->fsq_mesh.vertices, vertices, sizeof(vertices));
        memcpy(scene->fsq_mesh.indices, indices, sizeof(indices));
    }

    r_vb_init(&scene->fsq_vb, &scene->fsq_mesh, GL_TRIANGLES);

    char* shared_src = win32_load_text_file("shaders/shared.glsl");
    char* fsq_src[2] = {
        win32_load_text_file("shaders/fsq.vert"),
        win32_load_text_file("shaders/fsq.frag"),
    };

    scene->fsq = rc_shader_load_from_source(fsq_src[0], fsq_src[1], NULL,
                                            &shared_src, 1);

    // TODO: ARRAY_LENGTH
    for (int i = 0; i < 2; ++i)
        free(fsq_src[i]);
    free(shared_src);

    glGenTextures(1, &scene->fsq_tex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, scene->fsq_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, input->window_size.x,
                 input->window_size.y, 0, GL_RGBA, GL_FLOAT, NULL);
    glBindImageTexture(0, scene->fsq_tex, 0, GL_FALSE, 0, GL_WRITE_ONLY,
                       GL_RGBA32F);

    char* ray_tracer_src = win32_load_text_file("shaders/ray_tracer.comp");
    scene->ray_tracer =
        rc_shader_load_from_source(NULL, NULL, ray_tracer_src, NULL, 0);
    free(ray_tracer_src);

    scene->ray_tracer_global = (RayTracerGlobalUniform){
        .view = {.eye = {0, 0, 0},
                 .look = fvec3_normalize((FVec3){0, 0, -1}),
                 .dims = {10 * ((float)input->window_size.x /
                                (float)input->window_size.y),
                          10},
                 .dist = 5},
        .spheres =
            {
                {.c = {0, 0, 0}, .r = 5},
                {.c = {0, -105, 0}, .r = 100},
            },
        .spheres_count = 2,
    };

    glGenBuffers(1, &scene->ray_tracer_global_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, scene->ray_tracer_global_ubo);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(RayTracerGlobalUniform),
                 &scene->ray_tracer_global, GL_DYNAMIC_DRAW);
    glUseProgram(scene->ray_tracer);
    glUniformBlockBinding(scene->ray_tracer,
                          glGetUniformBlockIndex(scene->ray_tracer, "Global"),
                          0);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, scene->ray_tracer_global_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    glUseProgram(0);

    scene->orbit_deg_per_sec = 45;

    return scene;
}

SCENE_CLEANUP_FN_SIG(raytracer_scene_cleanup)
{
    RayTracerScene* scene = (RayTracerScene*)udata;

    rc_mesh_cleanup(&scene->fsq_mesh);
    r_vb_cleanup(&scene->fsq_vb);
    glDeleteProgram(scene->fsq);
    glDeleteTextures(1, &scene->fsq_tex);

    glDeleteProgram(scene->ray_tracer);
    glDeleteBuffers(1, &scene->ray_tracer_global_ubo);

    free(scene);
}

SCENE_UPDATE_FN_SIG(raytracer_scene_update)
{
    RayTracerScene* scene = (RayTracerScene*)udata;

    igText("haha");

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glClearColor(0.3f, 0.3f, 0.5f, 1.0f);
    glViewport(0, 0, input->window_size.x, input->window_size.y);

    scene->t += input->dt;
    scene->ray_tracer_global.view.eye.x =
        cosf(degtorad(scene->t * scene->orbit_deg_per_sec)) * 10;
    scene->ray_tracer_global.view.eye.z =
        sinf(degtorad(scene->t * scene->orbit_deg_per_sec)) * 10;
    scene->ray_tracer_global.view.look =
        fvec3_normalize(fvec3_mulf(scene->ray_tracer_global.view.eye, -1));
    scene->ray_tracer_global.spheres[0].c.y = 1 + sinf(scene->t);
    glBindBuffer(GL_UNIFORM_BUFFER, scene->ray_tracer_global_ubo);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(scene->ray_tracer_global),
                    &scene->ray_tracer_global);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    glUseProgram(scene->ray_tracer);
    glDispatchCompute((GLuint)input->window_size.x,
                      (GLuint)input->window_size.y, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    glEnable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glUseProgram(scene->fsq);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, scene->fsq_tex);
    r_vb_draw(&scene->fsq_vb);
}

int CALLBACK WinMain(HINSTANCE instance,
                     HINSTANCE prev_instance,
                     LPSTR cmdstr,
                     int cmd)
{
    d_set_print_callback(&win32_print);

    Win32App app = {0};
    ASSERT(win32_app_init(&app, instance, "Zen", 1280, 720, 32, 24, 8, 4, 6));

    int work_group_counts[3] = {0};
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &work_group_counts[0]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, &work_group_counts[1]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, &work_group_counts[2]);
    PRINTLN("Max global work group sizes: (%d,%d,%d)", work_group_counts[0],
            work_group_counts[1], work_group_counts[2]);
    int work_group_sizes[3] = {0};
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &work_group_sizes[0]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &work_group_sizes[1]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &work_group_sizes[2]);
    PRINTLN("Max local work group sizes: (%d,%d,%d)", work_group_sizes[0],
            work_group_sizes[1], work_group_sizes[2]);
    int max_work_group_invocations = 0;
    glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS,
                  &max_work_group_invocations);
    PRINTLN("Max local work group invocations: %d", max_work_group_invocations);

    r_gui_init();

    Input input = {0};
    win32_register_input(&input);

    Scene scene = {0};
    s_init(&scene, &input);
    s_switch_scene(&scene, EXAMPLE_LITERAL(normal_mapping));

    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    LARGE_INTEGER last_counter;
    QueryPerformanceCounter(&last_counter);

    bool running = true;
    while (running)
    {
        LARGE_INTEGER curr_counter;
        QueryPerformanceCounter(&curr_counter);
        input.dt = (float)(curr_counter.QuadPart - last_counter.QuadPart) /
                   (float)freq.QuadPart;
        last_counter = curr_counter;

        MSG msg = {0};
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                running = false;
                break;
            }

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        win32_update_input(&app);

        r_gui_new_frame(&input);

        s_update(&scene);

        r_gui_render();

        SwapBuffers(app.dc);
    }

    s_cleanup(&scene);

    r_gui_cleanup();

    win32_app_cleanup(&app);

    return 0;
}
