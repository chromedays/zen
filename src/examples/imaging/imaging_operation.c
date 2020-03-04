#include "imaging_operation.h"
#include "imaging_model.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static int32_t floor_to_int32(float value)
{
    int32_t result = (int)floorf(value);
    return result;
}

static FVec4 pixel_to_fvec4(const Image* image, Pixel pixel)
{
    FVec4 result = {
        (float)pixel.r / image->max_color,
        (float)pixel.g / image->max_color,
        (float)pixel.b / image->max_color,
        (float)pixel.a / image->max_color,
    };
    return result;
}

static Pixel fvec4_to_pixel(const Image* image, FVec4 v)
{
    Pixel result = {
        floor_to_int32(v.x * image->max_color),
        floor_to_int32(v.y * image->max_color),
        floor_to_int32(v.z * image->max_color),
        floor_to_int32(v.w * image->max_color),
    };
    return result;
}

void pixel_clamp(Pixel* pixel, const Image* image)
{
    pixel->r = HIMATH_CLAMP(pixel->r, 0, image->max_color);
    pixel->g = HIMATH_CLAMP(pixel->g, 0, image->max_color);
    pixel->b = HIMATH_CLAMP(pixel->b, 0, image->max_color);
    pixel->a = HIMATH_CLAMP(pixel->a, 0, image->max_color);
}

Image image_load_from_ppm(const char* filename)
{
    Image result = {0};

    FILE* f = fopen(filename, "r");
    char constant = 0;
    fscanf(f, "P%c\n", &constant);
    if (constant == '3')
    {
        int c = getc(f);
        while (c == '#')
        {
            while (c != '\n')
                c = getc(f);
            c = getc(f);
        }

        ungetc(c, f);

        int w, h, max_color;
        fscanf(f, "%d%d%d\n", &w, &h, &max_color);

        Pixel* pixels = malloc(w * h * sizeof(*pixels));

        for (int i = 0; i < w * h; i++)
        {
            c = getc(f);
            if (c == EOF)
                break;
            ungetc(c, f);

            int ir;
            int ig;
            int ib;
            fscanf(f, "%d%d%d\n", &ir, &ig, &ib);
            pixels[i].r = ir;
            pixels[i].g = ig;
            pixels[i].b = ib;
            pixels[i].a = max_color;
        }
        fclose(f);

        result.size.x = w;
        result.size.y = h;
        result.max_color = max_color;
        result.pixels = pixels;
    }

    return result;
}

Image image_copy(const Image* image)
{
    Image result = {0};
    result.size = image->size;
    result.max_color = image->max_color;
    result.pixels =
        (Pixel*)malloc(result.size.x * result.size.y * sizeof(*result.pixels));
    memcpy(result.pixels, image->pixels,
           result.size.x * result.size.y * sizeof(*result.pixels));
    return result;
}

uint image_make_gl_texture(const Image* image)
{
    FVec4* rgba32f_pixels =
        (FVec4*)malloc(image->size.x * image->size.y * sizeof(*rgba32f_pixels));
    for (int i = 0; i < image->size.x * image->size.y; i++)
        rgba32f_pixels[i] = pixel_to_fvec4(image, image->pixels[i]);

    uint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, image->size.x, image->size.y, 0,
                 GL_RGBA, GL_FLOAT, rgba32f_pixels);
    glBindTexture(GL_TEXTURE_2D, 0);

    free(rgba32f_pixels);

    return texture;
}

void image_cleanup(Image* image)
{
    free(image->pixels);
    image->pixels = NULL;
}

bool image_is_valid(const Image* image)
{
    bool result = (image->size.x > 0) && (image->size.y > 0) && (image->pixels);
    return result;
}

Pixel* image_get_pixel_ref(const Image* image, IVec2 tex_coords)
{
    ASSERT(tex_coords.x >= 0 && tex_coords.x < image->size.x &&
           tex_coords.y >= 0 && tex_coords.y < image->size.y);

    Pixel* result = &image->pixels[image->size.x * tex_coords.y + tex_coords.x];
    return result;
}

Pixel image_get_pixel_val(const Image* image, IVec2 tex_coords)
{
    Pixel result = *image_get_pixel_ref(image, tex_coords);
    return result;
}

void image_set_pixel(const Image* image, IVec2 tex_coords, Pixel value)
{
    Pixel* ref = image_get_pixel_ref(image, tex_coords);
    *ref = value;
}

