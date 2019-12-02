#ifndef SCENE_H
#define SCENE_H
#include <himath.h>

typedef struct Input_ Input;

#define SCENE_INIT_FN_SIG(name) void* name(const Input* input)
typedef SCENE_INIT_FN_SIG(SceneInitFn);

#define SCENE_CLEANUP_FN_SIG(name) void name(void* udata, const Input* input)
typedef SCENE_CLEANUP_FN_SIG(SceneCleanupFn);

#define SCENE_UPDATE_FN_SIG(name) void name(void* udata, const Input* input)
typedef SCENE_UPDATE_FN_SIG(SceneUpdateFn);

typedef struct SceneCallbacks_
{
    SceneInitFn* init;
    SceneCleanupFn* cleanup;
    SceneUpdateFn* update;
} SceneCallbacks;

typedef struct Scene_
{
    const Input* input;
    void* udata;
    SceneCallbacks callbacks;
} Scene;

void s_init(Scene* scene, const Input* input);
void s_switch_scene(Scene* scene, SceneCallbacks new_scene_callbacks);
void s_update(Scene* scene);
void s_cleanup(Scene* scene);

#endif // SCENE_H
