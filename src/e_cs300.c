#include "example.h"
#include "resource.h"
#include "filesystem.h"
#include "debug.h"
#include "util.h"
#include "app.h"
#include <histr.h>
#include <stdlib.h>
#include <math.h>

#define MAX_MODEL_FILES_COUNT 100
#define MAX_LIGHT_SOURCES_COUNT 10

typedef struct LightSource_
{
    FVec3 pos;
    FVec3 color;
} LightSource;

typedef struct CS300_
{
    Path model_file_paths[MAX_MODEL_FILES_COUNT];
    int model_file_paths_count;

    int current_model_index;
    Mesh model_mesh;
    VertexBuffer model_vb;
    FVec3 model_pos;
    float model_scale;
    uint model_shader;

    uint normal_debug_shader;

    Mesh light_source_mesh;
    VertexBuffer light_source_vb;
    LightSource light_sources[MAX_LIGHT_SOURCES_COUNT];
    int light_sources_count;
    uint light_source_shader;

    float orbit_speed_deg;
    float orbit_radius;

    float camera_distance;

    float t;
} CS300;

static FILE_FOREACH_FN_DECL(push_model_filename)
{
    CS300* s = (CS300*)udata;
    ASSERT(s->model_file_paths_count <= ARRAY_LENGTH(s->model_file_paths));
    s->model_file_paths[s->model_file_paths_count++] = fs_path_copy(*file_path);
}

static void try_switch_model(CS300* s, int new_model_index)
{
    if (s->current_model_index != new_model_index)
    {
        ASSERT((new_model_index >= 0) &&
               (new_model_index < s->model_file_paths_count));

        if (s->current_model_index >= 0)
        {
            rc_mesh_cleanup(&s->model_mesh);
            r_vb_cleanup(&s->model_vb);
        }

        rc_mesh_load_from_obj(
            &s->model_mesh, s->model_file_paths[new_model_index].abs_path_str);
        // Need to compute normals manually
        if (fvec3_length_sq(s->model_mesh.vertices[0].normal) == 0.f)
        {
            if (s->model_mesh.indices_count != 0)
            {
                for (int i = 0; i < s->model_mesh.indices_count; i += 3)
                {
                    Vertex* v0 =
                        &s->model_mesh.vertices[s->model_mesh.indices[i]];
                    Vertex* v1 =
                        &s->model_mesh.vertices[s->model_mesh.indices[i + 1]];
                    Vertex* v2 =
                        &s->model_mesh.vertices[s->model_mesh.indices[i + 2]];

                    FVec3 e0 = fvec3_sub(v1->pos, v0->pos);
                    FVec3 e1 = fvec3_sub(v2->pos, v0->pos);

                    // Normal with weight
                    FVec3 weight = fvec3_cross(e0, e1);

                    v0->normal = fvec3_add(v0->normal, weight);
                    v1->normal = fvec3_add(v1->normal, weight);
                    v2->normal = fvec3_add(v2->normal, weight);
                }
            }
            else
            {
                for (int i = 0; i < s->model_mesh.vertices_count; i += 3)
                {
                    Vertex* v0 = &s->model_mesh.vertices[i];
                    Vertex* v1 = &s->model_mesh.vertices[i + 1];
                    Vertex* v2 = &s->model_mesh.vertices[i + 2];

                    FVec3 e0 = fvec3_sub(v1->pos, v0->pos);
                    FVec3 e1 = fvec3_sub(v2->pos, v0->pos);

                    // Normal with weight
                    FVec3 weight = fvec3_cross(e0, e1);

                    v0->normal = fvec3_add(v0->normal, weight);
                    v1->normal = fvec3_add(v1->normal, weight);
                    v2->normal = fvec3_add(v2->normal, weight);
                }
            }

            for (int i = 0; i < s->model_mesh.vertices_count; i++)
            {
                Vertex* v = &s->model_mesh.vertices[i];
                v->normal = fvec3_normalize(v->normal);
            }
        }

        FVec3 bb_min = s->model_mesh.vertices[0].pos;
        FVec3 bb_max = s->model_mesh.vertices[0].pos;
        for (int j = 1; j < s->model_mesh.vertices_count; j++)
        {
            FVec3 pos = s->model_mesh.vertices[j].pos;
            if (bb_min.x > pos.x)
                bb_min.x = pos.x;
            if (bb_min.y > pos.y)
                bb_min.y = pos.y;
            if (bb_min.z > pos.z)
                bb_min.z = pos.z;

            if (bb_max.x < pos.x)
                bb_max.x = pos.x;
            if (bb_max.y < pos.y)
                bb_max.y = pos.y;
            if (bb_max.z < pos.z)
                bb_max.z = pos.z;
        }

        s->model_scale = 1.f / fvec3_length(fvec3_sub(bb_max, bb_min));
        s->model_pos = fvec3_negate(fvec3_mulf(
            fvec3_mulf(fvec3_add(bb_min, bb_max), 0.5f), s->model_scale));

        r_vb_init(&s->model_vb, &s->model_mesh, GL_TRIANGLES);

        s->current_model_index = new_model_index;
    }
}

float get_channel_function1(float x, float period)
{
    float coeff = (HIMATH_PI * 2.f) / period;
    return (0.5f * cosf(coeff * x) + 0.5f);
}

float get_channel_function2(float x, float period)
{
    float coeff = (HIMATH_PI * 2.f) / period;
    return (0.5f * cosf(coeff * (x + period * 0.5f)) + 0.5f);
}

