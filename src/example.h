#ifndef EXAMPLE_H
#define EXAMPLE_H
#include "scene.h"
#include "renderer.h"
#include "util.h"

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

Mesh e_mesh_load_from_obj(const Example* e, const char* obj_filename);
GLuint e_shader_load(const Example* e, const char* shader_name);
GLuint e_texture_load(const Example* e, const char* texture_filename);

typedef enum ExamplePhongLightType_
{
    ExamplePhongLightType_Directional = 1,
    ExamplePhongLightType_Point,
    ExamplePhongLightType_Spot,
} ExamplePhongLightType;

typedef struct ExamplePhongLight_
{
    ALIGN_AS(4) ExamplePhongLightType type;
    ALIGN_AS(16) FVec3 ambient;
    ALIGN_AS(16) FVec3 diffuse;
    ALIGN_AS(16) FVec3 specular;
    ALIGN_AS(16) FVec3 pos_or_dir;
    ALIGN_AS(4) float inner_angle;
    ALIGN_AS(4) float outer_angle;
    ALIGN_AS(4) float falloff;
    ALIGN_AS(4) float linear;
    ALIGN_AS(4) float quadratic;
} ExamplePhongLight;

#define MAX_PHONG_LIGHTS_COUNT 100

typedef struct ExamplePerFrameUBO_
{
    ALIGN_AS(16) Mat4 view;
    ALIGN_AS(16) Mat4 proj;
    ALIGN_AS(16) ExamplePhongLight phong_lights[MAX_PHONG_LIGHTS_COUNT];
    ALIGN_AS(4) int phong_lights_count;
    ALIGN_AS(16) FVec3 view_pos;
    ALIGN_AS(4) float t;
} ExamplePerFrameUBO;

typedef struct ExamplePerObjectUBO_
{
    Mat4 model;
    FVec3 color;
} ExamplePerObjectUBO;

void e_apply_per_frame_ubo(const Example* e, const ExamplePerFrameUBO* data);
void e_apply_per_object_ubo(const Example* e, const ExamplePerObjectUBO* data);

typedef struct ExampleFpsCamera_
{
    FVec3 pos;
    float yaw_deg;
    float pitch_deg;
} ExampleFpsCamera;

void e_fpscam_update(ExampleFpsCamera* cam, const Input* input, float speed);
FVec3 e_fpscam_get_look(const ExampleFpsCamera* cam);

EXAMPLE_DECL(hello_triangle);
EXAMPLE_DECL(hello_mesh);
EXAMPLE_DECL(phong);
EXAMPLE_DECL(simple_lights);
EXAMPLE_DECL(normal_mapping);
EXAMPLE_DECL(hdr);

EXAMPLE_DECL(cs300);

#endif // EXAMPLE_H
