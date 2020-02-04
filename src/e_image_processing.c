#include "example.h"
#include "debug.h"
#include "filesystem.h"
#include "resource.h"
#include "renderer.h"
#include "app.h"
#include <stdlib.h>
#include <math.h>

typedef struct Pixel_
{
    float r, g, b, a;
} Pixel;

typedef struct Image_
{
    int w;
    int h;
    int max_color;
    Pixel* pixels;
    uint texture;
} Image;

static void image_update_gl_texture(Image* image);

static void image_init(Image* image, int w, int h, int max_color)
{
    *image = (Image){
        .w = w,
        .h = h,
        .max_color = max_color,
    };
    image->pixels = calloc(w * h, sizeof(*image->pixels));

    glGenTextures(1, &image->texture);
    image_update_gl_texture(image);
}

static Image image_load_from_ppm(const char* filename)
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
            pixels[i].r = (float)ir / (float)max_color;
            pixels[i].g = (float)ig / (float)max_color;
            pixels[i].b = (float)ib / (float)max_color;
            pixels[i].a = 1;
        }
        fclose(f);

        GLuint texture;
        glGenTextures(1, &texture);

        result.w = w;
        result.h = h;
        result.max_color = max_color;
        result.pixels = pixels;
        result.texture = texture;
        image_update_gl_texture(&result);
    }

    return result;
}

static void image_cleanup(Image* image)
{
    if (image->pixels)
        free(image->pixels);
    if (image->texture)
        glDeleteTextures(1, &image->texture);

    *image = (Image){0};
}

static bool image_is_valid(const Image* image)
{
    bool result = (image->w > 0) && (image->h > 0) && (image->pixels);
    return result;
}

