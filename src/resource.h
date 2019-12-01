#ifndef RESOURCE_H
#define RESOURCE_H

#include "primitive.h"
#include <glad/glad.h>
#include <himath.h>

GLuint rc_shader_load_from_source(const char* vs_src,
                                  const char* fs_src,
                                  const char* cs_src,
                                  const char** includes,
                                  int includes_count);

typedef struct Vertex_
{
    FVec3 pos;
    FVec2 uv;
    FVec3 normal;
} Vertex;

typedef struct Mesh_
{
    int vertices_count;
    Vertex* vertices;

    int indices_count;
    uint* indices;
} Mesh;

void rc_mesh_cleanup(Mesh* mesh);
Mesh rc_mesh_make_raw(int vertices_count, int indices_count);
// Mesh rc_mesh_load_from_gltf(const char* filename);
Mesh rc_mesh_make_sphere(float radius, int slices_count, int stacks_count);

#endif // RESOURCE_H
