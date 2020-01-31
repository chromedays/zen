#include "example.h"
#include "debug.h"
#include "filesystem.h"
#include "resource.h"
#include "renderer.h"
#include "app.h"
#include <stdlib.h>

typedef struct ImageProcessing_
{
    GLuint texture;
    VertexBuffer vb;
    GLuint shader;
} ImageProcessing;

static GLuint load_ppm(const char* filename)
{
    FILE* f = fopen(filename, "r");
    {
        char c = 0;
        fscanf(f, "P%c\n", &c);
        if (c != '3')
            return 0;
    }

    int c = getc(f);
    while (c == '#')
    {
        while (c != '\n')
            c = getc(f);
        c = getc(f);
    }

    ungetc(c, f);

    int w, h, color_max;
    fscanf(f, "%d%d%d\n", &w, &h, &color_max);

    uint32_t* pixels = malloc(w * h * sizeof(uint32_t));

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
        uint8_t r = (uint8_t)(255.99f * (float)ir / (float)color_max);
        uint8_t g = (uint8_t)(255.99f * (float)ig / (float)color_max);
        uint8_t b = (uint8_t)(255.99f * (float)ib / (float)color_max);
        pixels[i] = ((r << 0) | (g << 8) | (b << 16) | (255 << 24));
    }

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 pixels);
    glBindTexture(GL_TEXTURE_2D, 0);

    free(pixels);
    fclose(f);

    return texture;
}

EXAMPLE_INIT_FN_SIG(image_processing)
{
    Example* e = e_example_make("image_processing", ImageProcessing);
    ImageProcessing* s = (ImageProcessing*)e->scene;
    Path path = fs_path_make_working_dir();
    fs_path_append3(&path, "image_processing", "images", "lena_gray_256.ppm");
    s->texture = load_ppm(path.abs_path_str);
    fs_path_cleanup(&path);

    Mesh mesh = rc_mesh_make_quad(2);
    r_vb_init(&s->vb, &mesh, GL_TRIANGLES);
    rc_mesh_cleanup(&mesh);

    s->shader = e_shader_load(e, "image");

    return e;
}

EXAMPLE_CLEANUP_FN_SIG(image_processing)
{
    Example* e = (Example*)udata;
    ImageProcessing* s = (ImageProcessing*)e->scene;
    glDeleteProgram(s->shader);
    r_vb_cleanup(&s->vb);
    glDeleteTextures(1, &s->texture);
    e_example_destroy(e);
}

EXAMPLE_UPDATE_FN_SIG(image_processing)
{
    Example* e = (Example*)udata;
    ImageProcessing* s = (ImageProcessing*)e->scene;

    glViewport(0, 0, input->window_size.x, input->window_size.y);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(s->shader);
    glBindTextureUnit(0, s->texture);
    r_vb_draw(&s->vb);
}
