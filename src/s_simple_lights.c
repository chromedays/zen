#include "scene.h"
#include "resource.h"
#include "renderer.h"
#include "debug.h"
#include "app.h"
#include "example.h"
#include <stdlib.h>
#include <math.h>

typedef struct SimpleLights_
{
    Mesh mesh;
    VertexBuffer vb;
    GLuint gooch_shader;
    float t;
} SimpleLights;

EXAMPLE_INIT_FN_SIG(simple_lights)
{
    Example* e = e_example_make("simple_lights", sizeof(SimpleLights));
    SimpleLights* scene = (SimpleLights*)e->scene;

    ASSERT(rc_mesh_load_from_obj(&scene->mesh, "shared/models/dragon.obj"));
    r_vb_init(&scene->vb, &scene->mesh, GL_TRIANGLES);

    scene->gooch_shader = e_shader_load(e, "gooch");

    return e;
}

EXAMPLE_CLEANUP_FN_SIG(simple_lights)
{
    Example* e = (Example*)udata;
    SimpleLights* scene = (SimpleLights*)e->scene;

    rc_mesh_cleanup(&scene->mesh);
    r_vb_cleanup(&scene->vb);
    glDeleteProgram(scene->gooch_shader);

    e_example_destroy(e);
}

EXAMPLE_UPDATE_FN_SIG(simple_lights)
{
    Example* e = (Example*)udata;
    SimpleLights* scene = (SimpleLights*)e->scene;

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
    glUseProgram(scene->gooch_shader);
    r_vb_draw(&scene->vb);
}
