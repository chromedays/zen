#ifndef GRAPHICS_BV_H
#define GRAPHICS_BV_H
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <float.h>

static void float3_copy(float* dst, float* src)
{
    memcpy(dst, src, 3 * sizeof(float));
}

static float float3_length_sq(float* v)
{
    float result = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
    return result;
}

static float float3_length(float* v)
{
    float result = sqrtf(float3_length_sq(v));
    return result;
}

static void float3_add_r(float* result, float* a, float* b)
{
    result[0] = a[0] + b[0];
    result[1] = a[1] + b[1];
    result[2] = a[2] + b[2];
}

static void float3_sub_r(float* result, float* a, float* b)
{
    result[0] = a[0] - b[0];
    result[1] = a[1] - b[1];
    result[2] = a[2] - b[2];
}

static void float3_divf(float* v, float s)
{
    v[0] /= s;
    v[1] /= s;
    v[2] /= s;
}

typedef struct aabb
{
    float c[3];
    float r[3];
} aabb_t;

static struct aabb
    calc_aabb(float* vertices, int vertices_count, int offset, int stride)
{
    float* p = (float*)((uint8_t*)vertices + offset);
    float max_bound[3];
    float3_copy(max_bound, p);
    float min_bound[3];
    float3_copy(min_bound, p);

    for (int i = 1; i < vertices_count; i++)
    {
        p = (float*)((uint8_t*)p + stride);

        for (int j = 0; j < 3; j++)
        {
            if (max_bound[j] < p[j])
                max_bound[j] = p[j];

            if (min_bound[j] > p[j])
                min_bound[j] = p[j];
        }
    }

    struct aabb result = {0};
    float3_add_r(result.c, max_bound, min_bound);
    float3_divf(result.c, 2);
    float3_sub_r(result.r, max_bound, min_bound);
    for (int i = 0; i < 3; i++)
        result.r[i] = fabsf(result.r[i]) * 0.5f;

    return result;
}

static float aabb_volume(struct aabb* aabb)
{
    float result = aabb->r[0] * aabb->r[1] * aabb->r[2] * 8;
    return result;
}

typedef struct bsphere
{
    float c[3];
    float r;
} bsphere_t;

static struct bsphere
    calc_bsphere(float* vertices, int vertices_count, int offset, int stride)
{
    struct aabb aabb = calc_aabb(vertices, vertices_count, offset, stride);
    struct bsphere result = {0};
    float3_copy(result.c, aabb.c);
    result.r = float3_length(aabb.r);

    return result;
}

static float bsphere_volume(struct bsphere* bsphere)
{
    float r = bsphere->r;
    float result = 4.f / 3.f * 3.141592f * r * r * r;
    return result;
}

typedef enum bv_type
{
    bv_type_aabb = 0,
    bv_type_sphere,
    bv_type_count,
} bv_type_t;

typedef struct bvolume
{
    enum bv_type type;
    union
    {
        struct aabb aabb;
        struct bsphere sphere;
    };
} bvolume_t;

#endif // GRAPHICS_BV_H