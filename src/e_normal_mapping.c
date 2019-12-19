#include "scene.h"
#include "resource.h"
#include "app.h"
#include "example.h"
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
    Example* e =
        (Example*)e_example_make("normal_mapping", sizeof(NormalMapping));
    NormalMapping* scene = (NormalMapping*)e->scene;

    scene->mesh = rc_mesh_make_raw(4, 6);
    {
        Vertex vertices[] = {
            {{-0.5f, 0, 0.5f}, {0, 0}},
            {{0.5f, 0, 0.5f}, {1, 0}},
            {{0.5f, 0, -0.5f}, {1, 1}},
            {{-0.5f, 0, -0.5f}, {0, 1}},
        };
        uint indices[] = {0, 1, 2, 2, 3, 0};
        memcpy(scene->mesh.vertices, vertices, sizeof(vertices));
        memcpy(scene->mesh.indices, indices, sizeof(indices));
    }

    r_vb_init(&scene->vb, &scene->mesh, GL_TRIANGLES);

    scene->normal_mapping_shader = e_shader_load(e, "normal_mapping");

    int normal_map_width;
    int normal_map_height;
    int normal_map_channels_count;
    stbi_uc* normal_map_data = stbi_load(
        "normal_mapping/normal_map.png", &normal_map_width, &normal_map_height,
        &normal_map_channels_count, STBI_rgb_alpha);
    glGenTextures(1, &scene->normal_map);
    glBindTexture(GL_TEXTURE_2D, scene->normal_map);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, normal_map_width, normal_map_height,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, normal_map_data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(normal_map_data);

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