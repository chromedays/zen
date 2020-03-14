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
    float* alpha = new float[n + 1];

    int i = 0;

    // Step 2.

    for (i = 1; i <= n - 1; i++)
        alpha[i] = 3.f * (*xs)[i + 1] - 6.f * (*xs)[i] + 3.f * (*xs)[i - 1];

    // Step 3.

    float* l = new float[n + 1];
    float* u = new float[n + 1];
    float* z = new float[n + 1];
    float* c = new float[n + 1];
    float* b = new float[n + 1];
    float* d = new float[n + 1];

    l[0] = 1;
    u[0] = 0;
    z[0] = 0;

    // Step 4.

    for (i = 1; i <= n - 1; i++)
    {
        l[i] = 4.f - u[i - 1];

        u[i] = 1.f / l[i];

        z[i] = (alpha[i] - z[i - 1]) / l[i];
    }

    // Step 5.

    l[n] = 1;
    z[n] = 0;
    c[n] = 0;

    // Step 6.

    for (i = n - 1; i >= 0; i--)
    {
        c[i] = z[i] - u[i] * c[i + 1];

        b[i] = ((*xs)[i + 1] - (*xs)[i]) - (c[i + 1] + 2 * c[i]) / 3.f;

        d[i] = (c[i + 1] - c[i]) / 3.f;
    }

    for (i = 0; i <= n - 1; i++)
    {
        float x = (float)i;

        float inc = 0.03f;

        for (; x < i + 1; x += inc)
        {
            float x_offset = x - i;

            float Sx = (*xs)[i] + b[i] * x_offset + c[i] * x_offset * x_offset +
                       d[i] * x_offset * x_offset * x_offset;

            out_xs->push_back(Sx);
        }
    }

    delete[] alpha;
    delete[] l;
    delete[] u;
    delete[] z;
    delete[] c;
    delete[] b;
    delete[] d;
}
