#ifndef EXAMPLE_H
#define EXAMPLE_H
#include "scene.h"
#include "renderer.h"

#define EXAMPLE_INIT_FN_SIG(scene_name) SCENE_INIT_FN_SIG(scene_name##_init)
#define EXAMPLE_CLEANUP_FN_SIG(scene_name)                                     \
    SCENE_CLEANUP_FN_SIG(scene_name##_cleanup)
#define EXAMPLE_UPDATE_FN_SIG(scene_name)                                      \
    SCENE_UPDATE_FN_SIG(scene_name##_update)

#define EXAMPLE_DECL(scene_name)                                               \
    EXAMPLE_INIT_FN_SIG(scene_name);                                           \
    EXAMPLE_CLEANUP_FN_SIG(scene_name);                                        \
    EXAMPLE_UPDATE_FN_SIG(scene_name)

#define EXAMPLE_LITERAL(scene_name)                                            \
    (SceneCallbacks)                                                           \
    {                                                                          \
        .init = scene_name##_init, .cleanup = scene_name##_cleanup,            \
        .update = scene_name##_update                                          \
    }

typedef struct Example_
{
    const char* name;
    GLuint per_frame_ubo;
    GLuint per_object_ubo;
    void* scene;
} Example;

Example* e_example_make(const char* name, size_t scene_size);
void e_example_destroy(Example* e);

GLuint e_shader_load(const Example* e, const char* shader_name);

typedef struct ExamplePerFrameUBO_
{
    Mat4 view;
    Mat4 proj;
    float t;
} ExamplePerFrameUBO;

typedef struct ExamplePerObjectUBO_
{
    Mat4 model;
    FVec3 color;
} ExamplePerObjectUBO;

void e_apply_per_frame_ubo(const Example* e, const ExamplePerFrameUBO* data);
void e_apply_per_object_ubo(const Example* e, const ExamplePerObjectUBO* data);

EXAMPLE_DECL(hello_triangle);
EXAMPLE_DECL(hello_mesh);
EXAMPLE_DECL(simple_lights);
EXAMPLE_DECL(normal_mapping);
EXAMPLE_DECL(hdr);

#endif // EXAMPLE_H