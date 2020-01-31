#include "example.h"
#include "scene.h"
#include "resource.h"
#include "app.h"
#include "util.h"
#include <stb_image.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct NormalMapping_
{
    Mesh mesh;
    VertexBuffer vb;
    GLuint normal_mapping_shader;
    GLuint normal_map;
    float t;
} NormalMapping;

EXAMPLE_INIT_FN_SIG(normal_mapping)
{
    Example* e = (Example*)e_example_make("normal_mapping", NormalMapping);
    NormalMapping* s = (NormalMapping*)e->scene;

    Vertex vertices[] = {
        {{-0.5f, 0, 0.5f}, {0, 0}},
        {{0.5f, 0, 0.5f}, {1, 0}},
        {{0.5f, 0, -0.5f}, {1, 1}},
        {{-0.5f, 0, -0.5f}, {0, 1}},
    };
    uint indices[] = {0, 1, 2, 2, 3, 0};
    s->mesh = rc_mesh_make_raw2(ARRAY_LENGTH(vertices), ARRAY_LENGTH(indices),
                                vertices, indices);
    r_vb_init(&s->vb, &s->mesh, GL_TRIANGLES);
    s->normal_mapping_shader = e_shader_load(e, "normal_mapping");
    s->normal_map = e_texture_load(e, "wall_normal.png");

    return e;
}

EXAMPLE_CLEANUP_FN_SIG(normal_mapping)
{
    Example* e = (Example*)udata;
    NormalMapping* scene = (NormalMapping*)e->scene;

    rc_mesh_cleanup(&scene->mesh);
    r_vb_cleanup(&scene->vb);
    glDeleteProgram(scene->normal_mapping_shader);
    glDeleteTextures(1, &scene->normal_map);

    e_example_destroy(e);
}

EXAMPLE_UPDATE_FN_SIG(normal_mapping)
{
    Example* e = (Example*)udata;
    NormalMapping* scene = (NormalMapping*)e->scene;

    scene->t += input->dt;

    ExamplePerFrameUBO per_frame = {0};

    per_frame.proj = mat4_persp(
        60, (float)input->window_size.x / (float)input->window_size.y, 0.1f,
        100);
    per_frame.view =
        mat4_lookat((FVec3){0, 1, 1}, (FVec3){0}, (FVec3){0, 1, 0});
    per_frame.t = scene->t;
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

    glUseProgram(scene->normal_mapping_shader);
    glBindTextureUnit(0, scene->normal_map);
    r_vb_draw(&scene->vb);
}