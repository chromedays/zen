#include <vector>
#include "himath.h"

static void cubic_spline(std::vector<float>* xs, std::vector<float>* out_xs);

extern "C" void calc_cubic_spline(const FVec3* input_points,
                                  int input_points_count,
                                  FVec3** out_points,
                                  int* out_points_count)
{
    std::vector<float> xs(input_points_count);
    std::vector<float> ys(input_points_count);
    for (int i = 0; i < input_points_count; i++)
    {
        xs[i] = input_points[i].x;
        ys[i] = input_points[i].y;
    }

    std::vector<float> out_xs;
    std::vector<float> out_ys;

    cubic_spline(&xs, &out_xs);
    cubic_spline(&ys, &out_ys);

    *out_points = (FVec3*)malloc(out_xs.size() * sizeof(FVec3));
    *out_points_count = (int)out_xs.size();

    for (int i = 0; i < *out_points_count; i++)
    {
        (*out_points)[i].x = out_xs[i];
        (*out_points)[i].y = out_ys[i];
        (*out_points)[i].z = 0;
    }
}

static void cubic_spline(std::vector<float>* xs, std::vector<float>* out_xs)
{
    int n = (int)xs->size() - 1;

    struct helper
    {
        float alpha, l, u, z, c, b, d;
    }* helpers = (struct helper*)malloc((n + 1) * sizeof(struct helper));

    helpers[0].l = 1;
    helpers[0].u = 0;
    helpers[0].z = 0;
    for (int i = 1; i <= n - 1; i++)
    {
        helpers[i].alpha =
            3.f * (*xs)[i + 1] - 6.f * (*xs)[i] + 3.f * (*xs)[i - 1];
        helpers[i].l = 4.f - helpers[i - 1].u;
        helpers[i].u = 1.f / helpers[i].l;
        helpers[i].z = (helpers[i].alpha - helpers[i - 1].z) / helpers[i].l;
    }

    helpers[n].l = 1;
    helpers[n].z = 0;
    helpers[n].c = 0;
    for (int i = n - 1; i >= 0; i--)
    {
        helpers[i].c = helpers[i].z - helpers[i].u * helpers[i + 1].c;

        helpers[i].b = ((*xs)[i + 1] - (*xs)[i]) -
                       (helpers[i + 1].c + 2 * helpers[i].c) / 3.f;

        helpers[i].d = (helpers[i + 1].c - helpers[i].c) / 3.f;
    }

    for (int i = 0; i <= n - 1; i++)
    {
        float inc = 0.03f;
        for (float t = (float)i; t < i + 1; t += inc)
        {
            float t_offset = t - i;

            float x = (*xs)[i] + helpers[i].b * t_offset +
                      helpers[i].c * t_offset * t_offset +
                      helpers[i].d * t_offset * t_offset * t_offset;

            out_xs->push_back(x);
        }
    }

    free(helpers);
}
