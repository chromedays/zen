#ifndef SCENE_H
#define SCENE_H
#include "himath.h"

typedef struct AppContext_
{
    IVec2 window_size;
    float window_aspect_ratio;
} AppContext;

#define SCENE_INIT_FN_SIG(name) void* name(const AppContext* app)
typedef SCENE_INIT_FN_SIG(SceneInitFn);

#define SCENE_CLEANUP_FN_SIG(name) void name(void* udata, const AppContext* app)
typedef SCENE_CLEANUP_FN_SIG(SceneCleanupFn);

#define SCENE_UPDATE_FN_SIG(name)                                              \
    void name(void* udata, const AppContext* app, float dt)
typedef SCENE_UPDATE_FN_SIG(SceneUpdateFn);

typedef struct SceneCallbacks_
{
    SceneInitFn* init;
    SceneCleanupFn* cleanup;
    SceneUpdateFn* update;
} SceneCallbacks;

typedef struct Scene_
{
    AppContext app;
    void* udata;
    SceneCallbacks callbacks;
} Scene;

void s_switch_scene(Scene* scene, SceneCallbacks new_scene_callbacks);
void s_update(Scene* scene, float dt);
void s_cleanup(Scene* scene);

#endif // SCENE_H
