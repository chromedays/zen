#ifndef RENDERER_H
#define RENDERER_H
#include <stdint.h>
#include <himath.h>

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

#endif // RENDERER_H