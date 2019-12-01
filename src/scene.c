#include "scene.h"

void s_switch_scene(Scene* scene, SceneCallbacks new_scene_callbacks)
{
    if (scene->callbacks.cleanup)
        scene->callbacks.cleanup(scene->udata, &scene->app);

    scene->callbacks = new_scene_callbacks;
    scene->udata = scene->callbacks.init(&scene->app);
}

void s_update(Scene* scene, float dt)
{
    if (scene->callbacks.update)
        scene->callbacks.update(scene->udata, &scene->app, dt);
}

void s_cleanup(Scene* scene)
{
    if (scene->callbacks.cleanup)
        scene->callbacks.cleanup(scene->udata, &scene->app);
}
