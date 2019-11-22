#include "resource.h"
#include <tinyobj_loader_c.h>
#include <stdio.h>
#include <stdlib.h>

bool rc_mesh_load_from_obj(Mesh* mesh, const char* filename)
{
    bool result = false;
    FILE* f = fopen(filename, "r");
    if (f)
    {
        fseek(f, 0, SEEK_END);
        size_t fsize = ftell(f);
        rewind(f);
        char* data = (char*)malloc((fsize + 1) * sizeof(char));
        fsize = fread(data, sizeof(char), fsize, f);
        data[fsize] = '\0';
        fclose(f);

        free(data);

        tinyobj_attrib_t attrib;
        tinyobj_attrib_init(&attrib);
        tinyobj_shape_t* shapes;
        size_t shapes_count;
        tinyobj_material_t* materials;
        size_t materials_count;
        tinyobj_parse_obj(&attrib, &shapes, &shapes_count, &materials,
                          &materials_count, data, fsize,
                          TINYOBJ_FLAG_TRIANGULATE);

        free(data);
    }

    return result;
}
