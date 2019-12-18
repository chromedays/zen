#include "scene.h"
#include "resource.h"
#include "app.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct PerFrame_
{
    Mat4 mvp;
} PerFrame;

typedef struct NormalMapping_
{
    Mesh mesh;
    VertexBuffer vb;
    GLuint normal_mapping_shader;
    GLuint per_frame_ubo;
    float t;
} NormalMapping;

SCENE_INIT_FN_SIG(normal_mapping_init)
{
    NormalMapping* scene = (NormalMapping*)calloc(1, sizeof(*scene));

    scene->mesh = rc_mesh_make_raw(4, 6);
    {
        Vertex vertices[] = {
            {{-0.5f, 0, 0.5f}, {0, 0}},
            {{0.5f, 0, 0.5f}, {1, 0}},
            {{0.5f, 0, -0.5f}, {1, 1}},
            {{-0.5f, 0, -0.5f}, {0, 1}},
        };
        uint indices[] = {0, 1, 2, 2, 3, 0};
        memcpy(scene->mesh.vertices, vertices, sizeof(vertices));
        memcpy(scene->mesh.indices, indices, sizeof(indices));
    }

    r_vb_init(&scene->vb, &scene->mesh, GL_TRIANGLES);

    scene->normal_mapping_shader = rc_shader_load_from_files(
        "normal_mapping/normal_mapping.vert",
        "normal_mapping/normal_mapping.frag", NULL, NULL, 0);

    glGenBuffers(1, &scene->per_frame_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, scene->per_frame_ubo);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(PerFrame), NULL, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, scene->per_frame_ubo);

    return scene;
}

SCENE_CLEANUP_FN_SIG(normal_mapping_cleanup)
{
    NormalMapping* scene = (NormalMapping*)udata;

    rc_mesh_cleanup(&scene->mesh);
    r_vb_cleanup(&scene->vb);
    glDeleteProgram(scene->normal_mapping_shader);
    glDeleteBuffers(1, &scene->per_frame_ubo);

    free(scene);
}

SCENE_UPDATE_FN_SIG(normal_mapping_update)
{
    NormalMapping* scene = (NormalMapping*)udata;

    PerFrame per_frame = {0};

    Mat4 proj = mat4_persp(
        60, (float)input->window_size.x / (float)input->window_size.y, 0.1f,
        100);
#if 0
    Mat4 view = mat4_lookat((FVec3){sinf(scene->t), 1, cosf(scene->t)},
                            (FVec3){0}, (FVec3){0, 1, 0});
#else
    Mat4 view = mat4_lookat((FVec3){0, 1, 1}, (FVec3){0}, (FVec3){0, 1, 0});
#endif

    per_frame.mvp = mat4_mul(&proj, &view);
    glBindBuffer(GL_UNIFORM_BUFFER, scene->per_frame_ubo);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(per_frame), &per_frame);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    glClearColor(0.1f, 0.1f, 0.1f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glUseProgram(scene->normal_mapping_shader);
    r_vb_draw(&scene->vb);
}