Pixel image_sample_nearest(const Image* image, FVec2 fuv)
{
    IVec2 uv = {
        (int)roundf(fuv.x * (float)(image->size.x - 1)),
        (int)roundf(fuv.y * (float)(image->size.y - 1)),
    };

    Pixel result = image_get_pixel_val(image, uv);
    return result;
}

static float do_log(float base, float value)
{
    float log_2_base = log2f((float)base);
    float log_2_value = log2f((float)value);
    float result = log_2_value / log_2_base;
    return result;
}

static int disjoint_sets_find(int* parents, int label)
{
    while (parents[label] != 0)
        label = parents[label];

    return label;
}

static void
    disjoint_sets_union(int* parents, int* ranks, int root_a, int root_b)
{
    if (root_a == root_b)
        return;

    int parent;
    int child;
    if (ranks[root_a] > ranks[root_b])
    {
        parent = root_a;
        child = root_b;
    }
    else
    {
        parent = root_b;
        child = root_a;
    }

    parents[child] = parent;
    ranks[child] = 0;
    ++ranks[parent];
}

void image_add(Image* image, const Image* other)
{
    for (int iy = 0; iy < image->size.y; iy++)
    {
        for (int ix = 0; ix < image->size.x; ix++)
        {
            FVec2 uv = {
                (float)ix / (float)image->size.x,
                (float)iy / (float)image->size.y,
            };
            FVec4 f0 = pixel_to_fvec4(
                image, image_get_pixel_val(image, (IVec2){ix, iy}));
            FVec4 f1 = pixel_to_fvec4(other, image_sample_nearest(other, uv));
            FVec4 result_f = fvec4_add(f0, f1);
            Pixel result_p = fvec4_to_pixel(image, result_f);
            pixel_clamp(&result_p, image);
            image_set_pixel(image, (IVec2){ix, iy}, result_p);
        }
    }
}

void image_sub(Image* image, const Image* other)
{
    for (int iy = 0; iy < image->size.y; iy++)
    {
        for (int ix = 0; ix < image->size.x; ix++)
        {
            FVec2 uv = {
                (float)ix / (float)image->size.x,
                (float)iy / (float)image->size.y,
            };
            FVec4 f0 = pixel_to_fvec4(
                image, image_get_pixel_val(image, (IVec2){ix, iy}));
            FVec4 f1 = pixel_to_fvec4(other, image_sample_nearest(other, uv));
            FVec4 result_f = fvec4_sub(f0, f1);
            Pixel result_p = fvec4_to_pixel(image, result_f);
            pixel_clamp(&result_p, image);
            image_set_pixel(image, (IVec2){ix, iy}, result_p);
        }
    }
}

void image_product(Image* image, const Image* other)
{
    for (int iy = 0; iy < image->size.y; iy++)
    {
        for (int ix = 0; ix < image->size.x; ix++)
        {
            FVec2 uv = {
                (float)ix / (float)image->size.x,
                (float)iy / (float)image->size.y,
            };
            FVec4 f0 = pixel_to_fvec4(
                image, image_get_pixel_val(image, (IVec2){ix, iy}));
            FVec4 f1 = pixel_to_fvec4(other, image_sample_nearest(other, uv));
            FVec4 result_f = fvec4_mul(f0, f1);
            Pixel result_p = fvec4_to_pixel(image, result_f);
            pixel_clamp(&result_p, image);
            image_set_pixel(image, (IVec2){ix, iy}, result_p);
        }
    }
}

void image_negate(Image* image)
{
    for (int i = 0; i < image->size.x * image->size.y; i++)
    {
        Pixel* p = &image->pixels[i];
        for (int j = 0; j < ARRAY_LENGTH(p->e); j++)
            p->e[j] = image->max_color - p->e[j];
    }
}

void image_log(Image* image, float constant, float base)
{
    for (int i = 0; i < image->size.x * image->size.y; i++)
    {
        Pixel* p = &image->pixels[i];
        FVec4 f = pixel_to_fvec4(image, *p);
        f.x = constant * do_log(base, f.x);
        f.y = constant * do_log(base, f.y);
        f.z = constant * do_log(base, f.z);
        f.w = constant * do_log(base, f.w);
        *p = fvec4_to_pixel(image, f);
        pixel_clamp(p, image);
    }
}

