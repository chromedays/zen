#ifndef RESOURCE_H
#define RESOURCE_H

#include "primitive.h"
#include <glad/glad_wgl.h>
#include <himath.h>

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

GLuint rc_shader_load_from_source(const char* vs_src,
                                  const char* fs_src,
                                  const char** includes,
                                  int includes_count);

Mesh rc_mesh_load_from_gltf(const char* filename);

#endif // RESOURCE_H
