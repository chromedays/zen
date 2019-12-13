#include "resource.h"
#include "debug.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

char* rc_read_text_file_all(const char* filename)
{
    char* result = NULL;

    if (filename)
    {
        FILE* f = fopen(filename, "r");
        if (f)
        {
            fseek(f, 0, SEEK_END);
            long size = ftell(f);
            rewind(f);

            char* buf = (char*)malloc(size + 1);
            ASSERT(size >= 0);
            size = (long)fread(buf, 1, (size_t)size, f);
            buf[size] = '\0';
            fclose(f);

            result = buf;
        }
    }

    return result;
}

static GLuint
    rc_shader_compile(GLenum type, const char** sources, int sources_count);
static GLuint rc_shader_link(GLuint* shaders, int shaders_count);

GLuint rc_shader_load_from_files(const char* vs_filename,
                                 const char* fs_filename,
                                 const char* cs_filename,
                                 const char** include_filenames,
                                 int includes_count)
{
    char* vs_src = rc_read_text_file_all(vs_filename);
    char* fs_src = rc_read_text_file_all(fs_filename);
    char* cs_src = rc_read_text_file_all(cs_filename);
    char** includes = malloc(includes_count * sizeof(const char*));

    for (int i = 0; i < includes_count; i++)
        includes[i] = rc_read_text_file_all(include_filenames[i]);

    GLuint result = rc_shader_load_from_source(vs_src, fs_src, cs_src, includes,
                                               includes_count);

    if (vs_src)
        free(vs_src);
    if (fs_src)
        free(fs_src);
    if (cs_src)
        free(cs_src);
    for (int i = 0; i < includes_count; i++)
    {
        if (includes[i])
            free(includes[i]);
    }

    free(includes);

    return result;
}

GLuint rc_shader_load_from_source(const char* vs_src,
                                  const char* fs_src,
                                  const char* cs_src,
                                  const char** includes,
                                  int includes_count)
{
    GLuint result = 0;

    int sources_count = includes_count + 1;
    const char** sources =
        (const char**)malloc(sources_count * sizeof(const char*));
    memcpy((void*)sources, (void*)includes,
           includes_count * sizeof(const char*));

    int shaders_count = 0;
    GLuint shaders[3] = {0};

    if (vs_src)
    {
        sources[includes_count] = vs_src;
        shaders[shaders_count++] =
            rc_shader_compile(GL_VERTEX_SHADER, sources, sources_count);
    }
    if (fs_src)
    {
        sources[includes_count] = fs_src;
        shaders[shaders_count++] =
            rc_shader_compile(GL_FRAGMENT_SHADER, sources, sources_count);
    }
    if (cs_src)
    {
        sources[includes_count] = cs_src;
        shaders[shaders_count++] =
            rc_shader_compile(GL_COMPUTE_SHADER, sources, sources_count);
    }

    if (shaders_count > 0)
    {
        result = rc_shader_link(shaders, shaders_count);

        for (int i = 0; i < shaders_count; ++i)
            glDeleteShader(shaders[i]);
    }

    free((void*)sources);

    return result;
}

static GLuint
    rc_shader_compile(GLenum type, const char** sources, int sources_count)
{
    GLuint result = 0;

    GLuint shader = glCreateShader(type);
    glShaderSource(shader, sources_count, sources, NULL);
    glCompileShader(shader);

    GLint compile_result;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_result);
    if (compile_result == GL_TRUE)
    {
        result = shader;
    }
    else
    {
        PRINT("Failed to compile shader!\n");
        GLint info_log_length;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_log_length);
        if (info_log_length > 0)
        {
            GLchar* log_buffer =
                (GLchar*)malloc((info_log_length + 1) * sizeof(GLchar));
            glGetShaderInfoLog(shader, info_log_length, NULL, log_buffer);
            PRINT(log_buffer);
            free(log_buffer);
        }

        glDeleteShader(shader);
    }

    return result;
}

static GLuint rc_shader_link(GLuint* shaders, int shaders_count)
{
    GLuint result = 0;

    GLuint program = glCreateProgram();

    for (int i = 0; i < shaders_count; ++i)
        glAttachShader(program, shaders[i]);

    glLinkProgram(program);

    for (int i = 0; i < shaders_count; ++i)
        glDetachShader(program, shaders[i]);

    GLint link_result;
    glGetProgramiv(program, GL_LINK_STATUS, &link_result);
    if (link_result == GL_TRUE)
    {
        result = program;
    }
    else
    {
        PRINT("Failed to link program!\n");
        GLint info_log_length;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info_log_length);
        if (info_log_length > 0)
        {
            GLchar* log_buffer =
                (GLchar*)malloc((info_log_length + 1) * sizeof(GLchar));
            glGetProgramInfoLog(program, info_log_length, NULL, log_buffer);
            PRINT(log_buffer);
            free(log_buffer);
        }

        glDeleteProgram(program);
    }

    return result;
}
