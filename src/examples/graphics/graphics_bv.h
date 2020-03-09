#ifndef GRAPHICS_BV_H
#define GRAPHICS_BV_H
#include <stdint.h>
#include <math.h>
#include <float.h>

typedef struct bsphere
{
    float r;
} bsphere_t;

static struct bsphere
    calc_bsphere(float* vertices, int vertices_count, int offset, int stride)
{
    float old[3] = {0};
    float r_sq = 0;
    float* p = (float*)((uint8_t*)vertices + offset);
    for (int i = 0; i < vertices_count; i++)
    {
        float new_r_sq = p[0] * p[0] + p[1] * p[1] + p[2] * p[2];
        if (new_r_sq > r_sq)
            r_sq = new_r_sq;
        p = (float*)((uint8_t*)p + stride);
    }

    struct bsphere result = {.r = sqrtf(r_sq)};
    return result;
}

typedef struct aabb
{
    float r[3];
} aabb_t;

static struct aabb
    calc_aabb(float* vertices, int vertices_count, int offset, int stride)
{
    struct bsphere bsphere =
        calc_bsphere(vertices, vertices_count, offset, stride);
    struct aabb result = {.r = {bsphere.r, bsphere.r, bsphere.r}};
    return result;
}

#endif // GRAPHICS_BV_H