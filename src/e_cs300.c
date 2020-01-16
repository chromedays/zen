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

typedef struct GBuffer_
{
    IVec2 dim;
    uint framebuffer;
    uint position_texture;
    uint normal_texture;
    uint albedo_texture;
    uint depth_stencil_buffer;
} GBuffer;

typedef enum DrawMode_
{
    DrawMode_FinalScene = 0,
    DrawMode_PositionMap,
    DrawMode_NormalMap,
    DrawMode_AlbedoMap,
} DrawMode;

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

    Mesh fsq_mesh;
    VertexBuffer fsq_vb;
    uint fsq_shader;
    uint fsq_target_texture;

    GBuffer gbuffer;
    uint deferred_first_pass_shader;
    uint deferred_second_pass_shader;

    DrawMode draw_mode;
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

static float get_channel_function1(float x, float period)
{
    float coeff = (HIMATH_PI * 2.f) / period;
    return (0.5f * cosf(coeff * x) + 0.5f);
}

static float get_channel_function2(float x, float period)
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

    Vertex fsq_vertices[4] = {
        {{-1, -1, 0}, {0, 0}},
        {{1, -1, 0}, {1, 0}},
        {{1, 1, 0}, {1, 1}},
        {{-1, 1, 0}, {0, 1}},
    };
    uint fsq_indices[6] = {0, 1, 2, 2, 3, 0};
    s->fsq_mesh =
        rc_mesh_make_raw2(ARRAY_LENGTH(fsq_vertices), ARRAY_LENGTH(fsq_indices),
                          fsq_vertices, fsq_indices);
    r_vb_init(&s->fsq_vb, &s->fsq_mesh, GL_TRIANGLES);
    s->fsq_shader = e_shader_load(e, "fsq");

    s->gbuffer.dim = input->window_size;
    glGenFramebuffers(1, &s->gbuffer.framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, s->gbuffer.framebuffer);

    glGenTextures(1, &s->gbuffer.position_texture);
    glBindTexture(GL_TEXTURE_2D, s->gbuffer.position_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, s->gbuffer.dim.x,
                 s->gbuffer.dim.y, 0, GL_RGB, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           s->gbuffer.position_texture, 0);

    glGenTextures(1, &s->gbuffer.normal_texture);
    glBindTexture(GL_TEXTURE_2D, s->gbuffer.normal_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, s->gbuffer.dim.x,
                 s->gbuffer.dim.y, 0, GL_RGB, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D,
                           s->gbuffer.normal_texture, 0);

    glGenTextures(1, &s->gbuffer.albedo_texture);
    glBindTexture(GL_TEXTURE_2D, s->gbuffer.albedo_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, s->gbuffer.dim.x,
                 s->gbuffer.dim.y, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D,
                           s->gbuffer.albedo_texture, 0);

    uint attachments[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1,
                          GL_COLOR_ATTACHMENT2};
    glDrawBuffers(3, attachments);

    glGenRenderbuffers(1, &s->gbuffer.depth_stencil_buffer);
    glBindRenderbuffer(GL_RENDERBUFFER, s->gbuffer.depth_stencil_buffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8,
                          s->gbuffer.dim.x, s->gbuffer.dim.y);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                              GL_RENDERBUFFER, s->gbuffer.depth_stencil_buffer);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    s->fsq_target_texture = s->gbuffer.position_texture;

    s->deferred_first_pass_shader =
        e_shader_load(e, "phong_deferred_first_pass");

    s->deferred_second_pass_shader =
        e_shader_load(e, "phong_deferred_second_pass");

    return e;
}

EXAMPLE_CLEANUP_FN_SIG(cs300)
{
    Example* e = (Example*)udata;
    CS300* s = (CS300*)e->scene;

    glDeleteProgram(s->deferred_second_pass_shader);
    glDeleteProgram(s->deferred_first_pass_shader);
    glDeleteFramebuffers(1, &s->gbuffer.framebuffer);
    glDeleteRenderbuffers(1, &s->gbuffer.depth_stencil_buffer);
    glDeleteTextures(1, &s->gbuffer.albedo_texture);
    glDeleteTextures(1, &s->gbuffer.normal_texture);
    glDeleteTextures(1, &s->gbuffer.position_texture);

    glDeleteProgram(s->fsq_shader);
    r_vb_cleanup(&s->fsq_vb);
    rc_mesh_cleanup(&s->fsq_mesh);

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

static void update_light_source_transforms(CS300* s)
{
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
    }
}

