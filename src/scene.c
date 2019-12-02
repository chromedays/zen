#include "scene.h"
#include "app.h"

void s_init(Scene* scene, const Input* input)
{
    scene->input = input;
}

void s_switch_scene(Scene* scene, SceneCallbacks new_scene_callbacks)
{
    if (scene->callbacks.cleanup)
        scene->callbacks.cleanup(scene->udata, scene->input);

    scene->callbacks = new_scene_callbacks;
    scene->udata = scene->callbacks.init(scene->input);
}

void s_update(Scene* scene)
{
    if (scene->callbacks.update)
        scene->callbacks.update(scene->udata, scene->input);
}

void s_cleanup(Scene* scene)
{
    if (scene->callbacks.cleanup)
        scene->callbacks.cleanup(scene->udata, scene->input);
}
