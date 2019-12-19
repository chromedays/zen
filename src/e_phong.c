#include "example.h"
#include "resource.h"
#include "renderer.h"
#include "util.h"
#include "app.h"

typedef struct Phong_
{
    Mesh mesh;
    VertexBuffer vb;
    GLuint diffuse_map;
    GLuint specular_map;
    GLuint phong_shader;
    GLuint blinn_shader;
    float t;
} Phong;

EXAMPLE_INIT_FN_SIG(phong)
{
    Example* e = (Example*)e_example_make("phong", sizeof(Phong));
    Phong* s = (Phong*)e->scene;

    Vertex vertices[] = {
        {{-0.5f, 0, 0.5f}, {0, 0}, {0, 1, 0}},
        {{0.5f, 0, 0.5f}, {1, 0}, {0, 1, 0}},
        {{0.5f, 0, -0.5f}, {1, 1}, {0, 1, 0}},
        {{-0.5f, 0, -0.5f}, {0, 1}, {0, 1, 0}},
    };
    uint indices[] = {0, 1, 2, 2, 3, 0};
    s->mesh = rc_mesh_make_raw2(ARRAY_LENGTH(vertices), ARRAY_LENGTH(indices),
                                vertices, indices);
    r_vb_init(&s->vb, &s->mesh, GL_TRIANGLES);
    s->diffuse_map = e_texture_load(e, "diffuse_map.png");
    s->specular_map = e_texture_load(e, "specular_map.png");
    s->phong_shader = e_shader_load(e, "phong");
    s->blinn_shader = e_shader_load(e, "blinn");

    return e;
}

EXAMPLE_CLEANUP_FN_SIG(phong)
{
    Example* e = (Example*)udata;
    Phong* s = (Phong*)e->scene;
    rc_mesh_cleanup(&s->mesh);
    r_vb_cleanup(&s->vb);
    glDeleteTextures(1, &s->diffuse_map);
    glDeleteTextures(1, &s->specular_map);
    glDeleteProgram(s->phong_shader);
    glDeleteProgram(s->blinn_shader);
    e_example_destroy(e);
}

EXAMPLE_UPDATE_FN_SIG(phong)
{
    Example* e = (Example*)udata;
    Phong* s = (Phong*)e->scene;

    ExamplePerFrameUBO per_frame = {0};

    per_frame.proj = mat4_persp(
        60, (float)input->window_size.x / (float)input->window_size.y, 0.1f,
        100);
    per_frame.view =
        mat4_lookat((FVec3){0, 1, 1}, (FVec3){0}, (FVec3){0, 1, 0});
    per_frame.phong_lights[0] = (ExamplePhongLight){
        .type = ExamplePhongLightType_Directional,
        .ambient = {1, 0, 0},
        .diffuse = {0, 1, 0},
        .specular = {0, 0, 1},
        .pos_or_dir = {0.5f, 0, 0},
        .inner_angle = 0.1f,
        .outer_angle = 0.3f,
        .falloff = 0.5f,
        .linear = 0.7f,
        .quadratic = 0.9f,
    };
    per_frame.phong_lights_count = 1;
    per_frame.t = s->t;
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

    glUseProgram(s->blinn_shader);
    glBindTextureUnit(0, s->diffuse_map);
    glBindTextureUnit(1, s->specular_map);

    r_vb_draw(&s->vb);
}
