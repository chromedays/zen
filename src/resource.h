#ifndef RESOURCE_H
#define RESOURCE_H

#include "primitive.h"
#include "renderer.h"
#include <glad/glad.h>
#include <himath.h>

#define SHADER_LOAD_DESC_MAX_FILES_COUNT 10

typedef struct ShaderLoadDesc_
{
    const char* filenames[SHADER_LOAD_DESC_MAX_FILES_COUNT];
    int filenames_count;
} ShaderLoadDesc;

GLuint rc_shader_load_from_files(ShaderLoadDesc vs_desc,
                                 ShaderLoadDesc fs_desc,
                                 ShaderLoadDesc gs_desc,
                                 ShaderLoadDesc cs_desc);
GLuint rc_shader_load_from_source(const char* vs_src,
                                  const char* fs_src,
                                  const char* gs_src,
                                  const char* cs_src

);

typedef struct Mesh_
{
    int vertices_count;
    Vertex* vertices;

    int indices_count;
    uint* indices;
} Mesh;

void rc_mesh_cleanup(Mesh* mesh);
Mesh rc_mesh_make_raw(int vertices_count, int indices_count);
// TODO: what a stupid naming..
Mesh rc_mesh_make_raw2(int vertices_count,
                       int indices_count,
                       Vertex* vertices,
                       uint* indices);
Mesh rc_mesh_make_quad();
Mesh rc_mesh_make_cube();
Mesh rc_mesh_make_sphere(float radius, int slices_count, int stacks_count);
bool rc_mesh_load_from_obj(Mesh* mesh, const char* filename);

#endif // RESOURCE_H
