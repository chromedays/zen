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

static Pixel image_sample_nearest(const Image* image, FVec2 fuv)
{
    IVec2 uv = {
        (int)roundf(fuv.x * (float)(image->w - 1)),
        (int)roundf(fuv.y * (float)(image->h - 1)),
    };

    Pixel result = image_get_pixel_val(image, uv);
    return result;
}

#if 0
typedef enum ImageOperationType_
{
    ImageOperationType_Undefined,
    ImageOperationType_UnaryTransform,
    ImageOperationType_BinaryTransform,
    ImageOperationType_ConnectedComponentLabeling,
    ImageOperationType_Count,
} ImageOperationType;

typedef enum ImageUnaryTransformType_
{
    ImageUnaryTransformType_Undefined,
    ImageUnaryTransformType_Addition,
    ImageUnaryTransformType_Subtraction,
    ImageUnaryTransformType_Product,
    ImageUnaryTransformType_Count,
} ImageUnaryTransformType;

typedef enum ImageBinaryTransformType_
{
    ImageBinaryTransformType_Undefined,
    ImageBinaryTransformType_Negative,
    ImageBinaryTransformType_Log,
    ImageBinaryTransformType_Power,
    ImageBinaryTransformType_Count,
} ImageBinaryTransformType_;
#endif

typedef enum ImageOperationType_
{
    ImageOperationType_Undefined,
    ImageOperationType_Addition,
    ImageOperationType_Subtraction,
    ImageOperationType_Product,
    ImageOperationType_Negative,
    ImageOperationType_Log,
    ImageOperationType_Power,
    ImageOperationType_CCL,
    ImageOperationType_Count,
} ImageOperationType;

typedef struct ImageOperationArgs_
{
    ImageOperationType type;
    union
    {
        struct
        {
            Image other;
        } add_sub_prod;

        struct
        {
            int _;
        } negative;

        struct
        {
            float constant;
            float base;
        } log;

        struct
        {
            float constant;
            float gamma;
        } power;

        struct
        {
            uint8_t neighbor_bits;
            float background_max_intensity;
        } ccl;
    };
} ImageOperationArgs;

#define IMAGE_OPERATION_FN_DECL(name)                                          \
    Image name(Image* image, const ImageOperationArgs* args)
typedef IMAGE_OPERATION_FN_DECL(ImageOperationFn);

IMAGE_OPERATION_FN_DECL(image_operation_binary)
{
    ASSERT((args->type == ImageOperationType_Addition) ||
           (args->type == ImageOperationType_Subtraction) ||
           (args->type == ImageOperationType_Product));

    const Image* other = &args->add_sub_prod.other;
    int common_w = HIMATH_MAX(image->w, other->w);
    int common_h = HIMATH_MAX(image->h, other->h);

    Image result = {0};
    image_init(&result, common_w, common_h, 255);

    for (int y = 0; y < result.h; y++)
    {
        for (int x = 0; x < result.w; x++)
        {
            FVec2 fuv = {
                (float)x / (float)result.w,
                (float)y / (float)result.h,
            };
            Pixel c0 = image_sample_nearest(image, fuv);
            Pixel c1 = image_sample_nearest(other, fuv);
            Pixel result_c = {0};
            switch (args->type)
            {
            case ImageOperationType_Addition:
                result_c = (Pixel){
                    .r = c0.r + c1.r,
                    .g = c0.g + c1.g,
                    .b = c0.b + c1.b,
                    .a = 1,
                };
                break;
            case ImageOperationType_Subtraction:
                result_c = (Pixel){
                    .r = c0.r - c1.r,
                    .g = c0.g - c1.g,
                    .b = c0.b - c1.b,
                    .a = 1,
                };
                break;
            case ImageOperationType_Product:
                result_c = (Pixel){
                    .r = c0.r * c1.r,
                    .g = c0.g * c1.g,
                    .b = c0.b * c1.b,
                    .a = 1,
                };
            default: ASSERT(false);
            }
            image_set_pixel(&result, (IVec2){x, y}, result_c);
        }
    }

    image_update_gl_texture(&result);

    return result;
}

float image_sub_operation_log(const Image* image,
                              float value,
                              float constant,
                              float base)
{
    float result = 0;
    if (value != 0)
    {
        float log_2_value = log2f((float)image->max_color * value);
        float log_2_base = log2f(base);

        result = HIMATH_CLAMP(
            (constant * log_2_value / log_2_base) / image->max_color, 0, 1);
    }

    return result;
}

float image_sub_operation_power(const Image* image,
                                float value,
                                float constant,
                                float gamma)
{
    float power_value = powf((float)image->max_color * value, gamma);
    float result =
        HIMATH_CLAMP((constant * power_value) / image->max_color, 0, 1);
    return result;
}

