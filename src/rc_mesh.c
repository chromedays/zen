#include "resource.h"
#include "debug.h"
#include "util.h"
#include <tinyobj_loader_c.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

void rc_mesh_cleanup(Mesh* mesh)
{
    if (mesh->vertices)
        free(mesh->vertices);
    if (mesh->indices)
        free(mesh->indices);
}

Mesh rc_mesh_make_raw(int vertices_count, int indices_count)
{
    Mesh result = {0};
    if (vertices_count > 0)
    {
        result.vertices =
            (Vertex*)malloc(vertices_count * sizeof(*result.vertices));
        result.vertices_count = vertices_count;
    }
    if (indices_count > 0)
    {
        result.indices = (uint*)malloc(indices_count * sizeof(*result.indices));
        result.indices_count = indices_count;
    }
    return result;
}

Mesh rc_mesh_make_raw2(int vertices_count,
                       int indices_count,
                       Vertex* vertices,
                       uint* indices)
{
    Mesh result = rc_mesh_make_raw(vertices_count, indices_count);
    if (vertices_count > 0)
        memcpy(result.vertices, vertices, vertices_count * sizeof(Vertex));
    if (indices_count > 0)
        memcpy(result.indices, indices, indices_count * sizeof(uint));
    return result;
}

Mesh rc_mesh_make_cube()
{
    // clang-format off
    Vertex vertices[] = {
        // Front
        {{-0.5f, -0.5f, 0.5f}, {1, 0}, {0, 0, 1}},
        {{0.5f, -0.5f, 0.5f}, {0, 0}, {0, 0, 1}},
        {{0.5f, 0.5f, 0.5f}, {0, 1}, {0, 0, 1}},
        {{-0.5f, 0.5f, 0.5f}, {1, 1}, {0, 0, 1}},
        // Back
        {{0.5f, -0.5f, -0.5f}, {1, 0}, {0, 0, -1}},
        {{-0.5f, -0.5f, -0.5f}, {0, 0}, {0, 0, -1}},
        {{-0.5f, 0.5f, -0.5f}, {0, 1}, {0, 0, -1}},
        {{0.5f, 0.5f, -0.5f}, {1, 1}, {0, 0, -1}},
        // Left
        {{0.5f, -0.5f, 0.5f}, {1, 0}, {1, 0, 0}},
        {{0.5f, -0.5f, -0.5f}, {0, 0}, {1, 0, 0}},
        {{0.5f, 0.5f, -0.5f}, {0, 1}, {1, 0, 0}},
        {{0.5f, 0.5f, 0.5f}, {1, 1}, {1, 0, 0}},
        // Right
        {{-0.5f, -0.5f, -0.5f}, {1, 0}, {-1, 0, 0}},
        {{-0.5f, -0.5f, 0.5f}, {0, 0}, {-1, 0, 0}},
        {{-0.5f, 0.5f, 0.5f}, {0, 1}, {-1, 0, 0}},
        {{-0.5f, 0.5f, -0.5f}, {1, 1}, {-1, 0, 0}},
        // Top
        {{0.5f, 0.5f, -0.5f}, {1, 0}, {0, 1, 0}},
        {{-0.5f, 0.5f, -0.5f}, {0, 0}, {0, 1, 0}},
        {{-0.5f, 0.5f, 0.5f}, {0, 1}, {0, 1, 0}},
        {{0.5f, 0.5f, 0.5f}, {1, 1}, {0, 1, 0}},
        // Bottom
        {{0.5f, -0.5f, 0.5f}, {1, 0}, {0, -1, 0}},
        {{-0.5f, -0.5f, 0.5f}, {0, 0}, {0, -1, 0}},
        {{-0.5f, -0.5f, -0.5f}, {0, 1}, {0, -1, 0}},
        {{0.5f, -0.5f, -0.5f}, {1, 1}, {0, -1, 0}},
    };

    uint indices[] = {
        0, 1, 2, 2, 3, 0,
        4, 5, 6, 6, 7, 4,
        8, 9, 10, 10, 11, 8,
        12, 13, 14, 14, 15, 12,
        16, 17, 18, 18, 19, 16,
        20, 21, 22, 22, 23, 20,
    };
    // clang-format on

    Mesh result = rc_mesh_make_raw2(ARRAY_LENGTH(vertices),
                                    ARRAY_LENGTH(indices), vertices, indices);
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
                Vertex* vertices =
                    (Vertex*)malloc(attrib.num_faces * sizeof(*vertices));
                if (vertices)
                {
                    int vertices_count = 0;

                    int face_offset = 0;
                    for (uint i = 0; i < attrib.num_face_num_verts; ++i)
                    {
                        ASSERT(attrib.face_num_verts[i] ==
                               3); // All faces must be triangles

                        for (int offset = 0; offset < attrib.face_num_verts[i];
                             ++offset)
                        {
                            tinyobj_vertex_index_t vi =
                                attrib.faces[face_offset + offset];

                            Vertex v = {0};
                            v.pos.x = attrib.vertices[vi.v_idx * 3];
                            v.pos.y = attrib.vertices[vi.v_idx * 3 + 1];
                            v.pos.z = attrib.vertices[vi.v_idx * 3 + 2];
                            if (attrib.num_texcoords > 0)
                            {
                                v.uv.x = attrib.texcoords[vi.vt_idx * 2];
                                v.uv.y = attrib.texcoords[vi.vt_idx * 2 + 1];
                            }
                            if (attrib.num_normals > 0)
                            {
                                v.normal.x = attrib.normals[vi.vn_idx * 3];
                                v.normal.y = attrib.normals[vi.vn_idx * 3 + 1];
                                v.normal.z = attrib.normals[vi.vn_idx * 3 + 2];
                            }
                            vertices[vertices_count++] = v;
                        }

                        face_offset += attrib.face_num_verts[i];
                    }

                    ASSERT(vertices_count == (int)attrib.num_faces);

                    mesh->vertices = vertices;
                    mesh->vertices_count = vertices_count;
                    ASSERT(!mesh->indices);
                    mesh->indices = NULL;
                    mesh->indices_count = 0;

                    result = true;
                }

                tinyobj_materials_free(materials, materials_count);
                tinyobj_shapes_free(shapes, shapes_count);
                tinyobj_attrib_free(&attrib);
            }

            free(data);
        }
    }

    return result;
}
