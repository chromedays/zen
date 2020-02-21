#include "../../example.h"
#include "../../debug.h"
#include "../../filesystem.h"
#include "../../resource.h"
#include "../../renderer.h"
#include "../../app.h"
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
            uint16_t neighbor_bits;
            float background_max_intensity;
        } ccl;
    };
} ImageOperationArgs;

void image_operation_args_cleanup(ImageOperationArgs* args)
{
    switch (args->type)
    {
    case ImageOperationType_Addition:
    case ImageOperationType_Subtraction:
    case ImageOperationType_Product:
        if (image_is_valid(&args->add_sub_prod.other))
            image_cleanup(&args->add_sub_prod.other);
        break;
    }
    *args = (ImageOperationArgs){0};
}

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
                break;
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

IMAGE_OPERATION_FN_DECL(image_operation_ccl)
{
    Image result = {0};

    int* parents = (int*)calloc(image->w * image->h, sizeof(*parents));
    int* ranks = (int*)calloc(image->w * image->h, sizeof(*ranks));
    int* labels = (int*)calloc(image->w * image->h, sizeof(*labels));

    int current_label = 1;

    for (int i = 0; i < image->w * image->h; i++)
    {
        int x = i % image->w;
        int y = i / image->w;

        if (image->pixels[i].r > args->ccl.background_max_intensity)
        {
            int label = current_label;
            int neighbor_labels[9] = {0};
            int neighbor_labels_count = 0;
            for (int j = 0; j < 9; j++)
            {
                if (args->ccl.neighbor_bits & (1 << j))
                {
                    int dx = (j % 3) - 1;
                    int dy = (j / 3) - 1;
                    int neighbor = (y + dy) * image->w + (x + dx);
                    if ((neighbor < i) && (neighbor >= 0) &&
                        (neighbor < (image->w * image->h)) &&
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

    for (int i = 0; i < image->w * image->h; i++)
    {
        labels[i] = disjoint_sets_find(parents, labels[i]);
    }

    Pixel* label_colors =
        (Pixel*)calloc(image->w * image->h, sizeof(*label_colors));

    image_init(&result, image->w, image->h, 255);
    for (int i = 0; i < result.w * result.h; i++)
    {
        Pixel* label_color = &label_colors[labels[i]];
        if ((label_color->r == 0) && (label_color->g == 0) &&
            (label_color->b == 0) && (label_color->a == 0))
        {
            *label_color = (Pixel){
                .r = (float)(rand() % 255) / 255.f,
                .g = (float)(rand() % 255) / 255.f,
                .b = (float)(rand() % 255) / 255.f,
                .a = 1,
            };
        }

        result.pixels[i] = *label_color;
    }
    image_update_gl_texture(&result);

    free(label_colors);
    free(labels);
    free(ranks);
    free(parents);

    return result;
}

typedef struct ImageOperationMeta_
{
    const char* name;
    ImageOperationFn* func;
} ImageOperationMeta;
ImageOperationMeta g_image_operation_meta[ImageOperationType_Count] = {
    {0},
    {"Addition", &image_operation_binary},
    {"Subtraction", &image_operation_binary},
    {"Product", &image_operation_binary},
    {"Negative", &image_operation_unary},
    {"Log", &image_operation_unary},
    {"Power", &image_operation_unary},
    {"CCL", &image_operation_ccl},
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
    ImageOperationArgs args;
    int other_image_filepath_index;
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
    s->other_image_filepath_index = -1;

    return e;
}

EXAMPLE_CLEANUP_FN_SIG(image_processing)
{
    Example* e = (Example*)udata;
    ImageProcessing* s = (ImageProcessing*)e->scene;
    image_operation_args_cleanup(&s->args);
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

static void reset_ui_state(ImageProcessing* s)
{
    image_operation_args_cleanup(&s->args);
    if (image_is_valid(&s->current_image))
        image_cleanup(&s->current_image);
    s->current_image_filepath_index = -1;
    s->other_image_filepath_index = -1;
}

EXAMPLE_UPDATE_FN_SIG(image_processing)
{
    Example* e = (Example*)udata;
    ImageProcessing* s = (ImageProcessing*)e->scene;

    igSetNextWindowPos((ImVec2){0, 0}, ImGuiCond_Once, (ImVec2){0, 0});
    igSetNextWindowSize((ImVec2){300, (float)input->window_size.y},
                        ImGuiCond_Once);
    igBegin("Menus", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

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

    if (igCollapsingHeader("Operation", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (igButton("Reset", (ImVec2){0}))
            reset_ui_state(s);

        if (image_is_valid(&s->current_image))
        {
            igSameLine(0, -1);
            if (igButton("Save", (ImVec2){0}))
                image_write_to_file(&s->current_image);
        }
        igSeparator();

        if (!image_is_valid(&s->current_image) ||
            (s->current_image_filepath_index < 0))
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
                s->image_filepaths[s->current_image_filepath_index].filename,
                true, ImGuiSelectableFlags_None, (ImVec2){0});
            igSeparator();

            if (s->args.type == ImageOperationType_Undefined)
            {
                igText("Select Operation");
                igSeparator();
                for (int i = 1; i < ImageOperationType_Count; i++)
                {
                    if (igSelectable(g_image_operation_meta[i].name,
                                     s->args.type == i,
                                     ImGuiSelectableFlags_None, (ImVec2){0}))
                    {
                        s->args.type = i;
                    }
                }
            }
            else
            {
                igSelectable(g_image_operation_meta[s->args.type].name, true,
                             ImGuiSelectableFlags_None, (ImVec2){0});
                igSeparator();

                bool is_args_complete = false;

                switch (s->args.type)
                {
                case ImageOperationType_Addition:
                case ImageOperationType_Subtraction:
                case ImageOperationType_Product: {
                    if (image_is_valid(&s->args.add_sub_prod.other) &&
                        (s->other_image_filepath_index >= 0))
                        is_args_complete = true;
                    break;
                case ImageOperationType_Negative:
                case ImageOperationType_Log:
                case ImageOperationType_Power:
                case ImageOperationType_CCL: {
                    is_args_complete = true;
                    break;
                }
                }
                }

                switch (s->args.type)
                {
                case ImageOperationType_Addition:
                case ImageOperationType_Subtraction:
                case ImageOperationType_Product: {
                    igPushIDInt(99);
                    if (!is_args_complete)
                    {
                        igText("Select Other Image");
                        igSeparator();
                        for (int i = 0; i < s->image_filepaths_count; i++)
                        {
                            if (igSelectable(s->image_filepaths[i].filename,
                                             s->other_image_filepath_index == i,
                                             ImGuiSelectableFlags_None,
                                             (ImVec2){0}))
                            {
                                s->args.add_sub_prod.other =
                                    image_load_from_ppm(
                                        s->image_filepaths[i].abs_path_str);
                                s->other_image_filepath_index = i;
                            }
                        }
                    }
                    else
                    {
                        igSelectable(
                            s->image_filepaths[s->other_image_filepath_index]
                                .filename,
                            true, ImGuiSelectableFlags_None, (ImVec2){0});
                    }
                    igPopID();
                    break;
                case ImageOperationType_Log: {
                    igSliderFloat("Constant", &s->args.log.constant, 0, 200,
                                  "%.3f", 1);
                    igSliderFloat("Base", &s->args.log.base, 2, 10, "%.3f", 1);
                    break;
                }
                case ImageOperationType_Power: {
                    igSliderFloat("Constant", &s->args.power.constant, 0, 5,
                                  "%.3f", 1);
                    igSliderFloat("Gamma", &s->args.power.gamma, 0, 10, "%.3f",
                                  1);
                    break;
                }
                case ImageOperationType_CCL: {
                    igText("Select Connected Neighbors");
                    igSeparator();
                    for (int i = 0; i < 9; i++)
                    {
                        int x = i % 3;
                        int y = i / 3;

                        bool enabled = false;

                        if ((x == 1) && (y == 1))
                        {
                            enabled = true;
                        }
                        else
                        {
                            enabled = (s->args.ccl.neighbor_bits & (1 << i));
                        }

                        char id[6] = {0};
                        sprintf_s(id, ARRAY_LENGTH(id), "##%d%d", y, x);

                        if (igRadioButtonBool(id, enabled))
                        {
                            s->args.ccl.neighbor_bits ^= (1 << i);
                        }
                        if (x < 2)
                            igSameLine(0, -1);
                    }

                    igText("Background Max Intensity");
                    igSliderFloat("##Background Max Intensity",
                                  &s->args.ccl.background_max_intensity, 0, 1,
                                  "%.3f", 1);

                    break;
                }
                }
                }

                if (is_args_complete)
                {
                    if (igButton("Execute", (ImVec2){0}))
                    {
                        Image result_image =
                            g_image_operation_meta[s->args.type].func(
                                &s->current_image, &s->args);
                        reset_ui_state(s);
                        s->current_image = result_image;
                    }
                }
            }
        }
    }

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