void image_power(Image* image, float constant, float gamma)
{
    for (int i = 0; i < image->size.x * image->size.y; i++)
    {
        Pixel* p = &image->pixels[i];
        FVec4 f = pixel_to_fvec4(image, *p);
        f.x = constant * powf(f.x, gamma);
        f.y = constant * powf(f.y, gamma);
        f.z = constant * powf(f.z, gamma);
        f.w = constant * powf(f.w, gamma);
        *p = fvec4_to_pixel(image, f);
        pixel_clamp(p, image);
    }
}

void image_ccl(Image* image,
               uint16_t neighbor_bits,
               int background_max_intensity)
{
    int* parents =
        (int*)calloc(image->size.x * image->size.y, sizeof(*parents));
    int* ranks = (int*)calloc(image->size.x * image->size.y, sizeof(*ranks));
    int* labels = (int*)calloc(image->size.x * image->size.y, sizeof(*labels));

    int current_label = 1;

    for (int i = 0; i < image->size.x * image->size.y; i++)
    {
        int x = i % image->size.x;
        int y = i / image->size.x;

        if (image->pixels[i].r > background_max_intensity)
        {
            int label = current_label;
            int neighbor_labels[9] = {0};
            int neighbor_labels_count = 0;
            for (int j = 0; j < 9; j++)
            {
                if (neighbor_bits & (1 << j))
                {
                    int dx = (j % 3) - 1;
                    int dy = (j / 3) - 1;
                    int neighbor = (y + dy) * image->size.x + (x + dx);
                    if ((neighbor < i) && (neighbor >= 0) &&
                        (neighbor < (image->size.x * image->size.y)) &&
                        (labels[neighbor] > 0))
                    {
                        bool should_push_neighbor_label = true;
                        for (int k = 0; k < neighbor_labels_count; k++)
                        {
                            if (neighbor_labels[k] == labels[neighbor])
                            {
                                should_push_neighbor_label = false;
                                break;
                            }
                        }

                        if (should_push_neighbor_label)
                        {
                            neighbor_labels[neighbor_labels_count++] =
                                labels[neighbor];
                        }

                        label = HIMATH_MIN(label, labels[neighbor]);
                    }
                }
            }

            int root = disjoint_sets_find(parents, label);

            for (int j = 0; j < neighbor_labels_count; j++)
            {
                int neighbor_root =
                    disjoint_sets_find(parents, neighbor_labels[j]);
                if (root != neighbor_root)
                    disjoint_sets_union(parents, ranks, root, neighbor_root);
            }

            labels[i] = label;
            if (label == current_label)
                ++current_label;
        }
    }

    for (int i = 0; i < image->size.x * image->size.y; i++)
    {
        labels[i] = disjoint_sets_find(parents, labels[i]);
    }

    Pixel* label_colors =
        (Pixel*)calloc(image->size.x * image->size.y, sizeof(*label_colors));

    for (int i = 0; i < image->size.x * image->size.y; i++)
    {
        Pixel* label_color = &label_colors[labels[i]];
        if ((label_color->r == 0) && (label_color->g == 0) &&
            (label_color->b == 0) && (label_color->a == 0))
        {
            *label_color = (Pixel){
                .r = rand() % 255,
                .g = rand() % 255,
                .b = rand() % 255,
                .a = 1,
            };
        }

        image->pixels[i] = *label_color;
    }

    free(label_colors);
    free(labels);
    free(ranks);
    free(parents);
}

void image_equalize_histogram(Image* image)
{
}

void image_match_histogram(Image* image, const Image* ref)
{
}

void image_process_operations(Image* original,
                              const OpInput* inputs,
                              int inputs_count,
                              OnFinishOpFn* on_finish_op_callback,
                              void* udata)
{
    Image target = image_copy(original);

    for (int i = 0; i < inputs_count; i++)
    {
        const OpInput* this_input = &inputs[i];
        switch (this_input->type)
        {
        case OpType_Add:
            image_add(&target, &this_input->add.ref_image_kv->value);
            break;
        case OpType_Sub:
            image_sub(&target, &this_input->sub.ref_image_kv->value);
            break;
        case OpType_Product:
            image_product(&target, &this_input->product.ref_image_kv->value);
            break;
        }

        if (on_finish_op_callback)
            on_finish_op_callback(&target, i, udata);
    }
}
