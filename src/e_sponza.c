#include "example.h"
#include "filesystem.h"
#include "resource.h"
#include <cgltf.h>
#include <stdlib.h>

typedef struct Model_
{
    int placeholder;
} Model;

typedef struct Sponza_
{
    int placeholder;
} Sponza;

EXAMPLE_INIT_FN_SIG(sponza)
{
    Example* e = e_example_make("sponza", sizeof(Sponza));
    Sponza* s = (Sponza*)e->scene;

    Path file_path = fs_path_make_working_dir();
    fs_path_append3(&file_path, "shared", "models", "box");
    fs_path_append(&file_path, "box.gltf");

    const char* filename = file_path.abs_path_str;

    histr_String buf = rc_read_text_file_all(filename);

    cgltf_options options = {0};
    cgltf_data* data = {0};
    cgltf_parse(&options, buf, histr_len(buf) + 1, &data);

    Mesh* out_meshes = (Mesh*)calloc(data->meshes_count, sizeof(*out_meshes));

    for (cgltf_size mi = 0; mi < data->meshes_count; mi++)
    {
        cgltf_mesh* mesh = &data->meshes[mi];
        Mesh* out_mesh = &out_meshes[mi];
        // rc_mesh_make_raw();
        for (cgltf_size pi = 0; pi < mesh->primitives_count; pi++)
        {
            cgltf_primitive* primitive = &mesh->primitives[pi];
            for (cgltf_size ai = 0; ai < primitive->attributes_count; ai++)
            {
                cgltf_attribute* attribute = &primitive->attributes[ai];
                cgltf_buffer_view* view = attribute->data->buffer_view;
                view->size;
            }
        }
    }

    free(out_meshes);

    // data->accessors->buffer_view

    cgltf_free(data);

    histr_destroy(buf);

    fs_path_cleanup(&file_path);

    return e;
}

EXAMPLE_CLEANUP_FN_SIG(sponza)
{
    Example* e = (Example*)udata;
    Sponza* s = (Sponza*)e->scene;
}

EXAMPLE_UPDATE_FN_SIG(sponza)
{
    Example* e = (Example*)udata;
    Sponza* s = (Sponza*)e->scene;
}