static void image_update_gl_texture(Image* image)
{
    glBindTexture(GL_TEXTURE_2D, image->texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, image->w, image->h, 0, GL_RGBA,
                 GL_FLOAT, image->pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
}

static void image_write_to_file(Image* image)
{
    if (!image_is_valid(image))
        return;

    Path output_path = fs_path_make_working_dir();
    fs_path_append3(&output_path, "image_processing", "output", "output.ppm");

    FILE* f = fopen(output_path.abs_path_str, "w");
    fprintf(f, "P3\n%d %d\n%d\n", image->w, image->h, image->max_color);

    for (int i = 0; i < image->w * image->h; i++)
    {
        Pixel p = image->pixels[i];
        fprintf(f, "%d %d %d ", (int)(p.r * image->max_color),
                (int)(p.g * image->max_color), (int)(p.b * image->max_color));
        if (i % 7 == 0)
            fprintf(f, "\n");
    }

    fclose(f);

    fs_path_cleanup(&output_path);
}

static Pixel* image_get_pixel_ref(const Image* image, IVec2 tex_coords)
{
    ASSERT(tex_coords.x >= 0 && tex_coords.x < image->w && tex_coords.y >= 0 &&
           tex_coords.y < image->h);

    Pixel* result = &image->pixels[image->w * tex_coords.y + tex_coords.x];
    return result;
}

static Pixel image_get_pixel_val(const Image* image, IVec2 tex_coords)
{
    Pixel result = *image_get_pixel_ref(image, tex_coords);
    return result;
}

static void image_set_pixel(const Image* image, IVec2 tex_coords, Pixel value)
{
    Pixel* ref = image_get_pixel_ref(image, tex_coords);
    *ref = value;
}

static void
    image_set_pixels_and_update_texture(Image* image, Pixel* pixels, IVec2 size)
{
    if (image->pixels)
        free(image->pixels);
    image->pixels = pixels;
    image->w = size.x;
    image->h = size.y;

    image_update_gl_texture(image);
}

static Pixel image_sample_nearest(const Image* image, FVec2 fuv)
{
    IVec2 uv = {
        (int)roundf(fuv.x * (float)(image->w - 1)),
        (int)roundf(fuv.y * (float)(image->h - 1)),
    };

    Pixel result = image_get_pixel_val(image, uv);
    return result;
}

typedef enum OperationType_
{
    OperationType_Undefined = 0,
    OperationType_Addition = 1,
    OperationType_Subtraction,
    OperationType_Product,
    OperationType_Negative,
    OperationType_Log,
    OperationType_Power,

    OperationType_Count,
} OperationType;

static const char* g_op_str[OperationType_Count] = {
    "", "Addition", "Subtraction", "Product", "Negative", "Log", "Power",
};

typedef struct OperationContext_ OperationContext;

#define OP_TRANSFORM_FN_DECL(name)                                             \
    float name(const OperationContext* op, const Image* images, float* values)
typedef OP_TRANSFORM_FN_DECL(OpTransformFn);

OP_TRANSFORM_FN_DECL(op_transform_add)
{
    float result = HIMATH_CLAMP(values[0] + values[1], 0, 1);
    return result;
}

OP_TRANSFORM_FN_DECL(op_transform_sub)
{
    float result = HIMATH_CLAMP(values[0] - values[1], 0, 1);
    return result;
}

OP_TRANSFORM_FN_DECL(op_transform_product)
{
    float result = HIMATH_CLAMP(values[0] * values[1], 0, 1);
    return result;
}

OP_TRANSFORM_FN_DECL(op_transform_negative)
{
    float result = HIMATH_CLAMP(1.f - values[0], 0, 1);
    return result;
}

OP_TRANSFORM_FN_DECL(op_transform_log);
OP_TRANSFORM_FN_DECL(op_transform_power);

OpTransformFn* g_op_transform_vtable[OperationType_Count] = {
    NULL,
    &op_transform_add,
    &op_transform_sub,
    &op_transform_product,
    &op_transform_negative,
    &op_transform_log,
    &op_transform_power,
};

typedef struct OperationContext_
{
    OperationType type;
    const Path* image_filepaths[2];
    int image_filepaths_count;

    float log_constant;
    float log_base;

    float power_constant;
    float power_gamma;
} OperationContext;

OP_TRANSFORM_FN_DECL(op_transform_log)
{
    if (values[0] == 0)
        return 0;

    float log_2_value = log2f((float)images[0].max_color * values[0]);
    float log_2_base = log2f(op->log_base);

    float result = HIMATH_CLAMP((op->log_constant * log_2_value / log_2_base) /
                                    images[0].max_color,
                                0, 1);
    return result;
}

OP_TRANSFORM_FN_DECL(op_transform_power)
{
    float power_value =
        powf((float)images[0].max_color * values[0], op->power_gamma);
    float result = HIMATH_CLAMP(
        (op->power_constant * power_value) / images[0].max_color, 0, 1);
    return result;
}

static bool op_is_image_selected(const OperationContext* op,
                                 const Path* image_filepath)
{
    for (int i = 0; i < op->image_filepaths_count; i++)
    {
        if (op->image_filepaths[i] == image_filepath)
            return true;
    }
    return false;
}

static void op_push_image_path(OperationContext* op, const Path* image_filepath)
{
    if (op->image_filepaths_count >= ARRAY_LENGTH(op->image_filepaths))
        return;
    op->image_filepaths[op->image_filepaths_count++] = image_filepath;
}

static Image op_execute(const OperationContext* op)
{
    Image result = {0};

    int max_color = 255;

    if (op->image_filepaths_count > 0)
    {
        Image images[ARRAY_LENGTH(op->image_filepaths)] = {0};
        IVec2 result_image_size = {0};
        for (int i = 0; i < op->image_filepaths_count; i++)
        {
            images[i] =
                image_load_from_ppm(op->image_filepaths[i]->abs_path_str);
            result_image_size.x = HIMATH_MAX(result_image_size.x, images[i].w);
            result_image_size.y = HIMATH_MAX(result_image_size.y, images[i].h);
        }

        image_init(&result, result_image_size.x, result_image_size.y,
                   max_color);

        for (int y = 0; y < result.h; y++)
        {
            for (int x = 0; x < result.w; x++)
            {
                FVec2 uv = {
                    result.w <= 1 ? 0 : (float)x / (float)(result.w - 1),
                    result.h <= 1 ? 0 : (float)y / (float)(result.h - 1),
                };
                float r[ARRAY_LENGTH(images)] = {0};
                float g[ARRAY_LENGTH(images)] = {0};
                float b[ARRAY_LENGTH(images)] = {0};

                for (int i = 0; i < op->image_filepaths_count; i++)
                {
                    Pixel c = image_sample_nearest(&images[i], uv);
                    r[i] = c.r;
                    g[i] = c.g;
                    b[i] = c.b;
                }

                image_set_pixel(
                    &result, (IVec2){x, y},
                    (Pixel){
                        .r = g_op_transform_vtable[op->type](op, images, r),
                        .g = g_op_transform_vtable[op->type](op, images, g),
                        .b = g_op_transform_vtable[op->type](op, images, b),
                        .a = 1,
                    });
            }
        }

        image_update_gl_texture(&result);

        for (int i = 0; i < op->image_filepaths_count; i++)
            image_cleanup(&images[i]);
    }

    return result;
}

typedef struct ImageProcessing_
{
    VertexBuffer vb;
    GLuint shader;

    GLuint sampler_nearest;
    GLuint sampler_bilinear;
    GLuint current_sampler;

    OperationContext op;

    bool connectivity_flag[8];
    IVec2 connectivity_direction[8];
    float background_range;

    Path image_filepaths[100];
    int image_filepaths_count;

    Image result_image;
} ImageProcessing;

static FILE_FOREACH_FN_DECL(append_filepath)
{
    ImageProcessing* s = (ImageProcessing*)udata;
    Path* p = &s->image_filepaths[s->image_filepaths_count++];
    *p = fs_path_copy(*file_path);
}

EXAMPLE_INIT_FN_SIG(image_processing)
{
    Example* e = e_example_make("image_processing", ImageProcessing);
    ImageProcessing* s = (ImageProcessing*)e->scene;

    Mesh mesh = rc_mesh_make_quad(2);
    r_vb_init(&s->vb, &mesh, GL_TRIANGLES);
    rc_mesh_cleanup(&mesh);

    s->shader = e_shader_load(e, "image");

    glGenSamplers(1, &s->sampler_nearest);
    glSamplerParameteri(s->sampler_nearest, GL_TEXTURE_WRAP_S,
                        GL_CLAMP_TO_EDGE);
    glSamplerParameteri(s->sampler_nearest, GL_TEXTURE_WRAP_T,
                        GL_CLAMP_TO_EDGE);
    glSamplerParameteri(s->sampler_nearest, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glSamplerParameteri(s->sampler_nearest, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glGenSamplers(1, &s->sampler_bilinear);
    glSamplerParameteri(s->sampler_bilinear, GL_TEXTURE_WRAP_S,
                        GL_CLAMP_TO_EDGE);
    glSamplerParameteri(s->sampler_bilinear, GL_TEXTURE_WRAP_T,
                        GL_CLAMP_TO_EDGE);
    glSamplerParameteri(s->sampler_bilinear, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glSamplerParameteri(s->sampler_bilinear, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    s->current_sampler = s->sampler_nearest;

    Path p = fs_path_make_working_dir();
    fs_path_append2(&p, "image_processing", "images");
    fs_for_each_files_with_ext(p, "ppm", &append_filepath, s);
    fs_path_cleanup(&p);

    s->connectivity_direction[0] = (IVec2){-1, -1};
    s->connectivity_direction[1] = (IVec2){0, -1};
    s->connectivity_direction[2] = (IVec2){1, -1};
    s->connectivity_direction[3] = (IVec2){-1, 0};
    s->connectivity_direction[4] = (IVec2){1, 0};
    s->connectivity_direction[5] = (IVec2){-1, 1};
    s->connectivity_direction[6] = (IVec2){0, 1};
    s->connectivity_direction[7] = (IVec2){1, 1};

    return e;
}

EXAMPLE_CLEANUP_FN_SIG(image_processing)
{
    Example* e = (Example*)udata;
    ImageProcessing* s = (ImageProcessing*)e->scene;
    for (int i = 0; i < s->image_filepaths_count; i++)
        fs_path_cleanup(&s->image_filepaths[i]);
    glDeleteProgram(s->shader);
    r_vb_cleanup(&s->vb);
    // glDeleteTextures(1, &s->texture);
    e_example_destroy(e);
}

EXAMPLE_UPDATE_FN_SIG(image_processing)
{
    Example* e = (Example*)udata;
    ImageProcessing* s = (ImageProcessing*)e->scene;

    igSetNextWindowPos((ImVec2){0, 0}, ImGuiCond_Once, (ImVec2){0, 0});
    /*igSetNextWindowSize((ImVec2){500, (float)input->window_size.y},
                        ImGuiCond_Once);*/
    igBegin("Control Panel", NULL,
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_AlwaysAutoResize);

    if (igCollapsingHeader("I/O", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (image_is_valid(&s->result_image))
        {
            if (igButton("Save", (ImVec2){0}))
                image_write_to_file(&s->result_image);
        }
        else
        {
            igText("Image not generated");
        }
    }

    if (igCollapsingHeader("Texture Sampling Method ",
                           ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (igRadioButtonBool("Nearest",
                              s->current_sampler == s->sampler_nearest))
        {
            s->current_sampler = s->sampler_nearest;
        }
        igSameLine(0, -1);
        if (igRadioButtonBool("Bilinear",
                              s->current_sampler == s->sampler_bilinear))
        {
            s->current_sampler = s->sampler_bilinear;
        }
    }

    if (igCollapsingHeader("Operations", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (igButton("Reset", (ImVec2){0}))
            s->op = (OperationContext){0};

        igTextColored((ImVec4){0, 1, 0, 1}, "Choose Operation");
        if (s->op.type != OperationType_Undefined)
        {
            igSelectable(g_op_str[s->op.type], true, 0, (ImVec2){0});
            igSeparator();
            switch (s->op.type)
            {
            case OperationType_Addition:
            case OperationType_Subtraction:
            case OperationType_Product:
                igTextColored((ImVec4){0, 1, 0, 1}, "Choose Two Images");
                if (s->op.image_filepaths_count == 2)
                {
                    for (int i = 0; i < s->op.image_filepaths_count; i++)
                    {
                        igSelectable(s->op.image_filepaths[i]->filename, true,
                                     0, (ImVec2){0});
                    }
                    igSeparator();
                    if (igButton("Execute", (ImVec2){0}))
                    {
                        image_cleanup(&s->result_image);
                        s->result_image = op_execute(&s->op);
                        s->op = (OperationContext){0};
                    }
                }
                else
                {
                    for (int i = 0; i < s->image_filepaths_count; i++)
                    {
                        if (igSelectable(s->image_filepaths[i].filename,
                                         op_is_image_selected(
                                             &s->op, &s->image_filepaths[i]),
                                         0, (ImVec2){0}))
                        {
                            op_push_image_path(&s->op, &s->image_filepaths[i]);
                        }
                    }
                }
                break;
            case OperationType_Negative:
            case OperationType_Log:
            case OperationType_Power:
                if (s->op.type == OperationType_Log)
                {
                    igSliderFloat("Constant", &s->op.log_constant, 0, 200,
                                  "%.3f", 1);
                    igSliderFloat("Base", &s->op.log_base, 2, 10, "%.3f", 1);
                }
                else if (s->op.type == OperationType_Power)
                {
                    igSliderFloat("Constant", &s->op.power_constant, 0, 5,
                                  "%.3f", 1);
                    igSliderFloat("Gamma", &s->op.power_gamma, 0, 10, "%.3f",
                                  1);
                }

                igTextColored((ImVec4){0, 1, 0, 1}, "Choose an Image");
                if (s->op.image_filepaths_count == 1)
                {
                    for (int i = 0; i < s->op.image_filepaths_count; i++)
                    {
                        igSelectable(s->op.image_filepaths[i]->filename, true,
                                     0, (ImVec2){0});
                    }
                    igSeparator();
                    if (igButton("Execute", (ImVec2){0}))
                    {
                        image_cleanup(&s->result_image);
                        s->result_image = op_execute(&s->op);
                        s->op = (OperationContext){0};
                    }
                }
                else
                {
                    for (int i = 0; i < s->image_filepaths_count; i++)
                    {
                        if (igSelectable(s->image_filepaths[i].filename,
                                         op_is_image_selected(
                                             &s->op, &s->image_filepaths[i]),
                                         0, (ImVec2){0}))
                        {
                            op_push_image_path(&s->op, &s->image_filepaths[i]);
                        }
                    }
                }
                break;
            }
        }
        else
        {
            for (int i = 1; i < OperationType_Count; i++)
            {
                if (igSelectable(g_op_str[i], s->op.type == i, 0, (ImVec2){0}))
                    s->op.type = i;
            }
        }
    }

    if (igCollapsingHeader("CCL", ImGuiTreeNodeFlags_DefaultOpen))
    {
        igTextColored((ImVec4){0, 1, 0, 1}, "Choose an Image");
        if (s->op.image_filepaths_count == 1)
        {
            for (int i = 0; i < s->op.image_filepaths_count; i++)
            {
                igSelectable(s->op.image_filepaths[i]->filename, true, 0,
                             (ImVec2){0});
            }
            igTextColored((ImVec4){0, 1, 0, 1}, "Choose Connectivity");

            if (igRadioButtonBool("##00", s->connectivity_flag[0]))
                s->connectivity_flag[0] = !s->connectivity_flag[0];
            igSameLine(0, 0);
            if (igRadioButtonBool("##01", s->connectivity_flag[1]))
                s->connectivity_flag[1] = !s->connectivity_flag[1];
            igSameLine(0, 0);
            if (igRadioButtonBool("##02", s->connectivity_flag[2]))
                s->connectivity_flag[2] = !s->connectivity_flag[2];
            if (igRadioButtonBool("##10", s->connectivity_flag[3]))
                s->connectivity_flag[3] = !s->connectivity_flag[3];
            igSameLine(0, 0);
            igRadioButtonBool("##11", false);
            igSameLine(0, 0);
            if (igRadioButtonBool("##12", s->connectivity_flag[4]))
                s->connectivity_flag[4] = !s->connectivity_flag[4];
            if (igRadioButtonBool("##20", s->connectivity_flag[5]))
                s->connectivity_flag[5] = !s->connectivity_flag[5];
            igSameLine(0, 0);
            if (igRadioButtonBool("##21", s->connectivity_flag[6]))
                s->connectivity_flag[6] = !s->connectivity_flag[6];
            igSameLine(0, 0);
            if (igRadioButtonBool("##22", s->connectivity_flag[7]))
                s->connectivity_flag[7] = !s->connectivity_flag[7];

            igTextColored((ImVec4){0, 1, 0, 1},
                          "Choose Background Color Range");
            igSliderFloat("##BackgroudnRange", &s->background_range, 0, 1,
                          "%.3f", 1);

            igSeparator();
            if (igButton("Execute", (ImVec2){0}))
            {
                image_cleanup(&s->result_image);

                Image image =
                    image_load_from_ppm(s->op.image_filepaths[0]->abs_path_str);

                int* labels = calloc(image.w * image.h, sizeof(int));
                int last_label = 0;

                int* relations = calloc(image.w * image.h, sizeof(int));

                for (int y = 0; y < image.h; y++)
                {
                    for (int x = 0; x < image.w; x++)
                    {
                        Pixel c = image_get_pixel_val(&image, (IVec2){x, y});
                        if (c.r > s->background_range)
                        {
                            int min_label = last_label + 1;

                            for (int i = 0;
                                 i < ARRAY_LENGTH(s->connectivity_direction);
                                 i++)
                            {
                                if (!s->connectivity_flag[i] ||
                                    s->connectivity_direction[i].x > 0 ||
                                    s->connectivity_direction[i].y > 0)
                                {
                                    continue;
                                }

                                int nx = x + s->connectivity_direction[i].x;
                                int ny = y + s->connectivity_direction[i].y;

                                if (nx >= 0 && nx < image.w && ny >= 0 &&
                                    ny < image.h)
                                {
                                    int nlabel = labels[ny * image.w + nx];
                                    if (nlabel > 0)
                                    {
#if 0
                                        if (min_label == last_label + 1 ||
                                            nlabel > min_label)
                                        {
                                            relations[y * image.w + x] =
                                                relations[y * image.w + x] ==
                                                        0 ?
                                                    nlabel :
                                                    HIMATH_MIN(
                                                        relations[y * image.w +
                                                                  x],
                                                        nlabel);
                                        }
#endif
                                        min_label =
                                            HIMATH_MIN(min_label, nlabel);
                                    }
                                }
                            }

                            if (min_label == last_label + 1)
                            {
                                ++last_label;
                                labels[y * image.w + x] = last_label;
                            }
                            else
                            {
                                labels[y * image.w + x] = min_label;
                            }
                        }
                    }
                }

                for (int y = 0; y < image.h; y++)
                {
                    for (int x = 0; x < image.w; x++)
                    {
                        int min_label = labels[y * image.w + x];

                        for (int i = 0;
                             i < ARRAY_LENGTH(s->connectivity_direction); i++)
                        {
                            if (!s->connectivity_flag[i])
                            {
                                continue;
                            }

                            int nx = x + s->connectivity_direction[i].x;
                            int ny = y + s->connectivity_direction[i].y;

                            if (nx >= 0 && nx < image.w && ny >= 0 &&
                                ny < image.h)
                            {
                                int nlabel = labels[ny * image.w + nx];
                                if (nlabel > 0)
                                    min_label = HIMATH_MIN(nlabel, min_label);
                            }
                        }

                        int* r = &relations[labels[y * image.w + x]];
                        if (*r == 0)
                        {
                            *r = min_label;
                        }
                        else
                        {
                            *r = HIMATH_MIN(*r, min_label);
                        }
                    }
                }

                for (int i = 0; i <= last_label; i++)
                {
                    while (relations[i] != relations[relations[i]])
                    {
                        relations[i] =
                            HIMATH_MIN(relations[i], relations[relations[i]]);
                    }
                }

                for (int y = 0; y < image.h; y++)
                {
                    for (int x = 0; x < image.w; x++)
                    {
                        if (labels[y * image.w + x] > 0)
                        {
                            labels[y * image.w + x] =
                                relations[labels[y * image.w + x]];
                        }
                    }
                }
                for (int y = 0; y < image.h; y++)
                {
                    for (int x = 0; x < image.w; x++)
                    {
                        int min_label = labels[y * image.w + x];

                        for (int i = 0;
                             i < ARRAY_LENGTH(s->connectivity_direction); i++)
                        {
                            if (!s->connectivity_flag[i])
                            {
                                continue;
                            }

                            int nx = x + s->connectivity_direction[i].x;
                            int ny = y + s->connectivity_direction[i].y;

                            if (nx >= 0 && nx < image.w && ny >= 0 &&
                                ny < image.h)
                            {
                                int nlabel = labels[ny * image.w + nx];
                                if (nlabel > 0)
                                    min_label = HIMATH_MIN(nlabel, min_label);
                            }
                        }

                        int* r = &relations[labels[y * image.w + x]];
                        if (*r == 0)
                        {
                            *r = min_label;
                        }
                        else
                        {
                            *r = HIMATH_MIN(*r, min_label);
                        }
                    }
                }

                for (int i = 0; i <= last_label; i++)
                {
                    while (relations[i] != relations[relations[i]])
                    {
                        relations[i] =
                            HIMATH_MIN(relations[i], relations[relations[i]]);
                    }
                }

                for (int y = 0; y < image.h; y++)
                {
                    for (int x = 0; x < image.w; x++)
                    {
                        if (labels[y * image.w + x] > 0)
                        {
                            labels[y * image.w + x] =
                                relations[labels[y * image.w + x]];
                        }
                    }
                }

                for (int y = 0; y < image.h; y++)
                {
                    for (int x = 0; x < image.w; x++)
                    {
                        float label = (float)labels[y * image.w + x];
                        if (label == 0)
                        {
                            image_set_pixel(&image, (IVec2){x, y}, (Pixel){0});
                        }
                        else
                        {
                            image_set_pixel(&image, (IVec2){x, y},
                                            (Pixel){
                                                .r = label * 0.5f,
                                                .g = label * 0.5f,
                                                .b = label * 0.5f,
                                                .a = 1,
                                            });
                        }
                    }
                }

                image_update_gl_texture(&image);

                free(relations);
                free(labels);

                s->result_image = image;
                s->op = (OperationContext){0};
            }
        }
        else
        {
            for (int i = 0; i < s->image_filepaths_count; i++)
            {
                if (igSelectable(
                        s->image_filepaths[i].filename,
                        op_is_image_selected(&s->op, &s->image_filepaths[i]), 0,
                        (ImVec2){0}))
                {
                    op_push_image_path(&s->op, &s->image_filepaths[i]);
                }
            }
        }
    }

    igEnd();

    glViewport(0, 0, input->window_size.x, input->window_size.y);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(s->shader);
    if (s->result_image.texture)
    {
        glBindTextureUnit(0, s->result_image.texture);
        glBindSampler(0, s->current_sampler);
        r_vb_draw(&s->vb);
    }
}
