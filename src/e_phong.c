#include "example.h"
#include "resource.h"
#include "renderer.h"
#include "util.h"
#include "app.h"
#include <math.h>
#include <assert.h>

typedef struct Phong_
{
    Mesh plane_mesh;
    VertexBuffer plane_vb;
    GLuint diffuse_map;
    GLuint specular_map;
    Mesh light_source_mesh;
    VertexBuffer light_source_vb;
    GLuint unlit_shader;
    GLuint phong_shader;
    GLuint blinn_shader;
    ExamplePhongLight lights[5];
    int lights_count;
    float t;
} Phong;

EXAMPLE_INIT_FN_SIG(phong)
{
    Example* e = (Example*)e_example_make("phong", sizeof(Phong));
    Phong* s = (Phong*)e->scene;

#if 0
    Vertex vertices[] = {
        {{-0.5f, 0, 0.5f}, {0, 0}, {0, 1, 0}},
        {{0.5f, 0, 0.5f}, {1, 0}, {0, 1, 0}},
        {{0.5f, 0, -0.5f}, {1, 1}, {0, 1, 0}},
        {{-0.5f, 0, -0.5f}, {0, 1}, {0, 1, 0}},
    };
    uint indices[] = {0, 1, 2, 2, 3, 0};
    s->plane_mesh = rc_mesh_make_raw2(ARRAY_LENGTH(vertices),
                                      ARRAY_LENGTH(indices), vertices, indices);
#endif
    s->plane_mesh = rc_mesh_make_cube();
    r_vb_init(&s->plane_vb, &s->plane_mesh, GL_TRIANGLES);
    s->diffuse_map = e_texture_load(e, "diffuse_map.png");
    s->specular_map = e_texture_load(e, "specular_map.png");

    s->light_source_mesh = rc_mesh_make_sphere(0.3f, 32, 32);
    r_vb_init(&s->light_source_vb, &s->light_source_mesh, GL_TRIANGLES);

    s->unlit_shader = e_shader_load(e, "unlit");
    s->phong_shader = e_shader_load(e, "phong");
    s->blinn_shader = e_shader_load(e, "blinn");

    s->lights[0] = (ExamplePhongLight){
        .type = ExamplePhongLightType_Directional,
        .ambient = {0.3f, 0.24f, 0.14f},
        .diffuse = {0.7f, 0.42f, 0.26f},
        .specular = {0.5f, 0.5f, 0.5f},
        .pos_or_dir = {-0.2f, -1, -0.3f},
    };

    FVec3 point_light_positions[] = {
        {0.7f, 0.2f, 2},
        {2.3f, -3.3f, -4},
        {-4, 2, -12},
        {0, 0, -3},
    };
    FVec3 point_light_colors[] = {
        {1, 0.6f, 0},
        {1, 0, 0},
        {1, 1, 0},
        {0.2f, 0.2f, 1},
    };
    static_assert(ARRAY_LENGTH(point_light_positions) ==
                      ARRAY_LENGTH(point_light_colors),
                  "positions and colors should have same number of elements");

    for (int i = 0; i < (int)ARRAY_LENGTH(point_light_colors); i++)
    {
        s->lights[i + 1] = (ExamplePhongLight){
            .type = ExamplePhongLightType_Point,
            .ambient = fvec3_mulf(point_light_colors[i], 0.1f),
            .diffuse = point_light_colors[i],
            .specular = point_light_colors[i],
            .pos_or_dir = point_light_positions[i],
            .linear = 0.09f,
            .quadratic = 0.032f,
        };
    }

    s->lights_count = (int)ARRAY_LENGTH(point_light_colors) + 1;

    return e;
}

EXAMPLE_CLEANUP_FN_SIG(phong)
{
    Example* e = (Example*)udata;
    Phong* s = (Phong*)e->scene;
    rc_mesh_cleanup(&s->plane_mesh);
    r_vb_cleanup(&s->plane_vb);
    glDeleteTextures(1, &s->diffuse_map);
    glDeleteTextures(1, &s->specular_map);
    rc_mesh_cleanup(&s->light_source_mesh);
    r_vb_cleanup(&s->light_source_vb);
    glDeleteProgram(s->unlit_shader);
    glDeleteProgram(s->phong_shader);
    glDeleteProgram(s->blinn_shader);
    e_example_destroy(e);
}

EXAMPLE_UPDATE_FN_SIG(phong)
{
    Example* e = (Example*)udata;
    Phong* s = (Phong*)e->scene;
    s->t += input->dt;

    ExamplePerFrameUBO per_frame = {0};

    per_frame.proj = mat4_persp(
        60, (float)input->window_size.x / (float)input->window_size.y, 0.1f,
        100);
    FVec3 view_pos = {0, 0, 3};
    per_frame.view = mat4_lookat(view_pos, (FVec3){0}, (FVec3){0, 1, 0});
    for (int i = 0; i < s->lights_count; i++)
        per_frame.phong_lights[i] = s->lights[i];
    per_frame.phong_lights_count = s->lights_count;
    per_frame.view_pos = view_pos;
    per_frame.t = s->t;
    e_apply_per_frame_ubo(e, &per_frame);

    glClearColor(0.1f, 0.1f, 0.1f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    glUseProgram(s->blinn_shader);
    glBindTextureUnit(0, s->diffuse_map);
    glBindTextureUnit(1, s->specular_map);

    {
        FVec3 cube_positions[] = {{0.0f, 0.0f, 0.0f},    {2.0f, 5.0f, -15.0f},
                                  {-1.5f, -2.2f, -2.5f}, {-3.8f, -2.0f, -12.3f},
                                  {2.4f, -0.4f, -3.5f},  {-1.7f, 3.0f, -7.5f},
                                  {1.3f, -2.0f, -2.5f},  {1.5f, 2.0f, -2.5f},
                                  {1.5f, 0.2f, -1.5f},   {-1.3f, 1.0f, -1.5f}};
        for (int i = 0; i < (int)ARRAY_LENGTH(cube_positions); i++)
        {
            ExamplePerObjectUBO per_object = {0};
            Mat4 trans = mat4_translation(cube_positions[i]);
            float angle = (float)(i + 1) * 20.f * s->t;
            Mat4 rot =
                quat_to_matrix(quat_rotate((FVec3){1, 0.3f, 0.5f}, angle));
            per_object.model = mat4_mul(&trans, &rot);
            per_object.color = (FVec3){1, 0, 0};
            e_apply_per_object_ubo(e, &per_object);
            r_vb_draw(&s->plane_vb);
        }
    }

    glUseProgram(s->unlit_shader);
    for (int i = 1; i < s->lights_count; i++)
    {
        ExamplePerObjectUBO per_object = {0};
        per_object.model = mat4_translation(s->lights[i].pos_or_dir);
        per_object.color = s->lights[i].diffuse;
        e_apply_per_object_ubo(e, &per_object);
        r_vb_draw(&s->light_source_vb);
    }
}