IMAGE_OPERATION_FN_DECL(image_operation_unary)
{
    ASSERT((args->type == ImageOperationType_Negative) ||
           (args->type == ImageOperationType_Log) ||
           (args->type == ImageOperationType_Power));

    Image result = {0};
    image_init(&result, image->w, image->h, 255);

    for (int y = 0; y < result.h; y++)
    {
        for (int x = 0; x < result.w; x++)
        {
            Pixel c = image_get_pixel_val(image, (IVec2){x, y});
            switch (args->type)
            {
            case ImageOperationType_Negative:
                c.r = 1.f - c.r;
                c.g = 1.f - c.g;
                c.b = 1.f - c.b;
                break;
            case ImageOperationType_Log:
                c.r = image_sub_operation_log(image, c.r, args->log.constant,
                                              args->log.base);
                c.g = image_sub_operation_log(image, c.g, args->log.constant,
                                              args->log.base);
                c.b = image_sub_operation_log(image, c.b, args->log.constant,
                                              args->log.base);
                break;
            case ImageOperationType_Power:
                c.r = image_sub_operation_power(
                    image, c.r, args->power.constant, args->power.gamma);
                c.g = image_sub_operation_power(
                    image, c.g, args->power.constant, args->power.gamma);
                c.b = image_sub_operation_power(
                    image, c.b, args->power.constant, args->power.gamma);
                break;
            }
            image_set_pixel(&result, (IVec2){x, y}, c);
        }
    }

    image_update_gl_texture(&result);

    return result;
}

IMAGE_OPERATION_FN_DECL(image_operation_ccl)
{
    Image result = {0};
    ASSERT(false);
    return result;
}

ImageOperationFn* g_image_operation_vtable[ImageOperationType_Count] = {
    NULL,
    &image_operation_binary,
    &image_operation_binary,
    &image_operation_binary,
    &image_operation_unary,
    &image_operation_unary,
    &image_operation_unary,
    &image_operation_ccl,
};

typedef struct ImageProcessing_
{
    VertexBuffer vb;
    GLuint shader;

    GLuint sampler_nearest;
    GLuint sampler_bilinear;
    GLuint current_sampler;

    Path image_filepaths[100];
    int image_filepaths_count;

    int current_image_filepath_index;
    Image current_image;

    // Args
    Image other_image;

    float log_constant;
    float log_base;

    float power_constant;
    float power_gamma;

    uint8_t connectivity_bits;
    float background_label_max_intensity;
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

    s->current_image_filepath_index = -1;

    return e;
}

EXAMPLE_CLEANUP_FN_SIG(image_processing)
{
    Example* e = (Example*)udata;
    ImageProcessing* s = (ImageProcessing*)e->scene;
    if (image_is_valid(&s->other_image))
        image_cleanup(&s->other_image);
    if (image_is_valid(&s->current_image))
        image_cleanup(&s->current_image);
    for (int i = 0; i < s->image_filepaths_count; i++)
        fs_path_cleanup(&s->image_filepaths[i]);
    glDeleteSamplers(1, &s->sampler_bilinear);
    glDeleteSamplers(1, &s->sampler_nearest);
    glDeleteProgram(s->shader);
    r_vb_cleanup(&s->vb);
    e_example_destroy(e);
}

EXAMPLE_UPDATE_FN_SIG(image_processing)
{
    Example* e = (Example*)udata;
    ImageProcessing* s = (ImageProcessing*)e->scene;

    igSetNextWindowPos((ImVec2){0, 0}, ImGuiCond_Once, (ImVec2){0, 0});
    igBegin("Menus", NULL,
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_AlwaysAutoResize);

    if (igButton("Reset", (ImVec2){0}))
    {
        if (image_is_valid(&s->current_image))
            image_cleanup(&s->current_image);
    }
    igSeparator();

    if (!image_is_valid(&s->current_image))
    {
        igText("Select Target Image");
        igSeparator();
        for (int i = 0; i < s->image_filepaths_count; i++)
        {
            if (igSelectable(s->image_filepaths[i].filename,
                             s->current_image_filepath_index == i,
                             ImGuiSelectableFlags_None, (ImVec2){0}))
            {
                s->current_image =
                    image_load_from_ppm(s->image_filepaths[i].abs_path_str);
                s->current_image_filepath_index = i;
            }
        }
    }
    else
    {
        igSelectable(
            s->image_filepaths[s->current_image_filepath_index].filename, true,
            ImGuiSelectableFlags_None, (ImVec2){0});
    }

#if 0
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
#endif

    igEnd();

    glViewport(0, 0, input->window_size.x, input->window_size.y);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(s->shader);
    if (image_is_valid(&s->current_image))
    {
        glBindTextureUnit(0, s->current_image.texture);
        glBindSampler(0, s->current_sampler);
        r_vb_draw(&s->vb);
    }
}
