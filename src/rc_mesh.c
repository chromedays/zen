#include "resource.h"
#include "debug.h"
#include <tinyobj_loader_c.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

void rc_mesh_cleanup(Mesh* mesh)
{
    if (mesh->vertices)
        free(mesh->vertices);
    if (mesh->indices)
        free(mesh->indices);
}

// TODO:
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
        if (data)
        {
            fsize = fread(data, sizeof(char), fsize, f);
            data[fsize] = '\0';
            fclose(f);

            tinyobj_attrib_t attrib;
            tinyobj_attrib_init(&attrib);
            tinyobj_shape_t* shapes = NULL;
            size_t shapes_count = 0;
            tinyobj_material_t* materials = NULL;
            size_t materials_count = 0;
            int parse_result = tinyobj_parse_obj(
                &attrib, &shapes, &shapes_count, &materials, &materials_count,
                data, fsize, TINYOBJ_FLAG_TRIANGULATE);
            if (parse_result == TINYOBJ_SUCCESS)
            {
                int face_offset = 0;
                for (uint i = 0; i < attrib.num_face_num_verts; ++i)
                {
                    ASSERT(attrib.face_num_verts[i] % 3 == 0);
                    for (int fi = 0; fi < attrib.face_num_verts[i] / 3; ++i)
                    {
                        tinyobj_vertex_index_t vi[3] = {
                            attrib.faces[face_offset + fi * 3],
                            attrib.faces[face_offset + fi * 3 + 1],
                            attrib.faces[face_offset + fi * 3 + 2],
                        };
                    }
                }
                attrib.face_num_verts;
                tinyobj_materials_free(materials, materials_count);
                tinyobj_shapes_free(shapes, shapes_count);
                tinyobj_attrib_free(&attrib);
            }

            free(data);
        }
    }

    return result;
}

Mesh rc_mesh_make_sphere(float radius, int slices_count, int stacks_count)
{
    Mesh result = {0};
    if (slices_count > 2 && stacks_count > 1)
    {
        const int vertices_count = (stacks_count - 2) * slices_count + 2;
        Vertex* vertices = (Vertex*)malloc(vertices_count * sizeof(*vertices));
        const int indices_count = (stacks_count - 2) * slices_count * 6;
        uint* indices = (uint*)malloc(indices_count * sizeof(*indices));

        const float d_phi = degtorad(180) / (float)stacks_count;
        const float d_theta = degtorad(360) / (float)slices_count;

        int vi = 0;

        for (int i = 1; i < stacks_count - 1; ++i)
        {
            float phi = degtorad(-90) + d_phi * (float)i;
            float y = radius * sinf(phi);
            float xzr = radius * cosf(phi);
            if (xzr < 0.f)
                xzr = -xzr;

            for (int j = 0; j < slices_count; ++j)
            {
                float x = xzr * cosf(d_theta * j);
                float z = xzr * sinf(d_theta * j);
                Vertex* v = &vertices[vi++];
                v->pos.x = x;
                v->pos.y = y;
                v->pos.z = z;
                v->normal = fvec3_normalize(v->pos);
            }
        }

        int ii = 0;

        for (int i = 1; i < stacks_count - 2; ++i)
        {
            for (int j = 0; j < slices_count; ++j)
            {
                int j0 = j;
                int j1 = (j + 1) % slices_count;

                indices[ii++] = (i - 1) * slices_count + j1;
                indices[ii++] = i * slices_count + j0;
                indices[ii++] = i * slices_count + j1;
                indices[ii++] = i * slices_count + j0;
                indices[ii++] = (i - 1) * slices_count + j1;
                indices[ii++] = (i - 1) * slices_count + j0;
            }
        }

        Vertex* v = &vertices[vi++];
        v->pos.x = 0.f;
        v->pos.y = radius;
        v->pos.z = 0.f;
        v->normal.x = 0.f;
        v->normal.y = 1.f;
        v->normal.z = 0.f;

        for (int i = 0; i < slices_count; ++i)
        {
            int i1 = (i + 1) % slices_count;
            indices[ii++] = (stacks_count - 3) * slices_count + i1;
            indices[ii++] = (stacks_count - 3) * slices_count + i;
            indices[ii++] = vi - 1;
        }

        v = &vertices[vi++];
        v->pos.x = 0.f;
        v->pos.y = -radius;
        v->pos.z = 0.f;
        v->normal.x = 0.f;
        v->normal.y = -1.f;
        v->normal.z = 0.f;
        for (int i = 0; i < slices_count; ++i)
        {
            int i1 = (i + 1) % (slices_count);
            indices[ii++] = i;
            indices[ii++] = i1;
            indices[ii++] = vi - 1;
        }

        ASSERT(vi == vertices_count);
        ASSERT(ii == indices_count);

        result.vertices_count = vertices_count;
        result.vertices = vertices;
        result.indices_count = indices_count;
        result.indices = indices;
    }

    return result;
}
