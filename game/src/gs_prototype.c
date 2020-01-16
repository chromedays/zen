#include <scene.h>
#include <renderer.h>
#include <resource.h>
#include <util.h>
#include <app.h>
#include <filesystem.h>
#include <stdlib.h>
#include <math.h>

typedef struct UniformPerFrame_
{
    Mat4 view;
    Mat4 proj;
} UniformPerFrame;

typedef struct UniformPerObject_
{
    Mat4 model;
    FVec4 color;
} UniformPerObject;

typedef struct PrototypeScene_
{
    int placeholder;
    Mesh circle_mesh;
    VertexBuffer circle_vb;

    uint per_frame_ubo;
    uint per_object_ubo;

    uint player_shader;
} PrototypeScene;

SCENE_INIT_FN_SIG(gs_prototype_init)
{
    PrototypeScene* s = (PrototypeScene*)malloc(sizeof(*s));

    Vertex vertices[64] = {0};
    float step = degtorad(360.f / (float)(ARRAY_LENGTH(vertices) - 2));
    for (int i = 1; i < ARRAY_LENGTH(vertices); i++)
    {
        vertices[i].pos = (FVec3){
            cosf(step * (float)i),
            sinf(step * (float)i),
            0,
        };
    }
    s->circle_mesh =
        rc_mesh_make_raw2(ARRAY_LENGTH(vertices), 0, vertices, NULL);

    r_vb_init(&s->circle_vb, &s->circle_mesh, GL_TRIANGLE_FAN);

    glGenBuffers(1, &s->per_frame_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, s->per_frame_ubo);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(UniformPerFrame), NULL,
                 GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, s->per_frame_ubo);

    glGenBuffers(1, &s->per_object_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, s->per_object_ubo);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(UniformPerObject), NULL,
                 GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, s->per_object_ubo);

    Path shader_base_path = fs_path_make_working_dir();
    fs_path_append3(&shader_base_path, "..", "game", "data");
    fs_path_append(&shader_base_path, "shaders");

    Path vs_filepath = fs_path_copy(shader_base_path);
    fs_path_append(&vs_filepath, "player.vert");
    Path fs_filepath = fs_path_copy(shader_base_path);
    fs_path_append(&fs_filepath, "player.frag");

    ShaderLoadDesc vs_desc = {
        .filenames = {vs_filepath.abs_path_str},
        .filenames_count = 1,
    };
    ShaderLoadDesc fs_desc = {
        .filenames = {fs_filepath.abs_path_str},
        .filenames_count = 1,
    };
    s->player_shader = rc_shader_load_from_files(
        vs_desc, fs_desc, (ShaderLoadDesc){0}, (ShaderLoadDesc){0});

    fs_path_cleanup(&fs_filepath);
    fs_path_cleanup(&vs_filepath);
    fs_path_cleanup(&shader_base_path);

    return s;
}

SCENE_CLEANUP_FN_SIG(gs_prototype_cleanup)
{
    PrototypeScene* s = (PrototypeScene*)udata;
    r_vb_cleanup(&s->circle_vb);
    rc_mesh_cleanup(&s->circle_mesh);
    free(s);
}

SCENE_UPDATE_FN_SIG(gs_prototype_update)
{
    PrototypeScene* s = (PrototypeScene*)udata;

    glClearColor(0.5f, 0.5f, 0.5f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // glDisable(GL_CULL_FACE);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    float aspect_ratio =
        (float)input->window_size.x / (float)input->window_size.y;
    float view_height = 10;
    float view_width = view_height * aspect_ratio;

    float left = -view_width * 0.5f;
    float right = view_width * 0.5f;
    float top = view_height * 0.5f;
    float bottom = -view_height * 0.5f;
    float far_z = 100.f;
    float near_z = 0.1f;

    Mat4 view_mat = mat4_lookat((FVec3){0, 0, 1}, (FVec3){0}, (FVec3){0, 1, 0});

    // clang-format off
    Mat4 ortho = {2.f / (right - left), 0.f, 0.f, 0.f, //
                   0.f, 2.f / (top - bottom), 0.f, 0.f, //
                   0.f, 0.f, -2.f / (far_z - near_z), -1.f, //
                   -(right + left) / (right - left),
                   -(top + bottom) / (top - bottom),
                   -(far_z + near_z) / (far_z - near_z),
                   1.f};
    // clang-format on

    UniformPerFrame per_frame = {
        .view = view_mat,
        .proj = ortho,
    };
    glBindBuffer(GL_UNIFORM_BUFFER, s->per_frame_ubo);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(per_frame), &per_frame);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    UniformPerObject per_object = {
        .model = mat4_identity(),
        .color = {1, 0, 0, 1},
    };
    glBindBuffer(GL_UNIFORM_BUFFER, s->per_object_ubo);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(per_object), &per_object);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    glUseProgram(s->player_shader);

    r_vb_draw(&s->circle_vb);
}
