#ifndef IMAGING_H
#define IMAGING_H
#include "imaging_gui.h"
#include "imaging_model.h"
#include "imaging_operation.h"
#include "../../all.h"
#include <stdlib.h>
#include <string.h>

typedef struct ImagingScene_
{
    ImageKeyValue image_keyvalues[100];
    int images_count;

    GUIState gui;
} ImagingScene;

void push_image(ImagingScene* s, const char* key, Image* value)
{
    ImageKeyValue* kv = &s->image_keyvalues[s->images_count];
    kv->key = histr_makestr(key);
    kv->value = *value;

    ++s->images_count;
}

static FILE_FOREACH_FN_DECL(load_and_append_image)
{
    ImagingScene* s = (ImagingScene*)udata;
    Image image = image_load_from_ppm(file_path->abs_path_str);
    if (image_is_valid(&image))
        push_image(s, file_path->filename, &image);
}

EXAMPLE_INIT_FN_SIG(imaging)
{
    Example* e = e_example_make("imaging", ImagingScene);
    ImagingScene* s = (ImagingScene*)e->scene;

    GUIState* gui = &s->gui;
    gui_init(gui);

    {
        Path path = fs_path_make_working_dir();
        fs_path_append2(&path, "image_processing", "images");
        fs_for_each_files_with_ext(path, "ppm", &load_and_append_image, s);
        fs_path_cleanup(&path);
    }

    return e;
}

EXAMPLE_CLEANUP_FN_SIG(imaging)
{
    Example* e = (Example*)udata;
    ImagingScene* s = (ImagingScene*)e->scene;

    GUIState* gui = &s->gui;

    for (int i = 0; i < s->images_count; i++)
    {
        ImageKeyValue* kv = &s->image_keyvalues[i];
        histr_destroy(kv->key);
        image_cleanup(&kv->value);
    }

    e_example_destroy(e);
}

EXAMPLE_UPDATE_FN_SIG(imaging)
{
    Example* e = (Example*)udata;
    ImagingScene* s = (ImagingScene*)e->scene;

    gui_render(&s->gui, s->image_keyvalues, s->images_count);
    if (s->gui.should_execute_operations)
    {
#if 0
        Image image = image_copy(s->gui.target_image.image);
        image_process_operations(&image, s->gui.selected_op.inputs,
                                 s->gui.selected_op.count);
#endif
    }

    glClear(GL_COLOR_BUFFER_BIT);
}

#define USER_INIT                                                              \
    Scene scene = {0};                                                         \
    s_init(&scene, &input);                                                    \
    s_switch_scene(&scene, EXAMPLE_LITERAL(imaging));

#define USER_UPDATE s_update(&scene);

#define USER_CLEANUP s_cleanup(&scene);

//#include "../../win32_main.inl"

#endif // IMAGING_H