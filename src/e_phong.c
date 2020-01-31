#include "example.h"
#include "resource.h"
#include "renderer.h"
#include "util.h"
#include "app.h"
#include <math.h>
#include <assert.h>

typedef struct Phong_
{
    Mesh cube_mesh;
    VertexBuffer cube_vb;
    GLuint diffuse_map;
    GLuint specular_map;

    Mesh quad_mesh;
    VertexBuffer transparent_vb;
    GLuint grass_texture;
    GLuint window_texture;
    GLuint transparent_shader;

    Mesh light_source_mesh;
    VertexBuffer light_source_vb;
    GLuint unlit_shader;
    GLuint phong_shader;
    GLuint depth_shader;
    ExamplePhongLight lights[5];
    int lights_count;
    float t;

    ExampleFpsCamera cam;

    bool debug_transparent_order;
} Phong;

EXAMPLE_INIT_FN_SIG(phong)
{
    Example* e = (Example*)e_example_make("phong", Phong);
    Phong* s = (Phong*)e->scene;

    s->cube_mesh = rc_mesh_make_cube();
    r_vb_init(&s->cube_vb, &s->cube_mesh, GL_TRIANGLES);
    s->diffuse_map = e_texture_load(e, "box_diffuse.png");
    s->specular_map = e_texture_load(e, "box_specular.png");

    s->quad_mesh = rc_mesh_make_quad();
    r_vb_init(&s->transparent_vb, &s->quad_mesh, GL_TRIANGLES);
    s->grass_texture = e_texture_load(e, "grass.png");
    s->window_texture = e_texture_load(e, "transparent_window.png");
    s->transparent_shader = e_shader_load(e, "transparent");

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
        {0.7f, 3.2f, 2},
        {2.3f, -3.3f, -4},
        {-4, 2, -12},
        {0, -1.5f, -3},
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
    rc_mesh_cleanup(&s->cube_mesh);
    r_vb_cleanup(&s->cube_vb);
    glDeleteTextures(1, &s->diffuse_map);
    glDeleteTextures(1, &s->specular_map);
    rc_mesh_cleanup(&s->quad_mesh);
    r_vb_cleanup(&s->transparent_vb);
    glDeleteTextures(1, &s->grass_texture);
    glDeleteProgram(s->transparent_shader);
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
        r_vb_draw(&s->cube_vb);
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
        igCheckbox("Debug transparent draw order", &s->debug_transparent_order);
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

    glEnable(GL_MULTISAMPLE);

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
    glDisable(GL_STENCIL_TEST);

    glUseProgram(s->unlit_shader);
    for (int i = 1; i < s->lights_count; i++)
    {
        ExamplePerObjectUBO per_object = {0};
        per_object.model = mat4_translation(s->lights[i].pos_or_dir);
        per_object.color = s->lights[i].diffuse;
        e_apply_per_object_ubo(e, &per_object);
        r_vb_draw(&s->light_source_vb);
    }

    // Draw plane
    {
        glUseProgram(s->phong_shader);
        glBindTextureUnit(0, s->diffuse_map);
        glBindTextureUnit(1, s->specular_map);
        ExamplePerObjectUBO per_object = {0};
        Mat4 trans = mat4_translation((FVec3){0, -1, 0});
        Mat4 scale = mat4_scalev((FVec3){10, 1, 10});
        per_object.model = mat4_mul(&trans, &scale);
        e_apply_per_object_ubo(e, &per_object);
        r_vb_draw(&s->cube_vb);
    }

    // Draw cubes
    {
        FVec3 positions[] = {
            {-1.5f, 0, -0.03f},
            {1.5f, 0, 0},
        };
        glUseProgram(s->phong_shader);
        glBindTextureUnit(0, s->diffuse_map);
        glBindTextureUnit(1, s->specular_map);
        for (int i = 0; i < ARRAY_LENGTH(positions); i++)
        {
            ExamplePerObjectUBO per_object = {0};
            per_object.model = mat4_translation(positions[i]);
            e_apply_per_object_ubo(e, &per_object);
            r_vb_draw(&s->cube_vb);
        }
    }

    // Draw transparent objects
    {
        glDisable(GL_CULL_FACE);

        FVec3 positions[] = {
            {-1.5f, 0, 0.48f}, {1.5f, 0, 0.51f}, {0, 0, 0.7f},
            {-0.3f, 0, 2.3f},  {0.5f, 0, -0.6f},
        };

        for (int i = 0; i < ARRAY_LENGTH(positions); i++)
        {
            for (int j = i + 1; j < ARRAY_LENGTH(positions); j++)
            {
                float a = fvec3_length_sq(fvec3_sub(s->cam.pos, positions[i]));
                float b = fvec3_length_sq(fvec3_sub(s->cam.pos, positions[j]));
                if (a < b)
                {
                    FVec3 temp = positions[i];
                    positions[i] = positions[j];
                    positions[j] = temp;
                }
            }
        }

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        if (s->debug_transparent_order)
            glUseProgram(s->unlit_shader);
        else
            glUseProgram(s->transparent_shader);
        glBindTextureUnit(0, s->window_texture);
        for (int i = 0; i < ARRAY_LENGTH(positions); i++)
        {
            ExamplePerObjectUBO per_object = {0};
            per_object.model = mat4_translation(positions[i]);
            float c = 1.f - (float)i / (float)(ARRAY_LENGTH(positions) - 1);
            per_object.color = (FVec3){c, c, c};
            e_apply_per_object_ubo(e, &per_object);
            r_vb_draw(&s->transparent_vb);
        }

        glEnable(GL_CULL_FACE);
    }
}
