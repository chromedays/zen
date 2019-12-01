#ifndef RENDERER_H
#define RENDERER_H
#include <himath.h>
#include <glad/glad.h>
#include <stdint.h>

typedef struct Mesh_ Mesh;

#define UNIFORM_PADXX(x) uint32_t __pad__##x
#define UNIFORM_PADX(x) UNIFORM_PADXX(x)
#define UNIFORM_PAD UNIFORM_PADX(__LINE__)

typedef struct RayTracerSphere_
{
    FVec3 c;
    float r;
} RayTracerSphere;

typedef struct RayTracerView_
{
    FVec3 eye;
    UNIFORM_PAD;
    FVec3 look;
    UNIFORM_PAD;
    FVec2 dims;
    float dist;
    UNIFORM_PAD;
} RayTracerView;

typedef struct RayTracerGlobalUniform_
{
    RayTracerView view;
    RayTracerSphere spheres[100];
    int spheres_count;
} RayTracerGlobalUniform;

typedef struct VertexBuffer_
{
    GLuint vao;
    GLuint vbo;
    GLuint ebo;
    GLuint count; // vertex or index count
    GLuint mode;
} VertexBuffer;

void r_vb_init(VertexBuffer* vb, const Mesh* mesh, GLenum mode);
void r_vb_cleanup(VertexBuffer* vb);
// TODO: Maybe need to belong to renderer rather than resource?
void r_vb_draw(const VertexBuffer* vb);

#endif // RENDERER_H