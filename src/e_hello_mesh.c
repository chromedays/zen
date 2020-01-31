#include "scene.h"
#include "resource.h"
#include "debug.h"
#include "app.h"
#include "example.h"
#include <himath.h>
#include <glad/glad.h>
#include <stdlib.h>
#include <math.h>

typedef struct HelloMesh_
{
    Mesh mesh;
    VertexBuffer vb;
    GLuint debug_shader;
    float t;
} HelloMesh;

EXAMPLE_INIT_FN_SIG(hello_mesh)
{
    Example* e = (Example*)e_example_make("hello_mesh", HelloMesh);
    HelloMesh* scene = (HelloMesh*)e->scene;

    ASSERT(rc_mesh_load_from_obj(&scene->mesh, "shared/models/dragon.obj"));
    r_vb_init(&scene->vb, &scene->mesh, GL_TRIANGLES);

    scene->debug_shader = e_shader_load(e, "debug");

    return e;
}

EXAMPLE_CLEANUP_FN_SIG(hello_mesh)
{
    Example* e = (Example*)udata;
    HelloMesh* scene = (HelloMesh*)e->scene;

    rc_mesh_cleanup(&scene->mesh);
    r_vb_cleanup(&scene->vb);
    glDeleteProgram(scene->debug_shader);

    e_example_destroy(e);
}

EXAMPLE_UPDATE_FN_SIG(hello_mesh)
{
    Example* e = (Example*)udata;
    HelloMesh* scene = (HelloMesh*)e->scene;

    scene->t += input->dt;

    ExamplePerFrameUBO per_frame = {0};
    per_frame.proj = mat4_persp(
        60, (float)input->window_size.x / (float)input->window_size.y, 0.1f,
        100);
    per_frame.view = mat4_lookat((FVec3){sinf(scene->t), 0, cosf(scene->t)},
                                 (FVec3){0}, (FVec3){0, 1, 0});
    e_apply_per_frame_ubo(e, &per_frame);

    ExamplePerObjectUBO per_object = {0};
    per_object.model = mat4_identity();
    e_apply_per_object_ubo(e, &per_object);

    glClearColor(0.1f, 0.1f, 0.1f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glUseProgram(scene->debug_shader);
    r_vb_draw(&scene->vb);
}
