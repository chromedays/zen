#include "example.h"
#include "resource.h"
#include "filesystem.h"
#include "debug.h"
#include "app.h"
#include <histr.h>
#include <stb_image.h>
#include <stdlib.h>
#include <math.h>

Example*
    e_example_make_impl(const char* name, size_t scene_size, size_t scene_align)
{
    void* mem = calloc(1, sizeof(Example) + scene_size + scene_align);
    Example* e = (Example*)mem;
    e->name = name;

    glGenBuffers(1, &e->per_frame_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, e->per_frame_ubo);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(ExamplePerFrameUBO), NULL,
                 GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, e->per_frame_ubo);

    glGenBuffers(1, &e->per_object_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, e->per_object_ubo);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(ExamplePerObjectUBO), NULL,
                 GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, e->per_object_ubo);

    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    uint8_t* scene_mem = (uint8_t*)mem + sizeof(Example);
    scene_mem += (scene_align - ((uintptr_t)scene_mem % scene_align));
    e->scene = scene_mem;

    return e;
}

void e_example_destroy(Example* e)
{
    glDeleteBuffers(1, &e->per_frame_ubo);
    glDeleteBuffers(1, &e->per_object_ubo);
    free(e);
}

Mesh e_mesh_load_from_obj(const Example* e, const char* obj_filename)
{
    Path path = fs_path_make_working_dir();
    fs_path_append3(&path, "shared", "models", obj_filename);
    Mesh result = {0};

    ASSERT(rc_mesh_load_from_obj(&result, path.abs_path_str));
    return result;
}

GLuint e_shader_load(const Example* e, const char* shader_name)
{
    PRINTLN("Building shader (%s)...", shader_name);

    Path shared_root_path = fs_path_make_working_dir();
    fs_path_append2(&shared_root_path, "shared", "shaders");

    Path shared_version_path = fs_path_copy(shared_root_path);
    fs_path_append(&shared_version_path, "version.glsl");

    Path shared_vertex_input_path = fs_path_copy(shared_root_path);
    fs_path_append(&shared_vertex_input_path, "vertex_input.glsl");

    Path shared_shared_path = fs_path_copy(shared_root_path);
    fs_path_append(&shared_shared_path, "shared.glsl");

    Path shared_vs_shared_path = fs_path_copy(shared_root_path);
    fs_path_append(&shared_vs_shared_path, "vs_shared.glsl");

    Path example_shader_root_path = fs_path_make_working_dir();
    fs_path_append(&example_shader_root_path, e->name);

    Path vs_filepath = fs_path_copy(example_shader_root_path);
    {
        histr_String vs_filename = histr_makestr(shader_name);
        histr_append(vs_filename, ".vert");
        fs_path_append(&vs_filepath, vs_filename);
        histr_destroy(vs_filename);
    }

    ShaderLoadDesc vs_desc = {
        .filenames =
            {
                shared_version_path.abs_path_str,
                shared_vertex_input_path.abs_path_str,
                shared_shared_path.abs_path_str,
                shared_vs_shared_path.abs_path_str,
                vs_filepath.abs_path_str,
            },
        .filenames_count = 5,
    };

    Path fs_filepath = fs_path_copy(example_shader_root_path);
    {
        histr_String fs_filename = histr_makestr(shader_name);
        histr_append(fs_filename, ".frag");
        fs_path_append(&fs_filepath, fs_filename);
        histr_destroy(fs_filename);
    }

    ShaderLoadDesc fs_desc = {
        .filenames =
            {
                shared_version_path.abs_path_str,
                shared_shared_path.abs_path_str,
                fs_filepath.abs_path_str,
            },
        .filenames_count = 3,
    };

    Path gs_filepath = fs_path_copy(example_shader_root_path);
    {
        histr_String gs_filename = histr_makestr(shader_name);
        histr_append(gs_filename, ".geom");
        fs_path_append(&gs_filepath, gs_filename);
        histr_destroy(gs_filename);
    }

    ShaderLoadDesc gs_desc = {
        .filenames =
            {
                shared_version_path.abs_path_str,
                shared_shared_path.abs_path_str,
                gs_filepath.abs_path_str,
            },
        .filenames_count = 3,
    };

    GLuint result = rc_shader_load_from_files(vs_desc, fs_desc, gs_desc,
                                              (ShaderLoadDesc){0});

    fs_path_cleanup(&gs_filepath);
    fs_path_cleanup(&fs_filepath);
    fs_path_cleanup(&vs_filepath);
    fs_path_cleanup(&example_shader_root_path);
    fs_path_cleanup(&shared_vs_shared_path);
    fs_path_cleanup(&shared_shared_path);
    fs_path_cleanup(&shared_vertex_input_path);
    fs_path_cleanup(&shared_version_path);
    fs_path_cleanup(&shared_root_path);

    return result;
}

GLuint e_texture_load(const Example* e, const char* texture_filename)
{
    GLuint result = 0;

    histr_String full_path = histr_makestr("shared/textures/");
    histr_append(full_path, texture_filename);

    int w, h, cc;
    stbi_uc* pixels = stbi_load(full_path, &w, &h, &cc, STBI_rgb_alpha);
    if (pixels && (cc == 3 || cc == 4))
    {
        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        if (cc == 4)
        {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA,
                         GL_UNSIGNED_BYTE, pixels);
        }
        else
        {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGBA,
                         GL_UNSIGNED_BYTE, pixels);
        }
        glBindTexture(GL_TEXTURE_2D, 0);

        result = texture;
    }
    stbi_image_free(pixels);

    return result;
}

void e_apply_per_frame_ubo(const Example* e, const ExamplePerFrameUBO* data)
{
    glBindBuffer(GL_UNIFORM_BUFFER, e->per_frame_ubo);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(*data), data);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void e_apply_per_object_ubo(const Example* e, const ExamplePerObjectUBO* data)
{
    glBindBuffer(GL_UNIFORM_BUFFER, e->per_object_ubo);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(*data), data);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void e_fpscam_update(ExampleFpsCamera* cam, const Input* input, float speed)
{
    int move_right = 0;
    int move_forward = 0;
    if (a_input_is_key_down(input, Key_W))
        move_forward += 1;
    if (a_input_is_key_down(input, Key_S))
        move_forward -= 1;
    if (a_input_is_key_down(input, Key_A))
        move_right -= 1;
    if (a_input_is_key_down(input, Key_D))
        move_right += 1;

    if (input->mouse_down[1])
    {
        cam->yaw_deg -= (float)input->mouse_delta.x * 0.5f;
        cam->pitch_deg -= (float)input->mouse_delta.y * 0.5f;
        cam->pitch_deg = HIMATH_CLAMP(cam->pitch_deg, -89, 89);
    }

    FVec3 look = e_fpscam_get_look(cam);

    FVec3 up = {0, 1, 0};
    FVec3 right = fvec3_normalize(fvec3_cross(look, up));

    FVec3 movement =
        fvec3_add(fvec3_mulf(right, (float)move_right * speed * input->dt),
                  fvec3_mulf(look, (float)move_forward * speed * input->dt));
    cam->pos = fvec3_add(cam->pos, movement);
}

FVec3 e_fpscam_get_look(const ExampleFpsCamera* cam)
{
    FVec3 look;
    float yaw_rad = degtorad(cam->yaw_deg + 180);
    float pitch_rad = degtorad(cam->pitch_deg);
    look.x = sinf(yaw_rad) * cosf(pitch_rad);
    look.y = sinf(pitch_rad);
    look.z = cosf(yaw_rad) * cosf(pitch_rad);
    look = fvec3_normalize(look);

    return look;
}