void prepare_per_frame(Example* e, const CS300* s, const Input* input)
{
    ExamplePerFrameUBO per_frame = {0};
    per_frame.phong_lights_count = s->light_sources_count;
    for (int i = 0; i < s->light_sources_count; i++)
    {
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
}

static void draw_deferred_objects(Example* e, const CS300* s)
{
    // First pass
    glBindFramebuffer(GL_FRAMEBUFFER, s->gbuffer.framebuffer);
    glClearColor(0, 0, 0, 0);
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
#else
        Mat4 rot_mat = mat4_identity();
#endif
        Mat4 trans_mat = mat4_translation(s->model_pos);
        per_object.model = mat4_mul(&trans_mat, &scale_mat);
        per_object.model = mat4_mul(&rot_mat, &per_object.model);
        e_apply_per_object_ubo(e, &per_object);
        glUseProgram(s->deferred_first_pass_shader);
        r_vb_draw(&s->model_vb);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Second pass
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    switch (s->draw_mode)
    {
    case DrawMode_FinalScene: {
        uint textures[] = {
            s->gbuffer.position_texture,
            s->gbuffer.normal_texture,
            s->gbuffer.albedo_texture,
        };
        glBindTextures(0, ARRAY_LENGTH(textures), textures);
        glUseProgram(s->deferred_second_pass_shader);
        r_vb_draw(&s->fsq_vb);
    }
    break;
    case DrawMode_PositionMap:
        glBindTextures(0, 1, &s->gbuffer.position_texture);
        glUseProgram(s->fsq_shader);
        r_vb_draw(&s->fsq_vb);
        break;
    case DrawMode_NormalMap:
        glBindTextures(0, 1, &s->gbuffer.normal_texture);
        glUseProgram(s->fsq_shader);
        r_vb_draw(&s->fsq_vb);
        break;
    case DrawMode_AlbedoMap:
        glBindTextures(0, 1, &s->gbuffer.albedo_texture);
        glUseProgram(s->fsq_shader);
        r_vb_draw(&s->fsq_vb);
        break;
    }
}

static void copy_depth_buffer(const CS300* s, IVec2 window_size)
{
    glClear(GL_DEPTH_BUFFER_BIT);

    // Copy depth buffer written from deferred rendering pass
    glBindFramebuffer(GL_READ_FRAMEBUFFER, s->gbuffer.framebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, s->gbuffer.dim.x, s->gbuffer.dim.y, 0, 0,
                      window_size.x, window_size.y, GL_DEPTH_BUFFER_BIT,
                      GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

static void draw_debug_objects(Example* e, const CS300* s)
{
    // Draw debug objects (e.g. light sources) in forward rendering
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

EXAMPLE_UPDATE_FN_SIG(cs300)
{
    Example* e = (Example*)udata;
    CS300* s = (CS300*)e->scene;

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

    if (igBeginMenu("View Mode", true))
    {
        if (igMenuItemBool("Final Scene", NULL, false, true))
        {
            s->draw_mode = DrawMode_FinalScene;
        }
        if (igMenuItemBool("Position Map", NULL, false, true))
        {
            s->draw_mode = DrawMode_PositionMap;
            s->fsq_target_texture = s->gbuffer.position_texture;
        }
        if (igMenuItemBool("Normal Map", NULL, false, true))
        {
            s->draw_mode = DrawMode_NormalMap;
            s->fsq_target_texture = s->gbuffer.normal_texture;
        }
        if (igMenuItemBool("Albedo Map", NULL, false, true))
        {
            s->draw_mode = DrawMode_AlbedoMap;
            s->fsq_target_texture = s->gbuffer.albedo_texture;
        }
        igEndMenu();
    }

    igEndMainMenuBar();

    s->t += input->dt;

    update_light_source_transforms(s);
    prepare_per_frame(e, s, input);
    draw_deferred_objects(e, s);
    copy_depth_buffer(s, input->window_size);
    draw_debug_objects(e, s);
}
