#include "scene.h"
#include "resource.h"
#include "debug.h"
#include "app.h"
#include <himath.h>
#include <glad/glad.h>
#include <stdlib.h>
#include <math.h>

typedef struct PerFrame_
{
    Mat4 mvp;
} PerFrame;

typedef struct HelloMesh_
{
    Mesh mesh;
    VertexBuffer vb;
    GLuint gooch_shader;
    GLuint per_frame_ubo;
    float t;
} HelloMesh;

SCENE_INIT_FN_SIG(hello_mesh_init)
{
    HelloMesh* scene = (HelloMesh*)calloc(1, sizeof(*scene));

    ASSERT(rc_mesh_load_from_obj(&scene->mesh, "shared/models/dragon.obj"));
    r_vb_init(&scene->vb, &scene->mesh, GL_TRIANGLES);

    scene->gooch_shader = rc_shader_load_from_files(
        "hello_mesh/gooch.vert", "hello_mesh/gooch.frag", NULL, NULL, 0);

    glGenBuffers(1, &scene->per_frame_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, scene->per_frame_ubo);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(PerFrame), NULL, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, scene->per_frame_ubo);

    return scene;
}

SCENE_CLEANUP_FN_SIG(hello_mesh_cleanup)
{
    HelloMesh* scene = (HelloMesh*)udata;

    rc_mesh_cleanup(&scene->mesh);
    r_vb_cleanup(&scene->vb);
    glDeleteProgram(scene->gooch_shader);

    free(scene);
}

SCENE_UPDATE_FN_SIG(hello_mesh_update)
{
    HelloMesh* scene = (HelloMesh*)udata;

    scene->t += input->dt;

    PerFrame per_frame = {0};
    Mat4 proj = mat4_persp(
        60, (float)input->window_size.x / (float)input->window_size.y, 0.1f,
        100);
    Mat4 view = mat4_lookat((FVec3){sinf(scene->t), 0, cosf(scene->t)},
                            (FVec3){0}, (FVec3){0, 1, 0});

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
    glUseProgram(scene->gooch_shader);
    r_vb_draw(&scene->vb);
}