#include "example.h"
#include "resource.h"
#include "renderer.h"
#include "debug.h"
#include "app.h"
#include <stdlib.h>
#include <float.h>

typedef struct DepthPeeling_
{
    Mesh model_mesh;
    VertexBuffer model_vb;
    FVec3 model_pos_offset;
    float model_scale_offset;
    FVec3 model_min_p;
    FVec3 model_max_p;

    Mesh plane_mesh;
    VertexBuffer plane_vb;

    Mesh cube_mesh;
    VertexBuffer cube_vb;

    GLuint unlit_shader;
    GLuint phong_shader;

    ExampleFpsCamera cam;
} DepthPeeling;

EXAMPLE_INIT_FN_SIG(depth_peeling)
{
    Example* e = e_example_make("depth_peeling", sizeof(DepthPeeling));
    DepthPeeling* s = (DepthPeeling*)e->scene;

    s->model_mesh = e_mesh_load_from_obj(e, "lucy_princeton.obj");
    r_vb_init(&s->model_vb, &s->model_mesh, GL_TRIANGLES);
    {
        FVec3 min_p = s->model_mesh.vertices[0].pos;
        FVec3 max_p = s->model_mesh.vertices[0].pos;

        for (int i = 1; i < s->model_mesh.vertices_count; i++)
        {
            FVec3 p = s->model_mesh.vertices[i].pos;
            min_p.x = HIMATH_MIN(min_p.x, p.x);
            min_p.y = HIMATH_MIN(min_p.y, p.y);
            min_p.z = HIMATH_MIN(min_p.z, p.z);
            max_p.x = HIMATH_MAX(max_p.x, p.x);
            max_p.y = HIMATH_MAX(max_p.y, p.y);
            max_p.z = HIMATH_MAX(max_p.z, p.z);
        }
        FVec3 bound = fvec3_sub(max_p, min_p);
        float max_side = HIMATH_MAX(HIMATH_MAX(bound.x, bound.y), bound.z);
        s->model_scale_offset = 2.f / max_side;
        s->model_pos_offset =
            fvec3_mulf(fvec3_add(min_p, max_p), -0.5f * s->model_scale_offset);
        s->model_min_p = min_p;
        s->model_max_p = max_p;
    }

    {
        Vertex plane_vertices[4] = {
            {{-0.5f, 0, 0.5f}, {0, 0}, {0, 1, 0}},
            {{0.5f, 0, 0.5f}, {1, 0}, {0, 1, 0}},
            {{0.5f, 0, -0.5f}, {1, 1}, {0, 1, 0}},
            {{-0.5f, 0, -0.5f}, {0, 1}, {0, 1, 0}},
        };
        uint plane_indices[6] = {0, 1, 2, 2, 3, 0};
        s->plane_mesh = rc_mesh_make_raw2(ARRAY_LENGTH(plane_vertices),
                                          ARRAY_LENGTH(plane_indices),
                                          plane_vertices, plane_indices);
    }
    r_vb_init(&s->plane_vb, &s->plane_mesh, GL_TRIANGLES);

    s->cube_mesh = rc_mesh_make_cube();
    r_vb_init(&s->cube_vb, &s->cube_mesh, GL_TRIANGLES);

    s->unlit_shader = e_shader_load(e, "unlit");
    s->phong_shader = e_shader_load(e, "phong");

    s->cam.pos = (FVec3){0, 0, 4};

    return e;
}

EXAMPLE_CLEANUP_FN_SIG(depth_peeling)
{
    Example* e = (Example*)udata;
    DepthPeeling* s = (DepthPeeling*)e->scene;

    glDeleteProgram(s->phong_shader);
    glDeleteProgram(s->unlit_shader);

    r_vb_cleanup(&s->cube_vb);
    rc_mesh_cleanup(&s->cube_mesh);

    r_vb_cleanup(&s->plane_vb);
    rc_mesh_cleanup(&s->plane_mesh);

    r_vb_cleanup(&s->model_vb);
    rc_mesh_cleanup(&s->model_mesh);

    free(e);
}

void draw_object(Example* e,
                 const DepthPeeling* s,
                 const VertexBuffer* vb,
                 FVec3 translation,
                 FVec3 scale,
                 Quat rotation,
                 FVec3 color)
{
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glFrontFace(GL_CCW);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(s->phong_shader);
    ExamplePerObjectUBO per_object = {
        .model = mat4_transform(translation, scale, rotation),
        .color = color,
    };
    e_apply_per_object_ubo(e, &per_object);
    r_vb_draw(vb);

    glCullFace(GL_FRONT);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glUseProgram(s->phong_shader);
    r_vb_draw(vb);
}

EXAMPLE_UPDATE_FN_SIG(depth_peeling)
{
    Example* e = (Example*)udata;
    DepthPeeling* s = (DepthPeeling*)e->scene;

    e_fpscam_update(&s->cam, input, 5);

    Mat4 view_mat = mat4_lookat(
        s->cam.pos, fvec3_add(s->cam.pos, e_fpscam_get_look(&s->cam)),
        (FVec3){0, 1, 0});

    ExamplePerFrameUBO per_frame = {
        .view = view_mat,
        .proj = mat4_persp(
            60, (float)input->window_size.x / (float)input->window_size.y, 0.1f,
            100),
        .phong_lights =
            {
                {
                    .type = ExamplePhongLightType_Directional,
                    .ambient = {0.1f, 0.1f, 0.1f},
                    .diffuse = {1, 1, 1},
                    .specular = {1, 1, 1},
                    .pos_or_dir = fvec3_normalize((FVec3){1, -1, -1}),
                },
            },
        .phong_lights_count = 1,
        .view_pos = s->cam.pos,
    };
    e_apply_per_frame_ubo(e, &per_frame);

    glClearColor(0.1f, 0.1f, 0.1f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glDisable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glUseProgram(s->unlit_shader);
    ExamplePerObjectUBO cube_per_object = {
        .model = mat4_scale(2),
        .color = (FVec3){0, 1, 0},
    };
    e_apply_per_object_ubo(e, &cube_per_object);
    r_vb_draw(&s->cube_vb);

    draw_object(e, s, &s->plane_vb, (FVec3){0, -1, 0}, (FVec3){10, 1, 10},
                quat_rotate((FVec3){0, 1, 0}, 0), (FVec3){1, 0, 0});
    draw_object(e, s, &s->model_vb, s->model_pos_offset,
                (FVec3){
                    s->model_scale_offset,
                    s->model_scale_offset,
                    s->model_scale_offset,
                },
                quat_rotate((FVec3){0, 1, 0}, 0), (FVec3){1, 1, 1});
}
