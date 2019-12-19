#include "example.h"
#include "resource.h"
#include <histr.h>
#include <stdlib.h>

Example* e_example_make(const char* name, size_t scene_size)
{
    Example* e = calloc(1, sizeof(Example) + scene_size);
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

    e->scene = e + 1;

    return e;
}

void e_example_destroy(Example* e)
{
    glDeleteBuffers(1, &e->per_frame_ubo);
    glDeleteBuffers(1, &e->per_object_ubo);
    free(e);
}

GLuint e_shader_load(const Example* e, const char* shader_name)
{
    const char* shared_src_path = "shared/shaders/shared.glsl";

    histr_String filename_base = histr_makestr(e->name);
    histr_append(filename_base, "/");
    histr_append(filename_base, shader_name);

    histr_String vs_filename = histr_makestr(filename_base);
    histr_append(vs_filename, ".vert");
    histr_String fs_filename = histr_makestr(filename_base);
    histr_append(fs_filename, ".frag");
    GLuint result = rc_shader_load_from_files(vs_filename, fs_filename, NULL,
                                              &shared_src_path, 1);

    histr_destroy(filename_base);
    histr_destroy(vs_filename);
    histr_destroy(fs_filename);

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