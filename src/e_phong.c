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
    GLuint depth_shader;
    ExamplePhongLight lights[5];
    int lights_count;
    float t;

    ExampleFpsCamera cam;

    bool draw_depth;
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
    s->diffuse_map = e_texture_load(e, "box_diffuse.png");
    s->specular_map = e_texture_load(e, "box_specular.png");

    s->light_source_mesh = rc_mesh_make_sphere(0.3f, 32, 32);
    r_vb_init(&s->light_source_vb, &s->light_source_mesh, GL_TRIANGLES);

    s->unlit_shader = e_shader_load(e, "unlit");
    s->phong_shader = e_shader_load(e, "phong");
    s->depth_shader = e_shader_load(e, "depth");

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

    for (int i = 0; i < ARRAY_LENGTH(point_light_colors); i++)
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

    s->lights_count = ARRAY_LENGTH(point_light_colors) + 1;

    s->cam.pos.z = 3;

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
    glDeleteProgram(s->depth_shader);
    e_example_destroy(e);
}

void draw_cubes(const Example* e, const Phong* s, bool is_outline)
{
    FVec3 cube_positions[] = {{0.0f, 0.0f, 0.0f},    {2.0f, 5.0f, -15.0f},
                              {-1.5f, -2.2f, -2.5f}, {-3.8f, -2.0f, -12.3f},
                              {2.4f, -0.4f, -3.5f},  {-1.7f, 3.0f, -7.5f},
                              {1.3f, -2.0f, -2.5f},  {1.5f, 2.0f, -2.5f},
                              {1.5f, 0.2f, -1.5f},   {-1.3f, 1.0f, -1.5f}};
    for (int i = 0; i < ARRAY_LENGTH(cube_positions); i++)
    {
        ExamplePerObjectUBO per_object = {0};
        Mat4 trans = mat4_translation(cube_positions[i]);
        float angle = (float)(i + 1) * 20.f * s->t;
        Mat4 scale = is_outline ? mat4_scale(1.1f) : mat4_scale(1);
        Mat4 rot = quat_to_matrix(quat_rotate((FVec3){1, 0.3f, 0.5f}, angle));
        per_object.model = mat4_mul(&rot, &scale);
        per_object.model = mat4_mul(&trans, &per_object.model);
        per_object.color = (FVec3){1, 0, 0};
        e_apply_per_object_ubo(e, &per_object);
        r_vb_draw(&s->plane_vb);
    }
}

EXAMPLE_UPDATE_FN_SIG(phong)
{
    Example* e = (Example*)udata;
    Phong* s = (Phong*)e->scene;
    s->t += input->dt;

    e_fpscam_update(&s->cam, input, 5);

    igSetNextWindowSize((ImVec2){300, (float)input->window_size.y},
                        ImGuiCond_Once);
    igSetNextWindowPos((ImVec2){0, 0}, ImGuiCond_Once, (ImVec2){0, 0});
    if (igBegin("Control Panel", NULL,
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
    {
        if (igCollapsingHeader("Depth Control", ImGuiTreeNodeFlags_DefaultOpen))
        {
            igCheckbox("Draw depth buffer", &s->draw_depth);
        }
    }

    igEnd();

    ExamplePerFrameUBO per_frame = {0};

    per_frame.proj = mat4_persp(
        60, (float)input->window_size.x / (float)input->window_size.y, 0.1f,
        100);
    FVec3 view_pos = {0, 0, 3};
    per_frame.view = mat4_lookat(
        s->cam.pos, fvec3_add(s->cam.pos, e_fpscam_get_look(&s->cam)),
        (FVec3){0, 1, 0});
    for (int i = 0; i < s->lights_count; i++)
        per_frame.phong_lights[i] = s->lights[i];
    per_frame.phong_lights_count = s->lights_count;
    per_frame.view_pos = view_pos;
    per_frame.t = s->t;
    e_apply_per_frame_ubo(e, &per_frame);

    glEnable(GL_DEPTH_TEST);
    glClearDepth(1);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    glEnable(GL_STENCIL_TEST);
    glClearStencil(0);
    glStencilMask(0xFF);
    glStencilFunc(GL_EQUAL, 1, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

    glClearColor(0.1f, 0.1f, 0.1f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    glStencilMask(0x00);
    glStencilFunc(GL_ALWAYS, 1, 0xFF);
    if (s->draw_depth)
        glUseProgram(s->depth_shader);
    else
        glUseProgram(s->unlit_shader);
    for (int i = 1; i < s->lights_count; i++)
    {
        ExamplePerObjectUBO per_object = {0};
        per_object.model = mat4_translation(s->lights[i].pos_or_dir);
        per_object.color = s->lights[i].diffuse;
        e_apply_per_object_ubo(e, &per_object);
        r_vb_draw(&s->light_source_vb);
    }

    if (s->draw_depth)
        glUseProgram(s->depth_shader);
    else
        glUseProgram(s->phong_shader);

    glBindTextureUnit(0, s->diffuse_map);
    glBindTextureUnit(1, s->specular_map);

    glStencilMask(0xFF);
    glStencilFunc(GL_ALWAYS, 1, 0xFF);
    glStencilOp(GL_KEEP, GL_REPLACE, GL_REPLACE);
    draw_cubes(e, s, false);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

    glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
    glStencilMask(0x00);
    glDisable(GL_DEPTH_TEST);
    glUseProgram(s->unlit_shader);
    draw_cubes(e, s, true);
    glEnable(GL_DEPTH_TEST);
}
