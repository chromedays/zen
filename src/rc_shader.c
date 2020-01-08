#include "resource.h"
#include "debug.h"
#include <histr.h>
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

histr_String rc_read_multiple_text_files_at_once(const char** filenames,
                                                 int filenames_count)
{
    histr_String result = NULL;
    if (filenames_count > 0)
    {
        result = histr_make();
        for (int i = 0; i < filenames_count; i++)
        {
            char* text = rc_read_text_file_all(filenames[i]);
            if (text)
            {
                histr_append(result, text);
                free(text);
            }
            else
            {
                PRINTLN("Can't load %s: The file doesn't exist", filenames[i]);
                histr_destroy(result);
                result = NULL;
                break;
            }
        }
    }

    return result;
}

GLuint rc_shader_load_from_files(ShaderLoadDesc vs_desc,
                                 ShaderLoadDesc fs_desc,
                                 ShaderLoadDesc gs_desc,
                                 ShaderLoadDesc cs_desc)
{
    histr_String vs_src = rc_read_multiple_text_files_at_once(
        vs_desc.filenames, vs_desc.filenames_count);
    histr_String fs_src = rc_read_multiple_text_files_at_once(
        fs_desc.filenames, fs_desc.filenames_count);
    histr_String gs_src = rc_read_multiple_text_files_at_once(
        gs_desc.filenames, gs_desc.filenames_count);
    histr_String cs_src = rc_read_multiple_text_files_at_once(
        cs_desc.filenames, cs_desc.filenames_count);

    GLuint result = rc_shader_load_from_source(vs_src, fs_src, gs_src, cs_src);

    histr_destroy(vs_src);
    histr_destroy(fs_src);
    histr_destroy(gs_src);
    histr_destroy(cs_src);

    return result;
}

GLuint rc_shader_load_from_source(const char* vs_src,
                                  const char* fs_src,
                                  const char* gs_src,
                                  const char* cs_src)
{
    GLuint result = 0;

    int shaders_count = 0;
    GLuint shaders[4] = {0};

    if (vs_src && vs_src[0] != '\0')
    {
        PRINTLN("Compiling vertex shader...");
        shaders[shaders_count++] =
            rc_shader_compile(GL_VERTEX_SHADER, &vs_src, 1);
    }
    if (fs_src && fs_src[0] != '\0')
    {
        PRINTLN("Compiling fragment shader...");
        shaders[shaders_count++] =
            rc_shader_compile(GL_FRAGMENT_SHADER, &fs_src, 1);
    }
    if (gs_src && gs_src[0] != '\0')
    {
        PRINTLN("Compiling geometry shader...");
        shaders[shaders_count++] =
            rc_shader_compile(GL_GEOMETRY_SHADER, &gs_src, 1);
    }
    if (cs_src && cs_src[0] != '\0')
    {
        PRINTLN("Compiling compute shader...");
        shaders[shaders_count++] =
            rc_shader_compile(GL_COMPUTE_SHADER, &cs_src, 1);
    }

    if (shaders_count > 0)
    {
        result = rc_shader_link(shaders, shaders_count);

        for (int i = 0; i < shaders_count; ++i)
            glDeleteShader(shaders[i]);
    }

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
