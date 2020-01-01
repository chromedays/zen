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

typedef struct CS300_
{
    Path model_file_paths[MAX_MODEL_FILES_COUNT];
    int model_file_paths_count;

    int current_model_index;
    Mesh model_mesh;
    VertexBuffer model_vb;

    FVec3 model_pos;
    float model_scale;

    uint shader;
    float t;
} CS300;

static FILE_FOREACH_FN_DECL(push_model_filename)
{
    CS300* s = (CS300*)udata;
    ASSERT(s->model_file_paths_count <= ARRAY_LENGTH(s->model_file_paths));
    s->model_file_paths[s->model_file_paths_count++] = fs_path_copy(*file_path);
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

    s->shader = e_shader_load(e, "debug");

    return e;
}

EXAMPLE_CLEANUP_FN_SIG(cs300)
{
    Example* e = (Example*)udata;
    CS300* s = (CS300*)e->scene;

    glDeleteProgram(s->shader);

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
                if (i != s->current_model_index)
                {
                    if (s->current_model_index >= 0)
                    {
                        rc_mesh_cleanup(&s->model_mesh);
                        r_vb_cleanup(&s->model_vb);
                    }

                    rc_mesh_load_from_obj(&s->model_mesh,
                                          s->model_file_paths[i].abs_path_str);
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

                    s->model_scale =
                        1.f / fvec3_length(fvec3_sub(bb_max, bb_min));
                    s->model_pos = fvec3_negate(
                        fvec3_mulf(fvec3_mulf(fvec3_add(bb_min, bb_max), 0.5f),
                                   s->model_scale));

                    r_vb_init(&s->model_vb, &s->model_mesh, GL_TRIANGLES);

                    s->current_model_index = i;
                }
            }
        }
        igEndMenu();
    }
    igEndMainMenuBar();

    ExamplePerFrameUBO per_frame = {0};
    per_frame.proj = mat4_persp(
        60, (float)input->window_size.x / (float)input->window_size.y, 0.1f,
        100);
    per_frame.view = mat4_lookat((FVec3){sinf(s->t), 0, cosf(s->t)}, (FVec3){0},
                                 (FVec3){0, 1, 0});
    e_apply_per_frame_ubo(e, &per_frame);

    ExamplePerObjectUBO per_object = {0};
    Mat4 scale_mat = mat4_scale(s->model_scale);
    Mat4 trans_mat = mat4_translation(s->model_pos);
    per_object.model = mat4_mul(&trans_mat, &scale_mat);
    e_apply_per_object_ubo(e, &per_object);

    glClearColor(0.2f, 0.2f, 0.2f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    if (s->current_model_index >= 0)
    {
        glUseProgram(s->shader);
        r_vb_draw(&s->model_vb);
    }
}