EXAMPLE_INIT_FN_SIG(cs300)
{
    Example* e = e_example_make("cs300", sizeof(CS300));
    CS300* s = (CS300*)e->scene;

    Path model_root_path = fs_path_make_working_dir();
    fs_path_append2(&model_root_path, "shared", "models");
    fs_for_each_files_with_ext(&model_root_path, "obj", &push_model_filename,
                               s);
    fs_path_cleanup(&model_root_path);

    s->current_model_index = -1;
    try_switch_model(s, 0);
    s->model_shader = e_shader_load(e, "phong");

    s->normal_debug_shader = e_shader_load(e, "visualize_normals");

    s->light_source_mesh = rc_mesh_make_sphere(0.05f, 32, 32);
    r_vb_init(&s->light_source_vb, &s->light_source_mesh, GL_TRIANGLES);
    s->light_sources_count = 10;

    // Smoothly interpolate from red to green to blue
    for (int i = 0; i < s->light_sources_count; i++)
    {
        float period = (float)(s->light_sources_count - 1) * 0.666666f;

        float rx1 = HIMATH_CLAMP((float)i, 0, period * 0.5f);
        float rx2 = HIMATH_CLAMP((float)i, period, period * 1.5f);
        float gx = HIMATH_CLAMP((float)i, 0, period);
        float bx = HIMATH_CLAMP((float)i, period * 0.5f, period * 1.5f);

        float r = get_channel_function1(rx1, period) +
                  get_channel_function2(rx2, period);
        float g = get_channel_function2(gx, period);
        float b = get_channel_function1(bx, period);

        s->light_sources[i].color.x = r;
        s->light_sources[i].color.y = g;
        s->light_sources[i].color.z = b;
    }

    s->light_source_shader = e_shader_load(e, "light_source");

    s->orbit_speed_deg = 30;
    s->orbit_radius = 0.5f;

    s->camera_distance = 2;

    return e;
}

EXAMPLE_CLEANUP_FN_SIG(cs300)
{
    Example* e = (Example*)udata;
    CS300* s = (CS300*)e->scene;

    glDeleteProgram(s->light_source_shader);
    r_vb_cleanup(&s->light_source_vb);
    rc_mesh_cleanup(&s->light_source_mesh);

    glDeleteProgram(s->model_shader);
    r_vb_cleanup(&s->model_vb);
    rc_mesh_cleanup(&s->model_mesh);

    for (int i = 0; i < s->model_file_paths_count; i++)
        fs_path_cleanup(&s->model_file_paths[i]);

    free(e);
}

EXAMPLE_UPDATE_FN_SIG(cs300)
{
    Example* e = (Example*)udata;
    CS300* s = (CS300*)e->scene;

    s->t += input->dt;

    igBeginMainMenuBar();
    if (igBeginMenu("Load Model", true))
    {
        for (int i = 0; i < s->model_file_paths_count; i++)
        {
            if (igMenuItemBool(s->model_file_paths[i].abs_path_str, NULL, false,
                               true))
            {
                try_switch_model(s, i);
            }
        }
        igEndMenu();
    }
    igEndMainMenuBar();

    ExamplePerFrameUBO per_frame = {0};
    per_frame.phong_lights_count = s->light_sources_count;
    for (int i = 0; i < s->light_sources_count; i++)
    {
        float deg = ((360.f / (float)s->light_sources_count) * (float)i +
                     s->t * s->orbit_speed_deg);
        float rad = degtorad(deg);
        FVec3 pos = {
            .x = cosf(rad) * s->orbit_radius,
            .z = sinf(rad) * s->orbit_radius,
        };
        s->light_sources[i].pos = pos;

        FVec3 color = s->light_sources[i].color;
        per_frame.phong_lights[i].type = ExamplePhongLightType_Point;
        per_frame.phong_lights[i].pos_or_dir = s->light_sources[i].pos;
        per_frame.phong_lights[i].ambient = fvec3_mulf(color, 0.1f);
        per_frame.phong_lights[i].diffuse = color;
        per_frame.phong_lights[i].specular = color;
        per_frame.phong_lights[i].linear = 0.09f;
        per_frame.phong_lights[i].quadratic = 0.032f;
    }
    per_frame.proj = mat4_persp(
        60, (float)input->window_size.x / (float)input->window_size.y, 0.1f,
        100);
    per_frame.view = mat4_lookat((FVec3){0, 1, s->camera_distance}, (FVec3){0},
                                 (FVec3){0, 1, 0});
    e_apply_per_frame_ubo(e, &per_frame);

    glClearColor(0.2f, 0.2f, 0.2f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    if (s->current_model_index >= 0)
    {
        ExamplePerObjectUBO per_object = {0};
        Mat4 scale_mat = mat4_scale(s->model_scale);
#if 0
        Mat4 rot_mat =
            quat_to_matrix(quat_rotate((FVec3){0, 1, 0}, 90.f * s->t));
#endif
        Mat4 rot_mat = mat4_identity();
        Mat4 trans_mat = mat4_translation(s->model_pos);
        per_object.model = mat4_mul(&trans_mat, &scale_mat);
        per_object.model = mat4_mul(&rot_mat, &per_object.model);
        e_apply_per_object_ubo(e, &per_object);
        glUseProgram(s->model_shader);
        r_vb_draw(&s->model_vb);
    }

    for (int i = 0; i < s->light_sources_count; i++)
    {
        Mat4 trans_mat = mat4_translation(s->light_sources[i].pos);
        ExamplePerObjectUBO per_object = {
            .model = trans_mat,
            .color = s->light_sources[i].color,
        };
        e_apply_per_object_ubo(e, &per_object);
        glUseProgram(s->light_source_shader);
        r_vb_draw(&s->light_source_vb);
    }
}
